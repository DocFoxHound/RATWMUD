#!/usr/bin/env python3
"""How much the talk library covers (Docs/Design/62-talk.md, section 6; it replaces doc 30's scene_report.py).

Counts the rules and scenes in Data/Talk by concept, by region (one for anywhere counts for every region), how many
lines are written for each kind of speaker, how many test each fact, and each grammar's symbols against plain;
estimates how many distinct renderings the library holds; lists the thin spots: concept x region pairs with fewer
than --min.

    python3 tools/talk_report.py [--min 4] [--region ridgemere]

The game's own loader (Core/RatwTalk.cpp; `talk_check --validate`) is the judge of whether a file is valid; this only
counts.
"""
import argparse
import collections
import re

import talk_files

REGIONS = ['ridgemere', 'frosthollow', 'coldwater', 'oakhaven', 'mirebridge', 'larkfield', 'southmeadow']
EVENTS = ['prices', 'caravan', 'bandits', 'crime', 'life', 'festival', 'newcomer', 'deed', 'price_cause']
EVERYDAY = ['smalltalk', 'work', 'family', 'weather', 'lore', 'player', 'gossip', 'news', 'quarrel', 'friends', 'day',
            'food', 'home', 'pastimes', 'animals', 'health', 'memories', 'dreams', 'faith', 'travel', 'land', 'season']


def renderings(entry):
    """Distinct ways a scene can come out for one speaker pair. A turn with as many alternatives as the one before
    answers it in parallel (Core/RatwTalk.cpp), so it adds no new ways."""
    total, before = 1, 0
    turn = []
    for who, optional, tags, alts in entry['lines'] + [('end', False, '', [])]:
        if turn and (who != turn[-1][0] or not turn[-1][2]):
            count = max(len(a) for _, _, _, a in turn)
            if count != before:
                total *= count
            before = count
            turn = []
        if who != 'end':
            turn.append((who, optional, tags, alts))
    return total


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--min', type=int, default=4, help='Thin: fewer than this for a concept in a region')
    parser.add_argument('--region', help='Only this region')
    args = parser.parse_args()
    library = talk_files.read()
    entries = library['entries']
    scenes = [e for e in entries if e['scene']]
    print(f'{len(scenes)} scenes and {len(entries) - len(scenes)} rules in {len({e["file"] for e in entries})} files; '
          f'about {sum(renderings(e) for e in entries):,} distinct renderings (before facts and speaker variants).')
    filler = sum(1 for e in entries if e['priority'] < 0)
    strict = sum(1 for e in entries if e['priority'] >= 10)
    print(f'{filler} filler (priority below 0: only when nothing else holds); {strict} that must be said (priority 10+).')
    by_concept = collections.Counter(c for e in entries for c in e['concepts'])
    print('\nBy concept: ' + ', '.join(f'{c} {n}' for c, n in sorted(by_concept.items())))
    tags = collections.Counter()
    for e in entries:
        for _, _, t, _ in e['lines']:
            for word in re.split(r'[\s,]+', t):
                if word:
                    tags[word] += 1
    print('\nLines written for: ' + ', '.join(f'{w} {n}' for w, n in tags.most_common(40)))
    facts = collections.Counter()
    for e in entries:
        for token in e['when']:
            facts[re.split(r'!?=|[<>]=?', token)[0]] += 1
        for _, _, t, _ in e['lines']:
            for word in re.split(r'[\s,]+', t):
                if '=' in word or '<' in word or '>' in word:
                    facts[re.split(r'!?=|[<>]=?', word)[0]] += 1
    print('\nFacts tested (by rules and scenes, and their lines): ' + ', '.join(f'{k} {n}' for k, n in facts.most_common()))
    grammars = library['grammars']
    plain = set(grammars.get('plain', {}))
    broad = lambda symbols: sum(1 for k in symbols if k.endswith('.broad'))
    print(f'\nGrammars: plain has {len(plain)} symbols.')
    for name, symbols in sorted(grammars.items()):
        if name != 'plain':
            print(f'  {name:20} {len(set(symbols) & plain):3} of plain\'s {len(plain)}, {broad(symbols)} broad'
                  + (f' (missing: {", ".join(sorted(plain - set(symbols))[:8])}...)' if plain - set(symbols) else ''))
    regions = [args.region] if args.region else REGIONS
    print('\nWhat can be said in each region (everyday / events / its own / barks):')
    thin = []
    for r in regions:
        here = [e for e in entries if e['regions'] is None or r in e['regions']]
        own = sum(1 for e in here if e['regions'] is not None)
        every = sum(1 for e in here if {'talk.' + t for t in EVERYDAY} & e['concepts'])
        events = sum(1 for e in here if {'talk.' + t for t in EVENTS} & e['concepts'])
        barks = sum(1 for e in here if any(c == 'bark' or c.startswith('bark.') for c in e['concepts']))
        print(f'  {r:20} {every:5} / {events:4} / {own:4} / {barks:4}')
        for t in EVENTS + EVERYDAY:
            n = sum(1 for e in here if 'talk.' + t in e['concepts'] and e['priority'] >= 0)
            if n < args.min:
                thin.append((r, t, n))
    # A rough measure of how long before a repeat: a player near talk hears about one exchange every two minutes;
    # with everyday scenes chosen evenly, the unheard ones last this many hours.
    everyday = sum(1 for e in entries if {'talk.' + t for t in EVERYDAY} - {'talk.weather'} & e['concepts'])
    print(f'\nAbout {everyday * 2 / 60:.1f} hours of listening before an everyday scene repeats '
          f'(one exchange every two minutes, before regional and speaker variety).')
    if thin:
        print(f'\nThin spots (fewer than {args.min}, not counting filler):')
        for r, t, n in thin:
            print(f'  {r:20} {t:12} {n}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
