#!/usr/bin/env python3
"""How much the NPC-to-NPC scene library covers (Docs/Design/30-towns-and-talk.md, phase 6).

Counts the scenes in Data/Voice/scenes by topic, by region (a scene for anywhere counts for every region), and how
many lines are written for each kind of speaker (sex, age, job), and estimates how many distinct renderings the
library holds. Lists the thin spots: topic x region pairs with fewer scenes than --min.

    python3 tools/scene_report.py [--min 4] [--region ridgemere]

The game's own parser (Core/RatwScenes.cpp) is the judge of whether a file is valid; this only counts.
"""
import argparse
import collections
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / 'Data' / 'Voice' / 'scenes'
REGIONS = ['upper_accord', 'ridgemere', 'ser_ferro', 'accord_crossing', 'saltreach', 'cinderbrook', 'westmarch',
           'lakeside', 'fenhollow', 'amberford', 'hollowmere_village', 'ghost_town', 'northern_fortress', 'isle_fortress',
           'dark_fortress']
EVENT_TOPICS = ['prices', 'caravan', 'bandits', 'crime', 'life', 'festival', 'newcomer']
EVERYDAY = ['smalltalk', 'work', 'family', 'weather', 'lore', 'player', 'gossip', 'news', 'quarrel', 'friends', 'day', 'bark']


def load():
    groups, scenes = {}, []
    files = sorted(ROOT.rglob('*.scene'), key=lambda p: (p.name != 'groups.scene', str(p)))
    for f in files:
        scene = None
        for line in f.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('group '):
                name, members = line[6:].split('=', 1)
                out = set()
                for m in members.split():
                    out |= groups.get(m, {m})
                groups[name.strip()] = out
            elif line.startswith('scene '):
                scene = {'id': line[6:].strip(), 'file': str(f.relative_to(ROOT)), 'topics': set(), 'regions': None,
                         'when': [], 'lines': []}
                scenes.append(scene)
            elif line.startswith('when '):
                for token in line[5:].split():
                    key, _, values = token.partition('=')
                    values = set(values.split('|'))
                    if key == 'topic':
                        scene['topics'] |= values
                    elif key == 'region':
                        scene['regions'] = set().union(*(groups.get(v, {v}) for v in values))
                    else:
                        scene['when'].append(token)
            elif re.match(r'^[ab]\??(\[[^\]]*\])?\s*:', line):
                head, _, text = line.partition(':')
                tags = re.findall(r'\[([^\]]*)\]', head)
                scene['lines'].append((head[0], tags[0] if tags else '', [t for t in text.split('|') if t.strip()]))
    return scenes


def renderings(scene):
    """Distinct ways the scene can come out for one speaker pair (the alternatives of the lines that apply)."""
    total = 1
    turn = []
    for who, tags, alts in scene['lines'] + [('end', '', [])]:
        if turn and (who != turn[-1][0] or not turn[-1][1]):
            total *= max(len(a) for _, _, a in turn)
            turn = []
        if who != 'end':
            turn.append((who, tags, alts))
    return total


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--min', type=int, default=4, help='Thin: fewer scenes than this for a topic in a region')
    parser.add_argument('--region', help='Only this region')
    args = parser.parse_args()
    scenes = load()
    print(f'{len(scenes)} scenes in {len({s["file"] for s in scenes})} files; '
          f'about {sum(renderings(s) for s in scenes):,} distinct renderings (before blanks and speaker variants).')
    by_topic = collections.Counter(t for s in scenes for t in s['topics'])
    print('\nBy topic: ' + ', '.join(f'{t} {by_topic[t]}' for t in EVENT_TOPICS + EVERYDAY))
    tags = collections.Counter()
    for s in scenes:
        for _, t, _ in s['lines']:
            for word in re.split(r'[\s,]+', t):
                if word:
                    tags[word] += 1
    print('\nLines written for: ' + ', '.join(f'{w} {n}' for w, n in tags.most_common(40)))
    regions = [args.region] if args.region else REGIONS
    print('\nScenes that can play in each region (everyday / events / its own):')
    thin = []
    for r in regions:
        here = [s for s in scenes if s['regions'] is None or r in s['regions']]
        own = sum(1 for s in here if s['regions'] is not None)
        every = sum(1 for s in here if s['topics'] & set(EVERYDAY))
        events = sum(1 for s in here if s['topics'] & set(EVENT_TOPICS))
        print(f'  {r:20} {every:5} / {events:4} / {own:4}')
        for t in EVENT_TOPICS + EVERYDAY:
            n = sum(1 for s in here if t in s['topics'])
            if n < args.min:
                thin.append((r, t, n))
    # A rough measure of how long before a repeat: a player near talk hears about one exchange every two minutes;
    # with everyday scenes chosen evenly, the unheard ones last this many hours.
    everyday = sum(1 for s in scenes if s['topics'] & set(EVERYDAY) - {'bark', 'weather'})
    print(f'\nAbout {everyday * 2 / 60:.1f} hours of listening before an everyday scene repeats '
          f'(one exchange every two minutes, before regional and speaker variety).')
    if thin:
        print(f'\nThin spots (fewer than {args.min}):')
        for r, t, n in thin:
            print(f'  {r:20} {t:10} {n}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
