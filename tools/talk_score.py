#!/usr/bin/env python3
"""The talk scorecard (Docs/Design/84-living-conversations.md §15.3-§15.5): what talk_sim's transcripts measure.

  python3 tools/talk_score.py RUN_DIR [RUN_DIR ...] [--json OUT.json] [--md OUT.md]
  python3 tools/talk_score.py RUN_DIR ... --baseline BASE.json --check      exit 1 if a gate breaks (below)
  python3 tools/talk_score.py RUN_DIR ... --bless BASE.json                 write the baseline

Each RUN_DIR is one talk_sim run (lines.jsonl, convs.jsonl, residents.jsonl, run.json); a run made with --ablate shuffle
also has the shuffled control (lines-shuffled.jsonl), and every metric is computed on it too (§15.5): a coherence
metric (C*) that scores the control within 30% of the real transcripts isn't measuring coherence, and is flagged.
Metrics are worked out per run (a profile), then over all the runs given.

What P0 can measure, before moves and tones exist (§15.1: "the text-only metrics still work: that is how the baseline
is measured"); the rest are listed as n/a with the phase that brings them:
  R1 first word-for-word identical conversation, per listener (hours; the earliest listener's)
  R2 first repeated opener, per listener (hours)
  R3 distinct openers in a listener's first 3,000 exchanges (fewer heard: scaled to 3,000)
  R4 a distinctive line (6+ words) heard again by the same listener within 10 h, per 100 h
  R5 collision index: the same distinctive line from two speakers in one place within an hour, per 100 h
  R6 the most-spoken line's share of all lines
  R7 distinct reply entries per 1,000 reply beats
  R8 habit saturation: the share of a speaker's lines using their oath! or saying! (speakers with 20+ lines: mean, p90)
  C1 questions answered: a line ending "?" followed by the other's line (the heuristic until moves owe answers)
  C3 endings: by a farewell or a break / at the beat cap (8 lines) / by nothing fitting
  C4 tone clashes per 1,000 beats: a grave line (death, grief, robbery, a beating...) followed within two beats by a light
     one (laughter, a jest) from the other, with no bridge: a lexicon until tones exist (Phase 2)
  C6 the same entry (or the same question) twice in one conversation, per 1,000 conversations
  C7 lines per conversation: mean, 90th percentile, and the share of 3-line ones
  C8 conversations with a third speaker
  T1 raw ids spoken (an identifier with an underscore), per 1,000 lines
  T2 presupposition failures per 1,000 lines (a provisional lexicon until truth.json, Phase 1): weather words against
     the place's weather or indoors, time-of-day words against the hour, kin against the speaker's family, fishing
     against the speaker's trade
  T3 self-reference: a speaker saying their own full name, per 1,000 lines
  K1 conversations on everyday topics (anything but news, gossip, crime, deeds, events, quarrels and the like)
  K2 the largest topic's share
  K3 the same news (topic, subject, claim) told twice to the same resident
  D1 lines carrying a dialect's words (a dialect grammar layer in the expansion), by the speaker's accent
  D2 another dialect's words in a speaker's line (by the expansion's layers, and by tools/dialect_check.py's lexicon)
  D3 a speaker's dialect guessed from 20 of their lines (naive Bayes on words, trained on the other speakers), light and
     broad speakers
  U1 a resident's lines from their own words (character tables now, mantles from Phase 9)
  U2 distinguishability: of two residents with the same eight axes (seven traits and dialect), held-out lines given to
     the right one
  B5 exchanges simulated a second (run.json)

Gates (--check against --baseline): a hard gate the baseline passed must still pass; one the baseline failed may not get
worse (each phase closes its own); a soft metric may not regress by more than 10%. The scorecard says which hard gates
are still open.
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
    "C1": ("questions answered (%)", "higher", "hard", lambda v: v >= 92, "97%; hard 92%", ""),
    "C2": ("replies at the best fit available (%)", "higher", "soft", None, "95%", "Phase 3"),
    "C3": ("endings by a farewell or a break (%)", "higher", "soft", lambda v: v >= 80, "80% / 3% / 5%", ""),
    "C4": ("tone clashes per 1,000 beats", "lower", "hard", lambda v: v <= 0, "0", ""),
    "C5": ("unmarked topic jumps per 1,000 beats", "lower", "soft", None, "5", "Phase 5"),
    "C6": ("same entry or question twice in a conversation, per 1,000", "lower", "hard", lambda v: v <= 0, "0", ""),
    "C7": ("lines per conversation (mean)", "higher", "soft", lambda v: v >= 5, "5-7; p90 9+", ""),
    "C8": ("conversations with a third speaker (%)", "higher", "soft", lambda v: 5 <= v <= 15, "5-15%", ""),
    "C9": ("callbacks per 10 conversations", "higher", "soft", None, "1.5+", "Phase 5"),
    "T1": ("raw ids spoken, per 1,000 lines", "lower", "hard", lambda v: v <= 0, "0", ""),
    "T2": ("presupposition failures, per 1,000 lines", "lower", "hard", lambda v: v <= 0, "0", ""),
    "T3": ("self-reference errors, per 1,000 lines", "lower", "hard", lambda v: v <= 0, "0", ""),
    "T4": ("identity drift", "lower", "hard", None, "0", "Phase 6"),
    "T5": ("telling what the listener already believes (%)", "lower", "soft", None, "2%", "Phase 4"),
    "D1": ("lines with a dialect's words (%): light speakers", "higher", "hard", lambda v: 25 <= v <= 50, "none 0%; light 25-50%; broad 60%+", ""),
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
         "harvest", "newcomer", "storyteller", "festival", "death"}
GRAVE = re.compile(r"\b(died|dead|death|dying|buried|burial|grave|funeral|mourn\w*|grie\w+|murder\w*|killed|"
                   r"robbed|beaten|beating|stabbed|hanged|drowned|widow\w*|orphan\w*|sick unto|plague)\b", re.I)
LIGHT = re.compile(r"\b(ha|hah|haha|heh|hee|laugh\w*|jest\w*|joke\w*|tease\w*|giggl\w*|grin\w*|wink\w*)\b", re.I)
BRIDGE = re.compile(r"\b(anyway|well then|enough of that|still|mind you|on a brighter note|but listen)\b", re.I)
FAREWELL = re.compile(r"\b(farewell|goodbye|good night|goodnight|go well|go in peace|fair winds|safe road|take care|"
                      r"see you|be seeing|i'?d best|i must be off|i'?ll be off|must go|be off|off with you|"
                      r"till (?:tomorrow|next)|until (?:tomorrow|next)|calm water|dry feet|walk warm|rest well|"
                      r"mind how you go|bright one keep you|gods keep|keep well|later,)\b", re.I)
RAW_ID = re.compile(r"\b[A-Za-z]+_[A-Za-z0-9_]+\b")
WORD = re.compile(r"[a-z']+")
# T2's provisional lexicon (truth.json, Phase 1, replaces it).
# Words that take rain as falling now (not "Rain covers footsteps", a saying, nor the Rain Hall, a place).
WET = re.compile(r"\b(this rain|the rain (?:is|won't|keeps|has)|raining|drizzl\w*|downpour|soaked|pouring|wet out|"
                 r"coming down|rained on)\b", re.I)
PLACES = re.compile(r"\bRain Hall\b")
FOG = re.compile(r"\b(fog\w*|mist\w*|murk)\b", re.I)
SNOW = re.compile(r"\b(snow\w*|blizzard|sleet)\b", re.I)
OUT_THERE = re.compile(r"\b(out there|outside|hear it coming down|look at that sky)\b", re.I)
NIGHT = re.compile(r"\b(tonight it|this night|good night|goodnight|at this hour of night|middle of the night)\b", re.I)
MORNING = re.compile(r"\b(good morning|this morning,? (?:already|so far)|morning to you|morning,)\b", re.I)
SPOUSE = re.compile(r"\bmy (wife|husband|man|woman|missus|mister|spouse)\b", re.I)
CHILD = re.compile(r"\bmy (son|daughter|pup|pups|lad|lass|little ones|young ones|children|child|boy|girl)\b", re.I)
LATE = re.compile(r"\bmy late (wife|husband|man|woman)\b", re.I)
FISH = re.compile(r"\b(fish\w*|nets?|netting|the catch|my catch|boats?|hook and line|trawl\w*)\b", re.I)
FISHERS = {"fisher", "fisherman", "fisherwoman", "boatman", "ferryman", "sailor", "dockhand", "netmaker", "fishmonger"}


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
    for who, cs in listeners.items():
        seen_conv, seen_open = {}, {}
        first_conv = first_open = None
        for i, c in enumerate(cs):
            key = "\n".join(l["text"] for l in c)
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
    # R4: heard again by the same listener within 10 h; R5: collisions in a place within an hour.
    heard: dict[tuple, float] = {}
    said_at: dict[tuple, list[tuple[float, str]]] = collections.defaultdict(list)
    r4 = r5 = 0
    for l in sorted(lines, key=lambda x: x["t"]):
        if len(words(l["text"])) < 6:
            continue
        k = (l.get("listener", ""), l["text"])
        if k in heard and l["t"] - heard[k] <= 36000:
            r4 += 1
        heard[k] = l["t"]
        place = (l.get("cell", ""), l["text"])
        if any(l["t"] - t <= 3600 and who != l["who"] for t, who in said_at[place]):
            r5 += 1
        said_at[place] = [(t, w) for t, w in said_at[place] if l["t"] - t <= 3600] + [(l["t"], l["who"])]
    m["R4"] = r4 * 100 / hours
    m["R5"] = r5 * 100 / hours
    counts = collections.Counter(l["text"] for l in lines)
    top = counts.most_common(1)
    m["R6"] = pct(top[0][1], len(lines)) if top else 0
    m["R6_detail"] = {"line": top[0][0] if top else "", "times": top[0][1] if top else 0}
    replies = [l for l in lines if l["beat"] > 0]
    m["R7"] = len({l["entry"] for l in replies}) * 1000 / len(replies) if replies else 0
    by_speaker = collections.defaultdict(list)
    for l in lines:
        by_speaker[l["who"]].append(l)
    sat = [pct(sum(1 for l in ls if any(h in ("oath", "saying") or h.startswith(("oath", "saying"))
                                        for h in l.get("habits", []))), len(ls))
           for ls in by_speaker.values() if len(ls) >= 20]
    m["R8"] = sum(sat) / len(sat) if sat else 0
    m["R8_detail"] = {"p90": quantile(sat, .9), "speakers": len(sat),
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
    m["C1"] = pct(answered, asked)
    m["C1_detail"] = {"questions": asked, "answered": answered}
    # C3: how conversations end.
    farewell = cap = nothing = broke = 0
    for cid, c in run.by_conv.items():
        head = run.conv_head.get(cid, {})
        reason = head.get("reason", "done")
        if reason not in ("done", "run_end"):
            broke += 1
        elif c and FAREWELL.search(c[-1]["text"]):
            farewell += 1
        elif int(head.get("written", len(c))) >= 8:
            cap += 1
        else:
            nothing += 1
    n = max(1, len(run.by_conv))
    m["C3"] = pct(farewell + broke, n)
    m["C3_detail"] = {"farewell": pct(farewell, n), "break": pct(broke, n), "cap": pct(cap, n), "nothing_fits": pct(nothing, n)}
    # C4: grave then light from the other within two beats, with no bridge.
    clashes = beats = 0
    for c in convs:
        beats += len(c)
        for i, l in enumerate(c):
            if not GRAVE.search(l["text"]):
                continue
            for j in (i + 1, i + 2):
                if j < len(c) and c[j]["who"] != l["who"] and LIGHT.search(c[j]["text"]) and not BRIDGE.search(c[j]["text"]):
                    clashes += 1
                    break
    m["C4"] = clashes * 1000 / max(1, beats)
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
    t2 = collections.Counter()
    t2_examples = []
    for cid, c in run.by_conv.items():
        facts = run.conv_head.get(cid, {}).get("facts", {})
        weather = facts.get("_weather", "")
        outdoors = facts.get("_outdoors", "")
        hour = float(run.conv_head.get(cid, {}).get("hour", 12))
        for l in c:
            who = run.residents.get(l["who"], {})
            text = PLACES.sub("", l["text"])
            kinds = []
            if WET.search(text) and weather not in ("rain", "storm"):
                kinds.append("weather")
            elif FOG.search(text) and weather != "fog" and re.search(r"\b(this|the) (fog|mist)\b|\bfoggy\b|\bmisty\b", text, re.I):
                kinds.append("weather")
            elif SNOW.search(text) and weather != "snow" and re.search(r"\b(this|the) snow\b|\bsnowing\b", text, re.I):
                kinds.append("weather")
            if OUT_THERE.search(text) and outdoors == "yes" and re.search(r"\bout there\b", text, re.I):
                pass
            if NIGHT.search(text) and 7 <= hour < 18:
                kinds.append("time")
            if MORNING.search(text) and not (4 <= hour < 12):
                kinds.append("time")
            if SPOUSE.search(text) and who.get("married") == "no" and who.get("widowed") != "yes":
                kinds.append("kin")
            if LATE.search(text) and who.get("widowed") == "no":
                kinds.append("kin")
            if CHILD.search(text) and who.get("parent") == "no" and l.get("topic") in ("family", "home"):
                kinds.append("kin")
            if FISH.search(text) and who and not ({who.get("job", ""), who.get("trade", "")} & FISHERS) \
                    and re.search(r"\b(threw|caught|my nets?|the nets|my catch|my boat|mending nets|gutting)\b", text, re.I):
                kinds.append("trade")
            for k in kinds:
                t2[k] += 1
            if kinds and len(t2_examples) < 6:
                t2_examples.append(f"{'/'.join(kinds)}: {text} ({l['who']}, {weather or '?'} {hour:.0f}h)")
    m["T2"] = sum(t2.values()) * 1000 / max(1, len(lines))
    m["T2_detail"] = {"by_kind": dict(t2), "examples": t2_examples}
    t3 = [l for l in lines if run.residents.get(l["who"], {}).get("name") and
          run.residents[l["who"]]["name"] in l["text"]]
    m["T3"] = len(t3) * 1000 / max(1, len(lines))
    m["T3_detail"] = {"examples": [f"{l['who']}: {l['text']}" for l in t3[:4]]}
    m["T1_detail"] = {"examples": t1_examples}
    # K1, K2, K3.
    topics = collections.Counter(run.conv_head.get(cid, {}).get("topic", c[0].get("topic", "")) for cid, c in run.by_conv.items())
    total = sum(topics.values())
    m["K1"] = pct(sum(n for t, n in topics.items() if t not in NEWSY), total)
    m["K2"] = pct(topics.most_common(1)[0][1], total) if topics else 0
    m["K2_detail"] = {"topics": {t: round(pct(n, total), 2) for t, n in topics.most_common()}}
    told = collections.Counter()
    for cid, c in run.by_conv.items():
        head = run.conv_head.get(cid, {})
        if head.get("topic") in NEWSY and (head.get("subject") or head.get("claim")):
            told[(head.get("other", ""), head.get("topic"), head.get("subject", ""), head.get("claim", ""))] += 1
    m["K3"] = sum(n - 1 for n in told.values() if n > 1)
    # D1, D2.
    by_accent = collections.defaultdict(lambda: [0, 0])
    d2_layers = 0
    d2_text = 0
    d2_examples = []
    mod, lex = DIALECT
    for l in lines:
        own = l.get("dialect", "")
        used = [u for u in l.get("used", []) if u != "plain" and not u.startswith("p_")]
        acc = l.get("accent") or "none"
        by_accent[acc][1] += 1
        if used:
            by_accent[acc][0] += 1
        if any(u != own for u in used):
            d2_layers += 1
        if lex is not None:
            others = {d for d, _ in mod.hits(l["text"], lex) if d != own and mod_dialect_of(own) != d}
            if others:
                d2_text += 1
                if len(d2_examples) < 4:
                    d2_examples.append(f"{own}: {l['text']} ({', '.join(sorted(others))})")
    m["D1"] = pct(*by_accent["light"]) if by_accent["light"][1] else 0
    m["D1_detail"] = {a: round(pct(*v), 2) for a, v in by_accent.items()}
    m["D2"] = d2_layers + d2_text
    m["D2_detail"] = {"by_layers": d2_layers, "by_lexicon": d2_text, "examples": d2_examples,
                      "lexicon": "tools/dialect_check.py" if lex is not None else "unavailable"}
    m["D3"], m["D3_detail"] = guess_dialects(lines)
    # U1, U2.
    own = sum(1 for l in lines if "." in l.get("entry", "") and l["entry"].split(".", 1)[0] == l["who"])
    m["U1"] = pct(own, len(lines))
    m["U2"], m["U2_detail"] = distinguish(lines, run.residents)
    m["B5"] = float(run.meta.get("exchanges_per_second") or 0)
    # X1: a reply (a line after the first, by the other) sharing a content word with the line before it.
    cohesive = replies_n = 0
    for c in convs:
        for i in range(1, len(c)):
            if c[i]["who"] == c[i - 1]["who"]:
                continue
            a = {w for w in words(c[i - 1]["text"]) if w not in STOP and len(w) > 2}
            b = {w for w in words(c[i]["text"]) if w not in STOP and len(w) > 2}
            replies_n += 1
            cohesive += bool(a & b)
    m["X1"] = pct(cohesive, replies_n)
    m["_counts"] = {"lines": len(lines), "conversations": len(convs), "hours": round(run.hours, 2),
                    "listeners": len(listeners), "speakers": len(by_speaker)}
    return m


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
        return 0.0, {"speakers": len(speakers), "note": "fewer than two dialects heard"}
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
    return pct(right, n), {a: round(pct(*v), 1) for a, v in by_accent.items()} | {"speakers": len(speakers)}


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
        else:
            out[mid] = sum(vals[k] * weights[k] for k in vals) / total
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
                row["separates"] = row["separation"] is not None and row["separation"] > .3
                row["control_required"] = mid in COHERENCE
                row["structural"] = mid in STRUCTURAL
        metrics[mid] = row
    details = {k: {m: v for m, v in r.items() if m.endswith("_detail") or m == "_counts"} for k, r in per_run.items()}
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


def check(card: dict, base: dict) -> list[str]:
    problems = []
    for mid, row in card["metrics"].items():
        b = base.get("metrics", {}).get(mid, {})
        now, was = row.get("value"), b.get("value")
        if now is None or was is None:
            continue
        if row["gate"] == "hard":
            if b.get("passes") and row.get("passes") is False:
                problems.append(f"{mid} ({row['name']}): {now:.3g} breaks the hard gate ({row['target']}); the baseline passed")
            elif b.get("passes") is False and worse(mid, now, was) > 0:
                problems.append(f"{mid} ({row['name']}): {now:.3g}, worse than the baseline's {was:.3g} on an open hard gate")
        elif worse(mid, now, was) > .10:
            problems.append(f"{mid} ({row['name']}): {now:.3g} regresses more than 10% from the baseline's {was:.3g}")
    return problems


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
        sep_s = "" if sep is None else f"{sep:.0%}" + ("" if row.get("separates") else " (within 30%)") + \
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
    args = parser.parse_args()
    card = scorecard(args.runs)
    if args.json:
        args.json.write_text(json.dumps(card, indent=1, ensure_ascii=False) + "\n")
    if args.bless:
        args.bless.write_text(json.dumps(card, indent=1, ensure_ascii=False) + "\n")
    text = markdown(card)
    if args.md:
        args.md.write_text(text)
    if not (args.json or args.md or args.bless or args.check):
        print(text)
    open_gates = [f"{m} {r['name']} ({fmt(r['value'])}; {r['target']})" for m, r in card["metrics"].items()
                  if r["gate"] == "hard" and r.get("passes") is False]
    unseparated = [m for m, r in card["metrics"].items() if r.get("control_required") and not r.get("separates")]
    print(f"talk_score: {len(card['runs'])} run(s); open hard gates: {len(open_gates)}"
          + (f"; coherence metrics within 30% of the control: {', '.join(unseparated)}" if unseparated else ""),
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
