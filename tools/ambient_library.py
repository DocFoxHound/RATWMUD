#!/usr/bin/env python3
"""Writes overheard exchanges for the ambient director's library (Docs/Design/28-ai-cost.md, step 4).

The small model (the config's light_model) writes exchanges with blanks, once; the game fills them in for free
whenever two residents talk. Each is checked before it is kept: two to four lines, both speaking, the blanks only from
the list ({teller} {listener} {subject} {claim} {news} {day}, and the ones its kind needs), no names or places of its
own, nothing already in the library. New ones are added to Data/Voice/library.json (or --out) beside the hand-written
ones; review them like any data before committing.

  python3 tools/ambient_library.py --per 20                 every kind and band, 20 each (paid: one call a combination)
  python3 tools/ambient_library.py --kinds gossip --per 5   just these
  python3 tools/ambient_library.py --fixture --out FILE     offline, for tests
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import npc_bridge as bridge
import npc_mind as mind

ROOT = Path(__file__).resolve().parent.parent
LIBRARY = ROOT / "Data" / "Voice" / "library.json"
CONFIG = ROOT / "Saved" / "Config" / "RATWNPCAI.local.json"
BLANKS = {"teller", "listener", "subject", "claim", "news", "day"}
NEEDS = {"gossip": {"subject", "claim"}, "news": {"news"}, "quarrel": set(), "friends": set(), "day": {"day"}}
BANDS = {"gossip": ("friends", "acquaintances", "rivals"), "news": ("friends", "acquaintances", "rivals"),
         "quarrel": ("rivals",), "friends": ("friends",), "day": ("friends", "acquaintances")}
TONES = ("plain", "warm", "gruff", "formal", "sly", "shy")
RULES = """Write overheard exchanges between two NPCs of Runs Against the World, a text-first roleplaying world of
quadrupedal wolves, for a library the game fills in later. Each exchange is two to four short spoken lines, alternating,
the first by "a". Use these blanks for every name and fact, exactly as written, and nothing else in braces:
{teller} (a's name), {listener} (b's name), {subject} (who a rumour is about), {claim} (what is said of them, a phrase
like "stole from the baker"), {news} (a short sentence of news, as a would tell it), {day} (a short sentence about the
day or the weather). Never invent names, places, events or numbers of your own. No narration, actions, hands or
standing upright. Vary the wording; each exchange must read naturally whatever fills the blanks."""


def schema() -> dict:
    line = {"type": "object", "additionalProperties": False, "required": ["speaker", "text"],
            "properties": {"speaker": {"type": "string", "enum": ["a", "b"]}, "text": {"type": "string"}}}
    one = {"type": "object", "additionalProperties": False, "required": ["lines"],
           "properties": {"lines": {"type": "array", "items": line}}}
    return {"type": "object", "additionalProperties": False, "required": ["exchanges"],
            "properties": {"exchanges": {"type": "array", "items": one}}}


def check(kind: str, lines: object) -> list | None:
    """The exchange as the library keeps it ([["a", text], ...]), or None if it breaks a rule."""
    if not isinstance(lines, list) or not 2 <= len(lines) <= 4:
        return None
    out, speakers, used = [], set(), set()
    for line in lines:
        if not isinstance(line, dict) or line.get("speaker") not in ("a", "b") or not isinstance(line.get("text"), str):
            return None
        text = " ".join(line["text"].split())
        if not text or len(text) > 240 or any(ord(c) < 32 for c in text):
            return None
        blanks = set(re.findall(r"\{([^{}]*)\}", text))
        if not blanks <= BLANKS or text.count("{") != text.count("}") or text.count("{") != len(re.findall(r"\{[^{}]*\}", text)):
            return None
        # A capitalised word that isn't a sentence's first, "I", or inside a blank is a name it made up.
        bare = re.sub(r"\{[^{}]*\}", "x", text)
        for match in re.finditer(r"(?<![.!?]\s)(?<!^)\b([A-Z][a-z']+)", bare):
            if match.group(1) not in ("I", "I'm", "I'll", "I've", "I'd"):
                return None
        used |= blanks
        speakers.add(line["speaker"])
        out.append([line["speaker"], text])
    if speakers != {"a", "b"} or out[0][0] != "a" or not NEEDS[kind] <= used:
        return None
    return out


def write(provider, model: str | None, kinds: list[str], per: int, existing: list[dict]) -> list[dict]:
    known = {json.dumps(e.get("lines")) for e in existing}
    made = []
    for kind in kinds:
        for band in BANDS[kind]:
            for tone in TONES:
                ask = {"kind": kind, "band": band, "tone": tone, "count": per,
                       "about": {"gossip": "a passes on a rumour about {subject}", "news": "a tells b some news of their own",
                                 "quarrel": "two who dislike each other trade sharp words",
                                 "friends": "two friends passing the time", "day": "two who know each other remark on the day"}[kind],
                       "band_means": {"friends": "close and warm", "acquaintances": "polite, not close",
                                      "rivals": "cold or hostile"}[band]}
                kw = {"model": model} if model else {}
                content, _ = provider.complete(RULES, json.dumps(ask), "npc_library", schema(), 2500, 60, **kw)
                for x in (content.get("exchanges") or [])[:per]:
                    lines = check(kind, x.get("lines") if isinstance(x, dict) else None)
                    if lines and json.dumps(lines) not in known:
                        known.add(json.dumps(lines))
                        made.append({"kind": kind, "band": band, "tone": tone, "lines": lines, "by": "small model"})
    return made


class Fixture:
    """Offline: two simple exchanges a call, one breaking the rules (a name of its own), to be refused."""

    def complete(self, system, user, name, schema_, max_tokens, timeout, model=None):
        ask = json.loads(user)
        need = {"gossip": "They say {subject} {claim}.", "news": "Did you hear? {news}", "day": "{day}"}.get(ask["kind"], "Well, {listener}.")
        return {"exchanges": [
            {"lines": [{"speaker": "a", "text": f"{need} ({ask['tone']}, {ask['band']})"}, {"speaker": "b", "text": "Is that so."}]},
            {"lines": [{"speaker": "a", "text": "Did you see Bertram today?"}, {"speaker": "b", "text": "No."}]}]}, {}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--per", type=int, default=20, help="exchanges asked for, per kind, band and tone")
    parser.add_argument("--kinds", default=",".join(NEEDS), help="comma-separated: " + ", ".join(NEEDS))
    parser.add_argument("--config", type=Path, default=CONFIG)
    parser.add_argument("--fixture", action="store_true", help="offline replies, no cost")
    parser.add_argument("--out", type=Path, default=LIBRARY)
    args = parser.parse_args()
    kinds = [k for k in args.kinds.split(",") if k]
    if any(k not in NEEDS for k in kinds) or not 1 <= args.per <= 50:
        parser.error("unknown kind, or --per outside 1..50")
    if args.fixture:
        provider, model = Fixture(), None
    else:
        config = bridge.load_config(args.config)
        provider, model = mind.OpenAIProvider(config), config.light
    library = json.loads(args.out.read_text()) if args.out.is_file() else {"exchanges": []}
    made = write(provider, model, kinds, args.per, library.get("exchanges", []))
    library.setdefault("exchanges", []).extend(made)
    args.out.write_text(json.dumps(library, indent=2, ensure_ascii=False) + "\n")
    print(f"{len(made)} new exchanges kept (of up to {args.per * sum(len(BANDS[k]) for k in kinds) * len(TONES)} asked "
          f"for); {len(library['exchanges'])} in {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
