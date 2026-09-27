"""The people of Upper Accord: about a hundred and fifty residents with homes, work, evenings and stories.

Reads the world as it is in DEV (so Atlas edits are respected) and the generator's building manifest (what each
building is for). Every place given to a resident is checked against DEV's actual tiles.

  cd tools && python3 -m worldgen.residents [--dry-run] [--replace]
"""
from __future__ import annotations

import argparse
import random
import re

import terrain_catalog
from . import upper_accord as UA

SEED = 4242
FACTIONS = [
    {'id': 'warden_order', 'name': 'The Warden Order', 'color': '#6f8fa8'},
    {'id': 'concord', 'name': 'The Concord', 'color': '#d9c27a'},
    {'id': 'accord_watch', 'name': 'The Accord Watch', 'color': '#a8584a'},
]
CLAIMS = {'upper_accord': ['concord', 'accord_watch'], 'concord_hall': ['concord'],
          'training_grounds': ['warden_order'], 'warden_order': ['warden_order']}
ECONOMY = {'treasury': 20000, 'storeHerbs': 1500, 'storeMeals': 900, 'dailyHerbs': 150, 'dailyMeals': 200}

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
          'remembers every name ever heard', 'collects smooth stones from the tarns', 'distrusts anything from the '
          'lowlands', 'feeds the plaza sparrows', 'argues with the weather', 'keeps a tally of favours owed',
          'has opinions about everyone\'s grandmother', 'is always a little late', 'quotes the Accord from memory',
          'sleeps badly before storms', 'tells the same three stories', 'cannot resist a wager']
HOOKS = ['owes a debt to someone in the Crafts ward and has not said to whom',
         'lost a sibling on the eastern ridge in a hard winter and still watches the trail',
         'once petitioned the Concord and lost, and has not forgiven it',
         'hopes a child of the family will be taken as a Warden pup',
         'came up the main road as a young wolf with nothing, and is quietly proud of what was built since',
         'keeps a letter from a Warden that was never answered',
         'is feuding with a neighbour over a well that is not really anyone\'s',
         'was a trainee once and washed out, and does not speak of it',
         'is saving coin toward something no one else knows about',
         'believes the old stones of the city wall remember things',
         'swore an oath at the Chapel of the First Oath and takes it seriously',
         'has family in the lowlands who never write',
         'knows a story about the Warden leader that would be unwise to repeat',
         'fears the mountain in a way that does not fit someone born here',
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


def populate(project, manifest, seed=SEED):
    rng = random.Random(seed)
    P = People(project, manifest, rng)
    evenings = [b for b in manifest if b['kind'] in ('tavern', 'inn')]
    plaza = UA.PLAZA_SPAWN

    def evening_out():
        if rng.random() < .45:
            return P.floor(rng.choice(evenings)['rooms'][0]['id'], share=True)
        return P.world(plaza[0] + rng.randint(-10, 10), plaza[1] + rng.randint(-8, 8))

    houses = by_kind(manifest, 'house')
    name_families(project, houses, rng)
    city_houses = [h for h in houses if h['district'] != 'order']
    rng.shuffle(city_houses)
    homes = iter(city_houses)

    # Shopkeepers, innkeepers and their households.
    for shop in by_kind(manifest, 'shop', 'tavern', 'inn'):
        house = next(homes)
        family = house['name'].replace(' House', '')
        trade = shop['trade'] or shop['kind']
        label = {'tavern': 'keeping the taproom', 'inn': 'keeping the inn'}.get(shop['kind'], f'keeping the {trade} shop'
                                                                             if trade != 'general' else 'keeping shop')
        start = rng.choice([6, 7, 7, 8])
        hours = (start, start + 12) if shop['kind'] == 'shop' else (11, 23.5)
        P.add(role='merchant', work_label=label, family=family, home=P.bed(house), work=P.work(shop),
              evening=P.spot(house['rooms'][0]['id'], *P.beds(house)[0][1:], share=True) if P.beds(house) else
              P.floor(house['rooms'][0]['id'], share=True), hours=hours,
              job=f'who keeps {shop["name"]}', greeting_line=f'This is {shop["name"]}. Herbs and meals, fair prices.')
        # A partner and sometimes a youngster or an elder share the house.
        for kin in range(rng.choice([0, 1, 1])):
            bed = P.bed(house)
            if not bed:
                break
            young = rng.random() < .35
            if young:
                P.add(role='civilian', work_label='playing in the plaza', family=family, age=rng.randint(4, 11),
                      home=bed, work=P.world(plaza[0] + rng.randint(-12, 12), plaza[1] + rng.randint(-9, 9)),
                      evening=bed, hours=(9, 15), paid=False, purse=0, job='a youngster of the household')
            else:
                P.add(role='civilian', work_label=f'helping at {shop["name"]}'[:40], family=family, home=bed,
                      work=P.floor(shop['rooms'][0]['id']), evening=evening_out(), hours=(hours[0] + 1, hours[1] - 1),
                      job=f'who works beside family at {shop["name"]}')

    # Craft and civic workers from other households.
    civic = {b['name']: b for b in manifest}
    jobs = [('House of Mending', 'tending the sick', 2), ('Southside Infirmary', 'tending the hurt', 1),
            ('Hall of Records', 'copying the city rolls', 2), ('Gate Warehouse', 'hauling crates', 2),
            ('Crafts Warehouse', 'tallying stores', 1), ('East Warehouse', 'minding the stores', 1)]
    for building, label, count in jobs:
        for _ in range(count):
            house = next(homes)
            P.add(role='civilian', work_label=label, family=house['name'].replace(' House', ''), home=P.bed(house),
                  work=P.work(civic[building]) if civic[building]['rooms'][0]['work'] else
                  P.floor(civic[building]['rooms'][0]['id']), evening=evening_out(), hours=(8, 17),
                  job=f'{label} at the {building}')
    for label, x, y in (('drawing water at the plaza well', plaza[0] - 15, plaza[1] - 11),
                        ('sweeping the Main Gate road', 300, 418), ('selling from a plaza stall', plaza[0] + 6, plaza[1] - 8),
                        ('selling from a plaza stall', plaza[0] - 6, plaza[1] + 8),
                        ('mending the old wall', 330, 460), ('telling old stories by the fountain', plaza[0], plaza[1] + 3)):
        house = next(homes)
        P.add(role='civilian', work_label=label, family=house['name'].replace(' House', ''), home=P.bed(house),
              work=P.world(x, y, share=False), evening=evening_out(), hours=(8, 17),
              age=rng.randint(60, 80) if 'stories' in label else None, paid='stories' not in label,
              job=label)

    # The Accord Watch: day and night shifts on three circuits, and the gate posts.
    ring = P.route('ring_circuit', 'Ring road circuit', [(300, 404), (326, 330), (362, 296), (396, 276), (486, 308),
                                                         (474, 372), (461, 452), (430, 492), (372, 470), (318, 456)])
    market = P.route('market_round', 'Market round', [(365, 377), (405, 377), (405, 407), (365, 407), (340, 408),
                                                      (378, 440), (374, 340)])
    gates = P.route('gate_walk', 'Gate walk', [(300, 421), (340, 408), (385, 392), (378, 440), (372, 474),
                                               (385, 392), (374, 340), (368, 294)])
    barracks = civic['South Watch Barracks']
    posts = {'Main Gate Guardhouse': (297, 421), 'South Gate Guardhouse': (372, 473), 'Stair Gate Guardhouse': (368, 294)}
    shifts = [(ring, (6, 18)), (ring, (18, 6)), (market, (6, 18)), (market, (18, 6)), (gates, (6, 18)),
              (gates, (18, 6)), (ring, (6, 18)), (market, (6, 18))]
    for route, hours in shifts:
        bed = P.bed(barracks)
        P.add(role='guard', work_label='on night patrol' if hours[0] == 18 else 'on patrol', route=route, home=bed,
              work=P.routes[[r['id'] for r in P.routes].index(route)]['posts'][0], evening=bed, hours=hours,
              species=rng.choice(['timber', 'arctic']), job='a watch guard in a dented Accord Watch collar',
              greeting_line='Keep the peace and we will get along.')
    for post, (x, y) in posts.items():
        house = civic[post]
        for hours in ((6, 18), (18, 6)):
            bed = P.bed(house) or P.bed(barracks)
            P.add(role='guard', work_label=f'holding the {post.replace(" Guardhouse", "").lower()}', home=bed,
                  work=P.world(x, y, share=False), evening=bed, hours=hours,
                  job=f'a gate guard of the {post.replace(" Guardhouse", "")}',
                  greeting_line='State your business at the gate.')

    # Concord Hall: councillors, clerks, the chaplain, the Annex keepers and the refectory.
    quarters = civic["Clerks' Quarters"]
    senate, chapel, annex = civic['The Grand Hall of Concord'], civic['The Chapel of the First Oath'], civic['The Concord Annex']
    hearings = [b for b in manifest if b['kind'] == 'hearing']
    refectory = civic['The Concord Refectory']
    P.add(role='civilian', work_label='presiding over the Concord', home=P.bed(next(homes)), work=P.work(senate),
          evening=P.floor(chapel['rooms'][0]['id'], share=True), hours=(9, 18), age=rng.randint(55, 70),
          job='the Speaker of the Concord, whose voice carries the Grand Hall', paid=True,
          greeting_line='The Concord hears every pack. Speak plainly and you will be heard.')
    for i in range(3):
        P.add(role='civilian', work_label='debating in the Grand Hall', home=P.bed(next(homes)),
              work=P.floor(senate['rooms'][0]['id']), evening=evening_out(), hours=(9, 17), age=rng.randint(40, 70),
              job='a councillor of the Concord', greeting_line='Every law here was argued for. Most of them twice.')
    for hall in hearings:
        P.add(role='civilian', work_label='hearing petitions', home=P.bed(quarters), work=P.work(hall),
              evening=P.floor(quarters['rooms'][0]['id'], share=True), hours=(9, 16),
              job=f'an arbiter of the {hall["name"].replace("The ", "")}',
              greeting_line='Bring your dispute in writing if you can, and your witnesses if you cannot.')
    P.add(role='civilian', work_label='keeping the chapel', home=P.bed(quarters), work=P.work(chapel),
          evening=P.floor(chapel['rooms'][0]['id'], share=True), hours=(6, 20),
          job='the chaplain of the First Oath', greeting_line='The oath binds the one who swears it. Sit, if you like.')
    for label, room in (('keeping the Annex', annex['rooms'][0]['id']), ('shelving in the upper stacks', annex['rooms'][1]['id']),
                        ('tending the scroll vaults', annex['rooms'][2]['id']), ('reading in the deep archive', annex['rooms'][3]['id'])):
        P.add(role='civilian', work_label=label, home=P.bed(quarters),
              work=P.work(annex) if label == 'keeping the Annex' else P.floor(room), evening=evening_out(),
              hours=(8, 18), job='a keeper of the Concord Annex',
              greeting_line='Quietly, please. The oldest books here are older than the wall.')
    P.add(role='merchant', work_label='running the refectory', home=P.bed(quarters), work=P.work(refectory),
          evening=P.floor(refectory['rooms'][0]['id'], share=True), hours=(6, 19), job='the Concord Refectory cook',
          greeting_line='There is always soup. Sometimes there is also bread.')
    for _ in range(3):
        P.add(role='civilian', work_label='carrying messages for the Concord', home=P.bed(quarters),
              work=P.floor(senate['rooms'][0]['id']), evening=evening_out(), hours=(8, 17),
              job='a Concord clerk with ink to the elbows', age=rng.randint(18, 35))

    # The Training Grounds: trainers, trainees and pups, the cook and the quartermaster.
    tx, ty, tw, th, _ = UA.TRAINING
    trainers, trainees, pups = civic["Trainers' Hall"], civic['Trainee Barracks'], civic['The Pup Den']
    cookhouse, stores = civic['The Training Cookhouse'], civic['Training Stores']
    yards = [(tx + 50, ty + 85, 'drilling trainees on the square'), (tx + 15, ty + 117, 'running sparring bouts'),
             (tx + 60, ty + 128, 'coaching at the archery range'), (tx + 30, ty + 188, 'running the obstacle course')]
    for x, y, label in yards:
        P.add(role='civilian', work_label=label, home=P.bed(trainers), work=P.world(x, y, share=False),
              evening=P.floor(cookhouse['rooms'][0]['id'], share=True), hours=(6, 17), age=rng.randint(32, 58),
              species=rng.choice(['timber', 'arctic']), job='a Warden trainer, harder than the mountain',
              greeting_line='If you are not training, you are in the way. Stand over there.')
    for i in range(8):
        x, y, _ = yards[i % len(yards)]
        P.add(role='civilian', work_label='training', home=P.bed(trainees),
              work=P.world(x + rng.randint(-6, 6), y + rng.randint(-4, 4), share=False),
              evening=P.floor(cookhouse['rooms'][0]['id'], share=True), hours=(6, 18), age=rng.randint(15, 22),
              job='a Warden trainee, bruised and determined',
              greeting_line='Can\'t stop. Trainer\'s watching. What is it?')
    for i in range(5):
        P.add(role='civilian', work_label='learning to track', home=P.bed(pups),
              work=P.world(tx + 40 + rng.randint(-8, 8), ty + 60 + rng.randint(-4, 4), share=False),
              evening=P.floor(pups['rooms'][0]['id'], share=True), hours=(7, 15), age=rng.randint(6, 11), paid=False,
              purse=0, job='a Warden pup, all paws and questions', greeting_line='Are you a Warden? Have you fought a bear?')
    P.add(role='merchant', work_label='feeding the Grounds', home=P.bed(trainers), work=P.work(cookhouse),
          evening=P.floor(cookhouse['rooms'][0]['id'], share=True), hours=(5, 19), job='the Training Cookhouse cook',
          greeting_line='Eat. You will need it. Everyone always needs it.')
    P.add(role='civilian', work_label='keeping the training stores', home=P.bed(trainers), work=P.work(stores),
          evening=P.floor(cookhouse['rooms'][0]['id'], share=True), hours=(6, 16), job='the Grounds quartermaster',
          greeting_line='Sign for it, bring it back, and bring it back whole.')

    # The Warden Order: the leader, the council, the Wardens and their keepers.
    ox, oy, ow, oh, _ = UA.ORDER
    leader_hall, library = civic["The Leader's Hall"], civic['The Warden Crypt Library']
    old, north = civic['The Old Barracks'], civic['The North Barracks']
    order_refectory, armory = civic['The Order Refectory'], civic['The Order Armory']
    order_houses = [h for h in houses if h['district'] == 'order']
    P.add(role='civilian', work_label='leading the Warden Order', sex='male', home=P.bed(leader_hall),
          work=P.work(leader_hall), evening=P.floor(leader_hall['rooms'][1]['id'], share=True), hours=(7, 20),
          age=58, species='timber', paid=True, job='the leader of the Warden Order, grey and unhurried',
          greeting_line='You came a long way up the mountain to stand in front of me. Say why.',
          hook='has led the Order for nineteen winters and has buried more Wardens than he will admit to')
    for house in order_houses[:4]:
        P.add(role='civilian', work_label='advising the leader', home=P.bed(house),
              work=P.floor(leader_hall['rooms'][0]['id']), evening=P.floor(order_refectory['rooms'][0]['id'], share=True),
              hours=(8, 18), age=rng.randint(45, 66), job='one of the leader\'s trusted advisors',
              greeting_line='The leader\'s time is not yours. Mine, perhaps, for a moment.')
    compound = P.route('order_round', 'Order compound round', [(ox + 64, oy + 4), (ox + 20, oy + 30), (ox + 50, oy + 50),
                                                               (ox + 90, oy + 30), (ox + 104, oy + 46), (ox + 50, oy + 74)])
    ridge = P.route('ridge_patrol', 'Ridge trail patrol', [(ox + 104, oy + 47), *UA.ORDER_TRAIL[1:], (tx + 23, ty + th - 3),
                                                           (tx + 23, ty + 3), *reversed(UA.CONCORD_TRAIL[1:-1]),
                                                           (UA.CONCORD[0] + UA.CONCORD[2] - 4, UA.CONCORD[1] + 46)])
    for route, hours, home in ((compound, (6, 18), old), (compound, (18, 6), old), (ridge, (6, 18), north),
                               (ridge, (18, 6), north), (compound, (6, 18), north), (ridge, (6, 18), old)):
        bed = P.bed(home)
        P.add(role='guard', work_label='on night watch' if hours[0] == 18 else 'on watch', route=route, home=bed,
              work=P.routes[[r['id'] for r in P.routes].index(route)]['posts'][0], evening=bed, hours=hours,
              species=rng.choice(['timber', 'arctic', 'maned']), job='a Warden of the Order in worn harness',
              greeting_line='Wardens keep the passes. What brings you to ours?')
    for house in order_houses[4:8]:
        P.add(role='civilian', work_label='drilling in the compound', home=P.bed(house),
              work=P.world(ox + 30 + rng.randint(0, 40), oy + 40 + rng.randint(0, 12), share=False),
              evening=P.floor(order_refectory['rooms'][0]['id'], share=True), hours=(6, 17),
              job='a Warden between patrols', greeting_line='Mind the stones; they are older than they look.')
    P.add(role='civilian', work_label='keeping the crypt library', home=P.bed(old), work=P.floor(library['rooms'][1]['id']),
          evening=P.floor(library['rooms'][0]['id'], share=True), hours=(7, 19), age=rng.randint(60, 78),
          job='the keeper of the crypt library, dust in every hair',
          greeting_line='The dead keep good records. Better than the living.')
    P.add(role='merchant', work_label='cooking for the Order', home=P.bed(north), work=P.work(order_refectory),
          evening=P.floor(order_refectory['rooms'][0]['id'], share=True), hours=(5, 19), job='the Order refectory cook',
          greeting_line='Plain food. It keeps you alive, which is the point.')
    P.add(role='merchant', work_label='keeping the armory', home=P.bed(north), work=P.work(armory),
          evening=P.floor(order_refectory['rooms'][0]['id'], share=True), hours=(7, 17), job='the Order armorer',
          greeting_line='Everything here is counted. Everything.')
    return P


def apply(project, P, replace=False):
    """The project with the residents, routes, factions, claims, economy and herb patch set."""
    if project.get('people') and not replace:
        raise ValueError(f'The world already has {len(project["people"])} residents; use --replace to swap them.')
    p = dict(project)
    p['people'], p['routes'] = P.people, P.routes
    known = {f['id'] for f in p.get('factions', [])}
    p['factions'] = p.get('factions', []) + [f for f in FACTIONS if f['id'] not in known]
    door_cell = {}
    for link in p['links']:
        door_cell[link['b']['cell']] = link['a']['cell']
    cell_ids = {c['id'] for c in p['cells']}
    for area in p['cells'] + p['rooms']:
        cell = area['id'] if area['id'] in cell_ids else door_cell.get(area['id'])
        while cell in door_cell:                       # An upper floor or a cellar: follow its stairs to the street.
            cell = door_cell[cell]
        claims = CLAIMS.get(cell)
        if claims:
            area['territory'] = {**area.get('territory', {}), 'region': 'upper_accord', 'claims': list(claims)}
    p['economy'] = ECONOMY
    p['herbPatch'] = P.world(UA.MAIN_ROAD[2][0], UA.MAIN_ROAD[2][1] - 8, share=False)
    return p


def main(argv=None):
    import world_db
    import world_store
    import map_editor
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true', help='check everything but do not save')
    parser.add_argument('--replace', action='store_true', help='replace residents already in the world')
    args = parser.parse_args(argv)
    _, site, _ = UA.build()
    with world_db.connect('dev', 'editor') as conn:
        wid = world_store.list_worlds(conn)[0]['id']
        project, revision = world_store.load_world(conn, wid)
        P = populate(project, site.manifest)
        merged = apply(project, P, args.replace)
        map_editor.check_project(merged, for_game=False)
        roles = {}
        for person in P.people:
            roles[person['role']] = roles.get(person['role'], 0) + 1
        print(f'{len(P.people)} residents ({", ".join(f"{n} {r}" for r, n in sorted(roles.items()))}), '
              f'{len(P.routes)} patrol routes, {len(FACTIONS)} factions.')
        if args.dry_run:
            return 0
        new = world_store.save_world(conn, merged, revision)
        print(f'Saved to {wid} in DEV, revision {new}.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
