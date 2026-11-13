#!/usr/bin/env python3
"""The world package (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 8 and Progress, "Phase 5's plan"):
a world exported as plain files, with every player taken out of its save, for a host's dedicated server.

  python3 tools/world_package.py export --world KEY --out DIR [--no-snapshot] [--version V]
        the world's newest build, its world row and areas, its NPC and economy seed rows, and (unless --no-snapshot)
        its save with the players stripped out; scanned, and written to DIR (which must not exist or be empty).
        Read only: one read-only transaction on the source. Nothing is left in DIR if anything fails.
  python3 tools/world_package.py scan DIR [--source KEY]
        the gate: no player id anywhere, no negative cash, goods within limits (--source: also the source world's
        usernames, and the names its players chose as warnings). Exit 1 if it fails.
  python3 tools/world_package.py pack DIR --out FILE.ratwpkg
  python3 tools/world_package.py unpack FILE.ratwpkg --out DIR
        a package as one file and back: a plain tar of its files (already compressed), as "Share my world" writes it
        (Docs/Design/78, 10.5; M9). Unpacking takes only the package's own file names, plain files, within a size cap.
        scan and import take a .ratwpkg file as well as a directory.
  python3 tools/world_package.py import DIR --key KEY [--name NAME]
        first start: makes world KEY's database (a dev world served as live: Docs/Design/78, 10.2), runs the migrations
        and imports the package in one transaction. Connection settings from the environment (RATW_*, POSTGRES_*;
        RATW_WORLDS_FILE a one-world registry, written if missing; a key the registry lacks is registered first), so it
        works in a container with no Database/.env. A package is untrusted (doc 78, 10.1): before anything is written
        (the registry, the database) it is read within its caps (each part's size packed and unpacked, rows a table,
        bytes a row), its tables and columns held to allowlists, its author columns to the words a package may carry,
        scanned for players as the export is, and its map checked by Atlas's validator (map_editor.check_project: the
        database alone takes ' \\ and ` as ground). A database that already holds a package keeps its world: the
        migrations run, a different package (a newer image's) is not applied, only its new seed rows are added (the
        add-only step: talk tables, voices, the economy's row; never a row or key the world had or has removed).
        Refuses a database holding a world no package made.

The package (format 2; format 1, slice 1's, without the map source, still imports), every part gzip-compressed:
  package.json          format, package id and version, source key, build id and fingerprint, protocol, client-data
                        hash, made_at, what is in it (counts only: never an id or a name)
  world.json.gz         the world row (world.worlds) and its areas (id and kind): NPCs, slots, posts, claims and the
                        spawn point have keys into world.areas
  live.json.gz          the seed rows: world_sync's live tables (spawned NPCs left out, patrol posts in their routes;
                        an NPC's authored band with it) and live.economy's kept columns (Docs/Design/79-a-staged-
                        economy.md, 13 and 14: the starting amounts, needs, seasons, services, and the rules with the
                        host's overrides: the slider's cause, the goods no ship brings, status.json's and market.json's
                        values; their words scanned for player ids and the forbidden word), as world_sync writes rows
                        (bytes and timestamps tagged)
  terrain.json.gz       the map's source, its ground (format 2; doc 78, 10.1 and M7): world.terrain_chunks, each
                        chunk's heights as world_store.encode_heights rows ({"$dense": [rows]}: about a fiftieth of the
                        "x,y" object; a chunk whose heights don't fit keeps its object)
  map.json.gz           the rest of the map's source (format 2): world.cells, interiors, links, resources, places,
                        chapters and minimaps, as Atlas holds them, so the importing world is a dev world Atlas edits
  build.tar.gz          the newest build as world_build.export writes it: world.ratw, cells/<id>.cell, seams/<id>
                        (still shipped: a first start doesn't pay for a build)
  snapshot.json.gz      the save (game.load_checkpoint, the journal after it replayed), stripped of players; without
                        its Prosperity report (society.report: the importing server works it out again)
  SHA256SUMS            sha256 of every file above

Out: every game.* table (the snapshot is one document, imported with game.save_checkpoint), dm.*, admin.*, Atlas's
edit log and editors (world.edits, world.editors), old builds, live.npc_state (newer rows would overwrite purses),
live.game_changes, keys and passwords. The scan (as the export runs it) also holds every name the source's author
columns and accounts give (world.builds, world.edits, world.editors, the DM tool's accounts, actions and audit,
admin's releases and server state, Database/dm-accounts.txt, this computer's login) out of the package, and the
shipped author columns (a voice's writer, a visitor's approver, a relation's updater) to the words a package carries.

The strip is a table (SECTIONS) of small functions, one for each top-level section of the save. A section the table
doesn't name stops the export (a section added to the game later can't slip through unstripped) unless it is listed
in KNOWN_SAFE. Players go by deletion (Docs/Design/79-a-staged-economy.md, 13): a player's purse, and the escrow a
player's things held (chapter:, contract:, letter:...@wolf- and a letter to a player, let:<lodging>, stall:, table:,
the player coins in a bandits: hoard), are deleted with their goods, never poured into the treasury. The staged
accounts (residents' purses, till:, stores:, town:, home:) are derived, so they are dropped too and the importing
server makes them at load; only the treasury is kept. Accounts under the dead prefixes (caravan:, fund:, bank:,
outside:, farm:, crew:, estate:, house:) go. Maker marks of players (item@wolf-...) are taken off items everywhere, the
goods merged within the 64 kinds and 10,000 a kind an account may hold. Society::restore lets a leftover player
account through, so only the scan catches an incomplete strip.
"""
from __future__ import annotations

import argparse
import copy
import datetime
import gzip
import hashlib
import io
import json
import os
import re
import shutil
import sys
import tarfile
import tempfile
import uuid
from pathlib import Path

import economy_contract
import world_db as W

ROOT = Path(__file__).resolve().parent.parent
FORMAT = 2
READS = (1, 2)                      # Format 1: slice 1's packages, without the map source (read as ever).
PARTS = ('world.json.gz', 'live.json.gz', 'build.tar.gz', 'snapshot.json.gz', 'terrain.json.gz', 'map.json.gz')
NEEDED = ('world.json.gz', 'live.json.gz', 'build.tar.gz')
SOURCE_PARTS = ('terrain.json.gz', 'map.json.gz')   # (Format 2's: the map's source, doc 78, 10.1.)
SUMS = 'SHA256SUMS'
MANIFEST = 'package.json'
SETTING = 'package'                 # admin.settings key recording the package a database was made from.

# Player ids (doc 77 Phase 5's plan): characters, development sessions' players, central accounts, dev-identity keys.
ID_PATTERN = r'wolf-[0-9a-f]{32}|player-[a-z0-9_-]{2,32}|acct-[0-9a-f]{32}|dev:'
# Chapters are all player-made (doc 32): every reference to one goes with them.
CHAPTER_PATTERN = r'chapter-\d+'
PLAYER = re.compile(f'{ID_PATTERN}|{CHAPTER_PATTERN}')
MAX_GOODS_KINDS = 64                # Society::MaxGoodsKinds
MONEY_LIMIT = 1_000_000_000         # Society::MoneyLimit (RatwSociety.cpp)
STOCK_LIMIT = 10000                 # Society::StockLimit
TREASURY = 'treasury'
# Escrow and player-owned facility accounts that go wholesale (doc 54's stakes; doc 32's Chapter treasuries).
CLOSED_PREFIXES = ('chapter:', 'table:')
# The account kinds (doc 79, 9.1; Society::classify): escrow holds players' coins or staged-sourced ones and stays (less
# what a player's things held); the dead prefixes' accounts are dropped at restore, so never packaged; everything else
# but a player's (real) is staged: derived, made again by the importing server.
ESCROW_PREFIXES = ('contract:', 'letter:', 'let:', 'stall:', 'table:', 'fest:', 'project:', 'ground:', 'bandits:')
DEAD_PREFIXES = ('caravan:', 'fund:', 'bank:', 'outside:', 'farm:', 'crew:', 'estate:', 'house:')


class PackageError(RuntimeError):
    pass


# --------------------------------------------------------------------------- The strip

class Strip:
    """One strip of one save document: what it closes and how much it removed, section by section."""

    def __init__(self, doc: dict):
        self.doc = doc
        self.removed: dict[str, int] = {}          # Entries dropped or fields cleared, by section.
        self.found: dict[str, int] = {}            # Distinct player ids in each section before the strip.
        self.closing: set[str] = set()             # Accounts a player's things held: deleted with their contents.
        self.letters: set[str] = set()             # Letters (people.documents) dropped.
        self.discarded = 0                         # Goods a maker mark's merge found no room for.
        self.closed = 0                            # Players' accounts and their escrow deleted.
        self.dropped = 0                           # Staged and dead accounts dropped (the server makes the staged again).
        self.usernames = {e.get('username') for e in (doc.get('accounts') or {}).get('entries', [])
                          if isinstance(e, dict) and isinstance(e.get('username'), str)}
        # Names players chose, looked for in free text as warnings: characters', Chapters' and their camps'.
        self.names = {p.get('name') for p in doc.get('players', []) if isinstance(p, dict) and p.get('name')}
        chapters, camps = doc.get('chapters'), doc.get('camps')
        self.names |= {c.get('name') for c in (chapters.get('chapters', []) if isinstance(chapters, dict) else [])
                       if isinstance(c, dict) and isinstance(c.get('name'), str) and c.get('name')}
        self.names |= {s.get('name') for s in (camps.get('sites', []) if isinstance(camps, dict) else [])
                       if isinstance(s, dict) and not town_site(s) and isinstance(s.get('name'), str) and s.get('name')}
        self.camp_staff: set[str] = set()          # NPCs who worked at a camp that goes (their "working at <camp>").
        self.lodgings: set[str] | None = None      # The lodgings kept (None: the save has no lodgings).
        day = (doc.get('society') or {}).get('careers', {}).get('day')
        self.day = day if isinstance(day, (int, float)) else int(doc.get('calendarDays') or 0)

    def count(self, section: str, n: int = 1):
        if n:
            self.removed[section] = self.removed.get(section, 0) + n

    def close(self, account: str):
        self.closing.add(account)


REMOVE = object()


def town_site(site) -> bool:
    """camp::townSite: a camp site a town owns (doc 57's projects stand on one), not a Chapter's."""
    return isinstance(site, dict) and isinstance(site.get('chapter'), str) and site['chapter'].startswith('town:')


def hit(text) -> bool:
    return isinstance(text, str) and bool(PLAYER.search(text))


def mentions(value) -> bool:
    """Whether a value names a player (or a Chapter) anywhere, in a key or a string."""
    if isinstance(value, str):
        return bool(PLAYER.search(value))
    if isinstance(value, dict):
        return any(PLAYER.search(k) or mentions(v) for k, v in value.items())
    if isinstance(value, list):
        return any(mentions(v) for v in value)
    return False


def unmarked(item: str) -> str:
    """items::unmarked: an item without its maker's mark."""
    return item.split('@', 1)[0]


def prune(value, st: Strip, section: str):
    """A relational section without what names a player: a list entry or a map entry that names one anywhere goes
    whole (a bond, a belief, a memory, a regard pair...); maps inside maps are pruned in turn."""
    if isinstance(value, list):
        kept = [v for v in value if not mentions(v)]
        st.count(section, len(value) - len(kept))
        return kept
    if not isinstance(value, dict):
        return value
    out = {}
    for k, v in value.items():
        if PLAYER.search(k) or (isinstance(v, str) and PLAYER.search(v)):
            st.count(section)
        elif isinstance(v, (list, dict)):
            out[k] = prune(v, st, section)
        else:
            out[k] = v
    return out


def scrub(value, st: Strip, section: str, add_merged: bool = False):
    """A record that stays (an NPC, a kept storyline) with the players taken out of it: a player's maker mark comes
    off an item (merged with the unmarked ones), a string naming a player is cleared (""), and a list entry or a map
    key naming one goes."""
    if isinstance(value, str):
        if not PLAYER.search(value):
            return value
        base, at, maker = value.partition('@')
        st.count(section)
        if at and base and PLAYER.search(maker) and not PLAYER.search(base):
            return base
        return REMOVE
    if isinstance(value, list):
        out = []
        for v in value:
            v = scrub(v, st, section, add_merged)
            if v is not REMOVE:
                out.append(v)
        return out
    if isinstance(value, dict):
        out = {}
        for k, v in value.items():
            key = scrub(k, st, section) if PLAYER.search(k) else k
            if key is REMOVE:
                continue
            v = scrub(v, st, section, add_merged)
            if v is REMOVE:
                v = ''
            if key in out:
                if add_merged and isinstance(out[key], (int, float)) and isinstance(v, (int, float)):
                    out[key] = min(STOCK_LIMIT, out[key] + v)
            else:
                out[key] = v
        return out
    return value


def emptied(value):
    """A section of players' own records emptied, keeping its shape and counters: lists and maps empty, numbers kept.
    For sections whose keys are field names (parties, chapters, standing); a key or a value naming a player goes."""
    if isinstance(value, list):
        return []
    if isinstance(value, dict):
        return {k: ([] if isinstance(v, list) else {} if isinstance(v, dict) else v) for k, v in value.items()
                if not PLAYER.search(k) and not hit(v)}
    return value


def _size(value) -> int:
    if isinstance(value, (list, dict)):
        return len(value)
    return 0


# Each section's strip: f(doc, key, st) changes doc[key] in place (or removes it).

def drop_section(doc, key, st):
    value = doc.pop(key)
    st.count(key, _size(value.get('entries')) if isinstance(value, dict) else 0)   # (accounts: one a login.)


def empty_section(doc, key, st):
    before = doc[key]
    st.count(key, sum(_size(v) for v in before.values()) if isinstance(before, dict) else _size(before))
    doc[key] = emptied(before)


def clear_section(doc, key, st):
    """A map keyed by player (commandReceipts, scenesHeard, aliases...) or naming one in every value (companions:
    resident -> the player who hired them): nothing in it is the world's, so all of it goes."""
    before = doc[key]
    st.count(key, _size(before))
    doc[key] = {} if isinstance(before, dict) else [] if isinstance(before, list) else before


def prune_section(doc, key, st):
    doc[key] = prune(doc[key], st, key)


def scrub_section(doc, key, st):
    doc[key] = scrub(doc[key], st, key)


def keep_section(doc, key, st):
    pass


def strip_journal(doc, key, st):
    doc[key] = 0                    # Imported with an empty game.journal: nothing after the checkpoint.


def strip_npcs(doc, key, st):
    out = []
    for e in doc[key]:
        e = scrub(e, st, key)
        if isinstance(e, dict) and e.get('id') in st.camp_staff and e.get('activity'):
            e['activity'] = ''              # ("working at <the camp's name>": a name a player chose.)
            st.count(key)
        out.append(e)
    doc[key] = out


def strip_camps(doc, key, st):
    """A Chapter's camps go with the Chapter (doc 32); a town's own sites (owner "town:<town>": doc 57's projects stand
    on them) stay, with their structures, staff and offers."""
    camps = doc[key]
    if not isinstance(camps, dict):
        return empty_section(doc, key, st)
    kept = set()
    sites = []
    for site in camps.get('sites', []):
        if town_site(site) and not mentions(site):
            sites.append(site)
            kept.add(site.get('id'))
        else:
            st.count(key)
    camps['sites'] = sites
    for part in ('structures', 'staff', 'offers'):
        entries = camps.get(part)
        if not isinstance(entries, list):
            continue
        keep = []
        for entry in entries:
            if isinstance(entry, dict) and entry.get('site') in kept and not mentions(entry):
                keep.append(entry)
                continue
            st.count(key)
            if part == 'staff' and isinstance(entry, dict) and isinstance(entry.get('npc'), str):
                st.camp_staff.add(entry['npc'])
        camps[part] = keep
    if isinstance(camps.get('offered'), dict):
        offered = camps['offered']
        camps['offered'] = {site: day for site, day in offered.items() if site in kept}
        st.count(key, len(offered) - len(camps['offered']))
    for k in list(camps):
        if k not in ('sites', 'structures', 'staff', 'offers', 'offered') and isinstance(camps[k], (list, dict)):
            camps[k] = prune(camps[k], st, key)


def strip_audiences(doc, key, st):
    """socialAudiences ("<session>|<member>" -> who last heard them): only for the sessions kept (socialSessions
    runs first), without players."""
    audiences = doc[key]
    if not isinstance(audiences, dict):
        return clear_section(doc, key, st)
    sessions = {s.get('id') for s in doc.get('socialSessions', []) if isinstance(s, dict)}
    out = {}
    for k, v in audiences.items():
        if k.split('|', 1)[0] in sessions and not PLAYER.search(k):
            out[k] = prune(v, st, key)
        else:
            st.count(key)
    doc[key] = out


def strip_chapters(doc, key, st):
    for chapter in doc[key].get('chapters', []):
        if isinstance(chapter, dict) and chapter.get('id'):
            st.close(f'chapter:{chapter["id"]}')
    empty_section(doc, key, st)


def strip_crime(doc, key, st):
    crime = doc[key]
    kept, gone = [], set()
    for incident in crime.get('incidents', []):
        if hit(incident.get('offender')) or hit(incident.get('victim')):
            gone.add(incident.get('id'))
            st.count(key)
            continue
        witnesses = incident.get('witnesses', [])
        incident['witnesses'] = [w for w in witnesses if not mentions(w)]
        st.count(key, len(witnesses) - len(incident['witnesses']))
        kept.append(scrub(incident, st, key))
    crime['incidents'] = kept
    warrants = []
    for warrant in crime.get('warrants', []):
        if hit(warrant.get('person')):
            st.count(key)
            continue
        had = warrant.get('incidents', [])
        warrant['incidents'] = [i for i in had if i not in gone]
        owed = warrant.get('restitution', [])
        warrant['restitution'] = [r for r in owed if not hit(r.get('to'))]
        st.count(key, len(owed) - len(warrant['restitution']))
        if had and not warrant['incidents']:
            st.count(key)                # (A warrant that rested only on what a player suffered.)
            continue
        warrants.append(scrub(warrant, st, key))
    crime['warrants'] = warrants
    custody = crime.get('custody', [])
    crime['custody'] = [c for c in custody if not hit(c.get('person'))]
    st.count(key, len(custody) - len(crime['custody']))
    for k in list(crime):
        if k not in ('incidents', 'warrants', 'custody'):
            crime[k] = prune(crime[k], st, key) if isinstance(crime[k], (list, dict)) else crime[k]


def strip_roads(doc, key, st):
    roads = doc[key]
    contracts = []
    for c in roads.get('contracts', []):
        if hit(c.get('poster')):
            st.close(f'contract:{c.get("id")}')
            st.count(key)
            continue
        if hit(c.get('taker')):
            c['taker'] = ''
            if c.get('status') == 'taken':
                c['status'] = 'open'
            st.count(key)
        if hit(c.get('offeredTo')):
            c.pop('offeredTo', None)
            c.pop('offeredUntil', None)
            st.count(key)
        contracts.append(scrub(c, st, key))
    roads['contracts'] = contracts
    caravans = []
    for t in roads.get('caravans', []):
        escorts = t.get('escorts', [])
        t['escorts'] = [e for e in escorts if not hit(e)]
        st.count(key, len(escorts) - len(t['escorts']))
        letters = t.get('letters', [])
        t['letters'] = [x for x in letters if x not in st.letters and not hit(x)]
        st.count(key, len(letters) - len(t['letters']))
        t['with'] = prune(t.get('with', {}), st, key)
        caravans.append(scrub(t, st, key))
    roads['caravans'] = caravans
    for k in list(roads):
        if k not in ('contracts', 'caravans'):
            roads[k] = prune(roads[k], st, key) if isinstance(roads[k], (list, dict)) else roads[k]


# people.*: players' own records emptied; the rest kept without the players.
PEOPLE_EMPTY = ('accounts', 'profiles', 'safety', 'friends', 'requests', 'inbox', 'known', 'recaps', 'circles',
                'starTallies', 'stars', 'books', 'ties', 'vouches', 'evenings', 'storytellers', 'storytellerLog',
                'credits')
PEOPLE_PRUNE = ('deeds', 'nicknames', 'troublesHeard', 'troublesSolved', 'troublesResting', 'protectedMarks', 'loans',
                'holds')


def people_lodgings(people, st, section):
    """A player's lodging goes, and its chest ("let:<id>", doc 54) with it. Its `account` is the
    landlord's (the till, the town's stores or the resident lodger's own purse the rent is paid into:
    Core/RatwLodgings.cpp), which stays. A lodging someone else holds stays, without the players among its guests."""
    kept = []
    for lodging in people['lodgings']:
        if not isinstance(lodging, dict):
            continue
        if hit(lodging.get('holder')) or not lodging.get('holder'):
            st.close(f'let:{lodging.get("id")}')
            st.count(section)
            continue
        if mentions(lodging):
            lodging = scrub(lodging, st, section)
        kept.append(lodging)
    people['lodgings'] = kept
    st.lodgings = {lodging.get('id') for lodging in kept}


def people_stalls(people, st, section):
    kept = []
    for stall in people['stalls']:
        if mentions(stall):
            st.close(f'stall:{stall.get("id")}')
            st.count(section)
        else:
            kept.append(stall)
    people['stalls'] = kept


def people_documents(people, st, section):
    kept = []
    for letter in people['documents']:
        if mentions(letter):
            st.letters.add(letter.get('id'))
            st.count(section)
        else:
            kept.append(letter)
    people['documents'] = kept


def people_projects(people, st, section):
    out = []
    for project in people['projects']:
        if isinstance(project, dict):
            if 'gifts' in project:
                project['gifts'] = prune(project['gifts'], st, section)
            if hit(project.get('namedFor')):
                project['namedFor'] = ''
                st.count(section)
            project = scrub(project, st, section)
        out.append(project)
    people['projects'] = out


def people_storylines(people, st, section):
    kept = []
    for story in people['storylines']:
        if not isinstance(story, dict):
            continue
        owners = [story.get(f) for f in ('owner', 'authorAccount', 'author', 'chapter')]
        if any(hit(o) or (isinstance(o, str) and o in st.usernames) for o in owners) or story.get('chapter'):
            st.count(section)
            continue
        for k, v in list(story.items()):
            story[k] = prune(v, st, section) if isinstance(v, (list, dict)) and k != 'steps' else v
        kept.append(scrub(story, st, section))
    people['storylines'] = kept


PEOPLE_SPECIAL = {'lodgings': people_lodgings, 'stalls': people_stalls, 'documents': people_documents,
                  'projects': people_projects, 'storylines': people_storylines}


def strip_people(doc, key, st):
    people = doc[key]
    unknown = sorted(set(people) - set(PEOPLE_EMPTY) - set(PEOPLE_PRUNE) - set(PEOPLE_SPECIAL))
    if unknown:
        raise PackageError(f'The save\'s people section has parts the strip doesn\'t know: {", ".join(unknown)}. '
                           'Add them to tools/world_package.py (PEOPLE_EMPTY, PEOPLE_PRUNE or PEOPLE_SPECIAL).')
    # (Letters first: a caravan carrying one and the escrow follow them.)
    for name in ('documents', 'lodgings', 'stalls', 'projects', 'storylines'):
        if name in people:
            PEOPLE_SPECIAL[name](people, st, key)
    for name in PEOPLE_EMPTY:
        if name in people:
            st.count(key, _size(people[name]))
            people[name] = emptied(people[name])
    for name in PEOPLE_PRUNE:
        if name in people:
            people[name] = prune(people[name], st, key)


def strip_careers(careers, st, section):
    positions = {}
    for pos, p in careers.get('positions', {}).items():
        if PLAYER.search(pos):
            st.count(section)
            continue
        if isinstance(p, dict):
            for field in ('holder', 'apprentice', 'lastHolder'):
                if hit(p.get(field)):
                    p[field] = ''
                    if field == 'holder':
                        p['vacantSince'] = st.day
                    st.count(section)
        positions[pos] = p
    careers['positions'] = positions
    for k in list(careers):
        if k != 'positions' and isinstance(careers[k], (list, dict)):
            careers[k] = prune(careers[k], st, section)


def pour(into: dict, stock: dict, st: Strip):
    """Goods into an account, as many as it may hold (64 kinds, 10,000 a kind); the rest is discarded. Goods only: no
    coin is ever moved by the strip (doc 79, 13)."""
    have = into.setdefault('stock', {})
    for item, n in stock.items():
        item = unmarked(item) if hit(item) else item
        if not isinstance(n, (int, float)) or n <= 0:
            continue
        if item not in have and len(have) >= MAX_GOODS_KINDS:
            st.discarded += n
            continue
        room = STOCK_LIMIT - have.get(item, 0)
        have[item] = have.get(item, 0) + min(n, room)
        st.discarded += max(0, n - room)


def account_kind(name: str) -> str:
    """Society::classify (doc 79, 9.1): 'real' (a player's purse, a Chapter's treasury), 'escrow', 'dead' or 'staged'
    (the treasury, stores:, till:, town:, home: and residents' purses)."""
    if PLAYER.search(name) and not name.startswith(ESCROW_PREFIXES) or name.startswith('chapter:'):
        return 'real'
    if name.startswith(ESCROW_PREFIXES):
        return 'escrow'
    if name.startswith(DEAD_PREFIXES):
        return 'dead'
    return 'staged'


# The staged state's parts keyed by account holders (usernames, which the player pattern can't see): never packaged.
STAGED_HOLDERS = ('faucets', 'welcomed')
# The society's state of the conserving economy (doc 79, 14: read past at restore): never packaged. Nor its Prosperity
# report (doc 79, Phase 9: the DM's tab's, worked out again by the importing server; it counts households by market).
DEAD_SOCIETY = ('loop', 'orchestrator', 'report')


def strip_staged(staged: dict, accounts: dict, st: Strip, section: str):
    """The staged state (doc 79, 6 and 9; society.staged): the faucet tallies and the welcomes (keyed by account
    holders) go whole; holds, lean days and escrow sources only for the accounts kept; a household lifted by a player's
    help is lifted no longer. Bands, overrides and events stay (they name households and residents)."""
    for k in STAGED_HOLDERS:
        if staged.pop(k, None):
            st.count(section)
    for k in ('holds', 'lean', 'escrowSource'):
        part = staged.get(k)
        if isinstance(part, dict):
            staged[k] = {name: v for name, v in part.items() if name in accounts}
            st.count(section, len(part) - len(staged[k]))
    for band in (staged.get('bands') or {}).values():
        if isinstance(band, dict) and isinstance(band.get('liftedUntil'), (int, float)) and band['liftedUntil'] >= 0:
            band['liftedUntil'] = -1
            st.count(section)


def strip_society(doc, key, st):
    society = doc[key]
    accounts = society.get('accounts', {})
    staged = society.get('staged') if isinstance(society.get('staged'), dict) else {}
    sources = staged.get('escrowSource') if isinstance(staged.get('escrowSource'), dict) else {}
    closing = set(st.closing)
    for name in accounts:
        if name == TREASURY:
            continue
        if account_kind(name) == 'real' or name.startswith(CLOSED_PREFIXES):
            closing.add(name)
        elif name.startswith('let:') and name[4:] not in (st.lodgings or ()):
            # A chest whose lodging is gone: a lodging given its id again (nextLodging_ counts from those kept) would
            # find what the last renter left in it.
            closing.add(name)
        elif name.startswith('letter:') and (hit(name) or name[7:].split('@', 1)[0] in st.letters):
            closing.add(name)               # (A letter that went: a player's, or a resident's to a player.)
    closed = set()
    for name in sorted(closing):
        if name in accounts and name != TREASURY:
            del accounts[name]              # Deleted with its goods: nothing poured (doc 79, 13).
            closed.add(name)
            st.closed += 1
            st.count(key)
    # The staged accounts are derived (the importing server makes them at load), and the dead ones are dropped at
    # restore: only the treasury and the escrow are packaged.
    for name in [n for n in accounts if n != TREASURY and account_kind(n) in ('staged', 'dead')]:
        del accounts[name]
        st.dropped += 1
    # A bandits' hoard keeps only what came from the staged side (its escrow source): the rest was robbed from players
    # (a save from before doc 79 has no sources, so all of its coins go). Its goods stay.
    for name, account in accounts.items():
        if name.startswith('bandits:') and account.get('cash', 0) > 0:
            keep = sources.get(name, 0)
            keep = int(keep) if isinstance(keep, (int, float)) and keep > 0 else 0
            if keep < account['cash']:
                account['cash'] = keep
                st.count(key)
    # Players' maker marks off every item kept (merged, within the limit).
    for name, account in accounts.items():
        stock = account.get('stock')
        if isinstance(stock, dict) and any(hit(item) for item in stock):
            fresh = {i: n for i, n in stock.items() if not hit(i)}
            account['stock'] = fresh
            pour(account, {i: n for i, n in stock.items() if hit(i)}, st)
            st.count(key)
    ledger = society.get('ledger', [])
    society['ledger'] = [e for e in ledger if not mentions(e) and e.get('from') not in closed and e.get('to') not in closed]
    st.count(key, len(ledger) - len(society['ledger']))
    if isinstance(society.get('careers'), dict):
        strip_careers(society['careers'], st, key)
    if staged:
        strip_staged(staged, accounts, st, key)
    for k in DEAD_SOCIETY:
        if k in society:
            del society[k]
            st.count(key)
    # The Market Board (doc 79, 5.2 and 13): a shipped world starts at normal prices, so its pressure is reset
    # (`cell` emptied) and the shops' budgets left today dropped (a new world's dawn sets them whole); its per-holder
    # records (pushed, the day's net and sell totals, keyed by account usernames the player pattern can't see) and the
    # Chapters' weekly discounts are never packaged. `recent` keeps only cause words, never names.
    board = society.get('board')
    if isinstance(board, dict):
        if board.get('cell'):
            st.count(key)
        board['cell'] = {}
        for k in ('pushed', 'netToday', 'soldToday', 'discountWeek', 'budgetLeft'):
            if board.pop(k, None):
                st.count(key)
    for k in list(society):
        if k not in ('accounts', 'ledger', 'careers') and isinstance(society[k], (list, dict)):
            society[k] = prune(society[k], st, key)


# The table. Every top-level section of the save, and what the strip does with it. Order matters only where noted:
# society runs last (the other sections name the accounts to close); people before roads (letters a caravan carries).
SECTIONS = {
    # Players' own records.
    'accounts': drop_section,       # (Left out, not emptied: an empty list of the wrong kind of accounts is refused.)
    'players': empty_section,
    'director': drop_section,       # (The operator bridge's own identity and receipts: a new world makes its own.)
    # (The host's account as this world last knew it (doc 78's R1: kept so a host with a bad token isn't a stranger).
    # The closing run of slice 2: any world that had been on central failed to share over it.)
    'lastHost': drop_section,
    # (Maps keyed by player, Core/RatwCheckpoint.cpp: a key per player who sent a command, heard a scene, chose an
    # alias; companions are resident -> the player who hired them; notes are kept as the game was given them.)
    'aliases': clear_section,
    'notes': clear_section,
    'scenesHeard': clear_section,
    'commandReceipts': clear_section,
    'responseReceipts': clear_section,
    'dmActs': drop_section,         # (The DM's acts by this database's dm.actions ids: a new world's ids start again.)
    'companions': clear_section,
    'socialStars': empty_section,
    'socialStories': empty_section,
    'standing': empty_section,
    'parties': empty_section,
    'camps': strip_camps,           # (Before the NPCs: camp staff of a camp that goes.)
    'chapters': strip_chapters,
    'people': strip_people,
    # NPC-side state that names players.
    'npcs': strip_npcs,
    'bonds': prune_section,
    'promises': prune_section,
    'beliefs': prune_section,
    'crime': strip_crime,
    'acquaintances': prune_section,
    'talkMemory': prune_section,
    'regardLedger': prune_section,
    'activeMemory': prune_section,
    'summaries': prune_section,
    'olderMemoryEvents': prune_section,
    'ledger': prune_section,
    'socialRecent': prune_section,
    'socialSessions': prune_section,
    'socialAudiences': strip_audiences,     # (After socialSessions.)
    'mapMemories': prune_section,
    'factions': prune_section,      # (Treaties, levies and House requests are Chapters'; standings, missions, burdens.)
    'estates': prune_section,       # (The places stay; leases to players or Chapters go.)
    'roads': strip_roads,
    'journal': strip_journal,
    'society': strip_society,
}
# Sections with nothing of players in them (the scan still reads them).
KNOWN_SAFE = ('schema', 'time', 'clockOffsetHours', 'calendarDays', 'sequence', 'revision', 'nextConversation',
              'nextStory', 'seasonalWeather', 'doors', 'weather', 'winds', 'lighting', 'weatherFronts', 'festivals')


def strip(doc: dict) -> Strip:
    """Takes every player out of a save document, in place. Raises PackageError for a section it doesn't know."""
    unknown = sorted(k for k in doc if k not in SECTIONS and k not in KNOWN_SAFE)
    if unknown:
        raise PackageError(f'The save has sections the strip doesn\'t know: {", ".join(unknown)}. Add each to SECTIONS '
                           '(or KNOWN_SAFE, if it can never name a player) in tools/world_package.py.')
    st = Strip(doc)
    for key, value in doc.items():
        ids = set(m.group(0) for m in PLAYER.finditer(json.dumps(value)))
        if ids:
            st.found[key] = len(ids)
    order = [k for k in SECTIONS if k not in ('roads', 'society')] + ['roads', 'society']
    for key in order:
        if key in doc:
            SECTIONS[key](doc, key, st)
    return st


# --------------------------------------------------------------------------- The scan

def _walk(value, path='$', field=''):
    """(path, text, field) for every key and every string in a value; field: the name of the map entry it is in (for a
    key, the map's own)."""
    if isinstance(value, str):
        yield path, value, field
    elif isinstance(value, dict):
        for k, v in value.items():
            yield f'{path}.{k}', k, field
            yield from _walk(v, f'{path}.{k}', k)
    elif isinstance(value, list):
        for i, v in enumerate(value):
            yield from _walk(v, f'{path}[{i}]', field)


# Fields that hold an account's key (a username) where they appear: a username there is an error even if it is also a
# word the game uses. So is any field named ...Account(s), as Core names them (giverAccount, recipientAccount:
# RatwStars.cpp; reporterAccount, reportedAccount: RatwReports.cpp; otherAccount: RatwNewcomers.cpp; authorAccount).
ACCOUNT_FIELDS = frozenset({'account', 'accounts', 'username', 'usernames', 'authorAccount', 'friend'})


def account_field(field) -> bool:
    return isinstance(field, str) and (field in ACCOUNT_FIELDS or field.endswith(('Account', 'Accounts')))
# The DM tool's accounts as the game and the tool write them ("dm:<username>"; Core/RatwGame.cpp, tools/dungeon_master.py).
DM_ACCOUNT = re.compile(r'(?<![A-Za-z0-9_])dm:[a-z]')
WORD = re.compile(r'[a-z][a-z0-9_-]{2,31}')
_vocabulary: frozenset | None = None


def vocabulary() -> frozenset:
    """The words the game itself uses: in Core's string literals and in Data's JSON (keys and strings), as usernames are
    spelt (3-32 characters, lowercase). A username that is one of them ("night", "guard", "timber", "open") is a word
    where it stands in a save (a lodging's period, a role), so only a warning, outside the account fields. Empty where
    the source isn't there (a package's container)."""
    global _vocabulary
    if _vocabulary is None:
        words = set()
        literal = re.compile(r'"((?:[^"\\\n]|\\.){1,400})"')
        for path in sorted((ROOT / 'Core').glob('*.[ch]*')):
            try:
                for text in literal.findall(path.read_text(encoding='utf-8', errors='replace')):
                    words.update(WORD.findall(text.lower()))
            except OSError:
                continue

        def walk(v):
            if isinstance(v, str):
                words.update(WORD.findall(v.lower()))
            elif isinstance(v, dict):
                for k, x in v.items():
                    words.update(WORD.findall(k.lower()))
                    walk(x)
            elif isinstance(v, list):
                for x in v:
                    walk(x)
        for path in sorted((ROOT / 'Data').rglob('*.json')):
            try:
                walk(json.loads(path.read_text(encoding='utf-8')))
            except (OSError, ValueError):
                continue
        _vocabulary = frozenset(words)
    return _vocabulary


def money(doc: dict) -> list[str]:
    """What restore would refuse in the society's books: a negative or overflowing account, an account over its goods
    limits. (Since doc 79 the accounts' sum is no longer minted - sunk: a package holds no player money and its staged
    accounts are made again at load. A missing treasury is only a warning: restore makes one, money_warnings.)"""
    errors = []
    society = doc.get('society')
    if not isinstance(society, dict):
        return errors
    for name, a in society.get('accounts', {}).items():
        stock = a.get('stock', {})
        cash = a.get('cash', 0)
        if not isinstance(cash, (int, float)) or cash < 0 or cash > MONEY_LIMIT:
            errors.append(f'money: account {name} holds {cash} (0 to {MONEY_LIMIT}).')
        if len(stock) > MAX_GOODS_KINDS:
            errors.append(f'goods: account {name} holds {len(stock)} kinds of goods (at most {MAX_GOODS_KINDS}).')
        for item, n in stock.items():
            if not isinstance(n, (int, float)) or n < 0 or n > STOCK_LIMIT:
                errors.append(f'goods: account {name} holds {n} {item} (0 to {STOCK_LIMIT}).')
    return errors


def money_warnings(doc: dict) -> list[str]:
    society = doc.get('society')
    if isinstance(society, dict) and TREASURY not in society.get('accounts', {}):
        return ['money: there is no treasury account (the importing server makes one).']
    return []


def scan(snapshot: dict | None, rows: dict, usernames=(), names=(), allowed_tables=None, limit=40,
         authors=()) -> tuple[list, list]:
    """(errors, warnings): player ids (wolf-, player-, acct-, dev:, Chapters) and DM tool accounts (dm:<name>) in any
    key or string of the snapshot or a shipped row; a source username (a player's or a DM tool's) as a whole value or a
    part of a compound key; the accounts' limits (money()); and the names players chose (characters', Chapters', camps') in free text
    (warnings). A username that is also one of the world's own identifiers (a resident, an account, an item, an area)
    is a warning; so is one that is a word the game uses (vocabulary()), except in a field that holds account keys
    (account_field). `authors`: the names the source's author columns and accounts give (author_names, doc 78, 10.1),
    held as usernames are, and warned of in free text. The shipped author columns hold only a package's own words
    (author_problems)."""
    errors, warnings = [], []
    authors = {a for a in authors if a}
    usernames = {u for u in usernames if u} | authors
    ambient = set()                 # (World identifiers a username may happen to equal: a warning, not an error.)
    if snapshot:
        ambient |= {e.get('id') for e in snapshot.get('npcs', []) if isinstance(e, dict)}
        for name, a in (snapshot.get('society') or {}).get('accounts', {}).items():
            ambient |= {name, *re.split(r'[|:@#]', name)}
            ambient |= set(a.get('stock', {}))
    for table, table_rows in rows.items():
        if table == 'world.areas':
            ambient |= {r[0] for r in table_rows}
    words = vocabulary() if usernames else frozenset()
    name_res = [(n, re.compile(r'\b' + re.escape(n) + r'\b')) for n in sorted({n for n in names if n and len(n) > 1})]
    author_res = [(a, re.compile(r'(?<![A-Za-z0-9_])' + re.escape(a) + r'(?![A-Za-z0-9_])', re.I)) for a in sorted(authors)]

    def check(where, value):
        for path, text, field in _walk(value, where):
            found = PLAYER.search(text)
            if found:
                errors.append(f'{path}: names a player or a Chapter ({found.group(0)[:12]}...)')
            found = DM_ACCOUNT.search(text)
            if found:
                errors.append(f'{path}: names a DM tool account ({found.group(0)}...)')
            if usernames:
                parts = [text] + re.split(r'[|:@#]', text)
                for part in parts:
                    if part in usernames:
                        if part in ambient:
                            warnings.append(f'{path}: a source username (also a world identifier)')
                        elif part in words and not account_field(field):
                            warnings.append(f'{path}: a source username (also a word the game uses)')
                        else:
                            errors.append(f'{path}: a source username (account)')
                        break
            for name, name_re in name_res:
                if name_re.search(text):
                    warnings.append(f'{path}: holds a name a player chose ("{name}")')
                    break
            for _, author_re in author_res:
                if text not in usernames and author_re.search(text):
                    warnings.append(f'{path}: holds a name an author column or account of the source gives')
                    break

    if snapshot is not None:
        check('snapshot', snapshot)
        errors.extend(money(snapshot))
        warnings.extend(money_warnings(snapshot))
    if allowed_tables is not None:
        for table in rows:
            if table in DEAD_TABLES:
                warnings.append(f'{table}: {DEAD_TABLES[table]}; its {len(rows[table])} row(s) are left out at import.')
            elif table not in allowed_tables:
                errors.append(f'{table}: not a table a package may hold.')
    for table, table_rows in rows.items():
        check(table, table_rows)
    errors.extend(economy_words(rows.get('live.economy')))
    errors.extend(economy_rows_problems(rows.get('live.economy')))
    errors.extend(author_problems(rows))
    if len(errors) > limit:
        errors = errors[:limit] + [f'... and {len(errors) - limit} more']
    return errors, warnings


# --------------------------------------------------------------------------- Reading the source

# Who wrote a seed row, where the DM tool writes its account's username (tools/dungeon_master.py): shipped as the
# package instead (dm.* stays out, and with it the DM tool's logins). Story visitors count as approved while approved_by
# is set (Core/RatwGameStoryteller.cpp), so it is kept set.
PROVENANCE = {'live.npc_voice': ('written_by', lambda v: 'dm' if isinstance(v, str) and v.startswith('dm:') else v),
              'live.story_visitors': ('approved_by', lambda v: None if v is None else 'package'),
              'live.faction_relations': ('updated_by', lambda v: '')}


def without_provenance(table: str, row: dict) -> dict:
    if table in PROVENANCE:
        column, value = PROVENANCE[table]
        if column in row:
            row[column] = value(row[column])
    return row


# The shipped columns that say who wrote a row (PROVENANCE's), and what a package may say there: its own words and an AI
# model's name, never a person's (doc 78, 10.1: an author column naming someone refuses the package, at the export's scan
# and at an import's). None is "nobody" (a visitor not approved).
MODEL_NAME = r'(?:gpt|o[1-9]|claude|gemini|llama|mistral)-?[a-z0-9._-]{0,60}'
AUTHOR_COLUMNS = {('live.npc_voice', 'written_by'): re.compile(rf'|seed|dm|package|{MODEL_NAME}'),
                  ('live.story_visitors', 'approved_by'): re.compile('package'),
                  ('live.faction_relations', 'updated_by'): re.compile('')}


def author_problems(rows: dict) -> list[str]:
    """A shipped author column that names a writer a package never names (a person, an account), each by its path."""
    errors = []
    for (table, column), allowed in AUTHOR_COLUMNS.items():
        for i, row in enumerate(rows.get(table) or []):
            if not isinstance(row, dict) or column not in row or row[column] is None:
                continue
            if not isinstance(row[column], str) or not allowed.fullmatch(row[column]):
                errors.append(f'{table}[{i}].{column}: names who wrote it, which a package never does (only its own '
                              'words or an AI model\'s name)')
    return errors


# Where a world records who did something (doc 78, 10.1: "every author column"): the names in them, and the DM tool's
# accounts file's, are held out of a package by the scan (scan's `authors`).
AUTHOR_SOURCES = (('world.builds', 'created_by'), ('world.edits', 'editor'), ('world.editors', 'editor'),
                  ('admin.releases', 'pushed_by'), ('admin.publish_attempts', 'editor'), ('admin.sync_base', 'taken_by'),
                  ('admin.server_state', 'changed_by'), ('live.faction_relation_log', 'by'),
                  ('dm.actions', 'requested_by'), ('dm.audit', 'who'), ('dm.stories', 'written_by'),
                  ('dm.admins', 'username'), ('dm.admins', 'created_by'), ('dm.invites', 'username'),
                  ('dm.invites', 'created_by'), ('dm.devices', 'username'), ('dm.sessions', 'username'))
PERSON = re.compile(r'[A-Za-z][A-Za-z0-9_.-]{2,39}')
# The tools' own names in those columns, and the package's words: never a person's.
TOOL_AUTHORS = frozenset({'seed', 'package', 'atlas', 'editor', 'world', 'test', 'tests'})
SERVICE_USERS = frozenset({'root', 'nobody', 'ratw', 'ratw-admin', 'ratw-server', 'ratw-backup', 'postgres'})
ACCOUNTS_FILE = ROOT / 'Database' / 'dm-accounts.txt'


def person_name(value) -> str | None:
    """A name a person may go by in an author column (an account's, a login's), or None for a tool's ("world_build.py",
    "world_db new") or a sentence: "dm:fern" and an Editor's "fern#a1b2c3" are "fern"."""
    if not isinstance(value, str):
        return None
    value = value.strip()
    if value.startswith('dm:'):
        value = value[3:]
    value = value.split('#', 1)[0]
    if not PERSON.fullmatch(value) or value.endswith('.py') or value.lower() in TOOL_AUTHORS:
        return None
    return value


def accounts_file_names(path: Path = ACCOUNTS_FILE) -> set:
    """The DM tool's account names in its accounts file (the first word of each account line; never its passwords)."""
    names = set()
    try:
        lines = path.read_text(encoding='utf-8').splitlines()
    except OSError:
        return names
    for line in lines:
        words = line.split()
        if len(words) >= 2 and not line.lstrip().startswith('#') and ':' not in words[0]:
            if (name := person_name(words[0])):
                names.add(name)
    return names


def login_names() -> set:
    """This computer's login (the user's own name, as the server's state and the edit log record it)."""
    names = set()
    try:
        import pwd
        names.add(pwd.getpwuid(os.getuid()).pw_name)
    except (ImportError, KeyError):
        pass
    names |= {os.environ.get(k, '') for k in ('USER', 'LOGNAME')}
    return {n for n in (person_name(n) for n in names) if n and n not in SERVICE_USERS}


def author_names(conn) -> set:
    """Every person-like name the source's author columns hold (read only), the accounts file's and the login's."""
    names = accounts_file_names() | login_names()
    for table, column in AUTHOR_SOURCES:
        if not conn.execute('SELECT to_regclass(%s) IS NOT NULL', (table,)).fetchone()[0]:
            continue
        schema, name = table.split('.')
        if not conn.execute('SELECT 1 FROM information_schema.columns WHERE table_schema = %s AND table_name = %s '
                            'AND column_name = %s', (schema, name, column)).fetchone():
            continue
        for (value,) in conn.execute(f'SELECT DISTINCT "{column}" FROM {table}').fetchall():
            if (found := person_name(value)):
                names.add(found)
    return names


def dm_usernames(conn) -> set:
    """The DM tool's account names in the source (read only), for the scan: none may ship."""
    if not conn.execute("SELECT to_regclass('dm.admins') IS NOT NULL").fetchone()[0]:
        return set()
    return {r[0] for r in conn.execute('SELECT username FROM dm.admins').fetchall()}


def live_specs():
    import world_sync
    return [s for s in world_sync.SYNCED if s.part == 'live']


def journal_replay(doc: dict, records: list[tuple[int, str]]) -> int:
    """Applies the journal records after the document's own (Core/RatwJournal.cpp, apply), in order. The highest seq
    applied. Raises PackageError for a record it can't apply."""
    after = int(max(0, doc.get('journal') or 0))
    last = after
    for seq, text in records:
        if seq <= after:
            continue
        try:
            changes = json.loads(text)
        except ValueError as error:
            raise PackageError(f'Journal record {seq} is not JSON ({error}); the save can\'t be exported.') from error
        if not isinstance(changes, list):
            raise PackageError(f'Journal record {seq} isn\'t a list of changes; the save can\'t be exported.')
        for change in changes:
            if not isinstance(change, dict):
                raise PackageError(f'Journal record {seq}: a change that isn\'t an object.')
            if 'set' in change or 'erase' in change:
                path = change.get('set' if 'set' in change else 'erase')
                if not isinstance(path, list) or not path or not all(isinstance(p, str) for p in path):
                    raise PackageError(f'Journal record {seq}: a path that isn\'t names.')
                at = doc
                for part in path[:-1]:
                    nxt = at.get(part)
                    if not isinstance(nxt, dict):
                        if 'erase' in change and 'set' not in change:
                            at = None
                            break
                        nxt = at[part] = {}
                    at = nxt
                if at is None:
                    continue
                if 'set' in change:
                    at[path[-1]] = change.get('value')
                else:
                    at.pop(path[-1], None)
            elif 'upsert' in change:
                name, key, value = change.get('upsert'), change.get('key'), change.get('value')
                if not name or not key or not isinstance(value, dict):
                    raise PackageError(f'Journal record {seq}: an upsert without its list, key or value.')
                entries = doc.get(name)
                if not isinstance(entries, list):
                    entries = doc[name] = []
                for i, entry in enumerate(entries):
                    if isinstance(entry, dict) and entry.get('id') == key:
                        entries[i] = value
                        break
                else:
                    entries.append(value)
            else:
                raise PackageError(f'Journal record {seq}: a change of no known kind.')
        last = max(last, seq)
    return last


def read_source(conn, with_snapshot=True, log=print) -> dict:
    """Everything a package takes from a world, in one read-only transaction (call inside one)."""
    import world_sync
    rows = conn.execute('SELECT id FROM world.worlds').fetchall()
    if len(rows) != 1:
        raise PackageError(f'The source holds {len(rows)} worlds; a package is made from exactly one.')
    world_id = rows[0][0]
    world_row = world_sync.to_json(next(iter(world_sync.read(conn, 'world.worlds', world_id).values())))
    areas = [list(r) for r in conn.execute('SELECT id, kind FROM world.areas WHERE world_id = %s ORDER BY id',
                                           (world_id,)).fetchall()]
    build = conn.execute('''SELECT id, fingerprint, created_at, files FROM world.builds WHERE world_id = %s
                            ORDER BY id DESC LIMIT 1''', (world_id,)).fetchone()
    if not build:
        raise PackageError('The source world has no build (python3 tools/world_build.py KEY makes one).')
    cells = conn.execute('SELECT count(*) FROM world.build_cells WHERE build_id = %s', (build[0],)).fetchone()[0]
    if not cells:
        raise PackageError(f'The newest build ({build[0]}) has no cells kept.')
    live = {}
    for spec in live_specs():
        if not world_sync.shape(conn, spec.table)[0]:
            continue
        found = world_sync.read(conn, spec.table, world_id, where=world_sync.ONLY.get(spec.table, ''))
        if spec.table == 'live.patrol_routes':
            posts = {}
            for route, seq, area, x, y in conn.execute('SELECT route_id, seq, area, x, y FROM live.patrol_posts '
                                                       'WHERE world_id = %s ORDER BY route_id, seq', (world_id,)).fetchall():
                posts.setdefault(route, []).append({'seq': seq, 'area': area, 'x': x, 'y': y})
            for row in found.values():
                row['posts'] = posts.get(row['id'], [])
        live[spec.table] = [without_provenance(spec.table, world_sync.normalized(r))
                            for _, r in sorted(found.items(), key=lambda kv: repr(kv[0]))]
    # (live.economy's kept columns only, doc 79, 14: the dead ones stay behind, D20.)
    live['live.economy'] = [{k: v for k, v in world_sync.normalized(r).items() if k in economy_shipped()}
                            for r in world_sync.read(conn, 'live.economy', world_id).values()]
    # The map edits the source holds that its newest build lacks (doc 78, 10.3): None when the build was made before
    # they were counted. The importing world's count starts from it (import_package).
    import world_publish
    out = {'world_id': world_id, 'world': world_row, 'areas': areas, 'live': live,
           'source': read_map_source(conn, world_id),
           'build': {'id': build[0], 'fingerprint': build[1], 'created_at': build[2].isoformat(), 'manifest': build[3],
                     'cells': cells, 'unpublished': world_publish.unpublished(conn)['count']},
           'snapshot': None, 'usernames': dm_usernames(conn), 'names': set(), 'authors': author_names(conn)}
    if with_snapshot:
        text = conn.execute('SELECT game.load_checkpoint(%s)', (world_id,)).fetchone()[0]
        if text is None:
            raise PackageError('The source world has no save to take a snapshot from (export with --no-snapshot).')
        doc = json.loads(text)
        if not isinstance(doc, dict) or doc.get('schema') != 1:
            raise PackageError('The source\'s save is not a schema 1 document; it can\'t be exported.')
        records = conn.execute('SELECT seq, record FROM game.journal WHERE world_id = %s ORDER BY seq',
                               (world_id,)).fetchall()
        last = journal_replay(doc, records)
        if records:
            log(f'Replayed {sum(1 for s, _ in records if s > int(doc.get("journal") or 0))} journal record(s) over the save '
                f'(to {last}).')
        out['snapshot'] = doc
    return out


def stream_build(conn, build_id, manifest, out: tarfile.TarFile) -> int:
    """The build into a tar as world_build.export writes it: world.ratw, cells/<id>.cell, seams/<id>."""
    def add(name, text):
        data = text.encode('utf-8') if isinstance(text, str) else json.dumps(text).encode('utf-8')
        info = tarfile.TarInfo(name)
        info.size, info.mtime, info.mode = len(data), 0, 0o644
        out.addfile(info, io.BytesIO(data))
        return len(data)
    size = 0
    for name, body in manifest.items():
        size += add(name, body)
    with conn.cursor(name='package_cells') as cur:
        cur.itersize = 64
        cur.execute('SELECT cell_id, body, seams FROM world.build_cells WHERE build_id = %s ORDER BY cell_id', (build_id,))
        for cell_id, body, seams in cur:
            size += add(f'cells/{cell_id}.cell', body)
            size += add(f'seams/{cell_id}', seams)
    return size


# --------------------------------------------------------------------------- Files

def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def write_json_gz(path: Path, value) -> int:
    data = json.dumps(value, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
    with gzip.GzipFile(path, 'wb', compresslevel=6, mtime=0) as f:
        f.write(data)
    return len(data)


def read_json_gz(path: Path):
    with gzip.open(path, 'rb') as f:
        return json.loads(f.read().decode('utf-8'))


def write_sums(directory: Path):
    names = [MANIFEST] + [p for p in PARTS if (directory / p).exists()]
    (directory / SUMS).write_text(''.join(f'{sha256(directory / n)}  {n}\n' for n in names), encoding='utf-8')


def verify(directory: Path) -> dict:
    """The package's manifest, after checking every file against SHA256SUMS. Raises PackageError."""
    sums = directory / SUMS
    if not sums.is_file():
        raise PackageError(f'{directory} has no {SUMS}: not a world package.')
    listed = {}
    for line in sums.read_text(encoding='utf-8').splitlines():
        digest, _, name = line.partition('  ')
        if not name or '/' in name or name.startswith('.'):
            raise PackageError(f'{SUMS} has a line that isn\'t "<sha256>  <name>": {line[:80]!r}')
        listed[name] = digest
    for name in (MANIFEST, *NEEDED):
        if name not in listed:
            raise PackageError(f'The package has no {name}.')
    for name in listed:
        if name not in (MANIFEST, *PARTS):
            raise PackageError(f'{SUMS} lists {name[:60]!r}, which isn\'t a world package\'s file.')
    for name, digest in listed.items():
        path = directory / name
        if not path.is_file():
            raise PackageError(f'The package lacks {name}.')
        if sha256(path) != digest:
            raise PackageError(f'{name} does not match its checksum: the package is damaged or was changed.')
    if (directory / MANIFEST).stat().st_size > MB:
        raise PackageError(f'{MANIFEST} is larger than a package\'s manifest may be.')
    try:
        manifest = json.loads((directory / MANIFEST).read_text(encoding='utf-8'))
    except (UnicodeDecodeError, ValueError) as error:
        raise PackageError(f'{MANIFEST} isn\'t JSON ({str(error)[:80]}).') from error
    if not isinstance(manifest, dict) or manifest.get('format') not in READS:
        format_ = manifest.get('format') if isinstance(manifest, dict) else None
        raise PackageError(f'The package is format {format_!r}; this tool reads formats '
                           f'{", ".join(map(str, READS))} (a newer package needs a newer image).')
    if manifest['format'] >= 2 and not all(p in listed for p in SOURCE_PARTS):
        raise PackageError('The package is format 2 but has no map source (terrain.json.gz, map.json.gz).')
    return manifest


def protocol() -> int | None:
    try:
        found = re.search(r'constexpr int Protocol = (\d+);', (ROOT / 'Core' / 'RatwBuildInfo.h').read_text())
        return int(found.group(1)) if found else None
    except OSError:
        return None


def client_data() -> str | None:
    try:
        import client_data_hash
        return client_data_hash.client_data_hash()
    except Exception:
        return None


def economy_shipped():
    """live.economy's kept columns: world_sync.ECONOMY_KEPT's (doc 79, 14), with its world_id."""
    import world_sync
    return ('world_id', *world_sync.ECONOMY_KEPT)


def economy_words(rows) -> list[str]:
    """The forbidden word in a shipped live.economy row's words (doc 79, 13 and Phase 9: the host's overrides, the
    slider's cause, an event's words, notices and cries, the ships' names), each by its path: refused."""
    errors = []

    def walk(value, path):
        if isinstance(value, str):
            # (A ship's name is held to more: the letters anywhere in it, as Core's forbiddenInName.)
            if economy_contract.forbidden_word(value) or ('.ships[' in path and 'dog' in value.lower()):
                errors.append(f'{path}: has the word no world text has')
        elif isinstance(value, dict):
            for k, v in value.items():
                walk(v, f'{path}.{k}')
        elif isinstance(value, list):
            for i, v in enumerate(value):
                walk(v, f'{path}[{i}]')
    for i, row in enumerate(rows or []):
        if isinstance(row, dict):
            walk(row.get('rules'), f'live.economy[{i}].rules')
    return errors


def economy_rows_problems(rows) -> list[str]:
    """What in a shipped live.economy row would leave the world it makes without its economy (doc 79, Phase 9's second
    repair: imports are untrusted, doc 78): its settings as a save checks them (map_editor.check_economy; a bad override
    is Core's to skip with a warning, so not refused), and its overrides no longer than the one manifest line Core reads
    (manifest_lines.OVERRIDES_MOST: a longer one is skipped whole, the slider's cause with it). Each by its path."""
    import manifest_lines
    import map_editor
    import world_store
    errors = []
    defaults = world_store.default_economy()
    for i, row in enumerate(rows or []):
        where = f'live.economy[{i}]'
        if not isinstance(row, dict):
            errors.append(f'{where}: not a row')
            continue
        shapes = [c for c in ('seasons', 'services', 'rules') if c in row and not isinstance(row[c], dict)]
        if shapes:
            errors += [f'{where}.{c}: must be an object' for c in shapes]
            continue
        full = {'treasury': defaults['treasury'], 'store_herbs': defaults['storeHerbs'], 'store_meals': defaults['storeMeals'], **row}
        entity = world_store.economy_entity(full)
        errors += [f'{where}: {p}' for p in map_editor.check_economy(entity)]
        overrides = json.dumps(entity['overrides'], ensure_ascii=False) if entity['overrides'] else ''
        if len(overrides.encode()) > manifest_lines.OVERRIDES_MOST:
            errors.append(f'{where}.rules: the host\'s overrides are {len(overrides.encode())} bytes: Core reads at most '
                          f'{manifest_lines.OVERRIDES_MOST}')
    return errors


def allowed_tables() -> set:
    return {s.table for s in live_specs()} | {'live.economy', 'world.worlds', 'world.areas'}


# Tables a package made before doc 79 holds that no package holds now (doc 79, 14 and 16.7: a pre-79 package still
# imports and loads): read past, with a warning at the scan and a note at the import, never refused.
# live.business_settings was the store room's per-business switch (doc 67, 6); the store room went with the city's loop
# (doc 79, 12 and 17) and migration 0061 drops the table.
DEAD_TABLES = {'live.business_settings': 'the store room\'s per-business switch, gone with the store room (doc 79)'}


# --------------------------------------------------------------------------- The map's source (doc 78, 10.1; M7)

TERRAIN = 'world.terrain_chunks'
MAP_TABLES = ('world.cells', 'world.interiors', 'world.links', 'world.resources', 'world.places', 'world.chapters',
              'world.minimaps')
SOURCE_TABLES = (TERRAIN,) + MAP_TABLES            # (world_sync.AUTHORED but the world row and its areas: world.json's.)
PART_OF = {TERRAIN: 'terrain.json.gz', **{t: 'map.json.gz' for t in MAP_TABLES}}
# Every column a package's world row and map source may hold (world_sync.read's, less a map row's updated_at): an
# import refuses any other (a later migration's column is added here with it) and leaves out one its database lacks.
SOURCE_COLUMNS = {
    'world.worlds': ('id', 'name', 'spawn_area', 'spawn_x', 'spawn_y', 'revision', 'updated_at', 'chunk_size', 'min_x',
                     'min_y', 'width', 'height'),
    TERRAIN: ('world_id', 'cx', 'cy', 'size', 'glyphs', 'heights'),
    'world.cells': ('world_id', 'id', 'kind', 'name', 'description', 'x', 'y', 'width', 'height', 'z', 'outdoors', 'weather',
                    'light_artificial', 'light_daylight', 'light_tone', 'region', 'chapter', 'claims', 'position', 'letting'),
    'world.interiors': ('world_id', 'id', 'kind', 'name', 'description', 'width', 'height', 'glyphs', 'heights', 'z',
                        'outdoors', 'weather', 'light_artificial', 'light_daylight', 'light_tone', 'region', 'chapter',
                        'claims', 'overview_x', 'overview_y', 'position', 'letting'),
    'world.links': ('world_id', 'id', 'name', 'kind', 'open', 'a_area', 'a_x', 'a_y', 'b_area', 'b_x', 'b_y', 'position'),
    'world.resources': ('world_id', 'id', 'kind', 'area', 'x', 'y'),
    'world.places': ('world_id', 'id', 'name', 'kind', 'area', 'x', 'y'),
    'world.chapters': ('world_id', 'id', 'name', 'position'),
    'world.minimaps': ('world_id', 'area', 'width', 'height', 'image'),
}
NOT_SHIPPED = ('updated_at',)       # (A map row's timestamp: the importing world's own.)
MB = 1 << 20
# Each part's most, packed and unpacked: an import (and the scan) reads no more of a part than this. The map's (doc 78,
# Progress, "M0b measured"): the terrain 128 MB packed and 512 MB unpacked (DEV's, dense: 0.7 MB and 16 MB; sparse it
# was 43 MB and 190 MB), the other map tables 8 MB each; the rest far above today's world's (a 46 MB package, 324 MB
# unpacked, nearly all of it the build).
PART_MOST = {'world.json.gz': (8 * MB, 64 * MB), 'live.json.gz': (64 * MB, 256 * MB),
             'snapshot.json.gz': (128 * MB, 1024 * MB), 'terrain.json.gz': (128 * MB, 512 * MB),
             'map.json.gz': (8 * MB * len(MAP_TABLES), 8 * MB * len(MAP_TABLES)), 'build.tar.gz': (1024 * MB, 4096 * MB)}
TABLE_MOST = 8 * MB                 # A map table's rows, as JSON (today's largest, world.interiors, 0.65 MB).
BUILD_FILE_MOST = 64 * MB           # One file of a build (a cell's is about 0.3 MB).
# Rows a table may hold (Atlas's own limits, map_editor.check_project and section 10.3's: 65,536 cells and interiors,
# 2,048 links, 128 Chapters), and a row's most as JSON (an interior of 256 x 256 tiles with every height set is about
# 0.9 MB; a 128-tile terrain chunk at most about 0.4 MB).
ROWS_MOST = {'world.areas': 65536, TERRAIN: 65536, 'world.cells': 65536, 'world.interiors': 65536, 'world.links': 2048,
             'world.resources': 4096, 'world.places': 65536, 'world.chapters': 128, 'world.minimaps': 65536}
ROW_MOST = {TERRAIN: 512 << 10}
ROW_MOST_ELSE = 1 * MB
DENSE = '$dense'                    # A terrain chunk's heights as world_store.encode_heights rows (format 2).


def read_map_source(conn, world_id) -> dict:
    """The map's source as Atlas holds it, table by table (world_sync.read's rows as JSON values), each terrain chunk's
    heights dense where they fit (encode_heights: format 2's)."""
    import world_store as S
    import world_sync
    out = {}
    for table in MAP_TABLES:
        if not world_sync.shape(conn, table)[0]:
            out[table] = []
            continue
        found = world_sync.read(conn, table, world_id, skip=NOT_SHIPPED)
        out[table] = [world_sync.normalized(r) for _, r in sorted(found.items(), key=lambda kv: repr(kv[0]))]
    # The ground a chunk at a time, its heights made dense as it comes: a whole world's "x,y" objects at once are
    # about 2.3 GB (doc 78, Progress, "M0b measured").
    out[TERRAIN] = []
    columns = [c for c in world_sync.shape(conn, TERRAIN)[0] if c not in NOT_SHIPPED]
    if columns:
        with conn.cursor(name='package_terrain') as cur:
            cur.itersize = 32
            cur.execute(f'SELECT {", ".join(columns)} FROM {TERRAIN} WHERE world_id = %s ORDER BY cx, cy', (world_id,))
            for values in cur:
                row = world_sync.normalized(dict(zip(columns, values)))
                heights, size = row.get('heights'), row.get('size')
                if heights and isinstance(size, int):
                    dense = S.encode_heights(heights, size, size)
                    if dense is not None:          # (A chunk whose heights don't fit the codes keeps its object.)
                        row['heights'] = {DENSE: dense}
                out[TERRAIN].append(row)
    return out


def chunk_heights(row) -> dict:
    """A package's terrain chunk's heights as the "x,y" object the database holds (dense rows decoded). Raises
    PackageError for dense rows that aren't the chunk's size or hold a character no height has."""
    import world_store as S
    heights = row.get('heights')
    if isinstance(heights, dict) and set(heights) == {DENSE}:
        rows, size = heights[DENSE], row.get('size')
        codes = set(S.HEIGHT_CODES) | {'.'}
        if (not isinstance(size, int) or not isinstance(rows, list) or len(rows) != size
                or not all(isinstance(r, str) and len(r) == size and set(r) <= codes for r in rows)):
            raise PackageError(f'{TERRAIN} {row.get("cx")},{row.get("cy")}: its heights aren\'t a chunk\'s rows of height codes.')
        return S.decode_heights(rows)
    if not isinstance(heights, dict):
        raise PackageError(f'{TERRAIN} {row.get("cx")},{row.get("cy")}: its heights aren\'t an object.')
    return heights


class LazyHeights:
    """A chunk's dense heights decoded each time they are read (world_store._cut reads a chunk's once a cell it
    covers), so a whole world's are never held twice over while its project is checked."""

    def __init__(self, row):
        self.row = {k: row.get(k) for k in ('cx', 'cy', 'size', 'heights')}

    def items(self):
        return chunk_heights(self.row).items()


def read_part(directory: Path, name: str):
    """A part's JSON, read within its caps (PART_MOST): a part larger than its most, packed or unpacked, is refused
    before more of it is read."""
    path = directory / name
    packed, unpacked = PART_MOST[name]
    size = path.stat().st_size
    if size > packed:
        raise PackageError(f'{name} is {size / MB:.1f} MB; a package\'s may be at most {packed / MB:.0f} MB.')
    with gzip.open(path, 'rb') as f:
        data = f.read(unpacked + 1)
    if len(data) > unpacked:
        raise PackageError(f'{name} unpacks to more than {unpacked / MB:.0f} MB, a package\'s most.')
    try:
        return json.loads(data.decode('utf-8'))
    except (UnicodeDecodeError, ValueError, RecursionError) as error:
        raise PackageError(f'{name} isn\'t JSON ({str(error)[:80]}).') from error


def read_contents(directory: Path, manifest: dict) -> dict:
    """Every part of a verified package, within its caps: {world, live, snapshot, source} (source: {table: rows}, None
    for a format 1 package, which has no map source)."""
    world = read_part(directory, 'world.json.gz')
    live = read_part(directory, 'live.json.gz')
    if not isinstance(world, dict) or not isinstance(live, dict) or not isinstance(live.get('tables'), dict):
        raise PackageError('The package\'s world or live part isn\'t what a package holds.')
    snapshot = read_part(directory, 'snapshot.json.gz') if (directory / 'snapshot.json.gz').exists() else None
    source = None
    present = [p for p in SOURCE_PARTS if (directory / p).exists()]
    if present:
        if len(present) != len(SOURCE_PARTS):
            raise PackageError(f'The package has {present[0]} without {[p for p in SOURCE_PARTS if p not in present][0]}.')
        source = {}
        for part in SOURCE_PARTS:
            tables = read_part(directory, part)
            tables = tables.get('tables') if isinstance(tables, dict) else None
            if not isinstance(tables, dict):
                raise PackageError(f'{part} isn\'t what a package holds.')
            for table, rows in tables.items():
                if PART_OF.get(table) != part:
                    raise PackageError(f'{part} holds {str(table)[:60]!r}, which a package\'s {part} may not.')
                source[table] = rows
    elif manifest.get('format', 1) >= 2:
        raise PackageError('The package is format 2 but has no map source (terrain.json.gz, map.json.gz).')
    return {'world': world, 'live': live['tables'], 'snapshot': snapshot, 'source': source}


def _row_size(row) -> int:
    return len(json.dumps(row, ensure_ascii=False, separators=(',', ':')).encode('utf-8'))


def shape_problems(contents: dict) -> list[str]:
    """What an untrusted package holds that no package does (doc 78, 10.1, "Imports are untrusted"): a table or a column
    off the allowlists, more rows than a table may hold, a row or a table over its size, a row of another world. Each by
    where it is."""
    errors = []
    world = contents['world']
    row = world.get('world')
    if not isinstance(row, dict) or not isinstance(row.get('id'), str) or not isinstance(row.get('name'), str):
        return ['world.json.gz: no world row.']
    world_id = row['id']
    extra = sorted(set(row) - set(SOURCE_COLUMNS['world.worlds']))
    if extra:
        errors.append(f'world.worlds: columns no package holds ({", ".join(c[:40] for c in extra[:5])}).')
    areas = world.get('areas')
    if (not isinstance(areas, list) or len(areas) > ROWS_MOST['world.areas']
            or not all(isinstance(a, list) and len(a) == 2 and isinstance(a[0], str) and a[1] in ('cell', 'interior')
                       for a in areas)):
        errors.append(f'world.areas: not a list of at most {ROWS_MOST["world.areas"]} [id, cell or interior] pairs.')
    allowed = allowed_tables() - {'world.worlds', 'world.areas'}
    for table, rows in contents['live'].items():
        if table in DEAD_TABLES:
            continue
        if table not in allowed:
            errors.append(f'live.json.gz: holds {str(table)[:60]!r}, which a package may not.')
        elif not isinstance(rows, list) or not all(isinstance(r, dict) for r in rows):
            errors.append(f'{table}: not a list of rows.')
        elif any(r.get('world_id', world_id) != world_id for r in rows):
            errors.append(f'{table}: holds rows of another world.')
    for table, rows in (contents['source'] or {}).items():
        if not isinstance(rows, list) or not all(isinstance(r, dict) for r in rows):
            errors.append(f'{table}: not a list of rows.')
            continue
        if len(rows) > ROWS_MOST[table]:
            errors.append(f'{table}: {len(rows)} rows; a package may hold at most {ROWS_MOST[table]}.')
            continue
        columns, most, total = set(SOURCE_COLUMNS[table]), ROW_MOST.get(table, ROW_MOST_ELSE), 0
        for i, r in enumerate(rows):
            extra = sorted(set(r) - columns)
            if extra:
                errors.append(f'{table}[{i}]: columns no package holds ({", ".join(str(c)[:40] for c in extra[:5])}).')
                break
            if r.get('world_id') != world_id:
                errors.append(f'{table}[{i}]: a row of another world.')
                break
            size = _row_size(r)
            if size > most:
                errors.append(f'{table}[{i}]: {size} bytes; a row may be at most {most}.')
                break
            total += size
        if table != TERRAIN and total > TABLE_MOST:
            errors.append(f'{table}: {total / MB:.1f} MB; a package\'s may be at most {TABLE_MOST / MB:.0f} MB.')
    return errors + size_problems(row, contents['source'] or {})


SIDE_MOST = 4096                    # A cell's or an interior's side, in tiles (today's cells are 256).
TILES_MOST = 256 * 1024 * 1024      # All the cells' tiles together (today's world: 15.8 million).
AWAY_MOST = 4_000_000               # How far from the origin a cell may lie, in tiles.


def size_problems(world_row: dict, source: dict) -> list[str]:
    """The numbers the map's rows give for sizes and places, held to what a world has, before the rows are made into a
    project (R2: a cell 2,000,000,000 tiles wide made the check itself build that much ground, and take the computer's
    memory with it). A list of what is wrong, the first of each table."""
    whole = lambda v, low, high: type(v) is int and low <= v <= high
    errors = []
    chunk = world_row.get('chunk_size', 128)
    if not whole(chunk, 8, 1024):
        return ['world.worlds: chunk_size is a whole number from 8 to 1024.']
    tiles = 0
    for i, r in enumerate(source.get('world.cells') or []):
        if not (whole(r.get('width'), 1, SIDE_MOST) and whole(r.get('height'), 1, SIDE_MOST)
                and whole(r.get('x'), -AWAY_MOST, AWAY_MOST) and whole(r.get('y'), -AWAY_MOST, AWAY_MOST)):
            errors.append(f'world.cells[{i}]: a cell is 1 to {SIDE_MOST} tiles a side, within {AWAY_MOST} tiles of the origin.')
            break
        tiles += r['width'] * r['height']
    if tiles > TILES_MOST:
        errors.append(f'world.cells: {tiles} tiles in all; a package\'s map may hold at most {TILES_MOST}.')
    for i, r in enumerate(source.get('world.interiors') or []):
        if not (whole(r.get('width'), 1, SIDE_MOST) and whole(r.get('height'), 1, SIDE_MOST)
                and isinstance(r.get('glyphs'), str) and len(r['glyphs']) == r['width'] * r['height']):
            errors.append(f'world.interiors[{i}]: an interior is 1 to {SIDE_MOST} tiles a side, with a glyph for each tile.')
            break
    for i, r in enumerate(source.get(TERRAIN) or []):
        if not (r.get('size') == chunk and whole(r.get('cx'), -AWAY_MOST, AWAY_MOST) and whole(r.get('cy'), -AWAY_MOST, AWAY_MOST)
                and isinstance(r.get('glyphs'), str) and len(r['glyphs']) == chunk * chunk):
            errors.append(f'{TERRAIN}[{i}]: a chunk is {chunk} tiles a side (the world\'s chunk size), with a glyph for each tile.')
            break
    return errors


def scan_rows(contents: dict) -> dict:
    """The rows the scan reads: the world row and areas, the live tables and the map's source without its ground
    (glyphs and heights, which are tiles, not text: Atlas's validator holds them)."""
    rows = {'world.worlds': [contents['world']['world']], 'world.areas': contents['world'].get('areas') or [],
            **contents['live']}
    for table, table_rows in (contents['source'] or {}).items():
        rows[table] = [{k: v for k, v in r.items() if k not in ('glyphs', 'heights')} if isinstance(r, dict) else r
                       for r in table_rows]
    return rows


# The live tables a project is made of (world_store.project_from_rows), beside the map's source.
PROJECT_LIVE = ('live.npcs', 'live.profession_slots', 'live.factions', 'live.faction_claims', 'live.patrol_routes',
                'live.npc_relations', 'live.economy')


def map_problems(contents: dict) -> tuple[list[str], list[str]]:
    """(errors, warnings) of Atlas's validator (map_editor.check_project, as a save and validate_dev run it: no client
    file limit, the game server streams) on the project the package's rows make, before anything is written. The
    database alone would take ' \\ and ` as ground (doc 78, Progress, "M0b measured"). None for a package without a
    map source."""
    import map_editor
    import world_store as S
    import world_sync
    if contents['source'] is None:
        return [], []
    tables = {t: [world_sync.from_json(r) for r in contents['live'].get(t) or []] for t in PROJECT_LIVE}
    for table in SOURCE_TABLES:
        tables[table] = [world_sync.from_json(r) for r in contents['source'].get(table) or []]
    for row in tables[TERRAIN]:
        heights = row.get('heights')
        row['heights'] = LazyHeights(row) if isinstance(heights, dict) and set(heights) == {DENSE} else heights
    try:
        project = S.project_from_rows(world_sync.from_json(contents['world']['world']), tables)
        _, warnings = map_editor.check_project(project, for_game=False)
    except map_editor.ValidationError as error:
        return list(error.errors), []
    except PackageError as error:
        return [str(error)], []
    except (KeyError, TypeError, ValueError, AttributeError, IndexError, MemoryError, RecursionError, OverflowError) as error:
        return [f'The map\'s rows don\'t make a world ({type(error).__name__}: {str(error)[:120]}).'], []
    return [], warnings


def unreached(world_row: dict, source: dict) -> list[str]:
    """The places no door, passage or stairs and no neighbouring cell reaches from the spawn (doc 78, 10.1: unreleased
    content is removed before export; these are for the user to check), by id."""
    cells = {r['id']: r for r in source.get('world.cells', [])}
    areas = set(cells) | {r['id'] for r in source.get('world.interiors', [])}
    start = world_row.get('spawn_area')
    if start not in areas:
        return []
    ways = {a: set() for a in areas}
    for link in source.get('world.links', []):
        if link.get('a_area') in ways and link.get('b_area') in ways:
            ways[link['a_area']].add(link['b_area'])
            ways[link['b_area']].add(link['a_area'])
    edge = sorted(cells.values(), key=lambda c: c['x'])
    for i, a in enumerate(edge):                        # Cells whose edges touch: walked across.
        for b in edge[i + 1:]:
            if b['x'] > a['x'] + a['width']:
                break
            side = (a['x'] + a['width'] == b['x'] or b['x'] + b['width'] == a['x']) and \
                a['y'] < b['y'] + b['height'] and b['y'] < a['y'] + a['height']
            top = (a['y'] + a['height'] == b['y'] or b['y'] + b['height'] == a['y']) and \
                a['x'] < b['x'] + b['width'] and b['x'] < a['x'] + a['width']
            if side or top:
                ways[a['id']].add(b['id'])
                ways[b['id']].add(a['id'])
    seen, todo = {start}, [start]
    while todo:
        for nxt in ways[todo.pop()] - seen:
            seen.add(nxt)
            todo.append(nxt)
    return sorted(areas - seen)


# --------------------------------------------------------------------------- Export

def export(key: str, out: Path, with_snapshot=True, version=None, log=print, transform=None) -> dict:
    """Writes world `key` as a package to `out` (refused unless missing or empty). Returns the manifest. `transform`
    (tests): called with the source's parts before the strip."""
    key = W.resolve(key)
    if out.exists() and (not out.is_dir() or any(out.iterdir())):
        raise PackageError(f'{out} exists and isn\'t empty; give a new directory.')
    out.parent.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=f'.{out.name}.partial-', dir=out.parent))
    try:
        with W.connect(key, 'owner', options='-c default_transaction_read_only=on') as conn:
            with conn.transaction():
                conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY')
                source = read_source(conn, with_snapshot, log)
                if transform:
                    transform(source)
                st = None
                if source['snapshot'] is not None:
                    st = strip(source['snapshot'])
                    source['usernames'] |= st.usernames
                    source['names'] |= st.names
                contents = {'world': {'world': source['world'], 'areas': source['areas']}, 'live': source['live'],
                            'source': source['source'], 'snapshot': source['snapshot']}
                rows = scan_rows(contents)
                errors, warnings = scan(source['snapshot'], rows, source['usernames'], source['names'],
                                        allowed_tables() | set(SOURCE_TABLES), authors=source['authors'])
                errors = shape_problems(contents) + errors
                if errors:
                    raise PackageError('The scan failed; nothing was written:\n' + '\n'.join(f'  {e}' for e in errors))
                away = unreached(source['world'], source['source'])
                if away:
                    log(f'{len(away)} place(s) no door, passage, stairs or neighbouring cell reaches from the spawn (hidden '
                        f'or unfinished? remove what isn\'t released before sharing): {", ".join(away[:20])}'
                        + (f' and {len(away) - 20} more' if len(away) > 20 else '') + '.')
                if source['build'].get('unpublished'):
                    log(f'{source["build"]["unpublished"]} map change(s) in this world are not published: the package '
                        'carries them in its map\'s source, and the world started from it shows them once it publishes '
                        '(publish first to send the map as players have it).')
                sizes = {}
                sizes['world.json.gz'] = write_json_gz(work / 'world.json.gz', {'world': source['world'],
                                                                                'areas': source['areas']})
                sizes['live.json.gz'] = write_json_gz(work / 'live.json.gz', {'tables': source['live']})
                sizes['terrain.json.gz'] = write_json_gz(work / 'terrain.json.gz',
                                                         {'tables': {TERRAIN: source['source'][TERRAIN]}})
                sizes['map.json.gz'] = write_json_gz(work / 'map.json.gz',
                                                     {'tables': {t: source['source'][t] for t in MAP_TABLES}})
                if source['snapshot'] is not None:
                    sizes['snapshot.json.gz'] = write_json_gz(work / 'snapshot.json.gz', source['snapshot'])
                with tarfile.open(work / 'build.tar.gz', 'w:gz', compresslevel=6) as tar:
                    sizes['build.tar.gz'] = stream_build(conn, source['build']['id'], source['build']['manifest'], tar)
        made = datetime.datetime.now(datetime.timezone.utc)
        manifest = {
            'format': FORMAT, 'id': uuid.uuid4().hex,
            'version': version or f'{key}-b{source["build"]["id"]}-{made:%Y%m%d%H%M}',
            'source': key, 'world_id': source['world_id'],
            'build': {'id': source['build']['id'], 'fingerprint': source['build']['fingerprint'],
                      'made': source['build']['created_at'], 'cells': source['build']['cells'],
                      'unpublished': source['build'].get('unpublished')},
            'protocol': protocol(), 'client_data': client_data(), 'made_at': made.isoformat(timespec='seconds'),
            'compression': 'gzip', 'snapshot': source['snapshot'] is not None,
            'rows': {t: len(r) for t, r in rows.items()},
            'map_source': True, 'unreached': len(away),
            'dense_heights': sum(1 for r in source['source'][TERRAIN] if set(r.get('heights') or {}) == {DENSE}),
            'unpacked_bytes': sizes,
            'strip': None if st is None else {'removed': st.removed, 'player_ids_found': st.found,
                                              'accounts_closed': st.closed, 'accounts_dropped': st.dropped,
                                              'goods_discarded': st.discarded},
            'warnings': len(warnings),
        }
        (work / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        write_sums(work)
        # Read back, as an import would, and scanned again from the files.
        verify(work)
        errors, _ = scan_directory(work)
        if errors:
            raise PackageError('The written package fails its scan:\n' + '\n'.join(f'  {e}' for e in errors))
        if out.exists():
            out.rmdir()
        os.replace(work, out)
    except BaseException:
        shutil.rmtree(work, ignore_errors=True)
        raise
    for w in warnings[:20]:
        log(f'warning: {w}')
    if len(warnings) > 20:
        log(f'warning: ... and {len(warnings) - 20} more')
    return manifest


# --------------------------------------------------------------------------- One file ("Share my world")

PACKED = '.ratwpkg'
PACK_NAMES = (MANIFEST, SUMS) + PARTS
PACK_MOST = 4 << 30                 # A packed package's files, at most this many bytes together (unpack refuses more).


def pack(directory: Path, out) -> int:
    """A package directory as one file (doc 78, 10.5; M9): a plain tar of its files, which are compressed already, the
    manifest and SHA256SUMS first. `out` is a binary file object. Each file is read without following a link and must be
    a plain one (the Share's export directory is another user's). Returns the bytes written."""
    import stat as stat_
    start = out.tell()
    with tarfile.open(fileobj=out, mode='w', format=tarfile.PAX_FORMAT) as tar:
        for name in PACK_NAMES:
            try:
                fd = os.open(directory / name, os.O_RDONLY | os.O_NOFOLLOW)
            except FileNotFoundError:
                if name in PARTS and name not in NEEDED:
                    continue                    # (No snapshot; a format 1 package's missing map source.)
                raise PackageError(f'The package has no {name}.')
            with os.fdopen(fd, 'rb') as f:
                info = os.fstat(f.fileno())
                if not stat_.S_ISREG(info.st_mode):
                    raise PackageError(f'{name} isn\'t a plain file.')
                entry = tarfile.TarInfo(name)
                entry.size, entry.mtime, entry.mode = info.st_size, int(info.st_mtime), 0o644
                tar.addfile(entry, f)
    return out.tell() - start


def unpack(file: Path, directory: Path) -> dict:
    """A .ratwpkg file into `directory` (made, or empty), checked: only the package's own names, each once, plain files,
    PACK_MOST bytes at most in all; then verified against its SHA256SUMS. An untrusted file can't write anywhere else
    (no paths, no links). Returns the manifest. Raises PackageError."""
    directory.mkdir(parents=True, exist_ok=True)
    if any(directory.iterdir()):
        raise PackageError(f'{directory} isn\'t empty; give a new directory.')
    seen, total = set(), 0
    try:
        with tarfile.open(file, mode='r:') as tar:
            for member in tar:
                if member.name not in PACK_NAMES or member.name in seen or not member.isreg() or member.size < 0:
                    raise PackageError(f'{file.name} holds {member.name[:60]!r}, which isn\'t a world package\'s file.')
                total += member.size
                if total > PACK_MOST:
                    raise PackageError(f'{file.name} is larger than a world package may be.')
                seen.add(member.name)
                source = tar.extractfile(member)
                with open(directory / member.name, 'xb') as target:
                    shutil.copyfileobj(source, target, 1 << 20)
    except (tarfile.TarError, EOFError) as error:
        raise PackageError(f'{file.name} isn\'t a packed world package ({error}).') from error
    if MANIFEST not in seen or SUMS not in seen:
        raise PackageError(f'{file.name} has no {MANIFEST if MANIFEST not in seen else SUMS}: not a world package.')
    return verify(directory)


class opened:
    """A package as a directory: itself, or a .ratwpkg file unpacked into a temporary one (removed afterwards)."""

    def __init__(self, path: Path):
        self.path, self.temp = Path(path), None

    def __enter__(self) -> Path:
        if self.path.is_dir():
            return self.path
        if not self.path.is_file():
            raise PackageError(f'{self.path} is neither a package directory nor a .ratwpkg file.')
        self.temp = Path(tempfile.mkdtemp(prefix='ratw-package-'))
        try:
            unpack(self.path, self.temp)
        except BaseException:
            shutil.rmtree(self.temp, ignore_errors=True)
            raise
        return self.temp

    def __exit__(self, *_):
        if self.temp is not None:
            shutil.rmtree(self.temp, ignore_errors=True)


def scan_directory(directory: Path, usernames=(), names=(), authors=()) -> tuple[list, list]:
    """(errors, warnings) of a package directory: its parts within their caps, its tables and columns on the
    allowlists (shape_problems), and the scan."""
    manifest = verify(directory)
    contents = read_contents(directory, manifest)
    problems = shape_problems(contents)
    errors, warnings = scan(contents['snapshot'], scan_rows(contents), usernames, names,
                            allowed_tables() | set(SOURCE_TABLES), authors=authors)
    return problems + errors, warnings


def source_people(key: str) -> tuple[set, set, set]:
    """The usernames (its players' and its DM tool's), the names players chose (characters', Chapters', camps'), and
    the names its author columns give (author_names), of a source world (read only), for scan --source."""
    with W.connect(W.resolve(key), 'owner', options='-c default_transaction_read_only=on') as conn:
        world_id = conn.execute('SELECT id FROM world.worlds').fetchone()
        text = conn.execute('SELECT game.load_checkpoint(%s)', (world_id[0],)).fetchone()[0] if world_id else None
        admins = dm_usernames(conn)
        authors = author_names(conn)
    doc = json.loads(text) if text else {}
    st = Strip(doc)
    return st.usernames | admins, st.names, authors


# --------------------------------------------------------------------------- Import (first start)

def database_of(key: str) -> str:
    """A package world's database: ratw_<key>, a '-' in the key as '_' (a database name is lowercase letters, digits and
    '_': world_db.check_registry)."""
    return f'ratw_{key.replace("-", "_")}'


def kind_of(directory: Path) -> str:
    """The kind a package's world is registered as (doc 78, 10.2): a dev world served as live when the package carries
    the map's source (Atlas edits it; the server is still run with --world-kind live), a live world, as slice 1 made
    them, when it doesn't (a format 1 package: nothing to edit)."""
    return 'dev' if all((directory / p).exists() for p in SOURCE_PARTS) else 'live'


def _write_registry(path: Path, key: str, name: str, kind: str = 'dev'):
    path.parent.mkdir(parents=True, exist_ok=True)
    entry = {'key': key, 'name': name, 'database': database_of(key), 'kind': kind}
    W.check_registry([entry])
    path.write_text('{"worlds": [\n  ' + json.dumps(entry) + '\n]}\n', encoding='utf-8')


def _database_exists(database: str) -> bool:
    with W.superuser() as su:
        return bool(su.execute('SELECT 1 FROM pg_database WHERE datname = %s', (database,)).fetchone())


def _held(database: str) -> str | None:
    """What an existing database holds that makes it no half-made import (read as the superuser, nothing changed):
    a world (world.worlds rows) or a package's marker; None if neither (empty, or made by an import stopped part way)."""
    with W.superuser(database) as su:
        if su.execute("SELECT to_regclass('admin.settings') IS NOT NULL").fetchone()[0]:
            have = su.execute(f"SELECT value FROM admin.settings WHERE key = '{SETTING}'").fetchone()
            if have:
                return f'package {have[0].get("version")} ({have[0].get("id")})'
        if su.execute("SELECT to_regclass('world.worlds') IS NOT NULL").fetchone()[0]:
            world_id = su.execute('SELECT id FROM world.worlds LIMIT 1').fetchone()
            if world_id:
                return f'world {world_id[0]}'
    return None


def _holds_something(key: str, registry: Path) -> bool:
    """Whether the database world `key` would use holds a world or a package already (read as the superuser, nothing
    changed): then the import keeps it, or refuses, and never writes the package; otherwise (no database, or one an
    import stopped part way left empty) the package is checked first."""
    database = database_of(key)
    if registry.exists():
        W.worlds()
        if key in W.keys():
            database = W.world(key)['database']
    return _database_exists(database) and _held(database) is not None


def checked_contents(directory: Path, manifest: dict, log=print) -> dict:
    """An untrusted package (doc 78, 10.1) read and checked, before anything is written: every part within its caps;
    its tables and columns on the allowlists, its rows within their counts and sizes (shape_problems); the scan as the
    export runs it (players, DM accounts, the accounts' limits, the economy, the author columns); then Atlas's
    validator on its map (map_problems). Raises PackageError saying what is wrong."""
    try:
        contents = read_contents(directory, manifest)
    except PackageError as error:
        raise PackageError(f'{error} The package won\'t do; nothing was written.') from error
    problems = shape_problems(contents)
    if not problems:
        problems, _ = scan(contents['snapshot'], scan_rows(contents), allowed_tables=allowed_tables() | set(SOURCE_TABLES))
    if problems:
        raise PackageError('The package won\'t do; nothing was written:\n' + '\n'.join(f'  {p}' for p in problems[:20]))
    errors, warnings = map_problems(contents)
    if errors:
        raise PackageError('The package\'s map won\'t do (Atlas\'s checks); nothing was written:\n'
                           + '\n'.join(f'  {e}' for e in errors[:20]) + (f'\n  ... and {len(errors) - 20} more' if len(errors) > 20 else ''))
    if warnings:
        log(f'note: Atlas\'s checks warn of {len(warnings)} thing(s) in the map, as they do in Atlas (the first: '
            f'{warnings[0][:200]}).')
    return contents


def import_package(directory: Path, key: str, name: str | None = None, log=print) -> str:
    """First start: world `key`'s database made and the package imported. Returns 'imported'; 'already' (this package
    is in it already); or 'kept' (it was started from another package: an update's image carries a newer one, and the
    world keeps its own state, with the migrations applied and the newer package's new seed rows added: seed_update).
    Raises PackageError if the database holds a world that no package made, and for a package that won't do, which is
    checked (checked_contents) before anything at all is written: the registry, a database, a migration. A key the
    registry lacked is registered first (a rerun after a crash part way finds it and completes the import); if the
    import then fails or is refused, the key is unregistered again and a database this run made is dropped, and an
    existing database it would take over is first checked to hold no world and no package (_held)."""
    manifest = verify(directory)
    world = read_part(directory, 'world.json.gz')
    world = world.get('world') if isinstance(world, dict) else None
    name = name or (world.get('name') if isinstance(world, dict) else None) or key
    registry = W.registry_file()
    contents = None if _holds_something(key, registry) else checked_contents(directory, manifest, log)
    kind = kind_of(directory)
    if not registry.exists():
        _write_registry(registry, key, name, kind)
        log(f'Wrote a one-world registry, {registry}.')
    W.worlds()
    if key in W.keys():
        return _import(directory, key, name, manifest, contents, log)
    # Registered before its database is made: a rerun after a crash part way finds the key and the database, and
    # completes the migrations (new_world unregistered would refuse a database it didn't register).
    if key in W.ALIASES:
        raise PackageError(f'{key} is another world\'s alias; give the package its own key.')
    database = database_of(key)
    existed = _database_exists(database)
    if existed:
        held = _held(database)
        if held:
            raise PackageError(f'A database called {database} already exists and holds {held}, but no registered world '
                               f'is {key}; nothing imported (choose another key, or register it by hand if it is this '
                               'world).')
        log(f'{database} is there already, with no world in it (an import stopped part way): completing it.')
    W.register({'key': key, 'name': name, 'database': database, 'kind': kind})
    log(f'Registered {key} ("{name}", {"a dev world served as live" if kind == "dev" else "live"}).')
    try:
        return _import(directory, key, name, manifest, contents, log)
    except BaseException:
        # (What register did, undone, as new_world registers only on success; a crash, which runs no cleanup, leaves
        # both for the rerun.)
        if not existed and _database_exists(database):
            from psycopg import sql
            with W.superuser() as su:
                su.execute(sql.SQL('DROP DATABASE IF EXISTS {} WITH (FORCE)').format(sql.Identifier(database)))
            log(f'Dropped {database} again.')
        W.unregister(key)
        log(f'Unregistered {key} again.')
        raise


ORIGINAL_ECONOMY = 'economy_original'   # admin.settings: the world's first live.economy (doc 78, 10.1 and 12.3).
SEEDED = 'seeded'                       # admin.settings: every seed key the world was ever given (seed_update).
# The add-only step's tables (doc 78, 12.5 and Q17): what a newer feature brings as new rows, keyed. Never the map,
# the NPCs or anything placed on the map (an official map's changes never reach a hosted world, 12.5), and never
# live.economy's keys (a new rule's default comes with the image's Data/Economy files; a key in the package's row is
# the source's own setting, which an update mustn't impose): only its row, when the world has none.
SEED_TABLES = ('live.dialects', 'live.figures', 'live.npc_voice')


def _seed_key(table: str, row: dict, keys) -> str:
    return table + '/' + '/'.join(str(row.get(k)) for k in keys if k != 'world_id')


def _record(conn, key: str, value) -> None:
    conn.execute('INSERT INTO admin.settings (key, value) VALUES (%s, %s) ON CONFLICT (key) DO UPDATE SET value = '
                 'excluded.value', (key, W.Jsonb(value)))


def _setting(conn, key: str):
    row = conn.execute('SELECT value FROM admin.settings WHERE key = %s', (key,)).fetchone()
    return row[0] if row else None


def seed_keys(conn, live: dict) -> list[str]:
    """The seed keys a package's rows give (SEED_TABLES), for the record."""
    import world_sync
    out = []
    for table in SEED_TABLES:
        if world_sync.shape(conn, table)[0]:
            keys = world_sync.shape(conn, table)[1]
            out += [_seed_key(table, r, keys) for r in live.get(table) or [] if isinstance(r, dict)]
    return sorted(set(out))


def seed_update(conn, directory: Path, manifest: dict, log) -> dict:
    """The add-only step on an update (doc 78, 12.5; Q17's (a)): the image's newer package's seed rows (SEED_TABLES) that
    the world has never had are inserted, and nothing it has, or had and removed, is touched: the world records every
    key it was given (admin.settings 'seeded'). A world from before the record (slice 1's) gets every row it lacks
    (its hosts could edit none of these tables). A voice for a resident the world hasn't is left out. live.economy: the
    row only, when the world has none. Also keeps the world's original economy (ORIGINAL_ECONOMY) where it has none.
    One transaction; {table: rows added}."""
    import world_sync
    live = read_part(directory, 'live.json.gz').get('tables') or {}
    added = {}
    with conn.transaction():
        world_id = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
        record = _setting(conn, SEEDED)
        seen = set(record.get('keys') or []) if isinstance(record, dict) else None
        npcs = {r[0] for r in conn.execute('SELECT id FROM live.npcs WHERE world_id = %s', (world_id,)).fetchall()}
        given = set(seen or ())
        for table in SEED_TABLES:
            if not isinstance(live.get(table), list) or not world_sync.shape(conn, table)[0]:
                continue
            keys = world_sync.shape(conn, table)[1]
            have = {_seed_key(table, r, keys) for r in world_sync.read(conn, table, world_id).values()}
            given |= have
            for row in live[table]:
                if not isinstance(row, dict):
                    continue
                k = _seed_key(table, row, keys)
                if k in have or (seen is not None and k in seen):
                    given.add(k)
                    continue
                if table == 'live.npc_voice' and row.get('npc_id') not in npcs:
                    continue                       # (A voice for a resident this world hasn't got.)
                row = {**row, 'world_id': world_id}
                if table == 'live.figures' and row.get('npc_id') and row['npc_id'] not in npcs:
                    row['npc_id'] = None             # (Someone known of, not yet here.)
                author = AUTHOR_COLUMNS.get((table, 'written_by'))
                if author and not author.fullmatch(str(row.get('written_by') or '')):
                    row['written_by'] = 'package'
                world_sync.write(conn, table, _fit(conn, table, world_sync.stored(table, without_provenance(table, row)), log))
                added[table] = added.get(table, 0) + 1
                given.add(k)
        economy = [r for r in live.get('live.economy') or [] if isinstance(r, dict)]
        if economy and not conn.execute('SELECT 1 FROM live.economy WHERE world_id = %s', (world_id,)).fetchone():
            if not (economy_rows_problems(economy) + economy_words(economy)):
                world_sync.write(conn, 'live.economy',
                                 _fit(conn, 'live.economy', world_sync.stored('live.economy', {**economy[0], 'world_id': world_id}), log))
                added['live.economy'] = 1
        _record(conn, SEEDED, {'keys': sorted(given), 'package': manifest.get('version'),
                               'at': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')})
        if _setting(conn, ORIGINAL_ECONOMY) is None and economy:
            # (A world imported before its import kept one: the image's package's, said so.)
            _record(conn, ORIGINAL_ECONOMY, {'row': {k: v for k, v in economy[0].items() if k != 'world_id'},
                                             'package': manifest.get('version'), 'from': 'update'})
    log(f'The add-only step: ' + (', '.join(f'{n} new {t}' for t, n in sorted(added.items())) if added else
                                   'no new seed rows') + f' from package {manifest.get("version")}.')
    return added


def _import(directory: Path, key: str, name: str, manifest: dict, contents: dict | None, log) -> str:
    import psycopg
    import world_sync
    entry = W.world(key)
    kind = entry['kind']
    W.ensure_roles([entry['database']] if _database_exists(entry['database']) else [])
    if not _database_exists(entry['database']):
        W.new_world(key, name, kind, registered=True, log=log)
        W.ensure_roles([entry['database']])
    with W.connect(key, 'owner') as conn:
        W.migrate(conn)
        have = conn.execute(f"SELECT value FROM admin.settings WHERE key = '{SETTING}'").fetchone()
        if have:
            if have[0].get('id') == manifest['id']:
                log(f'{key} already holds package {manifest["version"]} ({manifest["id"]}); nothing to do.')
                return 'already'
            # An update (doc 77, section 8): the image carries a newer package, the volume the world as it was played.
            log(f'{key}: this world was started from package {have[0].get("version")} ({have[0].get("id")}) and keeps '
                f'its own state; the image carries package {manifest["version"]} ({manifest["id"]}), which is not '
                'applied to an existing world; to start fresh from the new package run docker compose down -v (this '
                'deletes the world).')
            seed_update(conn, directory, manifest, log)
            return 'kept'
        worlds = conn.execute('SELECT id FROM world.worlds').fetchall()
        if worlds:
            raise PackageError(f'{key} already holds a world ({worlds[0][0]}) that no package made; nothing imported.')
        if contents is None:
            contents = checked_contents(directory, manifest, log)
        # (A database made by a run that stopped part way: what new_world does after making it, again; both idempotent.)
        W.grant_connect(entry['database'], kind)
        W.seed_publish_password(conn)
        world, live, snapshot, source = contents['world'], contents['live'], contents['snapshot'], contents['source']
        world_id = world['world']['id']
        counts = {}
        try:
            with conn.transaction():
                conn.execute('SET CONSTRAINTS ALL DEFERRED')
                world_sync.write(conn, 'world.worlds', _fit(conn, 'world.worlds', world_sync.from_json(world['world']), log))
                with conn.cursor() as cur:
                    cur.executemany('INSERT INTO world.areas (world_id, id, kind) VALUES (%s, %s, %s)',
                                    [(world_id, a, k) for a, k in world['areas']])
                # The map's source (format 2: doc 78, 10.1), as Atlas holds it.
                for table in SOURCE_TABLES:
                    rows = (source or {}).get(table) or []
                    if rows and not world_sync.shape(conn, table)[0]:
                        log(f'note: this database has no {table}; its {len(rows)} row(s) are left out.')
                        continue
                    for row in rows:
                        stored = world_sync.from_json(row)
                        if table == TERRAIN:
                            stored['heights'] = chunk_heights(row)
                        world_sync.write(conn, table, _fit(conn, table, stored, log))
                    counts[table] = len(rows)
                order = [s.table for s in live_specs()] + ['live.economy']
                for table in sorted(live, key=lambda t: order.index(t) if t in order else len(order)):
                    if table in DEAD_TABLES:
                        log(f'note: {table} ({DEAD_TABLES[table]}): its {len(live[table])} row(s) are left out.')
                        continue
                    if table not in allowed_tables() or table.startswith('world.'):
                        raise PackageError(f'The package holds {table}, which a package may not.')
                    if not world_sync.shape(conn, table)[0]:
                        log(f'note: this database has no {table}; its {len(live[table])} row(s) are left out.')
                        continue
                    for row in live[table]:
                        world_sync.write(conn, table, _fit(conn, table, world_sync.stored(table, row), log))
                        if table == 'live.patrol_routes':
                            for post in row.get('posts', []):
                                conn.execute('INSERT INTO live.patrol_posts (world_id, route_id, seq, area, x, y) '
                                             'VALUES (%s, %s, %s, %s, %s, %s)',
                                             (world_id, row['id'], post['seq'], post['area'], post['x'], post['y']))
                    counts[table] = len(live[table])
                build_id, cells = import_build(conn, world_id, directory / 'build.tar.gz', manifest)
                # "N map changes not published" starts at none (doc 78, 10.3; the closing run found a new world saying
                # "counted from the next Publish", with no Discard): the build is the source's, as far as the package
                # says. A package whose source was ahead of its build says how far, and that world counts from its first
                # Publish (the edits themselves, world.edits, never ship).
                import world_build
                ahead = manifest.get('build', {}).get('unpublished') if isinstance(manifest.get('build'), dict) else None
                if source is not None and world_build.publish_columns(conn) and not (
                        isinstance(ahead, int) and not isinstance(ahead, bool) and ahead > 0):
                    conn.execute('UPDATE world.builds SET edits_through = %s WHERE id = %s',
                                 (world_build.newest_edit(conn, world_id), build_id))
                if snapshot is not None:
                    conn.execute('SELECT game.save_checkpoint(%s, %s, %s)',
                                 (world_id, int(snapshot.get('revision') or 0), json.dumps(snapshot, ensure_ascii=False)))
                # (Settled now, not at commit, so a package whose rows don't fit together is refused with why: what the
                # laptop's import path runs and Atlas's save would.)
                conn.execute('SET CONSTRAINTS ALL IMMEDIATE')
                if found := world_sync.orphans(conn, world_id):
                    raise PackageError('The package places things where its map has nothing:\n'
                                       + '\n'.join(f'  {f}' for f in found[:20]))
                record = {k: manifest.get(k) for k in ('id', 'version', 'source', 'made_at', 'protocol', 'client_data',
                                                       'format')}
                record['map_source'] = source is not None
                record['imported_at'] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
                conn.execute('INSERT INTO admin.settings (key, value) VALUES (%s, %s)', (SETTING, W.Jsonb(record)))
                # The world's original economy, fixed, for "Reset economy rules" after updates (doc 78, 10.1 and 12.3);
                # and the seed keys it was given, for the add-only step's (seed_update).
                economy = [r for r in live.get('live.economy') or [] if isinstance(r, dict)]
                if economy:
                    _record(conn, ORIGINAL_ECONOMY, {'row': {k: v for k, v in economy[0].items() if k != 'world_id'},
                                                     'package': manifest.get('version'), 'from': 'import'})
                _record(conn, SEEDED, {'keys': seed_keys(conn, live), 'package': manifest.get('version'),
                                       'at': record['imported_at']})
        except psycopg.Error as error:
            first = (str(error).strip().splitlines() or [''])[0]
            raise PackageError(f'The package\'s rows don\'t fit together, nothing was written ({type(error).__name__}: '
                               f'{first[:300]}).') from error
    log(f'{key}: imported package {manifest["version"]}: world {world_id}, {len(world["areas"])} areas'
        + (f', the map\'s source ({counts.get(TERRAIN, 0)} terrain chunks, {counts.get("world.cells", 0)} cells, '
           f'{counts.get("world.interiors", 0)} interiors)' if source is not None else ', no map source (format 1)')
        + f', build {build_id} ({cells} cells), {sum(v for t, v in counts.items() if not t.startswith("world."))} seed rows'
        + (', the snapshot' if snapshot is not None else ', no snapshot') + '.')
    return 'imported'


_warned = set()


def _fit(conn, table, row, log):
    """A row without columns this database lacks (a package from a newer schema), each noted once."""
    import world_sync
    columns = set(world_sync.shape(conn, table)[0])
    extra = [c for c in row if c not in columns]
    for c in extra:
        if (table, c) not in _warned:
            _warned.add((table, c))
            log(f'note: this database has no {table}.{c}; left out.')
    return {k: v for k, v in row.items() if k in columns}


def import_build(conn, world_id, path: Path, manifest: dict) -> tuple[int, int]:
    """The build's rows (world.builds and world.build_cells) from the package's files, inserted directly:
    world_build.build needs Atlas's tables, which a package leaves out."""
    world_file, cells, seams = None, {}, {}
    build_id = None
    batch = []

    def flush():
        if batch:
            with conn.cursor() as cur:
                cur.executemany('INSERT INTO world.build_cells (build_id, cell_id, header, body, seams) VALUES (%s, %s, %s, %s, %s)',
                                batch)
            batch.clear()

    packed, most = PART_MOST['build.tar.gz']
    if path.stat().st_size > packed:
        raise PackageError(f'build.tar.gz is larger than a package\'s build may be ({packed / MB:.0f} MB).')
    total = 0
    with tarfile.open(path, 'r:gz') as tar:
        for member in tar:
            if not member.isfile():
                continue
            total += member.size
            if member.size > BUILD_FILE_MOST or total > most:
                raise PackageError(f'build.tar.gz holds more than a build may ({member.name[:60]!r}: {member.size} bytes).')
            text = tar.extractfile(member).read().decode('utf-8')
            if member.name == 'world.ratw':
                world_file = text
            elif member.name.startswith('cells/') and member.name.endswith('.cell'):
                cells[member.name[6:-5]] = text
            elif member.name.startswith('seams/'):
                seams[member.name[6:]] = text
            else:
                raise PackageError(f'build.tar.gz holds {member.name!r}, which a build hasn\'t.')
            if world_file is not None and build_id is None:
                build_id = conn.execute('''INSERT INTO world.builds (world_id, release, created_by, files, fingerprint)
                                           VALUES (%s, NULL, %s, %s, %s) RETURNING id''',
                                        (world_id, 'world_package.py', W.Jsonb({'world.ratw': world_file}),
                                         manifest.get('build', {}).get('fingerprint'))).fetchone()[0]
            if build_id is not None:
                for cell_id in [c for c in cells if c in seams]:
                    body = cells.pop(cell_id)
                    marker = body.find('\ngrid:\n')
                    if marker < 0:
                        raise PackageError(f'Cell {cell_id} of the build has no grid.')
                    batch.append((build_id, cell_id, body[:marker + 7], body, seams.pop(cell_id)))
                    if len(batch) >= 64:
                        flush()
    if world_file is None:
        raise PackageError('build.tar.gz has no world.ratw.')
    if cells or seams:
        raise PackageError(f'The build has cells without seams or seams without cells: {sorted(cells) + sorted(seams)}'[:200])
    flush()
    count = conn.execute('SELECT count(*) FROM world.build_cells WHERE build_id = %s', (build_id,)).fetchone()[0]
    expected = manifest.get('build', {}).get('cells')
    if expected is not None and count != expected:
        raise PackageError(f'The build has {count} cells; the package says {expected}.')
    return build_id, count


# --------------------------------------------------------------------------- Command line

def _dir_size(path: Path) -> int:
    return sum(p.stat().st_size for p in path.iterdir() if p.is_file())


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('export', help='write a world as a package')
    p.add_argument('--world', required=True, help='the world to export (a registered key)')
    p.add_argument('--out', required=True, type=Path, help='the directory to write (missing or empty)')
    p.add_argument('--no-snapshot', action='store_true', help='leave the save out (the world starts cold)')
    p.add_argument('--version', help='the package version (default: KEY-bBUILD-YYYYmmddHHMM)')
    p = sub.add_parser('scan', help='check a package')
    p.add_argument('directory', type=Path, help='the package: a directory, or a .ratwpkg file')
    p.add_argument('--source', help="also look for this world's usernames and characters' names")
    p = sub.add_parser('pack', help='a package directory as one .ratwpkg file')
    p.add_argument('directory', type=Path)
    p.add_argument('--out', required=True, type=Path, help='the file to write (must not exist)')
    p = sub.add_parser('unpack', help='a .ratwpkg file as a package directory')
    p.add_argument('file', type=Path)
    p.add_argument('--out', required=True, type=Path, help='the directory to write (missing or empty)')
    p = sub.add_parser('import', help="first start: make a world's database from a package")
    p.add_argument('directory', type=Path, help='the package: a directory, or a .ratwpkg file')
    p.add_argument('--key', required=True, help='the world key (kind live)')
    p.add_argument('--name', help="the world's name in the registry (default the package's world name)")
    args = parser.parse_args(argv)
    try:
        if args.command == 'export':
            m = export(args.world, args.out, not args.no_snapshot, args.version)
            print(f'Package {m["version"]} ({m["id"]}) written to {args.out}: build {m["build"]["id"]} '
                  f'({m["build"]["cells"]} cells), {sum(m["rows"].values())} rows'
                  + (', the snapshot' if m['snapshot'] else ', no snapshot')
                  + f'; {_dir_size(args.out) / 1e6:.1f} MB ({sum(m["unpacked_bytes"].values()) / 1e6:.1f} MB unpacked).')
            if m['strip']:
                removed = ', '.join(f'{k} {v}' for k, v in sorted(m['strip']['removed'].items())) or 'nothing'
                print(f'Strip: removed {removed}.')
        elif args.command == 'pack':
            verify(args.directory)
            with open(args.out, 'xb') as out:
                size = pack(args.directory, out)
            print(f'{args.out}: {size / 1e6:.1f} MB.')
        elif args.command == 'unpack':
            m = unpack(args.file, args.out)
            print(f'Package {m["version"]} ({m["id"]}) unpacked to {args.out}.')
        elif args.command == 'scan':
            usernames, names, authors = source_people(args.source) if args.source else ((), (), ())
            with opened(args.directory) as directory:
                errors, warnings = scan_directory(directory, usernames, names, authors)
            for w in warnings[:40]:
                print(f'warning: {w}')
            if errors:
                print('The scan failed:\n' + '\n'.join(f'  {e}' for e in errors), file=sys.stderr)
                return 1
            print(f'{args.directory}: the scan passed' + (f' ({len(warnings)} warnings).' if warnings else '.'))
        else:
            with opened(args.directory) as directory:
                import_package(directory, args.key, args.name)
    except (PackageError, W.DatabaseError, OSError, ValueError, tarfile.TarError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
