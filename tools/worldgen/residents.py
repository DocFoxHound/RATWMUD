"""The residents kit: names, looks and lines for generated people, and People, which gives each a home, work and an
evening on tiles checked against the world's own (worldgen.citizens, worldgen.towns, worldgen.country use it).
"""
from __future__ import annotations

import re

import terrain_catalog

FEMALE = ['Wren', 'Sorrel', 'Tamsin', 'Linden', 'Mara', 'Sella', 'Ivy', 'Hazel', 'Brisk', 'Nettle', 'Rue', 'Sage',
          'Willa', 'Fern', 'Aster', 'Briar', 'Juniper', 'Lark', 'Moss', 'Rowan', 'Senna', 'Tansy', 'Vale', 'Yarrow',
          'Ember', 'Frost', 'Heather', 'Isla', 'Kestrel', 'Laurel', 'Marsh', 'Nell', 'Opal', 'Quill', 'Reed', 'Skye',
          'Thistle', 'Umber', 'Vesper', 'Wisp', 'Aspen', 'Bramble', 'Clover', 'Dawn', 'Elm', 'Fallow', 'Gale', 'Holly',
          'Iris', 'Jessamy', 'Kindle', 'Lichen', 'Myrtle', 'Nightjar', 'Oriel', 'Pell', 'Rill', 'Sloe', 'Tarn', 'Wick']
MALE = ['Harl', 'Rook', 'Birch', 'Fennel', 'Vetch', 'Odo', 'Rusk', 'Harrow', 'Alder', 'Bram', 'Cinder', 'Dun',
        'Eskar', 'Flint', 'Garth', 'Hob', 'Ingram', 'Jory', 'Kell', 'Lorn', 'Moor', 'Nab', 'Oak', 'Pike', 'Quarry',
        'Ridge', 'Stone', 'Thorn', 'Ulric', 'Varg', 'Wolde', 'Ash', 'Brock', 'Cairn', 'Drift', 'Emrys', 'Fell',
        'Grist', 'Holt', 'Jarl', 'Knap', 'Ludo', 'Marl', 'Nokk', 'Orrin', 'Pell', 'Roan', 'Scree', 'Tor', 'Wald',
        'Brand', 'Corrie', 'Dray', 'Edric', 'Frey', 'Gorse', 'Heath', 'Kite', 'Loam', 'Slate']
SPECIES_WORDS = {'timber': 'grey timber wolf', 'maned': 'long-legged maned wolf', 'arctic': 'pale arctic wolf',
                 'red': 'rust-red wolf', 'ethiopian': 'slender russet Ethiopian wolf'}
# The coat palette's colours in words (Core/RatwNames.cpp colourWord), and each kind of wolf without a colour.
COAT_WORDS = ['cream', 'silver', 'grey', 'dun', 'brown', 'black', 'russet', 'sandy']
KIND_WORDS = {'timber': 'timber wolf', 'maned': 'long-legged maned wolf', 'arctic': 'arctic wolf', 'red': 'red wolf',
              'ethiopian': 'slender Ethiopian wolf'}
BUILDS = {'short': ['compact', 'stocky', 'small, quick'], 'average': ['lean', 'steady', 'rangy', 'solid'],
          'tall': ['tall', 'broad-shouldered', 'long-limbed']}
MARKS = ['a notched left ear', 'a grey-frosted muzzle', 'a white blaze down the chest', 'one torn ear', 'a crooked tail',
         'amber eyes that miss little', 'a scar across the nose', 'a coat kept meticulously clean', 'burrs forever in '
         'the ruff', 'a limp that comes and goes with the weather', 'pale, nearly silver eyes', 'a braided cord '
         'at the throat', 'a dark saddle across the back', 'ink-stained forepaws', 'a missing claw', 'soot on the paws']
TRAITS = ['patient', 'blunt', 'warm', 'guarded', 'curious', 'proud', 'wry', 'anxious', 'generous', 'stubborn',
          'devout', 'restless', 'shrewd', 'gentle', 'loud', 'meticulous', 'dreamy', 'suspicious', 'loyal', 'playful',
          'tired', 'ambitious', 'honest to a fault', 'quietly funny', 'easily offended', 'unhurried']
QUIRKS = ['counts things under the breath', 'hums old marching songs', 'never sits with a back to the door',
          'remembers every name ever heard', 'collects smooth stones from the riverbanks', 'distrusts anything from the '
          'city', 'feeds the sparrows', 'argues with the weather', 'keeps a tally of favours owed',
          'has opinions about everyone\'s grandmother', 'is always a little late', 'quotes old sayings for everything',
          'sleeps badly before storms', 'tells the same three stories', 'cannot resist a wager']
HOOKS = ['owes a debt to someone in Ridgemere and has not said to whom',
         'lost a sibling to a hard winter and still watches the road',
         'once petitioned the Council of Houses and lost, and has not forgiven it',
         'hopes a child of the family will be taken into service with one of the great Houses',
         'came along the road as a young wolf with nothing, and is quietly proud of what was built since',
         'keeps a letter from a lost love that was never answered',
         'is feuding with a neighbour over a well that is not really anyone\'s',
         'went to Ridgemere to make a fortune once and came back with nothing, and does not speak of it',
         'is saving coin toward something no one else knows about',
         'believes the old standing stones remember things',
         'swore an oath at a wedding and takes it seriously',
         'has family far away who never write',
         'knows a story about one of the great Houses that would be unwise to repeat',
         'fears the deep woods in a way that does not fit someone born here',
         'is the third generation to hold this trade and feels every one of them watching']


SURNAME_HEADS = ['Ash', 'Birch', 'Bramble', 'Cold', 'Crag', 'Dusk', 'Ember', 'Fell', 'Frost', 'Grey', 'Hollow', 'Iron',
                 'Kestrel', 'Lichen', 'Marsh', 'Moss', 'Pine', 'Rime', 'Slate', 'Snow', 'Stone', 'Storm', 'Thorn', 'Wind',
                 'Oak', 'Rook', 'Scree', 'Tarn', 'Heath', 'Flint']
SURNAME_TAILS = ['fall', 'mantle', 'paw', 'born', 'walker', 'foot', 'hide', 'coat', 'crest', 'mere', 'well', 'run',
                 'step', 'bark', 'back', 'mark', 'claw', 'shadow', 'brook', 'gate', 'vale', 'ridge', 'moor', 'heart',
                 'wood', 'water', 'song', 'tail', 'fang', 'howl']


def spoken(role_text, trait):
    openings = {'warm': 'Welcome, welcome.', 'blunt': 'Say what you need.', 'guarded': 'Yes?', 'wry': 'Well, look at you.',
                'curious': 'Oh, a new face. Where from?', 'proud': 'You have found the best in the city.',
                'gentle': 'Take your time; there is no hurry.', 'loud': 'Ho! Come in, come in!',
                'shrewd': 'Looking, or buying?', 'tired': 'Mm. Morning. Or whatever it is.'}
    return f'{openings.get(trait, "Good day to you.")} {role_text}'


class People:
    def __init__(self, project, manifest, rng):
        self.project, self.rng = project, rng
        self.areas = {a['id']: a for a in project['cells'] + project['rooms']}
        self.manifest = manifest
        self.people, self.routes, self.taken = [], [], set()
        self.ids, self.names = set(), set()
        self.voice = 0

    # -- places ------------------------------------------------------------------------------------------------
    def walkable(self, cell, x, y):
        a = self.areas.get(cell)
        return bool(a) and 0 <= y < a['height'] and 0 <= x < a['width'] and a['terrain'][y][x] not in terrain_catalog.SOLID

    def spot(self, cell, x, y, share=False):
        """A place for someone: this tile if free, else the nearest free walkable tile around it."""
        for r in range(0, 6):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    if max(abs(dx), abs(dy)) != r:
                        continue
                    p = (cell, x + dx, y + dy)
                    a = self.areas.get(cell)
                    if a and self.walkable(*p) and a['terrain'][p[2]][p[1]] not in '+^' and (share or p not in self.taken):
                        if not share:
                            self.taken.add(p)
                        return {'cell': cell, 'x': p[1], 'y': p[2]}
        raise ValueError(f'No free tile near {cell} {x},{y}')

    def floor(self, room, share=False):
        """Any free floor tile inside a room, away from its walls."""
        a = self.areas[room]
        tiles = [(x, y) for y in range(2, a['height'] - 2) for x in range(2, a['width'] - 2)
                 if self.walkable(room, x, y) and a['terrain'][y][x] not in '+^']
        self.rng.shuffle(tiles)
        for x, y in tiles:
            if share or (room, x, y) not in self.taken:
                return self.spot(room, x, y, share)
        raise ValueError(f'{room} is full')

    def world(self, x, y, share=True):
        """A tile given in world coordinates."""
        for c in self.project['cells']:
            if c['x'] <= x < c['x'] + c['width'] and c['y'] <= y < c['y'] + c['height']:
                return self.spot(c['id'], x - c['x'], y - c['y'], share)
        raise ValueError(f'({x}, {y}) is in no cell')

    def beds(self, building):
        out = []
        for room in building['rooms']:
            for b in room['beds']:
                if (room['id'], b['x'], b['y']) not in self.taken and self.walkable(room['id'], b['x'], b['y']):
                    out.append((room['id'], b['x'], b['y']))
        return out

    def bed(self, building):
        free = self.beds(building)
        if not free:
            return None
        cell, x, y = free[0]
        return self.spot(cell, x, y)

    def work(self, building):
        for room in building['rooms']:
            for w in room['work']:
                if (room['id'], w['x'], w['y']) not in self.taken and self.walkable(room['id'], w['x'], w['y']):
                    return self.spot(room['id'], w['x'], w['y'])
        return self.floor(building['rooms'][0]['id'])

    # -- people ------------------------------------------------------------------------------------------------
    def name(self, sex, family=None):
        pool = FEMALE if sex == 'female' else MALE
        for _ in range(400):
            first = self.rng.choice(pool)
            full = f'{first} {family}' if family else first
            if full not in self.names:
                self.names.add(full)
                return full
        raise ValueError('ran out of names')

    def add(self, *, role, work_label, home, work, evening, hours, sex=None, age=None, family=None, route='',
            job='', paid=None, purse=None, meals=1, herbs=0, species=None, greeting_line='', hook=None):
        rng = self.rng
        sex = sex or rng.choice(['female', 'male'])
        name = self.name(sex, family)
        pid = name.lower().replace(' ', '_').replace("'", '')
        while pid in self.ids:
            pid += '_'
        self.ids.add(pid)
        species = species or rng.choices(list(SPECIES_WORDS), [5, 1, 4, 2, 1])[0]
        stature = rng.choices(['short', 'average', 'tall'], [2, 5, 2])[0]
        age = age if age is not None else rng.randint(20, 62)
        traits = rng.sample(TRAITS, 2)
        look = {'species': species, 'sex': sex, 'stature': stature,
                'pattern': rng.choice(['solid', 'saddle', 'mantle', 'piebald']),
                'baseColor': rng.randrange(8), 'gradientColor': rng.randrange(8), 'markingColor': rng.randrange(8)}
        who = 'she' if sex == 'female' else 'he'
        description = (f'A {rng.choice(BUILDS[stature])} {SPECIES_WORDS[species]} with {rng.choice(MARKS)}, '
                       f'{job or work_label}.')
        # The kind's colour is the coat's own (the game says the same: names::fitCoat).
        description = description.replace(SPECIES_WORDS[species], COAT_WORDS[look['baseColor']] + ' ' + KIND_WORDS[species], 1)
        personality = (f'{traits[0].capitalize()} and {traits[1]}; {who} {rng.choice(QUIRKS)}.')
        backstory = f'{name.split()[0]} {hook or rng.choice(HOOKS)}.'
        self.voice = (self.voice + 7) % 32
        self.people.append({
            'id': pid, 'name': name, 'role': role, 'description': description,
            'greeting': spoken(greeting_line or f'I am {work_label} today.', traits[0]),
            'workLabel': work_label[:40], 'age': age, 'appearance': look, 'voice': self.voice,
            'hours': {'start': hours[0], 'end': hours[1]}, 'route': route,
            'paid': role != 'merchant' if paid is None else paid,
            'purse': (rng.randint(80, 220) if role == 'merchant' else rng.randint(15, 60)) if purse is None else purse,
            'herbs': herbs if role != 'merchant' else rng.randint(6, 20),
            'meals': meals if role != 'merchant' else rng.randint(10, 24),
            'home': home, 'work': work, 'evening': evening, 'personality': personality, 'backstory': backstory})
        return self.people[-1]

    def route(self, rid, name, posts):
        self.routes.append({'id': rid, 'name': name, 'posts': [self.world(x, y) for x, y in posts]})
        return rid


def name_families(project, houses, rng):
    """Every house belongs to a family: numbered houses (the generator ran out of family names) get a surname, and the
    house and its door in the world are renamed to match."""
    used = {h['name'].replace(' House', '') for h in houses if not re.fullmatch(r'House \d+', h['name'])}
    rooms = {r['id']: r for r in project['rooms']}
    for h in houses:
        if not re.fullmatch(r'House \d+', h['name']):
            continue
        while True:
            family = rng.choice(SURNAME_HEADS) + rng.choice(SURNAME_TAILS)
            if family not in used:
                break
        used.add(family)
        old, h['name'] = h['name'], f'{family} House'
        for room in h['rooms']:
            if room['id'] in rooms and rooms[room['id']]['name'] == old:
                rooms[room['id']]['name'] = h['name']
        for link in project['links']:
            if link['name'] == f'{old} door' and link['b']['cell'] in rooms and link['b']['cell'] in {r['id'] for r in h['rooms']}:
                link['name'] = f'{h["name"]} door'


def by_kind(manifest, *kinds):
    return [b for b in manifest if b['kind'] in kinds]
