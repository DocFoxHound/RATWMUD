"""What a line presupposes (Docs/Design/84 §7.8): Data/Talk/truth.json read for the tools. tools/talk_lint.py checks it
statically (an entry's criteria must imply what its words need); tools/talk_score.py on every simulated line (T2: the
moment's facts must hold). Only reads."""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

TALK = Path(__file__).resolve().parent.parent / 'Data' / 'Talk'
NEED = re.compile(r'^([a-z_]+\.[a-z_.]+)(!=|<=|>=|=|<|>)(.+)$')


@dataclass
class Need:
    key: str                     # "self.job", "here.weather", "pair.kin", "conv.tense"
    op: str                      # "=", "!=", "<", "<=", ">", ">="
    values: set = field(default_factory=set)
    number: float = 0.0

    @staticmethod
    def parse(text: str) -> 'Need':
        m = NEED.match(text.strip())
        if not m:
            raise ValueError(f'"{text}" is no need (ns.fact=value, ns.fact>=number)')
        key, op, rest = m.groups()
        n = Need(key, op)
        if op in ('=', '!='):
            n.values = set(rest.split('|'))
        else:
            n.number = float(rest)
        return n

    def holds(self, facts: dict) -> bool | None:
        """Whether the facts hold it; None when the fact isn't among them (unknown)."""
        if self.key not in facts:
            return None
        v = str(facts[self.key])
        if self.op == '=':
            return v in self.values
        if self.op == '!=':
            return v not in self.values
        try:
            x = float(v)
        except ValueError:
            return False
        return {'<': x < self.number, '<=': x <= self.number, '>': x > self.number, '>=': x >= self.number}[self.op]

    def implied_by(self, key: str, op: str, values: set) -> bool:
        """Whether a criterion (key op values, as a line's `when` has it) makes this need hold."""
        if key != self.key:
            return False
        if self.op == '=' and op == '=':
            return bool(values) and values <= self.values
        if self.op == '!=' and op == '=':
            return bool(values) and not (values & self.values)
        if self.op == '!=' and op == '!=':
            return self.values <= values
        if self.op in ('<', '<=', '>', '>=') and op in ('<', '<=', '>', '>='):
            try:
                n = float(next(iter(values)))
            except (StopIteration, ValueError):
                return False
            if self.op in ('>', '>=') and op in ('>', '>='):
                return n > self.number or (n == self.number and (self.op == '>=' or op == '>'))
            if self.op in ('<', '<=') and op in ('<', '<='):
                return n < self.number or (n == self.number and (self.op == '<=' or op == '<'))
        if self.op in ('<', '<=', '>', '>=') and op == '=':
            try:
                return all(Need(self.key, self.op, number=self.number).holds({self.key: v}) for v in values)
            except ValueError:
                return False
        return False


@dataclass
class Rule:
    id: str
    match: re.Pattern
    needs: list
    needs_any: list
    example: str = ''

    def judge(self, facts: dict) -> bool | None:
        """True if the facts back the words, False if they don't, None if none of what it needs is known."""
        known = False
        for n in self.needs:
            h = n.holds(facts)
            if h is None:
                continue
            known = True
            if not h:
                return False
        if self.needs_any:
            any_known = False
            for n in self.needs_any:
                h = n.holds(facts)
                if h is None:
                    continue
                any_known = known = True
                if h:
                    return True
            if any_known:
                return False
        return True if known else None


def concepts(root: Path = TALK) -> dict:
    """What a concept asked holds of itself (truth.json's `concepts`): {concept: [Need]}."""
    doc = json.loads((root / 'truth.json').read_text())
    return {c: [Need.parse(n) for n in needs] for c, needs in doc.get('concepts', {}).items()}


def load(root: Path = TALK) -> tuple[list, re.Pattern]:
    """The rules and the id pattern; a ValueError for a rule that can't be read."""
    doc = json.loads((root / 'truth.json').read_text())
    rules = []
    for r in doc.get('rules', []):
        if 'id' not in r or 'match' not in r or not (r.get('needs') or r.get('needs_any')):
            raise ValueError(f'truth.json: a rule needs an id, a match and needs or needs_any: {r}')
        rules.append(Rule(r['id'], re.compile(r['match'], re.I), [Need.parse(n) for n in r.get('needs', [])],
                          [Need.parse(n) for n in r.get('needs_any', [])], r.get('example', '')))
    return rules, re.compile(doc.get('ids', r'(?!x)x'))


SYMBOL = re.compile(r'#[^#\s]*#')


def matches(rules: list, text: str) -> list:
    """The rules a line's words call on."""
    return [r for r in rules if r.match.search(text)]
