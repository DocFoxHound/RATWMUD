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
  python3 tools/world_package.py import DIR --key KEY [--name NAME]
        first start: makes world KEY's database (a live world), runs the migrations and imports the package in one
        transaction. Connection settings from the environment (RATW_*, POSTGRES_*; RATW_WORLDS_FILE a one-world
        registry, written if missing; a key the registry lacks is registered first), so it works in a container with no
        Database/.env. A database that already holds a package keeps its world: the migrations run, and a different
        package (a newer image's) is not applied, only noted. Refuses a database holding a world no package made.

The package (format 1), every part gzip-compressed:
  package.json          format, package id and version, source key, build id and fingerprint, protocol, client-data
                        hash, made_at, what is in it (counts only: never an id or a name)
  world.json.gz         the world row (world.worlds) and its areas (id and kind): NPCs, slots, posts, claims and the
                        spawn point have keys into world.areas
  live.json.gz          the seed rows: world_sync's live tables (spawned NPCs left out, patrol posts in their routes)
                        and live.economy, as world_sync writes rows (bytes and timestamps tagged)
  build.tar.gz          the newest build as world_build.export writes it: world.ratw, cells/<id>.cell, seams/<id>
  snapshot.json.gz      the save (game.load_checkpoint, the journal after it replayed), stripped of players
  SHA256SUMS            sha256 of every file above

Out: every game.* table (the snapshot is one document, imported with game.save_checkpoint), dm.*, admin.*, Atlas's
source, old builds, live.npc_state (newer rows would overwrite purses), live.game_changes, keys and passwords.

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

import world_db as W

ROOT = Path(__file__).resolve().parent.parent
FORMAT = 1
PARTS = ('world.json.gz', 'live.json.gz', 'build.tar.gz', 'snapshot.json.gz')
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
# The society's state of the conserving economy (doc 79, 14: read past at restore): never packaged.
DEAD_SOCIETY = ('loop', 'orchestrator')


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
    # (Maps keyed by player, Core/RatwCheckpoint.cpp: a key per player who sent a command, heard a scene, chose an
    # alias; companions are resident -> the player who hired them; notes are kept as the game was given them.)
    'aliases': clear_section,
    'notes': clear_section,
    'scenesHeard': clear_section,
    'commandReceipts': clear_section,
    'responseReceipts': clear_section,
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


def scan(snapshot: dict | None, rows: dict, usernames=(), names=(), allowed_tables=None, limit=40) -> tuple[list, list]:
    """(errors, warnings): player ids (wolf-, player-, acct-, dev:, Chapters) and DM tool accounts (dm:<name>) in any
    key or string of the snapshot or a shipped row; a source username (a player's or a DM tool's) as a whole value or a
    part of a compound key; the accounts' limits (money()); and the names players chose (characters', Chapters', camps') in free text
    (warnings). A username that is also one of the world's own identifiers (a resident, an account, an item, an area)
    is a warning; so is one that is a word the game uses (vocabulary()), except in a field that holds account keys
    (account_field)."""
    errors, warnings = [], []
    usernames = {u for u in usernames if u}
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

    if snapshot is not None:
        check('snapshot', snapshot)
        errors.extend(money(snapshot))
        warnings.extend(money_warnings(snapshot))
    if allowed_tables is not None:
        for table in rows:
            if table not in allowed_tables:
                errors.append(f'{table}: not a table a package may hold.')
    for table, table_rows in rows.items():
        check(table, table_rows)
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
    live['live.economy'] = [world_sync.normalized(r) for r in world_sync.read(conn, 'live.economy', world_id).values()]
    out = {'world_id': world_id, 'world': world_row, 'areas': areas, 'live': live,
           'build': {'id': build[0], 'fingerprint': build[1], 'created_at': build[2].isoformat(), 'manifest': build[3],
                     'cells': cells},
           'snapshot': None, 'usernames': dm_usernames(conn), 'names': set()}
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
    for name in [MANIFEST, 'world.json.gz', 'live.json.gz', 'build.tar.gz']:
        if name not in listed:
            raise PackageError(f'The package has no {name}.')
    for name, digest in listed.items():
        path = directory / name
        if not path.is_file():
            raise PackageError(f'The package lacks {name}.')
        if sha256(path) != digest:
            raise PackageError(f'{name} does not match its checksum: the package is damaged or was changed.')
    manifest = json.loads((directory / MANIFEST).read_text(encoding='utf-8'))
    if manifest.get('format') != FORMAT:
        raise PackageError(f'The package is format {manifest.get("format")!r}; this tool reads format {FORMAT}.')
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


def allowed_tables() -> set:
    return {s.table for s in live_specs()} | {'live.economy', 'world.worlds', 'world.areas'}


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
                rows = {'world.worlds': [source['world']], 'world.areas': source['areas'], **source['live']}
                errors, warnings = scan(source['snapshot'], rows, source['usernames'], source['names'], allowed_tables())
                if errors:
                    raise PackageError('The scan failed; nothing was written:\n' + '\n'.join(f'  {e}' for e in errors))
                sizes = {}
                sizes['world.json.gz'] = write_json_gz(work / 'world.json.gz', {'world': source['world'],
                                                                                'areas': source['areas']})
                sizes['live.json.gz'] = write_json_gz(work / 'live.json.gz', {'tables': source['live']})
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
                      'made': source['build']['created_at'], 'cells': source['build']['cells']},
            'protocol': protocol(), 'client_data': client_data(), 'made_at': made.isoformat(timespec='seconds'),
            'compression': 'gzip', 'snapshot': source['snapshot'] is not None,
            'rows': {t: len(r) for t, r in rows.items()},
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


def scan_directory(directory: Path, usernames=(), names=()) -> tuple[list, list]:
    verify(directory)
    world = read_json_gz(directory / 'world.json.gz')
    live = read_json_gz(directory / 'live.json.gz')['tables']
    snapshot = read_json_gz(directory / 'snapshot.json.gz') if (directory / 'snapshot.json.gz').exists() else None
    rows = {'world.worlds': [world['world']], 'world.areas': world['areas'], **live}
    return scan(snapshot, rows, usernames, names, allowed_tables())


def source_people(key: str) -> tuple[set, set]:
    """The usernames (its players' and its DM tool's), and the names players chose (characters', Chapters', camps'), of
    a source world (read only), for scan --source."""
    with W.connect(W.resolve(key), 'owner', options='-c default_transaction_read_only=on') as conn:
        world_id = conn.execute('SELECT id FROM world.worlds').fetchone()
        text = conn.execute('SELECT game.load_checkpoint(%s)', (world_id[0],)).fetchone()[0] if world_id else None
        admins = dm_usernames(conn)
    doc = json.loads(text) if text else {}
    st = Strip(doc)
    return st.usernames | admins, st.names


# --------------------------------------------------------------------------- Import (first start)

def database_of(key: str) -> str:
    """A package world's database: ratw_<key>, a '-' in the key as '_' (a database name is lowercase letters, digits and
    '_': world_db.check_registry)."""
    return f'ratw_{key.replace("-", "_")}'


def _write_registry(path: Path, key: str, name: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    entry = {'key': key, 'name': name, 'database': database_of(key), 'kind': 'live'}
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


def import_package(directory: Path, key: str, name: str | None = None, log=print) -> str:
    """First start: world `key`'s database made and the package imported. Returns 'imported'; 'already' (this package
    is in it already); or 'kept' (it was started from another package: an update's image carries a newer one, and the
    world keeps its own state, with the migrations applied). Raises PackageError if the database holds a world that no
    package made. A key the registry lacked is registered first (a rerun after a crash part way finds it and completes
    the import); if the import then fails or is refused, the key is unregistered again and a database this run made is
    dropped, and an existing database it would take over is first checked to hold no world and no package (_held)."""
    manifest = verify(directory)
    world = read_json_gz(directory / 'world.json.gz')
    name = name or world['world'].get('name') or key
    registry = W.registry_file()
    if not registry.exists():
        _write_registry(registry, key, name)
        log(f'Wrote a one-world registry, {registry}.')
    W.worlds()
    if key in W.keys():
        return _import(directory, key, name, manifest, world, log)
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
    W.register({'key': key, 'name': name, 'database': database, 'kind': 'live'})
    log(f'Registered {key} ("{name}", live).')
    try:
        return _import(directory, key, name, manifest, world, log)
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


def _import(directory: Path, key: str, name: str, manifest: dict, world: dict, log) -> str:
    import world_sync
    entry = W.world(key)
    if entry['kind'] != 'live':
        raise PackageError(f'{key} is a {entry["kind"]} world; a package is imported into a live world.')
    W.ensure_roles([entry['database']] if _database_exists(entry['database']) else [])
    if not _database_exists(entry['database']):
        W.new_world(key, name, 'live', registered=True, log=log)
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
            return 'kept'
        worlds = conn.execute('SELECT id FROM world.worlds').fetchall()
        if worlds:
            raise PackageError(f'{key} already holds a world ({worlds[0][0]}) that no package made; nothing imported.')
        # (A database made by a run that stopped part way: what new_world does after making it, again; both idempotent.)
        W.grant_connect(entry['database'], 'live')
        W.seed_publish_password(conn)
        live = read_json_gz(directory / 'live.json.gz')['tables']
        snapshot_path = directory / 'snapshot.json.gz'
        snapshot = read_json_gz(snapshot_path) if snapshot_path.exists() else None
        world_id = world['world']['id']
        counts = {}
        with conn.transaction():
            conn.execute('SET CONSTRAINTS ALL DEFERRED')
            world_sync.write(conn, 'world.worlds', _fit(conn, 'world.worlds', world_sync.from_json(world['world']), log))
            with conn.cursor() as cur:
                cur.executemany('INSERT INTO world.areas (world_id, id, kind) VALUES (%s, %s, %s)',
                                [(world_id, a, k) for a, k in world['areas']])
            order = [s.table for s in live_specs()] + ['live.economy']
            for table in sorted(live, key=lambda t: order.index(t) if t in order else len(order)):
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
            if snapshot is not None:
                conn.execute('SELECT game.save_checkpoint(%s, %s, %s)',
                             (world_id, int(snapshot.get('revision') or 0), json.dumps(snapshot, ensure_ascii=False)))
            record = {k: manifest.get(k) for k in ('id', 'version', 'source', 'made_at', 'protocol', 'client_data')}
            record['imported_at'] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
            conn.execute('INSERT INTO admin.settings (key, value) VALUES (%s, %s)', (SETTING, W.Jsonb(record)))
    log(f'{key}: imported package {manifest["version"]}: world {world_id}, {len(world["areas"])} areas, build {build_id} '
        f'({cells} cells), {sum(counts.values())} seed rows' + (', the snapshot' if snapshot is not None else ', no snapshot')
        + '.')
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

    with tarfile.open(path, 'r:gz') as tar:
        for member in tar:
            if not member.isfile():
                continue
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
    p.add_argument('directory', type=Path)
    p.add_argument('--source', help="also look for this world's usernames and characters' names")
    p = sub.add_parser('import', help="first start: make a world's database from a package")
    p.add_argument('directory', type=Path)
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
        elif args.command == 'scan':
            usernames, names = source_people(args.source) if args.source else ((), ())
            errors, warnings = scan_directory(args.directory, usernames, names)
            for w in warnings[:40]:
                print(f'warning: {w}')
            if errors:
                print('The scan failed:\n' + '\n'.join(f'  {e}' for e in errors), file=sys.stderr)
                return 1
            print(f'{args.directory}: the scan passed' + (f' ({len(warnings)} warnings).' if warnings else '.'))
        else:
            import_package(args.directory, args.key, args.name)
    except (PackageError, W.DatabaseError, OSError, ValueError, tarfile.TarError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
