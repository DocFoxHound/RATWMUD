"""Central's word filter (Docs/Design/77-central-accounts-and-hosted-worlds.md, 7: "a word filter at registration"):
usernames at registration, wolf names at creation (beside ratw_identity's own rules), and world names, descriptions,
regions and tags in the heartbeat and on the Host page.

The words are in Central/blocked_words.txt, one a line ('#' starts a comment). A word is refused only as a whole word
(or with a plural s or es), so Scunthorpe, Essex and Penistone pass. A leading '*' refuses it inside a longer word
too, for the few words no ordinary word contains.

Text is put in one form before it is looked at: lower case, accents stripped, and the common look-alikes read as the
letters they stand for (0 o, 1 i, 3 e, 4 a, 5 s, 7 t, @ a, $ s). A whole word is a run of letters; '*' words are
looked for in each space-separated piece with everything but letters taken out, so "f.u.c.k" is caught too.

This is a first line only: the report button and delisting by hand are the rest (doc 77, 7).
"""
from __future__ import annotations

from pathlib import Path
import re
import threading
import unicodedata

WORDS = Path(__file__).resolve().parent / 'blocked_words.txt'
LOOK_ALIKES = str.maketrans({'0': 'o', '1': 'i', '3': 'e', '4': 'a', '5': 's', '7': 't', '@': 'a', '$': 's'})
_LETTERS = re.compile(r'[a-z]+')

_lock = threading.Lock()
_loaded: tuple[float, frozenset, tuple] | None = None


def normalise(text: str) -> str:
    """Lower case, accents stripped, look-alikes read as letters."""
    text = unicodedata.normalize('NFKD', text.casefold())
    text = ''.join(c for c in text if not unicodedata.combining(c))
    return text.translate(LOOK_ALIKES)


def _words(path: Path = WORDS) -> tuple[frozenset, tuple]:
    """(whole words, '*' words), read again when the file changes."""
    global _loaded
    try:
        mtime = path.stat().st_mtime
    except OSError:
        mtime = -1.0
    with _lock:
        if _loaded is not None and _loaded[0] == mtime:
            return _loaded[1], _loaded[2]
        whole, inside = set(), set()
        if mtime >= 0:
            for line in path.read_text(encoding='utf-8').splitlines():
                entry = line.split('#', 1)[0].strip()
                if not entry:
                    continue
                anywhere = entry.startswith('*')
                letters = ''.join(_LETTERS.findall(normalise(entry.lstrip('*'))))
                if letters:
                    (inside if anywhere else whole).add(letters)
        _loaded = (mtime, frozenset(whole), tuple(sorted(inside)))
        return _loaded[1], _loaded[2]


def refused(text: str, path: Path = WORDS) -> bool:
    """True if the text holds a blocked word."""
    if not isinstance(text, str) or not text:
        return False
    whole, inside = _words(path)
    plain = normalise(text)
    for word in _LETTERS.findall(plain):
        if word in whole or (word.endswith('s') and (word[:-1] in whole or
                                                    (word.endswith('es') and word[:-2] in whole))):
            return True
    for chunk in plain.split():
        squashed = ''.join(_LETTERS.findall(chunk))
        if any(word in squashed for word in inside):
            return True
    return False
