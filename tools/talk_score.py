#!/usr/bin/env python3
"""The talk scorecard (Docs/Design/84-living-conversations.md §15.3-§15.5): what talk_sim's transcripts measure.

  python3 tools/talk_score.py RUN_DIR [RUN_DIR ...] [--json OUT.json] [--md OUT.md]
  python3 tools/talk_score.py RUN_DIR ... --baseline BASE.json --check      exit 1 if a gate breaks (below)
  python3 tools/talk_score.py RUN_DIR ... --bless BASE.json                 write the baseline
  python3 tools/talk_score.py RUN_DIR ... --baseline BASE.json --refreeze T1,T3 --why TEXT
                                                                         take only those rows into the baseline

Each RUN_DIR is one talk_sim run (lines.jsonl, convs.jsonl, residents.jsonl, run.json); a run made with --ablate shuffle
also has the shuffled control (lines-shuffled.jsonl), and every metric is computed on it too (§15.5): a coherence
metric (C*) that scores the control within 30% of the real transcripts isn't measuring coherence, and is flagged.
Metrics are worked out per run (a profile), then over all the runs given.

What P0 can measure, before moves and tones exist (§15.1: "the text-only metrics still work: that is how the baseline
is measured"); the rest are listed as n/a with the phase that brings them:
  R1 first word-for-word identical conversation, per listener (hours; the earliest listener's): since Slice 2's review,
     the talk between its ceremonies (the opener's own words, `bare`, without the greeting, "Back again?" or recall said
     in its breath; the closing left out), so a goodbye said another way no longer makes the same talk new
  R2 first repeated opener, per listener (hours)
  R3 distinct openers in a listener's first 3,000 exchanges (fewer heard: scaled to 3,000)
  R4 a distinctive sentence (6+ words) heard again by the same listener within 10 h, per 100 h (whole lines until
     Slice 2's review: a sentence spliced into longer lines went uncounted)
  R5 collision index: the same distinctive line from two speakers in one place within an hour, per 100 h
  R6 the most-spoken sentence's share of the lines (Slice 2's review; the most-spoken whole line in R6_detail)
  R7 distinct reply entries per 1,000 reply beats: replies to what was said, the closing pairs apart (Phase 3: a
     preclose, a farewell and their answers are formulaic by design, §8.4; their own variety is in R7_detail)
  R8 habit saturation: the share of a speaker's lines using their oath! or saying! (speakers with 20+ lines: mean, p90)
  C1 owed answers answered, by moves (Phase 2's C1M, the hard gate from Phase 3): a line that owes an answer (`owes`,
     written, handed on by `then`, or what its last move expects; Data/Talk/moves.json) followed by the one it is owed
     by, doing one of the owed moves (or a variant of one) or a fallback of one (down to ack). A line with no moves owes
     nothing, so it is measured where lines carry moves
  C1Q questions answered: a line ending "?" followed by the other's line (the heuristic C1 was until Phase 3; it doesn't
     separate from the control, so it is a diagnostic)
  M1 lines carrying moves (%): how much of what is said says what it does (Phase 2 tags the 40 most-spoken families and
     every answer concept by its alias; Phase 7 every entry)
  C2 replies at the best fit available (Phase 3): of the lines that answered something owed, those whose fit (3: the
     handed-on concept's own or the exact variant; 1: another line doing an owed move; 0: a fallback) was the best any
     line that held had (talk_sim's `fit` and `best`)
  C3 endings: by a farewell or a marked break / at the beat cap / by nothing fitting (a talk cut off by a speaker walking
     away, or the other busy, ends with no line: it is counted apart, never as a good ending). A farewell: the last line
     makes a closing move that takes leave (farewell, farewell.return, leave, break), or its words do (the lexicon, with
     the dialects' farewells). The cap: the turn engine's `ended` (Phase 3), else 8 lines written. A curt ending (`ended`
     curt: the engine's own short close, §8.4) is a good ending too since Slice 2's review
  C4 tone clashes per 1,000 beats: a grave line followed within two beats by a light one from the other, with no bridge:
     by the lines' tones (Phase 2's moves; a bridge is change_subject, assess or preclose) or by the lexicon (death,
     grief, robbery... then laughter, a jest); and (Slice 2's review) a light bridge into grave news within one line
     ("There's better news than that. They're burying...")
  C6 the same entry (or the same question) twice in one conversation, per 1,000 conversations
  C7 lines per conversation: mean, 90th percentile, and the share of 3-line ones
  C8 conversations with a third speaker (Phase 4: one who joined, §8.9)
  T1 raw ids spoken (an identifier with an underscore), per 1,000 lines
  T2 presupposition failures per 1,000 lines: Data/Talk/truth.json's rules (Phase 1; tools/talk_truth.py) over every
     line, against the moment's facts (here.*, pair.*, world.*) and the speaker's (self.*); P0's provisional lexicon
     (weather, hour, kin, fishing) before it, so Phase 1 froze T2 again as its own baseline
  T3 self-reference errors per 1,000 lines (§15.3): a speaker saying their own full name (the teller as the victim in the
     third person); the listener named in the third person to their face ("Varg Acorndell snores like a mill." said to
     Varg; a name said to them as a call, "..., Varg Acorndell." or "Varg Acorndell, ...", is not); and an unbacked
     first-person claim: an eyewitness's words ("I saw...", "with my own eyes") about a crime, a rumour or a camp from one
     who didn't see it (the conversation's `seen`, from talk_sim; a run without it isn't judged on this)
  T5 telling what the listener already believes (Phase 4): of the lines passing on the topic's claim (asserts) whose
     listener talk_sim judged (to_knew), those told to one who held it before the talk began and not answered by them
     with "I'd heard" (react.news.known or .again)
  K1 conversations on everyday topics: those Data/Talk/topics.json calls everyday (Phase 5; before it, anything but
     news, gossip, crime, deeds, events, quarrels and the like)
  K2 the largest topic's share
  K3 the same news (topic, subject, claim; one's own news by its teller) told twice to the same resident; where the run
     says what each line tells (talk_sim's told_key, Slice 2's review), every piece of news a talk tells, at its start or
     turned to, to everyone in it (a joiner too), the town's crime and news among them
  C5 unmarked topic jumps per 1,000 lines (Phase 5): a line on another topic than the line before (talk_sim's topic_now)
     with no pivot, change of subject or return among its moves
  C9 callbacks per 10 conversations (Phase 5): lines standing on the talk ledger (their criteria read it: pair.last_talk,
     pair.talked.*, a promise, an invitation; a return to a thread). "Back again?" isn't one since Slice 2's review
  D1 lines carrying a dialect's words (a dialect grammar layer in the expansion) spoken in accent none: the hard gate
     (§15.3: "none 0%", "hard for none"; §10: none sounds plain); D1L lines spoken light (25-50%) and D1B broad (60%+),
     soft. Phase 6: by the accent a line was spoken in (talk_sim's spoken_accent: a step broader toward kin and friends
     of one's own speech, plainer toward a class above, a stranger from elsewhere, a hostile pair; §10.6), the speaker's
     own where a run has none. D1A: the lines spoken in another accent than the speaker's own (broader, plainer). Since
     Slice 2's review D1L and D1B judge what is said: a greeting, "Back again?" or the closing (moves all ceremony) left out
     (D1_detail's "body"; every line beside it)
  T4 identity drift (Phase 6): of the lines airing a stance (issue, stance), one whose speaker's side changed from their
     last within 30 days on the same basis (the same leans held, the same drift: nothing in their life moved it) and no
     concede between, or an answer against the speaker's own
     side (agreeing with the other side, disagreeing with one's own), per 1,000 stance lines. O1 opinions given per 10
     conversations (a diagnostic)
  T6 "they", "them", "their" said after the one known resident a line names (Phase 6; the lint's they_named is the
     hard gate on the library's lines, this the soft one on what is said), per 1,000 lines; and (Slice 2's review) the
     answer to a line naming one known wolf that names nobody and says "they" ("Aud Kemp's arrested." / "Then they'll be
     out by noon."): T6_detail's across
  D2 another dialect's words in a speaker's line (by the expansion's layers, and by tools/dialect_check.py's lexicon)
  D3 a speaker's dialect guessed from 20 of their lines (naive Bayes on words, trained on the other speakers), light and
     broad speakers
  U1 a resident's lines from their own words (character tables now, mantles from Phase 9)
  U2 distinguishability: of two residents with the same eight axes (seven traits and dialect), held-out lines given to
     the right one
  B1 the director's timings (run.json's beat_ms and look_ms, Phase 4; §16): B1 a beat's mean (0.6 ms), B1P its 99th
     percentile (3 ms), B1L a look's 99th percentile (5 ms); the slowest profile's, with the run's load. Soft here: the
     budget's gate is Tests/talk_bench, alone on the machine
  B5 exchanges simulated a second (run.json)

Gates (--check against --baseline): a hard gate the baseline passed must still pass; one the baseline failed may not get
worse (each phase closes its own); a soft metric may not regress by more than 10%. "Worse" means beyond noise: a metric
that counts events (EVENTS: R4, R5, R8, C1, C1Q, C3, C4, C5, C6, C8, C9, T1-T3, T5, K3, D1, D1L, D1B, D3) only if the exact test of two Poisson counts
(today's events against the baseline's, each over its own exposure: a binomial split of the two together) puts the
difference beyond chance at ALPHA (1%, one-sided), so both cards' noise counts; R1 and R2, first-time waits that are the
earliest of many listeners', by more than half; B5, a timing, by more than 25%, and B1's by more than half. A profile the baseline has and the card
hasn't is a problem in itself. The scorecard says which hard gates are still open.
"""
from __future__ import annotations

import argparse
import collections
import datetime
import importlib.util
import json
import math
import random
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ------------------------------------------------------------------ The metrics' definitions

# id: (name, better ("higher"/"lower"), gate ("hard"/"soft"), target as a test or None, target's words, phase)
METRICS = {
    "R1": ("first identical conversation, per listener (h)", "higher", "hard", lambda v: v >= 20, "40 h+; hard 20 h+", ""),
    "R2": ("first repeated opener, per listener (h)", "higher", "soft", lambda v: v >= 8, "8 h+", ""),
    "R3": ("distinct openers per 3,000 exchanges", "higher", "soft", lambda v: v >= 1500, "L1 1,500+", ""),
    "R4": ("distinctive line heard again within 10 h, per 100 h", "lower", "soft", lambda v: v <= 5, "5 or fewer", ""),
    "R5": ("collision index, per 100 h", "lower", "hard", lambda v: v <= 3, "1 or fewer; hard 3", ""),
    "R6": ("most-spoken line's share (%)", "lower", "soft", lambda v: v <= .4, "0.4%", ""),
    "R7": ("distinct reply entries per 1,000 reply beats", "higher", "soft", lambda v: v >= 500, "500+", ""),
    "R8": ("habit saturation (% of a speaker's lines, mean)", "lower", "soft", lambda v: v <= 15, "15% or less", ""),
    "C1": ("owed answers answered, by moves (%)", "higher", "hard", lambda v: v >= 92, "97%; hard 92%", ""),
    "C1Q": ("questions answered: the '?' heuristic (%)", "higher", "soft", None,
            "diagnostic (C1 until Phase 3; doesn't separate from the control)", ""),
    "M1": ("lines carrying moves (%)", "higher", "soft", None, "100% at Phase 7 (every entry)", ""),
    "C2": ("replies at the best fit available (%)", "higher", "soft", lambda v: v >= 95, "95%", "Phase 3"),
    "C3": ("endings by a farewell or a marked break (%)", "higher", "soft", lambda v: v >= 80, "80% / 3% / 5%", ""),
    "C4": ("tone clashes per 1,000 beats", "lower", "hard", lambda v: v <= 0, "0", ""),
    "C5": ("unmarked topic jumps per 1,000 beats", "lower", "soft", lambda v: v <= 5, "5", "Phase 5"),
    "C6": ("same entry or question twice in a conversation, per 1,000", "lower", "hard", lambda v: v <= 0, "0", ""),
    "C7": ("lines per conversation (mean)", "higher", "soft", lambda v: v >= 5, "5-7; p90 9+", ""),
    "C8": ("conversations with a third speaker (%)", "higher", "soft", lambda v: 5 <= v <= 15, "5-15%", ""),
    "C9": ("callbacks per 10 conversations", "higher", "soft", lambda v: v >= 1.5, "1.5+", "Phase 5"),
    "T1": ("raw ids spoken, per 1,000 lines", "lower", "hard", lambda v: v <= 0, "0", ""),
    "T2": ("presupposition failures, per 1,000 lines", "lower", "hard", lambda v: v <= 0, "0", ""),
    "T3": ("self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines",
           "lower", "hard", lambda v: v <= 0, "0", ""),
    "T4": ("identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines",
           "lower", "hard", lambda v: v <= 0, "0", "Phase 6"),
    "T6": ("'they' said of one known wolf named in the line, per 1,000 lines", "lower", "soft", lambda v: v <= 0, "0",
           "Phase 6 (the lint's they_named is the hard gate)"),
    "D1A": ("lines said a step broader or plainer than the speaker's own accent (%)", "higher", "soft", None,
            "diagnostic: accommodation (§10.6)", "Phase 6"),
    "O1": ("opinions given per 10 conversations", "higher", "soft", None, "diagnostic (§8.8)", "Phase 6"),
    "T5": ("telling what the listener already believes, with no 'known' answer (%)", "lower", "soft", lambda v: v <= 2, "2%", ""),
    "D1": ("lines with a dialect's words (%): accent none", "lower", "hard", lambda v: v <= 0, "none 0%", ""),
    "D1L": ("lines with a dialect's words (%): light speakers", "higher", "soft", lambda v: 25 <= v <= 50, "light 25-50%", ""),
    "D1B": ("lines with a dialect's words (%): broad speakers", "higher", "soft", lambda v: v >= 60, "broad 60%+", ""),
    "D2": ("another dialect's words in a speaker's line", "lower", "hard", lambda v: v <= 0, "0", ""),
    "D3": ("dialect guessed from 20 lines (%): light and broad", "higher", "soft", lambda v: v >= 85, "85%+", ""),
    "U1": ("a resident's lines from their own words (%)", "higher", "soft", lambda v: 20 <= v <= 35, "20-35%", ""),
    "U2": ("distinguishability of look-alike residents (%)", "higher", "soft", lambda v: v >= 90, "90%+", ""),
    "K1": ("conversations on everyday topics (%)", "higher", "soft", lambda v: v >= 35, "35%+", ""),
    "K2": ("largest topic's share (%)", "lower", "soft", lambda v: v <= 25, "25% or less", ""),
    "K3": ("same news told twice to the same resident", "lower", "hard", lambda v: v <= 0, "0", ""),
    "W1": ("window lines from mantles (%)", "higher", "soft", None, "40%+", "Phase 13"),
    "W2": ("free-text understanding", "higher", "hard", None, "85%+; wrong 8% or less", "Phase 13"),
    "W3": ("a resident answers chat with the option off", "lower", "hard", None, "0", "Phase 12"),
    # (B1's gate is Tests/talk_bench's, alone on the machine: §16. A card's runs share it ten at a time, and a fixture's
    # restart every game midnight starts each day's world cold, so here they are watched, not gated: soft.)
    "B1": ("one beat: the director's mean (ms)", "lower", "soft", lambda v: v <= .6, "0.6 ms or less (gate: talk_bench)", ""),
    "B1P": ("one beat: the director's 99th percentile (ms)", "lower", "soft", lambda v: v <= 3, "3 ms or less (gate: talk_bench)", ""),
    "B1L": ("a director look: 99th percentile (ms)", "lower", "soft", lambda v: v <= 5, "5 ms or less (gate: talk_bench)", ""),
    "B5": ("exchanges simulated a second", "higher", "hard", lambda v: v >= 150, "150+", ""),
    "X1": ("lexical cohesion: replies sharing a content word with the line before (%)", "higher", "soft", None,
           "diagnostic (not in §15.3): shows the control is discriminating", ""),
    "H1": ("blind naturalness 1-5", "higher", "soft", None, "3.8+", "slice ends (talk_rate.py)"),
    "H2": ("'written whole by a writer?' (%)", "higher", "soft", None, "40%+", "slice ends (talk_rate.py)"),
}
# The coherence metrics, which must separate the real transcripts from the shuffled control (§15.5). C7 (length) and C8
# (a third speaker) are structural: the control keeps each conversation's length and people, so they match it by
# construction; R, D, K and U metrics are about variety, voice and topics, not order.
COHERENCE = {"C1", "C3", "C4", "C6", "X1"}
STRUCTURAL = {"C7", "C8"}
STOP = set("""a an the and or but if then so of to in on at by for with from up down out over under about as is are was
were be been being am i you he she it we they me him her us them my your his its our their this that these those there
here what who whom which when where why how not no nor do does did done have has had having will would shall should can
could may might must just too very all any some more most much many such only own same than s t don't i'm you're it's
that's i'll you'll we'll i've i'd you'd isn't aren't wasn't weren't won't wouldn't can't couldn't oh ah aye yes well
now still yet ever again one two get got go going gone come came say said tell told know knew think thought like
see saw look""".split())

NEWSY = {"gossip", "crime", "news", "deed", "event", "quarrel", "loss", "prices", "caravan", "bandits", "ships",
         "harvest", "newcomer", "storyteller", "festival", "death", "life"}


def everyday_topics() -> set[str] | None:
    """The topics Data/Talk/topics.json calls everyday (Phase 5: K1 counts them, as the director's everyday floor does);
    None without the file (a library from before Phase 5: K1 is everything but NEWSY)."""
    path = ROOT / "Data" / "Talk" / "topics.json"
    try:
        topics = json.loads(path.read_text())["topics"]
    except (OSError, ValueError, KeyError):
        return None
    return {t for t, v in topics.items() if isinstance(v, dict) and v.get("everyday")}


EVERYDAY = everyday_topics()
GRAVE = re.compile(r"\b(died|dead|death|dying|buried|burial|grave|funeral|mourn\w*|grie\w+|murder\w*|killed|"
                   r"robbed|beaten|beating|stabbed|hanged|drowned|widow\w*|orphan\w*|sick unto|plague)\b", re.I)
LIGHT = re.compile(r"\b(ha|hah|haha|heh|hee|laugh\w*|jest\w*|joke\w*|tease\w*|giggl\w*|grin\w*|wink\w*)\b", re.I)
BRIDGE = re.compile(r"\b(anyway|well then|enough of that|still|mind you|on a brighter note|but listen)\b", re.I)
FAREWELL = re.compile(r"\b(farewell|goodbye|good night|goodnight|go well|go in peace|fair winds|safe road|take care|"
                      r"see you|be seeing|i'?d best|i must be off|i'?ll be off|must go|be off|off with you|"
                      r"till (?:tomorrow|next)|until (?:tomorrow|next)|calm water|dry feet|walk warm|rest well|"
                      r"mind how you go|bright one keep you|gods keep|keep well|later,|sleep (?:well|sound)|"
                      r"good day to you|peace on your road|water on your way|keep your lamp lit|lamp lit|root well|"
                      r"shade keep you|good trading|ripe days|look after yourself|mind yourself|bye)\b", re.I)
# The dialects' farewells that are ordinary words elsewhere ("a fair price", "a good harvest"): a farewell only as a
# line's last words (Data/Talk/grammar/*.grammar's farewell).
FAREWELL_END = re.compile(r"(?:^|[.!?,]\s*)(fair price|good harvest)(?:,[^.!?]*)?[.!]?\s*$", re.I)
# The moves that take leave, and the closing family (doc 84 §7.2, §8.4).
LEAVING = ("farewell", "farewell.return", "leave", "break")
CLOSING = ("preclose", "accept_preclose", "extend", "farewell", "farewell.return", "leave", "break")
BRIDGES = ("change_subject", "assess", "preclose")
# The ceremony of a talk (greetings, "Back again?", kin's first words, the closing): R1 and D1L/D1B judge what is said
# between them (Slice 2's review: a farewell said in another way made a talk heard word for word "new").
CEREMONY = CLOSING + ("greet", "greet.return", "again.open", "kin.open")
# A light bridge said in the same breath as grave news (C4 within a line: "There's better news than that, at least.
# They're burying Gunnar Harrowgate."; Slice 2's review).
LIGHT_BRIDGE = re.compile(r"\b(better news|not all the news is bad|that's the kind news|the good news is)\b", re.I)
SENTENCE = re.compile(r"(?<=[.!?])\s+")
BURY = re.compile(r"\b(bury\w*|burie\w*)\b", re.I)
RAW_ID = re.compile(r"\b[A-Za-z]+_[A-Za-z0-9_]+\b")
sys.path.insert(0, str(Path(__file__).resolve().parent))
import talk_truth  # noqa: E402
TRUTH, _TRUTH_IDS = talk_truth.load()
WORD = re.compile(r"[a-z']+")


# ------------------------------------------------------------------ Reading runs

def relative(path: Path) -> str:
    """A path as the repository sees it (no home directory in what is committed), else as given."""
    try:
        return str(Path(path).resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def read_jsonl(path: Path) -> list[dict]:
    if not path.is_file():
        return []
    out = []
    with path.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                try:
                    out.append(json.loads(line))
                except ValueError:
                    pass
    return out


class Run:
    def __init__(self, directory: Path, shuffled: bool = False):
        self.dir = directory
        self.meta = json.loads((directory / "run.json").read_text()) if (directory / "run.json").is_file() else {}
        suffix = "-shuffled" if shuffled else ""
        self.lines = read_jsonl(directory / f"lines{suffix}.jsonl")
        self.convs = read_jsonl(directory / f"convs{suffix}.jsonl")
        self.residents = {r["id"]: r for r in read_jsonl(directory / "residents.jsonl") if "id" in r}
        self.name = self.meta.get("profile") or directory.name
        self.shuffled = shuffled
        self.by_conv: dict[int, list[dict]] = collections.defaultdict(list)
        for line in self.lines:
            self.by_conv[int(line["conv"])].append(line)
        for c in self.by_conv.values():
            c.sort(key=lambda l: l["beat"])
        self.conv_head = {int(c["conv"]): c for c in self.convs}
        self.hours = float(self.meta.get("hours") or 0) or (max((l["t"] for l in self.lines), default=0) / 3600)

    def has_control(self) -> bool:
        return (self.dir / "lines-shuffled.jsonl").is_file()


def words(text: str) -> list[str]:
    return WORD.findall(text.lower())


def pct(n: float, d: float) -> float:
    return 100.0 * n / d if d else 0.0


def quantile(xs: list[float], q: float) -> float:
    if not xs:
        return 0.0
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(q * len(xs)))]


def dialect_lexicon():
    """tools/dialect_check.py's distinctive phrases for each dialect (None if it can't be read)."""
    try:
        spec = importlib.util.spec_from_file_location("dialect_check", ROOT / "tools" / "dialect_check.py")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        allow = json.loads(mod.ALLOW.read_text()) if mod.ALLOW.is_file() else {}
        return mod, mod.lexicon(mod.grammars(), allow)
    except Exception:  # noqa: BLE001 (a missing or changed tool: D2's text check is skipped, said so)
        return None, None


DIALECT = dialect_lexicon()


# ------------------------------------------------------------------ Per run

def move_book() -> dict:
    """Data/Talk/moves.json's moves (not its aliases): what C1M walks for fallbacks. {} without it."""
    try:
        doc = json.loads((ROOT / "Data" / "Talk" / "moves.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return {k: v for k, v in doc.get("moves", {}).items() if "alias" not in v}


def meets(done: str, owed: str, book: dict | None = None) -> bool:
    """A move done meets an owed one: the same, or a variant of it (RatwTalk.h, Moves::kindOf: a move of its own, such
    as farewell.return, is never another's kind)."""
    return done == owed or (done.startswith(owed + ".") and (book is None or done not in book))


def closing(line: dict, book: dict | None = None) -> bool:
    """A line of the closing (a preclose, a farewell and their answers: §8.4), by its moves."""
    ms = line.get("moves") or []
    return bool(ms) and all(any(meets(m_, c, book) for c in CLOSING) for m_ in ms)


def ceremony(line: dict, book: dict | None = None) -> bool:
    """A greeting, "Back again?", kin's first words or a line of the closing: the talk's ceremony, by its moves."""
    ms = line.get("moves") or []
    return bool(ms) and all(any(meets(m_, c, book) for c in CEREMONY) for m_ in ms)


def body_key(c: list[dict], book: dict | None = None) -> str:
    """A conversation as R1 compares it (Slice 2's review): the words between its ceremonies, the opener's own words
    without what was said in its breath (a greeting, "Back again?", a recall), the closing left out."""
    out = []
    for i, l in enumerate(c):
        if ceremony(l, book):
            continue
        out.append(l.get("bare") or l["text"] if i == 0 else l["text"])
    return "\n".join(out) if out else "\n".join(l["text"] for l in c)


def sentences(text: str) -> list[str]:
    return [s.strip() for s in SENTENCE.split(text) if s.strip()]


def fallbacks(book: dict, move: str) -> list[str]:
    """A move and its fallbacks, walked breadth first (Moves::chain); a variant's are its move's."""
    out, todo = [], [move]
    while todo:
        m_ = todo.pop(0)
        if m_ in out:
            continue
        out.append(m_)
        base = m_ if m_ in book else m_.rsplit(".", 1)[0]
        todo += book.get(base, {}).get("fallback", [])
    return out


def per_listener(run: Run) -> dict[str, list[list[dict]]]:
    """Each listener's conversations, in order, as their lines."""
    out = collections.defaultdict(list)
    for cid in sorted(run.by_conv):
        lines = run.by_conv[cid]
        if lines:
            out[lines[0].get("listener", "")].append(lines)
    return out


def score_run(run: Run) -> dict:
    m: dict[str, object] = {}
    lines, convs = run.lines, [run.by_conv[c] for c in sorted(run.by_conv)]
    hours = max(run.hours, 1e-9)
    listeners = per_listener(run)
    # R1, R2, R3: per listener.
    r1, r2, r3 = [], [], []
    r1_censored = r2_censored = False
    first_exchange = None
    book = move_book()
    for who, cs in listeners.items():
        seen_conv, seen_open = {}, {}
        first_conv = first_open = None
        for i, c in enumerate(cs):
            key = body_key(c, book)
            if first_conv is None and key in seen_conv and len(c) > 0:
                first_conv = (c[0]["t"] / 3600, i + 1)
            seen_conv.setdefault(key, i)
            o = c[0]["text"]
            if first_open is None and o in seen_open:
                first_open = (c[0]["t"] / 3600, i + 1)
            seen_open.setdefault(o, i)
        listened = cs[-1][-1]["t"] / 3600 if cs else 0
        if first_conv is None:
            r1_censored = True
        elif first_exchange is None or first_conv[0] < first_exchange[0]:
            first_exchange = first_conv
        if first_open is None:
            r2_censored = True
        r1.append(first_conv[0] if first_conv else listened)
        r2.append(first_open[0] if first_open else listened)
        firsts = [c[0]["text"] for c in cs[:3000]]
        r3.append(len(set(firsts)) * (3000 / len(firsts)) if firsts else 0)
    m["R1"] = min(r1) if r1 else 0
    m["R1_detail"] = {"listeners": len(r1), "median": quantile(r1, .5), "censored": r1_censored,
                      "exchange": first_exchange[1] if first_exchange else None}
    m["R2"] = min(r2) if r2 else 0
    m["R2_detail"] = {"median": quantile(r2, .5), "censored": r2_censored}
    m["R3"] = sum(r3) / len(r3) if r3 else 0
    # R4: a distinctive sentence (6 words or more) heard again by the same listener within 10 h (Slice 2's review: a
    # sentence spliced into longer lines, "Sit yourself down.", counts as itself); R5: collisions in a place within an
    # hour (whole lines, as said).
    heard: dict[tuple, float] = {}
    said_at: dict[tuple, list[tuple[float, str]]] = collections.defaultdict(list)
    r4 = r5 = 0
    for l in sorted(lines, key=lambda x: x["t"]):
        for s in sentences(l["text"]):
            if len(words(s)) < 6:
                continue
            k = (l.get("listener", ""), s)
            if k in heard and l["t"] - heard[k] <= 36000:
                r4 += 1
            heard[k] = l["t"]
        if len(words(l["text"])) < 6:
            continue
        place = (l.get("cell", ""), l["text"])
        if any(l["t"] - t <= 3600 and who != l["who"] for t, who in said_at[place]):
            r5 += 1
        said_at[place] = [(t, w) for t, w in said_at[place] if l["t"] - t <= 3600] + [(l["t"], l["who"])]
    m["R4"] = r4 * 100 / hours
    m["R5"] = r5 * 100 / hours
    # R6: the most-spoken sentence's share of the lines it is in (Slice 2's review: whole lines missed a host's "Sit
    # yourself down." in 7% of a room's lines, said before every opener); the whole line's beside it.
    counts = collections.Counter(s for l in lines for s in set(sentences(l["text"])))
    top = counts.most_common(1)
    whole = collections.Counter(l["text"] for l in lines).most_common(1)
    m["R6"] = pct(top[0][1], len(lines)) if top else 0
    m["R6_detail"] = {"sentence": top[0][0] if top else "", "times": top[0][1] if top else 0,
                      "line": whole[0][0] if whole else "", "line_pct": round(pct(whole[0][1], len(lines)), 3) if whole else 0}
    replies = [l for l in lines if l["beat"] > 0 and not closing(l, book)]
    m["R7"] = len({l["entry"] for l in replies}) * 1000 / len(replies) if replies else 0
    closings = [l for l in lines if l["beat"] > 0 and closing(l, book)]
    m["R7_detail"] = {"replies": len(replies), "closing_beats": len(closings),
                      "closings_distinct_per_1000": round(len({(l["entry"], l["text"]) for l in closings}) * 1000 / len(closings), 1)
                      if closings else None}
    by_speaker = collections.defaultdict(list)
    for l in lines:
        by_speaker[l["who"]].append(l)
    sat = [pct(sum(1 for l in ls if any(h in ("oath", "saying") or h.startswith(("oath", "saying"))
                                        for h in l.get("habits", []))), len(ls))
           for ls in by_speaker.values() if len(ls) >= 20]
    m["R8"] = sum(sat) / len(sat) if sat else 0
    m["R8_detail"] = {"p90": quantile(sat, .9), "speakers": len(sat),
                      "lines20": sum(len(ls) for ls in by_speaker.values() if len(ls) >= 20),
                      "any_habit_mean": (sum(pct(sum(1 for l in ls if l.get("habits")), len(ls))
                                             for ls in by_speaker.values() if len(ls) >= 20) / len(sat)) if sat else 0}
    # C1: questions answered by the other.
    asked = answered = 0
    for c in convs:
        for i, l in enumerate(c):
            if l["text"].rstrip().endswith("?"):
                asked += 1
                if i + 1 < len(c) and c[i + 1]["who"] != l["who"]:
                    answered += 1
    m["C1Q"] = pct(answered, asked)
    questions = asked
    m["C1Q_detail"] = {"questions": asked, "answered": answered}
    # C1M (Phase 2): what a line owes, met by the next line from the one it is owed by (another speaker; the same one
    # for "self:"; anyone for "any:"), doing an owed move, a kind of one (react.news.known meets react.news), or one of
    # its fallbacks. M1: the lines that carry moves at all.
    owed = met = 0
    for c in convs:
        for i, l in enumerate(c):
            for o in l.get("owes") or []:
                who, alts = o.split(":", 1) if ":" in o else ("other", o)
                owed += 1
                nxt = c[i + 1] if i + 1 < len(c) else None
                if not nxt or (who == "other" and nxt["who"] == l["who"]) or (who == "self" and nxt["who"] != l["who"]):
                    continue
                targets = {t for a in alts.split("|") for t in fallbacks(book, a)}
                if any(meets(m_, t, book) for m_ in nxt.get("moves") or [] for t in targets):
                    met += 1
    m["C1"] = pct(met, owed) if owed else None
    m["C1_detail"] = {"owed": owed, "met": met}
    # C2 (Phase 3): replies at the best fit that held.
    fitted = [l for l in lines if isinstance(l.get("best"), (int, float)) and l["best"] >= 0]
    m["C2"] = pct(sum(1 for l in fitted if l.get("fit", -1) >= l["best"]), len(fitted)) if fitted else None
    m["C2_detail"] = {"answers": len(fitted), "fallbacks": sum(1 for l in fitted if l.get("fit") == 0)}
    carrying = collections.Counter((l.get("route", ""), bool(l.get("moves"))) for l in lines)
    m["M1"] = pct(sum(n for (r, has), n in carrying.items() if has), len(lines)) if lines else 0
    m["M1_detail"] = {route: pct(carrying[(route, True)], carrying[(route, True)] + carrying[(route, False)])
                      for route in sorted({r for r, _ in carrying})}
    # C3: how conversations end. A good ending is a farewell or a marked break (a line that breaks off, said: §15.3;
    # the conversation's `marked`, from Phase 4). A talk cut off with no line (one walked away, the other was busy: "gone",
    # "cell", "apart", "busy", "chat"; §8.10) or by the run's end or a snapshot is counted apart, never as a good ending.
    farewell = marked = cap = nothing = cut = curt = 0
    for cid, c in run.by_conv.items():
        head = run.conv_head.get(cid, {})
        reason = head.get("reason", "done")
        last = c[-1] if c else {}
        if head.get("marked"):
            marked += 1
        elif reason not in ("done", "run_end", "snapshot"):
            cut += 1
        elif c and (any(meets(m_, x, book) for m_ in last.get("moves") or [] for x in LEAVING) or
                    FAREWELL.search(last["text"]) or FAREWELL_END.search(last["text"])):
            farewell += 1
        elif head.get("ended") == "cap" or ("ended" not in head and int(head.get("written", len(c))) >= 8):
            cap += 1
        elif head.get("ended") == "curt":
            curt += 1                               # (Ended short on purpose, §8.4: a good ending. Slice 2's review.)
        else:
            nothing += 1
    n = max(1, len(run.by_conv))
    m["C3"] = pct(farewell + marked + curt, n)
    m["C3_detail"] = {"farewell": pct(farewell, n), "marked_break": pct(marked, n), "cut_off": pct(cut, n), "cap": pct(cap, n),
                      "nothing_fits": pct(nothing, n), "curt": pct(curt, n)}
    # C4: grave then light from the other within two beats, with no bridge.
    clashes = beats = by_tone = 0
    for c in convs:
        beats += len(c)
        for i, l in enumerate(c):
            grave_words, grave_tone = bool(GRAVE.search(l["text"])), l.get("tone") == "grave"
            if not (grave_words or grave_tone):
                continue
            for j in (i + 1, i + 2):
                if j >= len(c):
                    break
                if any(meets(m_, b, book) for m_ in c[j].get("moves") or [] for b in BRIDGES):
                    break                           # (Bridged: "Well. Enough of that.")
                if c[j]["who"] == l["who"]:
                    continue
                tone_clash = grave_tone and c[j].get("tone") == "light"
                word_clash = grave_words and LIGHT.search(c[j]["text"]) and not BRIDGE.search(c[j]["text"])
                if tone_clash or word_clash:
                    clashes += 1
                    by_tone += bool(tone_clash)
                    break
    # (Within a line: a light bridge into grave news in the same breath. Slice 2's review.)
    in_line = sum(1 for l in lines if LIGHT_BRIDGE.search(l["text"]) and (GRAVE.search(l["text"]) or BURY.search(l["text"])))
    clashes += in_line
    m["C4"] = clashes * 1000 / max(1, beats)
    m["C4_detail"] = {"clashes": clashes, "by_tone": by_tone, "in_line": in_line}
    # C6: an entry again after another (a scene's own lines are one), or the same question twice.
    repeats = 0
    for c in convs:
        units, qs, bad = [], set(), False
        for l in c:
            if not units or units[-1] != l["entry"]:
                if l["entry"] and l["entry"] in units:
                    bad = True
                units.append(l["entry"])
            if l["text"].rstrip().endswith("?"):
                q = l["text"].strip().lower()
                if q in qs:
                    bad = True
                qs.add(q)
        repeats += bad
    m["C6"] = repeats * 1000 / max(1, len(convs))
    sizes = [len(c) for c in convs]
    m["C7"] = sum(sizes) / len(sizes) if sizes else 0
    m["C7_detail"] = {"p90": quantile(sizes, .9), "three_lines": pct(sum(1 for s in sizes if s == 3), len(sizes)),
                      "four_lines": pct(sum(1 for s in sizes if s == 4), len(sizes))}
    m["C8"] = pct(sum(1 for c in convs if len({l["who"] for l in c}) > 2), len(convs))
    # T1, T2, T3.
    m["T1"] = sum(1 for l in lines if RAW_ID.search(l["text"])) * 1000 / max(1, len(lines))
    t1_examples = [l["text"] for l in lines if RAW_ID.search(l["text"])][:3]
    # T2 (Phase 1): truth.json's rules over every line, against the moment's facts (the conversation's here.*, pair.*,
    # world.*) and the speaker's own (self.*); a rule none of whose needs was recorded isn't judged.
    t2 = collections.Counter()
    t2_examples = []
    for cid, c in run.by_conv.items():
        head = run.conv_head.get(cid, {})
        moment = {k: v for k, v in head.get("facts", {}).items() if not k.startswith("_")}
        if "_weather" in head.get("facts", {}) and "here.outdoors" not in moment:
            moment["here.outdoors"] = head["facts"]["_weather"]     # (A run from before here.outdoors.)
        moment.setdefault("here.hour", str(int(float(head.get("hour", 12)))))
        for l in c:
            text = l["text"]
            rules = talk_truth.matches(TRUTH, text)
            if not rules:
                continue
            facts = dict(moment)
            if "hour" in l:                         # (The line's own hour, Phase 5: a talk that began by day may close at dusk.)
                hour = float(l["hour"])
                facts["here.hour"] = str(int(hour))
                facts["here.time"] = ("night" if hour < 5 or hour >= 20 else "dawn" if hour < 7.5 else "day" if hour < 18
                                      else "dusk")
            for k, v in run.residents.get(l["who"], {}).items():
                if not k.startswith("_") and isinstance(v, (str, int, float)):
                    facts["self." + k] = str(v)
            # (Phase 6: the topic's claim kind and kind as the line was said; a rule scoped to topics judges only theirs.)
            if l.get("claim_kind"):
                facts["topic.claim_kind"] = l["claim_kind"]
            if l.get("topic_kind"):
                facts["topic.kind"] = l["topic_kind"]
            topic = l.get("topic_now") or head.get("topic") or l.get("topic")
            failed = [r.id for r in rules if r.scoped(topic) and r.judge(facts) is False]
            for k in failed:
                t2[k] += 1
            if failed and len(t2_examples) < 6:
                t2_examples.append(f"{'/'.join(failed)}: {text} ({l['who']}, {moment.get('here.weather', '?')}/"
                                   f"{moment.get('here.outdoors', '?')} {moment.get('here.hour', '?')}h)")
    m["T2"] = sum(t2.values()) * 1000 / max(1, len(lines))
    m["T2_detail"] = {"by_kind": dict(t2), "examples": t2_examples}
    t3 = collections.Counter()
    t3_examples = []
    for cid, c in run.by_conv.items():
        head = run.conv_head.get(cid, {})
        for l in c:
            kind = self_reference(l, head, run.residents)
            if kind:
                t3[kind] += 1
                if len(t3_examples) < 6:
                    t3_examples.append(f"{kind}: {l['who']} to {l.get('to', '')}: {l['text']}")
    m["T3"] = sum(t3.values()) * 1000 / max(1, len(lines))
    m["T3_detail"] = {"by_kind": dict(t3), "examples": t3_examples,
                      "eyewitness_judged": any("seen" in h for h in run.conv_head.values())}
    # T4 (Phase 6, §15.3: identity drift). Of the lines that air a stance (talk_sim's issue and stance: an opinion given,
    # agreed or disagreed with, insisted on, conceded, a third siding or correcting): one whose speaker's side on the issue
    # changed from their last such line within 30 days with no event moving it (its drift steps the same) and no concede
    # of theirs between; and an answer against one's own side (agreeing with one on the other side, disagreeing with one
    # on the same), per 1,000 stance lines.
    stance_lines = drifted = against_own = 0
    t4_examples = []
    last_side = {}
    conceded = set()
    for l in sorted((l for l in lines if l.get("issue")), key=lambda l: (float(l.get("day", 0)), float(l.get("t", 0)))):
        stance_lines += 1
        key = (l["who"], l["issue"])
        moves_ = l.get("moves") or []
        prev = last_side.get(key)
        # (The same basis, the same leans and drift: nothing in the wolf's life moved it. A run from before Phase 6's basis
        # compares the drift alone.)
        if prev and prev[0] != l.get("stance") and prev[1] == (l.get("drift", 0), l.get("basis")) and \
                float(l.get("day", 0)) - prev[2] <= 30 and key not in conceded:
            drifted += 1
            if len(t4_examples) < 4:
                t4_examples.append(f"{l['who']} on {l['issue']}: {prev[0]} then {l.get('stance')}: {l['text']}")
        last_side[key] = (l.get("stance"), (l.get("drift", 0), l.get("basis")), float(l.get("day", 0)))
        if any(meets(m_, "concede", book) for m_ in moves_):
            conceded.add(key)
        else:
            conceded.discard(key)
    for cid, c in run.by_conv.items():
        # (An answer is judged against the side of the one it is said to: the opinion's giver, or a joiner's the one who
        # spoke last.)
        held = {}
        for l in c:
            if not l.get("issue"):
                continue
            moves_ = l.get("moves") or []
            held[(l["who"], l["issue"])] = l.get("stance")
            if any(meets(m_, "give.opinion", book) for m_ in moves_):
                continue
            other = held.get((l.get("to", ""), l["issue"]))
            mine = l.get("stance")
            if not other or other == "mixed" or mine == "mixed":
                continue
            agrees = any(meets(m_, "agree", book) or meets(m_, "join.side", book) for m_ in moves_)
            differs = any(meets(m_, "disagree", book) or meets(m_, "join.correct", book) for m_ in moves_)
            if (agrees and mine != other) or (differs and mine == other):
                against_own += 1
                if len(t4_examples) < 6:
                    t4_examples.append(f"{l['who']} ({mine}) to one {other} on {l['issue']}: {l['text']}")
    m["T4"] = (drifted + against_own) * 1000 / stance_lines if stance_lines else 0
    m["T4_detail"] = {"stance_lines": stance_lines, "drifted": drifted, "answered_against_own": against_own,
                      "examples": t4_examples}
    m["O1"] = sum(1 for l in lines if l.get("issue") and any(meets(m_, "give.opinion", book) for m_ in (l.get("moves") or []))) \
        * 10 / max(1, len(convs))
    # T6 (Phase 6): "they", "them" or "their" after the one known resident a line names (the lint's they_named, at run
    # time: residents.jsonl's names), the town's "they say" apart.
    names = {r.get("name") for r in run.residents.values() if isinstance(r, dict) and r.get("name")}
    t6 = 0
    t6_examples = []
    # (Each line's names once: the line's and the line after's checks both read them.)
    capital = re.compile(r"\b[A-Z][a-z]+ [A-Z][a-z]+")
    first_names = {n.split(" ", 1)[0] for n in names if n}
    named_in: dict[int, list[str]] = {}

    def named(l: dict) -> list[str]:
        k = id(l)
        if k not in named_in:
            text = l["text"]
            named_in[k] = [n for n in names if n and n.split(" ", 1)[0] in text and n in text] \
                if any(m_.group(0).split(" ", 1)[0] in first_names for m_ in capital.finditer(text)) else []
        return named_in[k]
    for l in lines:
        text = l["text"]
        found = named(l)
        if len(found) != 1:
            continue
        after = text[text.index(found[0]) + len(found[0]):]
        for w in THEY.finditer(after):
            if TOWN_THEY.match(after, w.start()) or re.search(r"\band\b", after[:w.start()]):
                continue
            t6 += 1
            if len(t6_examples) < 6:
                t6_examples.append(f"{l['who']}: {text}")
            break
    # (Slice 2's review: and across a line: the answer to a line naming one known wolf, naming nobody, says "they":
    # "Aud Kemp's arrested." / "Then they'll be out by noon.")
    t6_cross = 0
    for c in convs:
        for i in range(1, len(c)):
            before, l = c[i - 1], c[i]
            if l["who"] == before["who"]:
                continue
            named_before = named(before)
            if len(named_before) != 1 or re.search(r"\band\b", before["text"][before["text"].index(named_before[0]):]) or \
                    named(l):
                continue
            w = THEY.search(l["text"])
            if w and not TOWN_THEY.match(l["text"], w.start()):
                t6 += 1
                t6_cross += 1
                if len(t6_examples) < 8:
                    t6_examples.append(f"{before['text']} / {l['who']}: {l['text']}")
    m["T6"] = t6 * 1000 / max(1, len(lines))
    m["T6_detail"] = {"lines": t6, "across": t6_cross, "examples": t6_examples}
    m["T1_detail"] = {"examples": t1_examples}
    # K1, K2, K3.
    topics = collections.Counter(run.conv_head.get(cid, {}).get("topic", c[0].get("topic", "")) for cid, c in run.by_conv.items())
    total = sum(topics.values())
    m["K1"] = pct(sum(n for t, n in topics.items() if (t in EVERYDAY if EVERYDAY is not None else t not in NEWSY)), total)
    m["K2"] = pct(topics.most_common(1)[0][1], total) if topics else 0
    m["K2_detail"] = {"topics": {t: round(pct(n, total), 2) for t, n in topics.most_common()}}
    told = collections.Counter()
    if any("told_key" in l for l in lines):
        # (Slice 2's review: every piece of news a talk tells, at its start or turned to, and to everyone in it, a joiner
        # too; the town's crime and news counted with the claims. Each talk once: one who hears it told is told it.)
        for cid, c in run.by_conv.items():
            people = {x for l in c for x in (l["who"], l.get("to", "")) if x}
            heard_here = set()
            for i, l in enumerate(c):
                key = l.get("told_key")
                if not key or (i > 0 and c[i - 1].get("told_key") == key):
                    continue                        # (Only where it starts to be told.)
                for p in people - {l["who"]}:
                    heard_here.add((p, key))
            for k in heard_here:
                told[(k[0], "", k[1], "")] += 1
    for cid, c in ([] if told or any("told_key" in l for l in lines) else run.by_conv.items()):
        head = run.conv_head.get(cid, {})
        if head.get("topic") in NEWSY and (head.get("subject") or head.get("claim")):
            # (One's own news has no subject but its teller: "I married Runa" and "I married Maren", told to one wolf by
            # two, are two weddings, not the same news twice. Slice 2's ticking listen counted them as one.)
            about = head.get("subject") or ("self:" + head.get("teller", "") if head.get("topic") == "news" else "")
            told[(head.get("other", ""), head.get("topic"), about, head.get("claim", ""))] += 1
    m["K3"] = sum(n - 1 for n in told.values() if n > 1)
    m["K3_detail"] = {"repeats": [f"{k[0]} told {k[1]}: {k[2]} {k[3]} x{n}" for k, n in told.most_common(4) if n > 1]}
    # C5, C9 (Phase 5): a talk that turns to another topic (the line's topic_now) without a pivot, a change of subject
    # or a return marking it, per 1,000 lines; and the lines that stand on the talk ledger (callbacks: a line whose
    # criteria read it, a return to a thread, "Back again?") per 10 conversations. Runs from before Phase 5 have no
    # topic_now: not judged.
    if any("topic_now" in l for l in lines):
        jumps = turned = 0
        jump_examples = []
        for c in convs:
            for i in range(1, len(c)):
                a, b = c[i - 1].get("topic_now"), c[i].get("topic_now")
                if not a or not b or a == b:
                    continue
                turned += 1
                if not any(x in ("pivot", "change_subject", "return") for x in c[i].get("moves") or []):
                    jumps += 1
                    if len(jump_examples) < 4:
                        jump_examples.append(f"{a} -> {b}: {c[i]['text']}")
        m["C5"] = jumps * 1000 / max(1, len(lines))
        # ("Back again?" isn't one: it recalls nothing said, and the raters marked it down most. Slice 2's review; the
        # engine no longer counts it as one, and an older run's line made only of it is left out here.)
        callbacks = sum(1 for l in lines if l.get("callback") or "return" in (l.get("moves") or []))
        m["C9"] = callbacks * 10 / max(1, len(convs))
        topics_per = [len({l.get("topic_now") for l in c if l.get("topic_now")}) for c in convs]
        goals = collections.Counter(l["goal"] for l in lines if l.get("goal"))
        m["C5_detail"] = {"turns": turned, "unmarked": jumps, "examples": jump_examples,
                          "topics_per_talk": round(sum(topics_per) / max(1, len(topics_per)), 3),
                          "turned_talks": sum(1 for n in topics_per if n > 1)}
        m["C9_detail"] = {"callbacks": callbacks, "goals": dict(goals.most_common())}
    # T5 (Phase 4): a line passing on the topic's claim (asserts) to one who held it before the talk began (talk_sim's
    # to_knew), not answered by them with "I'd heard" (react.news.known or .again), of the asserting lines judged.
    asserted = knew_unsaid = 0
    t5_examples = []
    for c in convs:
        for i, l in enumerate(c):
            if not l.get("asserts") or l.get("to_knew") not in ("yes", "no"):
                continue
            asserted += 1
            if l["to_knew"] != "yes":
                continue
            reply = next((x for x in c[i + 1:] if x["who"] == l.get("to")), None)
            if not reply or not any(meets(m_, "react.news.known", book) or meets(m_, "react.news.again", book)
                                    for m_ in reply.get("moves") or []):
                knew_unsaid += 1
                if len(t5_examples) < 4:
                    t5_examples.append(f"{l['text']} / {reply['text'] if reply else '(nothing)'}")
    m["T5"] = pct(knew_unsaid, asserted) if asserted else None
    m["T5_detail"] = {"asserted": asserted, "told_what_they_knew": knew_unsaid, "examples": t5_examples}
    # D1, D2.
    by_accent = collections.defaultdict(lambda: [0, 0])
    body_accent = collections.defaultdict(lambda: [0, 0])  # (D1L, D1B: what is said, not the greetings and goodbyes.)
    d2_layers = 0
    d2_text = 0
    d2_examples = []
    mod, lex = DIALECT
    shifted = collections.Counter()
    for l in lines:
        own = l.get("dialect", "")
        used = [u for u in l.get("used", []) if u != "plain" and not u.startswith("p_")]
        # (Phase 6: by the accent the line was spoken in, a step broader or plainer toward the one spoken to; §10.6.)
        acc = l.get("spoken_accent") or l.get("accent") or "none"
        if l.get("spoken_accent") and l.get("accent") and l["spoken_accent"] != l["accent"]:
            levels = {"none": 0, "light": 1, "broad": 2}
            shifted["broader" if levels.get(l["spoken_accent"], 1) > levels.get(l["accent"], 1) else "plainer"] += 1
        by_accent[acc][1] += 1
        if used:
            by_accent[acc][0] += 1
        if not ceremony(l, book):
            body_accent[acc][1] += 1
            body_accent[acc][0] += bool(used)
        if any(u != own for u in used):
            d2_layers += 1
        if lex is not None:
            others = {d for d, _ in mod.hits(l["text"], lex) if d != own and mod_dialect_of(own) != d}
            if others:
                d2_text += 1
                if len(d2_examples) < 4:
                    d2_examples.append(f"{own}: {l['text']} ({', '.join(sorted(others))})")
    m["D1"] = pct(*by_accent["none"]) if by_accent["none"][1] else 0
    # (Slice 2's review: light and broad speakers judged by what they say, not their goodbyes in dialect, which were most
    # of a broad speaker's dialect lines; D1, the hard gate for none, by every line.)
    m["D1L"] = pct(*body_accent["light"]) if body_accent["light"][1] else 0
    m["D1B"] = pct(*body_accent["broad"]) if body_accent["broad"][1] else 0
    m["D1_detail"] = {a: round(pct(*v), 2) for a, v in by_accent.items()}
    m["D1_detail"]["body"] = {a: round(pct(*v), 2) for a, v in body_accent.items()}
    m["D1A"] = pct(sum(shifted.values()), len(lines))
    m["D1A_detail"] = dict(shifted)
    m["D2"] = d2_layers + d2_text
    m["D2_detail"] = {"by_layers": d2_layers, "by_lexicon": d2_text, "examples": d2_examples,
                      "lexicon": "tools/dialect_check.py" if lex is not None else "unavailable"}
    m["D3"], m["D3_detail"] = guess_dialects(lines)
    # U1, U2.
    own = sum(1 for l in lines if "." in l.get("entry", "") and l["entry"].split(".", 1)[0] == l["who"])
    m["U1"] = pct(own, len(lines))
    m["U2"], m["U2_detail"] = distinguish(lines, run.residents)
    m["B5"] = float(run.meta.get("exchanges_per_second") or 0)
    # B1 (Phase 4): the director's own timings, as talk_sim measured them (the run's load included: tools/talk_card runs
    # profiles side by side; Tests/talk_bench measures alone).
    beat, look = run.meta.get("beat_ms") or {}, run.meta.get("look_ms") or {}
    if beat.get("n"):
        m["B1"], m["B1P"] = float(beat.get("mean", 0)), float(beat.get("p99", 0))
    if look.get("n"):
        m["B1L"] = float(look.get("p99", 0))
    m["B1_detail"] = {"beats": beat.get("n", 0), "beat_max": beat.get("max"), "looks": look.get("n", 0), "look_mean": look.get("mean"),
                      "look_max": look.get("max")}
    # X1: a reply (a line after the first, by the other) sharing a content word with the line before it.
    cohesive = replies_n = 0
    for c in convs:
        for i in range(1, len(c)):
            if c[i]["who"] == c[i - 1]["who"] or closing(c[i], book):
                continue                            # (A farewell shares no words with what it ends: R7's closings apart.)
            a = {w for w in words(c[i - 1]["text"]) if w not in STOP and len(w) > 2}
            b = {w for w in words(c[i]["text"]) if w not in STOP and len(w) > 2}
            replies_n += 1
            cohesive += bool(a & b)
    m["X1"] = pct(cohesive, replies_n)
    m["_counts"] = {"lines": len(lines), "conversations": len(convs), "hours": round(run.hours, 2),
                    "listeners": len(listeners), "speakers": len(by_speaker), "questions": questions, "owed": owed,
                    "asserted": asserted, "stance_lines": stance_lines,
                    "none_lines": by_accent["none"][1], "light_lines": by_accent["light"][1],
                    "broad_lines": by_accent["broad"][1]}
    return m


# T6 (Phase 6): "they" for one known wolf, as tools/talk_lint.py's they_named has it; the town's "they say" apart.
THEY = re.compile(r"\b(they|them|their|theirs|themselves|themself|they're|they'd|they'll|they've)\b", re.I)
TOWN_THEY = re.compile(r"\b(they (?:say|said|reckon|tell|told|do say|are saying|were saying|call|found|took)|they're saying|"
                       r"so they say|what they say|they've (?:said|been saying)|they'll (?:say|tell))\b", re.I)
EYEWITNESS = re.compile(r"\b(I saw (?!(?:them|him|her|#[^#]*#) only\b)|I watched|I seen|saw it myself|saw it with my own|"
                        r"with my own eyes|I was there when)", re.I)
# Topics whose eyewitness words claim the deed itself was seen (a death's "I saw them only on Restday" is not one).
WITNESSED = {"gossip", "crime", "bandits", "deed"}


def calls(text: str, at: int, end: int) -> bool:
    """Whether a name at text[at:end] is said to someone as a call ("Varg Acorndell, a word?", "..., Varg Acorndell.",
    "Between us, Varg Acorndell: ..."), not of them in the third person ("Varg Acorndell snores", "I married Varg
    Acorndell.")."""
    before = text[:at].rstrip()
    after = text[end:]
    opens = not before or before[-1] in ".!?\"" or before.endswith((",", "--"))
    closes = not after.strip() or after[:1] in ",.!?;:"
    return (opens and (after[:1] in ",:" or not after.strip())) or (before.endswith(",") and closes)


def self_reference(line: dict, head: dict, residents: dict) -> str:
    """T3's kind for a line, or "": its speaker's own full name ("own_name"); the listener's full name said of them in the
    third person to their face ("listener_third"); an eyewitness's words from one who didn't see it ("unbacked")."""
    text = line["text"]
    me = residents.get(line["who"], {}).get("name")
    if me and me in text:
        return "own_name"
    them = residents.get(line.get("to", ""), {}).get("name")
    if them and " " in them:
        for found in re.finditer(re.escape(them), text):
            if not calls(text, found.start(), found.end()):
                return "listener_third"
    if "seen" in head and head.get("topic") in WITNESSED and EYEWITNESS.search(text):
        backed = line["who"] == head.get("teller") and head.get("seen") == "yes"
        if not backed:
            return "unbacked"
    return ""


def mod_dialect_of(d: str) -> str:
    return {"fenshore": "mirebridge"}.get(d, d)


def guess_dialects(lines: list[dict], per: int = 20, seed: int = 7):
    """D3: for each speaker with 20+ lines (light or broad), 20 of their lines classified by a naive Bayes over words
    trained on every other speaker's lines (leave one speaker out)."""
    by = collections.defaultdict(list)
    for l in lines:
        by[l["who"]].append(l)
    speakers = {w: ls for w, ls in by.items() if len(ls) >= per and ls[0].get("dialect")}
    if len({ls[0]["dialect"] for ls in speakers.values()}) < 2:
        # (Not judged: a profile that heard one dialect says nothing of D3, rather than 0% weighed by its lines.)
        return None, {"speakers": len(speakers), "judged": 0, "note": "fewer than two dialects heard"}
    counts = collections.defaultdict(collections.Counter)
    totals = collections.Counter()
    per_speaker = {}
    for w, ls in speakers.items():
        c = collections.Counter(t for l in ls for t in words(l["text"]))
        per_speaker[w] = c
        counts[ls[0]["dialect"]].update(c)
        totals[ls[0]["dialect"]] += sum(c.values())
    vocab = len(set().union(*counts.values())) or 1
    rng = random.Random(seed)
    right = n = 0
    by_accent = collections.defaultdict(lambda: [0, 0])
    for w, ls in speakers.items():
        accent = ls[0].get("accent") or "none"
        truth = ls[0]["dialect"]
        sample = rng.sample(ls, per)
        tokens = [t for l in sample for t in words(l["text"])]
        best, best_score = None, -1e300
        for d in counts:
            c = counts[d] - per_speaker[w] if d == truth else counts[d]
            tot = totals[d] - (sum(per_speaker[w].values()) if d == truth else 0)
            if tot <= 0:
                continue
            score = sum(math.log((c[t] + 1) / (tot + vocab)) for t in tokens)
            if score > best_score:
                best, best_score = d, score
        ok = best == truth
        by_accent[accent][0] += ok
        by_accent[accent][1] += 1
        if accent in ("light", "broad"):
            right += ok
            n += 1
    return (pct(right, n) if n else None), {a: round(pct(*v), 1) for a, v in by_accent.items()} | {"speakers": len(speakers),
                                                                                                    "judged": n}


def distinguish(lines: list[dict], residents: dict, seed: int = 11):
    """U2: pairs of residents with the same eight axes, each with 30+ lines; each one's held-out half attributed by
    unigram models trained on the other halves."""
    axes = ("temper", "warmth", "humour", "honesty", "drive", "piety", "talk", "_dialect")
    by = collections.defaultdict(list)
    for l in lines:
        by[l["who"]].append(l["text"])
    groups = collections.defaultdict(list)
    for w, ts in by.items():
        r = residents.get(w)
        if r and len(ts) >= 30:
            groups[tuple(r.get(a, "") for a in axes)].append(w)
    rng = random.Random(seed)
    right = n = pairs = 0
    for members in groups.values():
        rng.shuffle(members)
        for a, b in zip(members[0::2], members[1::2]):
            pairs += 1
            halves = {}
            for w in (a, b):
                ts = by[w][:]
                rng.shuffle(ts)
                halves[w] = (ts[: len(ts) // 2], ts[len(ts) // 2:])
            model = {w: collections.Counter(t for x in halves[w][0] for t in words(x)) for w in (a, b)}
            vocab = len(set(model[a]) | set(model[b])) or 1
            for w in (a, b):
                for text in halves[w][1]:
                    toks = words(text)
                    score = {v: sum(math.log((model[v][t] + 1) / (sum(model[v].values()) + vocab)) for t in toks) for v in (a, b)}
                    guess = max(score, key=score.get) if score[a] != score[b] else rng.choice([a, b])
                    right += guess == w
                    n += 1
            if pairs >= 200:
                break
    return pct(right, n), {"pairs": pairs, "lines": n}


# ------------------------------------------------------------------ Over runs, the control, gates

def combine(per_run: dict[str, dict]) -> dict:
    """A scorecard over the runs: each metric's value over all of them (the worst listener for R1/R2, a mean weighted
    by lines for rates, sums for counts)."""
    out = {}
    weights = {k: v["_counts"]["lines"] for k, v in per_run.items()}
    total = sum(weights.values()) or 1
    heard = {k for k, v in per_run.items() if v["_counts"]["conversations"] > 0}   # (A run that heard nothing says nothing.)
    for mid in METRICS:
        vals = {k: v[mid] for k, v in per_run.items() if isinstance(v.get(mid), (int, float)) and k in heard}
        if not vals:
            continue
        if mid in ("R1", "R2"):
            out[mid] = min(vals.values())
        elif mid in ("K3", "D2"):
            out[mid] = sum(vals.values())
        elif mid == "B5":
            out[mid] = min(vals.values())
        elif mid in ("B1", "B1P", "B1L"):                # (Timings: the slowest profile's.)
            out[mid] = max(vals.values())
        else:                                       # (Over the profiles that say something of it: D3 isn't judged
            judged = sum(weights[k] for k in vals) or total   # where one dialect was heard, so it weighs nothing there.)
            out[mid] = sum(vals[k] * weights[k] for k in vals) / judged
    return out


def passes(mid: str, value) -> bool | None:
    test = METRICS[mid][3]
    if test is None or value is None:
        return None
    return bool(test(value))


def worse(mid: str, now: float, base: float) -> float:
    """How much worse `now` is than `base`, as a share of base (0 if not worse)."""
    better = METRICS[mid][1]
    if better == "higher":
        d = base - now
    else:
        d = now - base
    if d <= 1e-9:
        return 0.0
    return d / abs(base) if abs(base) > 1e-9 else float("inf")


def separation(real: float, control: float) -> float | None:
    if real is None or control is None:
        return None
    scale = max(abs(real), abs(control))
    return abs(real - control) / scale if scale > 1e-9 else 0.0


def real_better(mid: str, real: float, control: float) -> bool:
    """Whether the real transcripts score better than the shuffled control (§15.5: a coherence metric the shuffle beats
    measures nothing, or the opposite of what it says, however far apart the two are)."""
    return real > control if METRICS[mid][1] == "higher" else real < control


def scorecard(dirs: list[Path]) -> dict:
    per_run, per_control = {}, {}
    for d in dirs:
        run = Run(d)
        key = run.name
        if key in per_run:
            key = f"{key}:{d.name}"
        per_run[key] = score_run(run)
        if run.has_control():
            per_control[key] = score_run(Run(d, shuffled=True))
    overall = combine(per_run)
    control = combine(per_control) if per_control else {}
    metrics = {}
    for mid, (name, better, gate, test, target, phase) in METRICS.items():
        row = {"name": name, "better": better, "gate": gate, "target": target}
        if phase and mid not in overall:
            row["value"] = None
            row["note"] = f"n/a until {phase}"
        else:
            row["value"] = overall.get(mid)
            row["passes"] = passes(mid, overall.get(mid))
            row["by_profile"] = {k: round(v[mid], 3) for k, v in per_run.items() if isinstance(v.get(mid), (int, float))}
            if mid in control:
                row["control"] = control[mid]
                row["separation"] = separation(overall.get(mid), control[mid])
                row["real_better"] = real_better(mid, overall.get(mid), control[mid])
                row["separates"] = row["separation"] is not None and row["separation"] > .3 and row["real_better"]
                row["control_required"] = mid in COHERENCE
                row["structural"] = mid in STRUCTURAL
        metrics[mid] = row
    details = {k: {m: v for m, v in r.items() if m.endswith("_detail") or m == "_counts"} for k, r in per_run.items()}
    for mid, row in metrics.items():                # (Each counted metric's events and exposure, for --check's noise.)
        if mid in EVENTS and row.get("by_profile"):
            counted = events({"metrics": {mid: row}, "details": details}, mid)
            if counted:
                row["events"] = [counted[0], counted[1]]
    return {
        "about": "Docs/Design/84 §15.3: the talk scorecard (tools/talk_score.py), from talk_sim transcripts.",
        "generated": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "runs": {k: relative(d) for k, d in zip(per_run, dirs)},
        "metrics": metrics,
        "details": details,
        "control": {"runs": sorted(per_control), "how": "talk_sim --ablate shuffle: lines after the first replaced by "
                    "lines said at the same side of other conversations on the same topic, by a speaker of the same "
                    "dialect where possible (§15.5)"},
    }


# The metrics that count events (id: the profile count they are per, and per how many; None: the metric is the count
# itself), so --check can tell a change in the events from noise. A card's value is a mean of the profiles' rates
# weighted by lines, so it moves when the profiles' line counts do even with the very same events (Slice 1's end: C6 3
# against 3, C4 1 against 1, both flagged "worse").
EVENTS = {"R4": ("hours", 100), "R5": ("hours", 100), "C1": ("owed", 100), "C1Q": ("questions", 100),
          "C3": ("conversations", 100), "C4": ("lines", 1000),
          "C6": ("conversations", 1000), "T1": ("lines", 1000), "T2": ("lines", 1000), "T3": ("lines", 1000),
          "K3": ("conversations", None), "D1": ("none_lines", 100), "D1L": ("light_lines", 100),
          "D1B": ("broad_lines", 100), "R8": ("lines20", 100), "D3": ("judged", 100), "T5": ("asserted", 100),
          "C5": ("lines", 1000), "C9": ("conversations", 10), "C8": ("conversations", 100),
          "T4": ("stance_lines", 1000), "T6": ("lines", 1000), "O1": ("conversations", 10)}
# Timings, which move with the machine's load: the same build measured B5 frozen at 59, 72 and 80. And first-time waits,
# the earliest of many listeners' (R1, R2): one listener hearing a repeat a little sooner moves them more than any rate.
TIMING = {"B5": .25, "R1": .5, "R2": .5, "B1": .5, "B1P": .5, "B1L": .5}
# The chance, under no real change, that a counted metric is called worse (one-sided, per metric): with a dozen counted
# metrics on a card, about one card in ten flags one by chance at most (the exact test is conservative), where one
# standard deviation flagged two cards in three (the slice-end review).
ALPHA = .01


def events(card: dict, mid: str) -> tuple[float, float] | None:
    """A metric's events and exposure over a card's profiles: the row's own (`events`, written by scorecard()), else
    worked out from each profile's value and counts (an older card; a run that heard nothing is left out, as combine()
    leaves it out), or None if the card can't say."""
    if mid not in EVENTS:
        return None
    row = card.get("metrics", {}).get(mid, {})
    if isinstance(row.get("events"), list) and len(row["events"]) == 2 and row["events"][1] > 0:
        return float(row["events"][0]), float(row["events"][1])
    key, per = EVENTS[mid]
    n = exposure = 0.0
    for profile, value in row.get("by_profile", {}).items():
        counts = card.get("details", {}).get(profile, {}).get("_counts", {})
        if not counts.get("conversations"):
            continue
        x = float(counts.get(key) or card.get("details", {}).get(profile, {}).get(f"{mid}_detail", {}).get(key) or 0)
        if x <= 0:                                  # (An older card's C1 has its questions in C1_detail.)
            continue
        n += value if per is None else value * x / per
        exposure += x
    return (round(n), exposure) if exposure > 0 else None


def binomial_tail(k: int, n: int, p: float, upper: bool) -> float:
    """P(X >= k) (upper) or P(X <= k) for X ~ Binomial(n, p), summed exactly in logs."""
    if n <= 0:
        return 1.0
    p = min(max(p, 1e-12), 1 - 1e-12)
    terms = range(k, n + 1) if upper else range(0, k + 1)
    logs = [math.lgamma(n + 1) - math.lgamma(i + 1) - math.lgamma(n - i + 1) + i * math.log(p) + (n - i) * math.log1p(-p)
            for i in terms]
    if not logs:
        return 0.0
    top = max(logs)
    return min(1.0, math.exp(top) * sum(math.exp(x - top) for x in logs))


def beyond_noise(mid: str, card: dict, base: dict) -> tuple[bool, str]:
    """Whether a counted metric got worse beyond noise: today's events against the baseline's, each over its own
    exposure, by the exact (conditional binomial) test of two Poisson counts: given both together, today's share is
    binomial with today's share of the exposure under no change, so the baseline's own noise counts as much as today's.
    Worse only below ALPHA. (True, with no words, for a metric that counts nothing.)"""
    now, was = events(card, mid), events(base, mid)
    if now is None or was is None:
        return True, ""
    k, n0 = int(round(now[0])), int(round(was[0]))
    share = now[1] / (now[1] + was[1])
    expected = was[0] * now[1] / was[1]
    upper = METRICS[mid][1] != "higher"
    chance = binomial_tail(k, k + n0, share, upper)
    return chance < ALPHA, f" ({k} events against {expected:.0f} at the baseline's rate; by chance {chance:.2g})"


def check(card: dict, base: dict) -> list[str]:
    """A hard gate the baseline passed must still pass; an open one may not get worse; a soft metric may not regress by
    more than 10%. "Worse" is beyond noise: a counted metric by more than its events' noise and the baseline's
    (beyond_noise), a timing or a first-time wait by more than TIMING's share. Every profile the baseline scored must be
    on the card: one missing (its run failed, or never ran) is a problem, not a smaller card."""
    problems = []
    missing = sorted(set(base.get("runs", {})) - set(card.get("runs", {})))
    if missing and card.get("runs"):
        problems.append(f"profiles the baseline has and this card hasn't: {', '.join(missing)} (a run failed, or never ran)")
    for mid, row in card["metrics"].items():
        b = base.get("metrics", {}).get(mid, {})
        now, was = row.get("value"), b.get("value")
        if now is None or was is None:
            continue
        if row["gate"] == "hard" and b.get("passes") and row.get("passes") is False:
            # (A timing moves with the machine's load: past its gate it breaks only beyond TIMING's share of noise too.)
            if mid not in TIMING or worse(mid, now, was) > TIMING[mid]:
                problems.append(f"{mid} ({row['name']}): {now:.3g} breaks the hard gate ({row['target']}); the baseline passed")
            continue
        if row["gate"] == "hard" and b.get("passes") is not False:
            continue
        allowed = TIMING.get(mid, 0.0 if row["gate"] == "hard" else .10)
        if worse(mid, now, was) <= allowed:
            continue
        noisy, said = beyond_noise(mid, card, base)
        if not noisy:
            continue
        if row["gate"] == "hard":
            problems.append(f"{mid} ({row['name']}): {now:.3g}, worse than the baseline's {was:.3g} on an open hard gate{said}")
        else:
            problems.append(f"{mid} ({row['name']}): {now:.3g} regresses more than 10% from the baseline's {was:.3g}{said}")
    return problems


def refreeze(base: dict, card: dict, ids: list[str], why: str) -> dict:
    """The baseline with these metrics' rows (and their profiles' values) taken from the card, the rest kept: a gate a
    phase closed is locked where it closed it (T1 at 0), and a metric whose definition changed is measured again (P1's T2).
    Said why in the baseline itself."""
    out = json.loads(json.dumps(base))
    for mid in ids:
        if mid not in card.get("metrics", {}):
            raise SystemExit(f"talk_score: {mid} isn't on the card")
        out.setdefault("metrics", {})[mid] = card["metrics"][mid]
        for profile, d in card.get("details", {}).items():
            for k, v in d.items():
                if k == f"{mid}_detail":
                    out.setdefault("details", {}).setdefault(profile, {})[k] = v
    out.setdefault("refrozen", []).append({"metrics": ids, "why": why, "from": card.get("runs", {}),
                                           "on": card.get("generated", "")})
    return out


def fmt(v) -> str:
    if v is None:
        return "n/a"
    if isinstance(v, bool):
        return "yes" if v else "no"
    if isinstance(v, float):
        return f"{v:,.3g}" if abs(v) < 1000 else f"{v:,.0f}"
    return str(v)


def markdown(card: dict) -> str:
    out = ["# Talk scorecard", "", f"Generated {card['generated']} by `tools/talk_score.py` (Docs/Design/84 §15.3).", ""]
    out.append("Runs: " + ", ".join(f"`{k}`" for k in card["runs"]) + ".")
    if card["control"]["runs"]:
        out.append(f"Shuffled control ({card['control']['how']}): {', '.join(card['control']['runs'])}.")
    out += ["", "| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |", "|---|---|---|---|---|---|---|---|"]
    for mid, row in card["metrics"].items():
        if row.get("value") is None:
            out.append(f"| {mid} | {row['name']} | n/a | | | {row['target']} | {row['gate']} | {row.get('note', '')} |")
            continue
        sep = row.get("separation")
        sep_s = "" if sep is None else f"{sep:.0%}" + ("" if row.get("separates") else
                                                       " (the control scores better)" if row.get("real_better") is False
                                                       else " (within 30%)") + \
            (" (structural)" if row.get("structural") else " (coherence)" if row.get("control_required") else "")
        out.append(f"| {mid} | {row['name']} | {fmt(row['value'])} | {fmt(row.get('control'))} | {sep_s} | "
                   f"{row['target']} | {row['gate']} | {fmt(row.get('passes'))} |")
    out += ["", "## By profile", "", "| Id | " + " | ".join(card["runs"]) + " |", "|---|" + "---|" * len(card["runs"])]
    for mid, row in card["metrics"].items():
        if row.get("by_profile"):
            out.append(f"| {mid} | " + " | ".join(fmt(row["by_profile"].get(k)) for k in card["runs"]) + " |")
    out += ["", "## Details", ""]
    for k, d in card["details"].items():
        out.append(f"- **{k}**: " + json.dumps(d, ensure_ascii=False)[:1500])
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("runs", nargs="+", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--md", type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--bless", type=Path)
    parser.add_argument("--refreeze", default="")
    parser.add_argument("--why", default="")
    args = parser.parse_args()
    if args.refreeze and not (args.baseline and args.why):
        parser.error("--refreeze needs --baseline and --why")
    card = scorecard(args.runs)
    if args.json:
        args.json.write_text(json.dumps(card, indent=1, ensure_ascii=False) + "\n")
    if args.bless:
        args.bless.write_text(json.dumps(card, indent=1, ensure_ascii=False) + "\n")
    if args.refreeze:
        ids = [m for m in args.refreeze.split(",") if m]
        args.baseline.write_text(json.dumps(refreeze(json.loads(args.baseline.read_text()), card, ids, args.why), indent=1,
                                            ensure_ascii=False) + "\n")
    text = markdown(card)
    if args.md:
        args.md.write_text(text)
    if not (args.json or args.md or args.bless or args.check or args.refreeze):
        print(text)
    open_gates = [f"{m} {r['name']} ({fmt(r['value'])}; {r['target']})" for m, r in card["metrics"].items()
                  if r["gate"] == "hard" and r.get("passes") is False]
    unseparated = [m for m, r in card["metrics"].items() if r.get("control_required") and not r.get("separates")]
    print(f"talk_score: {len(card['runs'])} run(s); open hard gates: {len(open_gates)}"
          + (f"; coherence metrics not separating from the control (within 30%, or the control better): "
             f"{', '.join(unseparated)}" if unseparated else ""),
          file=sys.stderr)
    if args.check:
        if not args.baseline:
            print("talk_score: --check needs --baseline", file=sys.stderr)
            return 2
        problems = check(card, json.loads(args.baseline.read_text()))
        for p in problems:
            print("talk_score: " + p, file=sys.stderr)
        return 1 if problems else 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
