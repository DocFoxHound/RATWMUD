"""Central's word filter (Docs/Design/77-central-accounts-and-hosted-worlds.md, 7: "a word filter at registration"):
usernames at registration, wolf names at creation (beside ratw_identity's own rules), and world names, descriptions,
regions and tags in the heartbeat and on the Host page.

The words are in Central/blocked_words.txt, one a line ('#' starts a comment). A word is refused only as a whole word
(or with a plural: s, or es after s, x, z, ch or sh), so Scunthorpe, Essex, Penistone and spices pass. A leading '*'
refuses it inside a longer word too, for the few words no ordinary word contains; a leading '!' names an ordinary word
that holds a '*' word (snigger), which is let through.

Text is put in one form before it is looked at (normalise): accents and every invisible or decorative mark taken out
(combining and enclosing marks, format characters such as zero-width spaces, joiners, soft hyphens and variation
selectors), Cyrillic and Greek letters that look like Latin ones read as those (a small TR39-style table), and lower
case. The look-alikes are then read as every letter they could stand for: 0 o, 1 i or l, | i or l, 3 e, 4 a, 5 s, 7 t,
@ a, $ s. A letter repeated (shiiit) matches the letter once, though an entry's own double letters stay needed.

A word is looked for in three forms of the text: as written (with the look-alikes), with every digit as a break between
words (bitch1), and with the digits at each word's ends taken off (shit0). '*' words are looked for in each
space-separated piece with everything but letters and look-alikes taken out, so "f.u.c.k" is caught too.

This is a first line only: the report button and delisting by hand are the rest (doc 77, 7).
"""
from __future__ import annotations

from pathlib import Path
import re
import threading
import unicodedata

WORDS = Path(__file__).resolve().parent / 'blocked_words.txt'

# What each look-alike may stand for (and each letter, what may stand for it).
LOOK_ALIKES = {'0': 'o', '1': 'il', '|': 'il', '3': 'e', '4': 'a', '5': 's', '7': 't', '@': 'a', '$': 's'}
_STANDS_FOR: dict[str, str] = {}
for _sign, _letters in LOOK_ALIKES.items():
    for _letter in _letters:
        _STANDS_FOR[_letter] = _STANDS_FOR.get(_letter, '') + _sign
WORD_SIGNS = 'a-z' + re.escape(''.join(LOOK_ALIKES))      # (What a word is made of, as a character class's inside.)
_WORD = re.compile(f'[{WORD_SIGNS}]+')
_NOT_WORD = re.compile(f'[^{WORD_SIGNS}]+')
_EDGE_DIGITS = re.compile(f'(?<![{WORD_SIGNS}])[0-9]+|[0-9]+(?![{WORD_SIGNS}])')
_LETTERS = re.compile(r'[a-z]+')

# Cyrillic and Greek letters drawn like Latin ones (after Unicode TR39's confusables; the common ones only), both cases,
# and a few Latin letters NFKD leaves alone.
CONFUSABLES = str.maketrans({
    # Cyrillic capitals and small letters.
    'А': 'a', 'В': 'b', 'Е': 'e', 'Ѐ': 'e', 'Ё': 'e', 'К': 'k', 'М': 'm', 'Н': 'h', 'О': 'o', 'Р': 'p', 'С': 'c',
    'Т': 't', 'У': 'y', 'Х': 'x', 'І': 'i', 'Ї': 'i', 'Ј': 'j', 'Ѕ': 's', 'Ԁ': 'd', 'Ԛ': 'q', 'Ԝ': 'w', 'Ү': 'y',
    'Һ': 'h', 'Ӏ': 'l',
    'а': 'a', 'в': 'b', 'е': 'e', 'ѐ': 'e', 'ё': 'e', 'к': 'k', 'м': 'm', 'н': 'h', 'о': 'o', 'р': 'p', 'с': 'c',
    'т': 't', 'у': 'y', 'х': 'x', 'і': 'i', 'ї': 'i', 'ј': 'j', 'ѕ': 's', 'ԁ': 'd', 'ԛ': 'q', 'ԝ': 'w', 'ү': 'y',
    'һ': 'h', 'ӏ': 'l', 'ɡ': 'g',
    # Greek capitals and small letters.
    'Α': 'a', 'Β': 'b', 'Ε': 'e', 'Ζ': 'z', 'Η': 'h', 'Ι': 'i', 'Κ': 'k', 'Μ': 'm', 'Ν': 'n', 'Ο': 'o', 'Ρ': 'p',
    'Τ': 't', 'Υ': 'y', 'Χ': 'x', 'Ϲ': 'c', 'Ϳ': 'j',
    'α': 'a', 'β': 'b', 'ε': 'e', 'ι': 'i', 'κ': 'k', 'ν': 'v', 'ο': 'o', 'ρ': 'p', 'τ': 't', 'υ': 'u', 'χ': 'x',
    'ϲ': 'c', 'ϳ': 'j',
    # Latin letters with strokes, and dotless i.
    'ı': 'i', 'ł': 'l', 'Ł': 'l', 'ø': 'o', 'Ø': 'o', 'đ': 'd', 'Đ': 'd', 'ħ': 'h', 'Ħ': 'h', 'ŧ': 't', 'Ŧ': 't',
    'ƒ': 'f', 'ɑ': 'a', 'ɩ': 'i',
})

_lock = threading.Lock()
_loaded: tuple | None = None


def invisible(c: str) -> bool:
    """A character that draws nothing of its own or only decorates the one before it: combining and enclosing marks,
    format characters (zero-width spaces and joiners, soft hyphens, direction marks) and variation selectors."""
    return unicodedata.category(c) in ('Mn', 'Me', 'Cf')


def normalise(text: str) -> str:
    """Accents and invisible marks out, Cyrillic and Greek look-alikes as Latin letters, lower case. The look-alike
    digits and signs stay (see look_alike)."""
    text = unicodedata.normalize('NFKD', text).translate(CONFUSABLES)
    text = unicodedata.normalize('NFKD', text.casefold())
    return ''.join(c for c in text if not invisible(c)).translate(CONFUSABLES)


def forms(text: str) -> list[str]:
    """The text's three forms that words are looked for in: as written, with digits as breaks between words, and with
    the digits at each word's ends taken off."""
    plain = normalise(text)
    return [plain, re.sub(r'[^a-z]+', ' ', plain), _EDGE_DIGITS.sub(' ', plain)]


def look_alike(word: str) -> str:
    """A regular expression for a plain word: each letter also as its look-alikes, and repeated as often as wanted."""
    out = []
    for letter in word:
        signs = _STANDS_FOR.get(letter, '')
        out.append(f'[{letter}{re.escape(signs)}]+' if signs else f'{re.escape(letter)}+')
    return ''.join(out)


def _plural(word: str) -> str:
    """A word's pattern with its plural: s; es only after s, x, z, ch or sh (so spic doesn't take spices)."""
    s = look_alike('s')
    if word.endswith(('s', 'x', 'z', 'ch', 'sh')):
        return f'{look_alike(word)}(?:{look_alike("e")}{s})?'
    return f'{look_alike(word)}(?:{s})?'


def _never(text: str) -> bool:
    return False


def _words(path: Path = WORDS) -> tuple:
    """(whole-word matcher, '*' searcher, '!' remover, whole words, '*' words), read again when the file changes."""
    global _loaded
    try:
        mtime = path.stat().st_mtime
    except OSError:
        mtime = -1.0
    with _lock:
        if _loaded is not None and _loaded[0] == (path, mtime):
            return _loaded[1]
        whole, inside, allowed = set(), set(), set()
        if mtime >= 0:
            for line in path.read_text(encoding='utf-8').splitlines():
                entry = line.split('#', 1)[0].strip()
                if not entry:
                    continue
                kind = entry[0] if entry[0] in '*!' else ''
                letters = ''.join(_LETTERS.findall(normalise(entry.lstrip('*!'))))
                if letters:
                    {'*': inside, '!': allowed, '': whole}[kind].add(letters)
        whole_match = re.compile('|'.join(_plural(w) for w in sorted(whole))).fullmatch if whole else _never
        inside_search = re.compile('|'.join(look_alike(w) for w in sorted(inside))).search if inside else _never
        allowed_sub = re.compile('|'.join(look_alike(w) for w in sorted(allowed))).sub if allowed else \
            (lambda _, text: text)
        found = (whole_match, inside_search, allowed_sub, frozenset(whole), tuple(sorted(inside)))
        _loaded = ((path, mtime), found)
        return found


def refused(text: str, path: Path = WORDS) -> bool:
    """True if the text holds a blocked word."""
    if not isinstance(text, str) or not text:
        return False
    whole_match, inside_search, allowed_sub, _, _ = _words(path)
    every = forms(text)
    for form in every:
        if any(whole_match(word) for word in _WORD.findall(form)):
            return True
    for chunk in every[0].split():
        squashed = allowed_sub('', _NOT_WORD.sub('', chunk))
        if inside_search(squashed):
            return True
    return False


def holds(text: str, words: tuple[str, ...]) -> bool:
    """True if any of these plain words is in the text read every way above, spaces and stops between its letters
    included ("Of.fi cial", "0ff1c1al")."""
    if not isinstance(text, str) or not text:
        return False
    search = re.compile('|'.join(look_alike(w) for w in words)).search
    return any(search(_NOT_WORD.sub('', form)) for form in forms(text))
