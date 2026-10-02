"""The people of Ridgemere and Ser Ferro: homes, work, evenings and a line of story for each, in the buildings the
city designer (cities.py) made for them.

Ridgemere: the five great Houses (their families, stewards, servants and workforces), the Council of Houses and its
officials, the City Watch, shopkeepers, and the Sump. Lord Grayrock travels the Accord Road on House business and is
almost never home; Lady Grayrock almost never leaves it.

Ser Ferro: the King and his court, the palace guard, the Archprelate and the cathedral clergy, the elite families of
the Heights, the guilds, shopkeepers and craftsfolk of the middle town, the city guard, and the bargemen and poor of
the wharf.
"""
from __future__ import annotations

import random

from . import residents as R
from .cities import HOUSES, ELITE_FAMILIES

SEED = 5151
FACTIONS = [
    {'id': 'council_of_houses', 'name': 'The Council of Houses', 'color': '#7d8a96'},
    {'id': 'ridgemere_watch', 'name': 'The Ridgemere Watch', 'color': '#5d6b4f'},
    {'id': 'house_grayrock', 'name': 'House Grayrock', 'color': '#8e8e8a'},
    {'id': 'house_fell', 'name': 'House Fell', 'color': '#4f7a4a'},
    {'id': 'house_ashcombe', 'name': 'House Ashcombe', 'color': '#b0703a'},
    {'id': 'house_brinewater', 'name': 'House Brinewater', 'color': '#3f6f96'},
    {'id': 'house_vesk', 'name': 'House Vesk', 'color': '#9aa6a0'},
    {'id': 'crown_of_ser_ferro', 'name': 'The Crown of Ser Ferro', 'color': '#c9423a'},
    {'id': 'church_iron_saint', 'name': 'The Church of the Iron Saint', 'color': '#e3d6a4'},
    {'id': 'ser_ferro_guard', 'name': 'The Ser Ferro Guard', 'color': '#a8453e'},
    {'id': 'merchants_guild', 'name': "The Merchants' Guild", 'color': '#d6a64a'},
]

RM_FEMALE = ['Maren', 'Isolde', 'Greta', 'Hilde', 'Bryn', 'Edda', 'Ottilie', 'Sigrun', 'Tove', 'Wenna', 'Agna', 'Brenna',
             'Corra', 'Dagny', 'Elspet', 'Frida', 'Gudrun', 'Hanne', 'Ingrid', 'Jorunn', 'Kari', 'Liv', 'Magda', 'Nessa',
             'Orla', 'Petra', 'Runa', 'Solveig', 'Thyra', 'Ulla', 'Vigdis', 'Yrsa', 'Ada', 'Beatha', 'Clemence',
             'Dorcas', 'Ebba', 'Gwen', 'Hester', 'Mab', 'Aud', 'Berit', 'Disa', 'Grete', 'Ilse', 'Karin', 'Lotte',
             'Moll', 'Nan', 'Oda', 'Pim', 'Rika', 'Signe', 'Tilde', 'Una', 'Wynn']
RM_MALE = ['Aldric', 'Brand', 'Corwin', 'Dagfinn', 'Edric', 'Fenwick', 'Gunnar', 'Halvard', 'Ivo', 'Jarl', 'Kjell',
           'Leif', 'Magnus', 'Njal', 'Osric', 'Piet', 'Ragnar', 'Sten', 'Torvald', 'Ulf', 'Vidar', 'Wystan', 'Egil',
           'Hakon', 'Orm', 'Tobias', 'Ansel', 'Bertil', 'Caspar', 'Dunstan', 'Emmerich', 'Gerrit', 'Hollis', 'Jasper',
           'Lothar', 'Mathis', 'Nils', 'Oskar', 'Roald', 'Silas', 'Arne', 'Bo', 'Dirk', 'Erland', 'Finn', 'Gorm',
           'Hjalte', 'Ivar', 'Knut', 'Lars', 'Mogens', 'Olaf', 'Palle', 'Rune', 'Soren', 'Tage']
RM_HEADS = ['Soot', 'Tar', 'Brine', 'Rust', 'Slag', 'Cinder', 'Wet', 'Grey', 'Mud', 'Kelp', 'Ash', 'Coal', 'Salt',
            'Rope', 'Plank', 'Nail', 'Mill', 'Rain', 'Drip', 'Fog', 'Gull', 'Pitch', 'Wick', 'Bilge']
RM_TAILS = ['wick', 'by', 'well', 'ford', 'hand', 'stack', 'worth', 'ley', 'gate', 'more', 'ridge', 'cot', 'ham', 'ton',
            'marsh', 'dale']
SF_FEMALE = ['Aurelia', 'Beatrice', 'Chiara', 'Donatella', 'Elena', 'Fiora', 'Giulia', 'Ilaria', 'Livia', 'Lucia',
             'Marzia', 'Nerina', 'Ottavia', 'Paola', 'Rosalba', 'Serafina', 'Teodora', 'Valeria', 'Viola', 'Allegra',
             'Bianca', 'Carlotta', 'Emilia', 'Flavia', 'Gemma', 'Isotta', 'Lavinia', 'Mirella', 'Orsola', 'Perla',
             'Renata', 'Simona', 'Tullia', 'Vittoria', 'Zita', 'Agnese', 'Cosima', 'Dalia', 'Fosca', 'Nives', 'Alba',
             'Brunella', 'Clelia', 'Delia', 'Ersilia', 'Gilda', 'Loretta', 'Mafalda', 'Noemi', 'Pia', 'Rina', 'Sveva']
SF_MALE = ['Aurelio', 'Bastiano', 'Cassio', 'Dario', 'Emilio', 'Fabrizio', 'Giacomo', 'Ilario', 'Leandro', 'Marcello',
           'Nico', 'Orsino', 'Piero', 'Raffaele', 'Silvio', 'Tommaso', 'Ugo', 'Valerio', 'Vittore', 'Alessio',
           'Benedetto', 'Corrado', 'Duccio', 'Enzo', 'Filippo', 'Gualtiero', 'Lorenzo', 'Matteo', 'Ottone', 'Pasquale',
           'Renzo', 'Salvatore', 'Teodoro', 'Umberto', 'Vasco', 'Achille', 'Brunello', 'Ciro', 'Eliseo', 'Manfredi',
           'Amedeo', 'Bruno', 'Cosimo', 'Dante', 'Fausto', 'Gino', 'Lapo', 'Mauro', 'Nando', 'Primo', 'Rocco', 'Taddeo']
RM_QUIRKS = ['coughs in the damp and pretends not to', 'never goes out without an oilcloth hood', 'can name every ship '
             'in the harbour', 'keeps a pocketful of coal for luck', 'hums sea songs under the breath', 'counts the '
             'bells from the Chapel of the Tide', 'trusts no one from a rival House', 'remembers every debt, owed or '
             'owing', 'watches the rain as if it might stop', 'smells faintly of smoke whatever the weather',
             'swears by the forge and the sea', 'carries a wet-stone whetting knife everywhere']
RM_HOOKS = ['owes a House factor money and has been told to find it by the new moon',
            'lost a brother to the quarry and does not forgive House Grayrock for it',
            'dreams of a berth on a Brinewater ship bound somewhere dry',
            'keeps a secret list of which Houses pay the Watch to look away',
            'was born in the Sump and means to die somewhere better',
            'has a cousin in the Fell logging camps who writes when he can',
            'believes the Houses will fall out with each other before the next winter',
            'once saw the Chancellor weep in the Rain Hall and has told no one',
            'is saving for a stall on the quay, coin by coin',
            'carries a letter meant for Lord Grayrock, who is never home to take it']
SF_QUIRKS = ['bows the head at every mention of the Iron Saint', 'knows the name of every palazzo and who lives in '
             'it', 'sings at work, badly and happily', 'always has a flower tucked in the collar', 'cannot pass the '
             'bakery without buying something', 'argues about wine with anyone', 'keeps a small saint\'s medal in a '
             'pocket', 'counts the steps between the tiers', 'loves a festival more than a feast', 'dresses a little '
             'above the family\'s station']
SF_HOOKS = ['hopes a child will be taken into the palace household',
            'owes the Bank of the Red Roof more than anyone knows',
            'grew up on the wharf and has climbed every tier since',
            'has a petition before the Hall of Petitions that never seems to be heard',
            'believes the Iron Saint once answered a prayer, and has never said which',
            'has a sweetheart in a palazzo on the Heights and a family who must not know',
            'remembers the flood that drowned half the wharf, and watches the river',
            'is certain the King\'s heir will be a better ruler than his father',
            'keeps a shop account book that does not quite add up',
            'is saving for a house one tier higher']


class Folk(R.People):
    """The residents kit, with each city's own names, quirks and histories, and IDs that carry the city prefix."""

    def __init__(self, project, manifest, rng, prefix, female, male, heads, tails, quirks, hooks):
        super().__init__(project, manifest, rng)
        self.prefix, self.female, self.male = prefix, female, male
        self.heads, self.tails, self.quirks, self.hooks = heads, tails, quirks, hooks
        self.ids |= {p['id'] for p in project.get('people', [])}
        self.families = set()

    def family(self):
        for _ in range(500):
            name = self.rng.choice(self.heads) + self.rng.choice(self.tails)
            if name not in self.families:
                self.families.add(name)
                return name
        raise ValueError('ran out of family names')

    def name(self, sex, family=None):
        pool = self.female if sex == 'female' else self.male
        for _ in range(600):
            full = f'{self.rng.choice(pool)} {family or self.family()}'
            if full not in self.names:
                self.names.add(full)
                return full
        raise ValueError('ran out of names')

    def add(self, *, personality=None, backstory=None, name=None, **kw):
        saved_q, saved_h = R.QUIRKS, R.HOOKS
        R.QUIRKS, R.HOOKS = self.quirks, self.hooks
        try:
            if name:
                self.names.add(name)
                self.name = lambda sex, family=None, _n=name: _n
            person = super().add(**kw)
            if name:
                del self.name
        finally:
            R.QUIRKS, R.HOOKS = saved_q, saved_h
        self.ids.discard(person['id'])
        person['id'] = (self.prefix + person['id'])[:48]
        while person['id'] in self.ids:
            person['id'] = person['id'][:46] + '_x'
        self.ids.add(person['id'])
        person['description'] = person['description'].replace('..', '.')
        if personality:                       # A key figure speaks in their own voice, not a stock opening.
            person['personality'] = personality
            person['greeting'] = kw.get('greeting_line') or person['greeting']
        if backstory:
            person['backstory'] = backstory
        return person

    def at(self, point, share=True):
        """A canvas point (the city designer's coordinates) as a place: that tile, or the nearest open one."""
        from .western import OX, OY
        x, y = int(point[0]) + OX, int(point[1]) + OY
        for r in range(0, 48, 4):
            for dx, dy in [(0, 0)] if r == 0 else [(r, 0), (-r, 0), (0, r), (0, -r), (r, r), (-r, -r), (r, -r), (-r, r)]:
                try:
                    return self.world(x + dx, y + dy, share)
                except ValueError:
                    continue
        raise ValueError(f'No open ground near {point}')

    def route_at(self, rid, name, points):
        from .western import OX, OY
        return self.route(self.prefix + rid, name, [(int(x) + OX, int(y) + OY) for x, y in points])


def family_of(house):
    """A family house's family (\"Bellandi House\" -> Bellandi); tenements and halls have none."""
    return house['name'][:-6] if house['kind'] == 'house' and house['name'].endswith(' House') else None


def index(manifest, city):
    return {b['name']: b for b in manifest if b.get('city') == city}


def household_of(P, b):
    return P.bed(b)


def ridgemere(project, manifest, city, world, rng):
    P = Folk(project, manifest, rng, 'rm_', RM_FEMALE, RM_MALE, RM_HEADS, RM_TAILS, RM_QUIRKS, RM_HOOKS)
    B = index(manifest, 'ridgemere')
    homes = [b for b in manifest if b.get('city') == 'ridgemere' and b['kind'] in ('house', 'tenement')
             and not b['district'].startswith('estate_')]
    rng.shuffle(homes)
    homes.sort(key=lambda b: b['kind'] == 'tenement')         # Houses first for the better-off.
    tenements = [b for b in homes if b['kind'] == 'tenement']

    def home_in(pool):
        for b in pool:
            bed = P.bed(b)
            if bed:
                return bed, b
        raise ValueError('Ridgemere has run out of beds')

    taverns = [B[n] for n in ('The Drowned Lantern', 'The Slag & Anchor', 'The Tarred Rope') if n in B]

    def evening():
        if rng.random() < .55 and taverns:
            return P.floor(rng.choice(taverns)['rooms'][0]['id'], share=True)
        return P.at(city.spots['market'])

    rain = 'The rain has not stopped since morning. It never does.'
    # -- The great Houses --------------------------------------------------------------------------------------
    heads = {
        'grayrock': [('Aldric', 'male', 52, 'travelling on House business', 'traveller',
                      'Lord of House Grayrock, always on the road: to Upper Accord, to the quarries, to anyone who '
                      'will buy iron. His family sees him a few days a year.',
                      'Restless, charming and hard as his own stone; he trusts contracts more than people.',
                      'Aldric has not slept a full week in Grayrock Hall in six years; he tells himself the House '
                      'needs him on the road, and half believes it.'),
                     ('Maren', 'female', 47, 'keeping Grayrock Hall', 'home',
                      'Lady of House Grayrock, who runs the Hall, the ledgers and, some say, the House itself while '
                      'her lord is away.',
                      'Patient, sharp-eyed and lonely in a way she would never admit; the Hall is her whole world.',
                      'Maren married into Grayrock at nineteen and has not left the Hall\'s grounds in years; every '
                      'letter from her husband is filed, and none are answered in kind.'),
                     ('Tobias', 'male', 23, 'overseeing the ironworks', 'works', 'the Grayrock heir, soot to the '
                      'elbows', 'Earnest, anxious and better with iron than with people.',
                      'Tobias would rather run the forge than the House, and knows he will have to do both.'),
                     ('Ingrid', 'female', 16, 'at her lessons in the Hall', 'home', 'the Grayrock daughter',
                      'Bright, bored and forever at the window watching the road.',
                      'Ingrid writes to her father every week, and waits for a reply.')],
        'fell': [('Isolde', 'female', 58, 'ruling House Fell', 'home', 'the Lady of House Fell, widowed and '
                  'unbending', 'Unbending, shrewd and cold until she decides you are useful.',
                  'Isolde has ruled Fell since her husband drowned under a log raft, and has never let a debt go.'),
                 ('Corwin', 'male', 31, 'driving the logging crews', 'works', 'the Fell heir, loud and broad as a '
                  'cedar', 'Loud, proud and generous with other people\'s coin.',
                  'Corwin wants the Council to give Fell the eastern forests, and says so in every tavern.'),
                 ('Gudrun', 'female', 29, 'keeping the Fell accounts', 'home', 'Corwin Fell\'s wife, who does the '
                  'sums he does not', 'Quiet, precise and far more ambitious than her husband.',
                  'Gudrun was a Vesk cousin once, and still writes to them more than Isolde knows.')],
        'ashcombe': [('Edric', 'male', 61, 'working the glass himself', 'works', 'Lord of House Ashcombe, who '
                      'still blows glass with his own breath', 'Gentle, stubborn and happiest at the furnace.',
                      'Edric built the glassworks from one furnace and refuses to let anyone else fire the first '
                      'gather of the day.'),
                     ('Solveig', 'female', 57, 'keeping Ashcombe House', 'home', 'Lady of House Ashcombe',
                      'Warm, practical and the only one who can make Edric rest.',
                      'Solveig keeps the House\'s seat on the Council and votes as Edric never bothers to.'),
                     ('Erland', 'male', 26, 'minding the charcoal burns', 'works', 'the Ashcombe heir', 'Watchful, '
                      'soft-spoken and smelling of smoke.', 'Erland spends more nights at the kilns than in his bed.')],
        'brinewater': [('Petra', 'female', 44, 'ruling House Brinewater', 'home', 'Lady of House Brinewater, '
                        'shipbuilder and seat on the Council', 'Brisk, ambitious and unafraid of the other Houses.',
                        'Petra means to build a fleet, and every other House suspects what for.'),
                       ('Leif', 'male', 46, 'walking the ropewalk', 'works', 'Petra Brinewater\'s consort, who '
                        'keeps the ropewalk', 'Easygoing, meticulous and ignored at Council, which he prefers.',
                        'Leif was a ropemaker\'s son; he still twists the first rope of the season himself.'),
                       ('Nessa', 'female', 19, 'learning the shipyard', 'quay', 'the Brinewater heir', 'Fearless, '
                        'blunt and forever tar-stained.', 'Nessa has already designed a ship her mother will not '
                        'let her build.')],
        'vesk': [('Magnus', 'male', 56, 'counting the catch', 'quay', 'Lord of House Vesk, who owns the boats, '
                  'the smoke and the salt', 'Genial, greedy and never without a fish joke.',
                  'Magnus is quietly buying the Sump\'s tenements, one by one.'),
                 ('Thyra', 'female', 52, 'keeping Vesk Manor', 'home', 'Lady of House Vesk', 'Pious, careful and '
                  'kind to servants.', 'Thyra funds the Soup Kitchen, and wants the Sump to know it.'),
                 ('Sten', 'male', 24, 'running the salt pans', 'works', 'the Vesk heir', 'Dour, loyal and '
                  'salt-crusted.', 'Sten wants to marry out of the fish trade and into a quieter House.'),
                 ('Una', 'female', 12, 'playing in the Vesk gardens', 'home', 'the youngest Vesk', 'Curious and '
                  'loud.', 'Una knows every hiding place in Vesk Manor.')],
    }
    journey = None
    for house, family in heads.items():
        seat = B[HOUSES[house][4]]
        hall, chambers, below = (r['id'] for r in seat['rooms'])
        estate = f'estate_{house}'
        works = [b for b in manifest if b.get('city') == 'ridgemere' and b['district'] == f'{estate}_works'
                 and b['kind'] == 'works']
        quayworks = {'brinewater': 'The Brinewater Yard', 'vesk': 'The Fish Hall'}.get(house)
        surname = house.title()
        for first, sex, age, label, where, role_text, personality, backstory in family:
            person_name = f'{first} {surname}'
            bed = P.bed(seat)
            if where == 'traveller':
                if journey is None:
                    journey = grayrock_journey(P, world, city)
                P.add(role='civilian', work_label=label, sex=sex, age=age, family=surname, home=bed,
                      work=P.at(city.spots.get('estate_grayrock_gate', city.spots['market'])), evening=bed,
                      hours=(5, 21), route=journey, name=person_name, job=role_text, paid=False, purse=400,
                      meals=6, greeting_line='I have an hour, perhaps less. The road does not wait.',
                      personality=personality, backstory=backstory)
                continue
            if where == 'home':
                work = P.floor(hall) if age > 20 else P.floor(chambers)
            elif where == 'quay' and quayworks and quayworks in B:
                work = P.work(B[quayworks])
            elif works:
                work = P.work(works[0])
            else:
                work = P.floor(hall)
            P.add(role='civilian', work_label=label, sex=sex, age=age, family=surname, home=bed, work=work,
                  evening=P.floor(chambers, share=True), hours=(7, 20) if where == 'home' else (6, 18),
                  name=person_name, job=role_text, paid=False, purse=250, personality=personality,
                  backstory=backstory, greeting_line=f'You stand in the House of {surname}. Be brief.')
        # The steward and the servants of the seat.
        P.add(role='civilian', work_label=f'stewarding {HOUSES[house][4]}'[:40], home=P.bed(seat) or P.floor(below),
              work=P.work(seat), evening=P.floor(below, share=True), hours=(6, 20), age=rng.randint(40, 65),
              job=f'the steward of House {surname}', greeting_line='The family is not receiving. I can take a message.')
        for label in ('cooking for the House', 'serving in the hall', 'keeping the chambers'):
            P.add(role='civilian', work_label=label, home=P.bed(seat) or P.floor(below, share=True),
                  work=P.floor(below if 'cooking' in label else hall if 'hall' in label else chambers),
                  evening=P.floor(below, share=True), hours=(6, 21), job=f'a servant of House {surname}',
                  greeting_line='Mind the floors, they have just been done.')
    # The Houses' workforces.
    workforce = {
        'grayrock': [('The Grayrock Ironworks', 'at the forge', 5, 'a Grayrock forgehand, singed and deaf in one ear'),
                     ('quarry', 'cutting stone in the quarry', 5, 'a Grayrock quarryman grey with dust'),
                     ('The Quarry Office', 'foreman of the quarry', 1, 'the Grayrock quarry foreman')],
        'fell': [('The Fell Sawmill', 'working the saws', 5, 'a Fell sawyer with sawdust in the ruff'),
                 ('logyard', 'hauling logs in the yard', 3, 'a Fell logger back from the camps')],
        'ashcombe': [('The Ashcombe Glassworks', 'blowing glass', 3, 'an Ashcombe glassblower with scarred paws'),
                     ('kilns', 'tending the charcoal kilns', 3, 'an Ashcombe charcoal burner black to the eyes')],
        'brinewater': [('The Brinewater Yard', 'building ships', 4, 'a Brinewater shipwright smelling of tar'),
                       ('The Brinewater Ropewalk', 'twisting rope', 3, 'a Brinewater ropemaker')],
        'vesk': [('Vesk Smokehouse', 'smoking fish', 3, 'a Vesk smoker, eyes red from the fires'),
                 ('Vesk Smokehouse by the Stair', 'smoking fish', 2, 'a Vesk smoker'),
                 ('The Vesk Salt Store', 'raking salt', 2, 'a Vesk salt raker with cracked pads')],
    }
    for house, jobs in workforce.items():
        estate = f'estate_{house}'
        lodgings = [b for b in manifest if b.get('city') == 'ridgemere' and b['district'] == f'{estate}_works'
                    and b['kind'] in ('barracks', 'house', 'tenement')]
        for place, label, count, job in jobs:
            for _ in range(count):
                bed = None
                for b in lodgings:
                    bed = P.bed(b)
                    if bed:
                        break
                if not bed:
                    bed, _ = home_in(tenements)                   # The rest walk out from the Sump every day.
                if place in B:
                    work = P.work(B[place])
                else:
                    spot = city.spots.get({'quarry': f'{estate}_quarry', 'logyard': f'{estate}_logyard',
                                           'kilns': f'{estate}_kilns'}[place])
                    work = P.at((spot[0] + rng.randint(-6, 6), spot[1] + rng.randint(-5, 5)), share=False)
                P.add(role='civilian', work_label=label, home=bed, work=work, evening=evening(), hours=(6, 18),
                      job=job, greeting_line=f'House {house.title()} work. Hard, and it pays. Mostly.',
                      species=rng.choice(['timber', 'timber', 'arctic', 'red']))
    # -- The Council and the city's officers ---------------------------------------------------------------------
    officers = [('The Rain Hall', 'presiding over the Council', 'the Chancellor of Ridgemere, who serves the Houses '
                 'and belongs to none', 'The Houses are in session. Whether they agree on anything is another matter.'),
                ("The Magistrate's Court", 'hearing cases', 'the magistrate of Ridgemere, grey and tired',
                 'Guilty or not, you will be heard. That is all I promise.'),
                ("The Magistrate's Court", 'keeping order in court', 'the court bailiff', 'Quiet in the court.'),
                ('The Gaol', 'keeping the gaol', 'the gaoler', 'Visiting, or staying?'),
                ('The Hall of Tallies', 'tallying the Houses\' dues', 'a tally clerk', 'Every coin is counted twice.'),
                ('The Hall of Tallies', 'copying the rolls', 'a tally clerk', 'Name and House, please.'),
                ('The Customs House', 'taxing the cargoes', 'the Customs officer', 'Declare it, or lose it.'),
                ('The Customs House', 'weighing bales', 'a customs tallyman', 'Stand clear of the scales.'),
                ("The Harbourmaster's Office", 'keeping the harbour', 'the harbourmaster', 'Berths are full. Always.'),
                ('The Chapel of the Tide', 'keeping the chapel', 'the chaplain of the Tide',
                 'Light a candle for them. The sea listens, sometimes.'),
                ('The Sump Infirmary', 'tending the sick', 'a Sump healer', 'Sit. Cough. Again.'),
                ('The Sump Infirmary', 'boiling rags', 'a Sump healer\'s helper', 'Mind the pots.')]
    for building, label, job, line in officers:
        if building not in B:
            continue
        bed, _ = home_in(homes)
        P.add(role='civilian', work_label=label, home=bed, work=P.work(B[building]), evening=evening(), hours=(8, 18),
              job=job, greeting_line=f'{line} {rain}' if rng.random() < .2 else line, age=rng.randint(30, 66))
    # The Soup Kitchen and the taverns sell food and drink.
    for building, label, job in (('The Soup Kitchen', 'ladling soup', 'the Soup Kitchen cook'),
                                 ('The Drowned Lantern', 'keeping the taproom', 'the keeper of the Drowned Lantern'),
                                 ('The Slag & Anchor', 'keeping the taproom', 'the keeper of the Slag & Anchor'),
                                 ('The Rainbarrel Inn', 'keeping the inn', 'the innkeeper of the Rainbarrel'),
                                 ('The Tarred Rope', 'keeping the taproom', 'the keeper of the Tarred Rope')):
        if building not in B:
            continue
        bed, _ = home_in(homes)
        P.add(role='merchant', work_label=label, home=bed, work=P.work(B[building]),
              evening=P.floor(B[building]['rooms'][0]['id'], share=True), hours=(10, 23.5) if 'tap' in label else (6, 20),
              job=job, greeting_line=f'Dry yourself by the fire. {rain.split(".")[0]}.')
    # Shopkeepers and their households.
    for b in [b for b in manifest if b.get('city') == 'ridgemere' and b['kind'] == 'shop']:
        bed, house = home_in(homes)
        keeper = P.add(role='merchant', work_label=f'keeping {b["name"]}'[:40], home=bed, work=P.work(b),
                       family=family_of(house),
                       evening=P.floor(house['rooms'][0]['id'], share=True), hours=(7, 19), job=f'who keeps {b["name"]}',
                       greeting_line=f'{b["name"]}. Shut the door behind you, the rain gets in.')
        if rng.random() < .5:
            family = keeper['name'].split()[-1]
            kin = P.bed(house)
            if kin:
                P.add(role='civilian', work_label=f'helping at {b["name"]}'[:40], family=family, home=kin,
                      work=P.floor(b['rooms'][0]['id']), evening=evening(), hours=(8, 18),
                      job=f'who helps in the family shop')
    # The City Watch.
    watch_house = B['The Watch House']
    ring_pts = wall_walk(city)
    wall_route = P.route_at('wall_walk', 'Wall walk', ring_pts)
    quay_route = P.route_at('quay_patrol', 'Quay patrol', quay_walk(city))
    P.add(role='guard', work_label='commanding the Watch', home=P.bed(watch_house), work=P.work(watch_house),
          evening=P.floor(watch_house['rooms'][0]['id'], share=True), hours=(7, 19), age=rng.randint(40, 55),
          job='the Captain of the Ridgemere Watch', greeting_line='The Watch keeps the walls. The Houses keep the '
          'rest. Trouble?')
    for route, hours in ((wall_route, (6, 18)), (wall_route, (18, 6)), (quay_route, (6, 18)), (quay_route, (18, 6)),
                         (wall_route, (6, 18)), (quay_route, (6, 18))):
        bed = P.bed(watch_house)
        P.add(role='guard', work_label='on night patrol' if hours[0] == 18 else 'on patrol', route=route, home=bed,
              work=P.routes[-1]['posts'][0], evening=bed, hours=hours, job='a Watch guard in a sodden cloak',
              greeting_line='Keep moving. Keep dry, if you can.')
    for gate in ('south_gate', 'east_gate', 'quay_gate'):
        spot = city.spots.get(gate)
        if not spot:
            continue
        bed = P.bed(watch_house)
        P.add(role='guard', work_label=f'holding the {gate.replace("_", " ")}', home=bed, work=P.at(spot, share=False),
              evening=bed, hours=(6, 18), job='a gate guard of the Watch', greeting_line='Name and business.')
    # The Sump.
    sump_jobs = [('unloading ships on the quay', 6, 'quay', 'a dockhand with rope-burned paws'),
                 ('washing clothes by the quay', 2, 'quay', 'a washerwoman of the Sump'),
                 ('picking rags', 2, 'market', 'a rag-picker'),
                 ('begging in the market', 2, 'market', 'a beggar of the Sump'),
                 ('running errands', 3, 'market', 'a Sump child quick with messages')]
    for label, count, where, job in sump_jobs:
        for _ in range(count):
            bed, _ = home_in(tenements)
            spot = city.spots[where]
            child = 'child' in job
            P.add(role='civilian', work_label=label, home=bed,
                  work=P.at((spot[0] + rng.randint(-12, 12), spot[1] + rng.randint(-10, 10)), share=False),
                  evening=evening() if not child else bed, hours=(7, 19), paid='beg' not in label and not child,
                  purse=rng.randint(0, 8), age=rng.randint(7, 12) if child else None, job=job,
                  greeting_line='Spare a copper? No? Then what do you want?' if 'beg' in label else
                  'What is it? I have work.', sex='female' if 'wash' in label else None)
    return P


def grayrock_journey(P, world, city):
    """Lord Grayrock's road: from his estate gate along the Grayrock Road and the Accord Road to Upper Accord's
    Main Gate, and back again."""
    from . import upper_accord as UA
    from .western import OX, OY
    line = world.road_lines.get('The Grayrock Road', []) + world.road_lines.get('The Accord Road', [])
    pts, last = [], None
    for x, y in line:
        if last is None or ((x - last[0]) ** 2 + (y - last[1]) ** 2) ** .5 >= 70:
            pts.append((int(x) + OX, int(y) + OY))
            last = (x, y)
    ua = [(x, y) for x, y in UA.MAIN_ROAD[1:]][:4]
    out = pts[:28] + ua
    posts = out + list(reversed(out[1:-1]))
    rid = P.prefix + 'grayrock_journey'
    places = []
    for x, y in posts[:64]:
        try:
            places.append(P.world(x, y))
        except ValueError:
            continue                              # Past the world's edge (a world without Upper Accord).
    P.routes.append({'id': rid, 'name': 'Lord Grayrock\'s road', 'posts': places})
    return rid


def wall_walk(city):
    return city.spots.get('wall_walk', [])


def quay_walk(city):
    return city.spots.get('quay_walk', [])


def ser_ferro(project, manifest, city, world, rng):
    P = Folk(project, manifest, rng, 'sf_', SF_FEMALE, SF_MALE,
             ['Bel', 'Cas', 'Dar', 'Fer', 'Gal', 'Lan', 'Mor', 'Ner', 'Ors', 'Ros', 'Sal', 'Tor', 'Val', 'Ven', 'Mar',
              'Col', 'Ben', 'Pal', 'Riv', 'Cor'],
             ['andi', 'elli', 'ano', 'ante', 'etti', 'ini', 'ucci', 'one', 'esi', 'ari', 'otti', 'ieri', 'uzzi', 'ale'],
             SF_QUIRKS, SF_HOOKS)
    B = index(manifest, 'ser_ferro')
    houses = [b for b in manifest if b.get('city') == 'ser_ferro' and b['kind'] == 'house'
              and b['name'] != 'The Clergy House']
    rng.shuffle(houses)
    tenements = [b for b in manifest if b.get('city') == 'ser_ferro' and b['kind'] == 'tenement']

    def home_in(pool):
        for b in pool:
            bed = P.bed(b)
            if bed:
                return bed, b
        raise ValueError('Ser Ferro has run out of beds')

    taverns = [B[n] for n in ('The Sunlit Cup', 'The Three Bells', 'The Golden Sheaf Inn', 'The Muddy Oar') if n in B]

    def evening():
        if rng.random() < .5 and taverns:
            return P.floor(rng.choice(taverns)['rooms'][0]['id'], share=True)
        return P.at((city.spots['plaza'][0] + rng.randint(-14, 14), city.spots['plaza'][1] + rng.randint(-8, 8)))

    # -- The palace and the court ------------------------------------------------------------------------------
    pal = B['The Palace of Ser Ferro']
    court, royal, kitchens = (r['id'] for r in pal['rooms'])
    royals = [('Aurelio', 'male', 58, 'holding court', 'King of Ser Ferro, of the House of Castellane',
               'Magnificent, vain and genuinely devoted to his city; he remembers every slight and every kindness.',
               'Aurelio has ruled for thirty years and rebuilt the cathedral\'s great windows at his own cost; he '
               'fears his heir is too clever and his second son not clever enough.'),
              ('Livia', 'female', 54, 'receiving at court', 'Queen of Ser Ferro, his consort',
               'Gracious, watchful and the finest diplomat in the palace.',
               'Livia came from the Valmonte palazzo on the Heights and has never let the other families forget it.'),
              ('Cassio', 'male', 28, 'attending the court', 'the King\'s heir',
               'Clever, restless and impatient with ceremony.',
               'Cassio corresponds with Ridgemere\'s Houses in secret; he thinks Ser Ferro should buy their iron and '
               'their ships.'),
              ('Serafina', 'female', 19, 'at her studies in the palace', 'the King\'s daughter',
               'Bookish, stubborn and adored by the city.', 'Serafina slips out to the wharf in plain clothes, and '
               'half the guard knows it.'),
              ('Matteo', 'male', 11, 'playing in the palace gardens', 'the youngest prince', 'Mischievous and brave.',
               'Matteo has named every statue in the gardens.')]
    for first, sex, age, label, role_text, personality, backstory in royals:
        bed = P.bed(pal)
        work = P.work(pal) if age > 25 else P.at((city.spots['forecourt'][0] + rng.randint(4, 10),
                                                   city.spots['forecourt'][1] - rng.randint(4, 10)), share=False)
        P.add(role='civilian', work_label=label, sex=sex, age=age, family='di Castellane', home=bed, work=work,
              evening=P.floor(royal, share=True), hours=(9, 19), name=f'{first} di Castellane', job=role_text,
              paid=False, purse=1000, personality=personality, backstory=backstory,
              greeting_line='You are in the presence of the Crown of Ser Ferro.')
    court_folk = [('The Hall of Petitions', 'hearing petitions', 'Ottavia Lanza', 'female', 50,
                   'the Chancellor of Ser Ferro, who hears the city\'s petitions for the King'),
                  (None, 'announcing the court', None, None, None, 'the herald of the court, with a voice like a bell'),
                  (None, 'attending the King', None, None, None, 'a courtier in red silk'),
                  (None, 'attending the Queen', None, None, None, 'a lady of the court'),
                  (None, 'gossiping at court', None, None, None, 'a courtier with opinions on everyone')]
    for building, label, name, sex, age, job in court_folk:
        bed, _ = home_in(houses)
        P.add(role='civilian', work_label=label, home=bed, sex=sex, age=age, name=name,
              work=P.work(B[building]) if building else P.floor(court), evening=evening(), hours=(9, 18), job=job,
              greeting_line='The court is in session. Your name?')
    P.add(role='merchant', work_label='cooking for the palace', home=P.bed(pal), work=P.work(pal),
          evening=P.floor(kitchens, share=True), hours=(5, 20), job='the palace cook, ruler of the kitchens',
          greeting_line='Out of my kitchen, unless you are here to carry something.')
    for label in ('serving the royal table', 'keeping the royal apartments', 'scrubbing the kitchens',
                  'polishing the throne room', 'running errands for the household'):
        bed = P.bed(pal)
        P.add(role='civilian', work_label=label, home=bed or P.floor(kitchens), work=P.floor(
            court if 'throne' in label or 'table' in label else royal if 'apart' in label else kitchens),
            evening=P.floor(kitchens, share=True), hours=(6, 21), job='a servant of the palace household',
            greeting_line='The household is busy. Always.')
    guardhouse = B['The Palace Guardhouse']
    fx, fy = city.spots['forecourt']
    palace_round = P.route_at('palace_round', 'Palace grounds round',
                              [(fx, fy + 8), (fx + 14, fy), (fx + 14, fy - 30), (fx - 6, fy - 40), (fx - 12, fy - 10)])
    P.add(role='guard', work_label='commanding the palace guard', home=P.bed(guardhouse), work=P.work(guardhouse),
          evening=P.floor(guardhouse['rooms'][0]['id'], share=True), hours=(7, 19), age=48,
          job='the Captain of the Palace Guard, in a red cloak and polished steel',
          greeting_line='The palace is closed to petitioners today. It is always closed to petitioners today.')
    for hours in ((6, 18), (18, 6), (6, 18), (18, 6), (6, 18), (6, 18)):
        bed = P.bed(guardhouse)
        P.add(role='guard', work_label='guarding the palace', route=palace_round, home=bed,
              work=P.at((fx, fy + 8)), evening=bed, hours=hours, job='a palace guard in a red cloak',
              greeting_line='Stand back from the gate.')
    if 'The Chapel Royal' in B:
        bed, _ = home_in(houses)
        P.add(role='civilian', work_label='keeping the Chapel Royal', home=bed, work=P.work(B['The Chapel Royal']),
              evening=P.floor(B['The Chapel Royal']['rooms'][0]['id'], share=True), hours=(7, 19),
              job='the royal chaplain', greeting_line='The Saint keeps the King. The King keeps the rest of us.')
    # -- The cathedral ---------------------------------------------------------------------------------------------
    cath = B['The Cathedral of the Iron Saint']
    nave, crypt = (r['id'] for r in cath['rooms'])
    clergy = B['The Clergy House']
    P.add(role='civilian', work_label='celebrating the Mass', home=P.bed(clergy), work=P.work(cath),
          evening=P.floor(nave, share=True), hours=(6, 20), age=66, sex='male', name='Benedetto Albani',
          job='the Archprelate of the Iron Saint, white-robed and ancient',
          personality='Serene, formidable and quietly political; he has buried two Kings and crowned one.',
          backstory='Benedetto was a wharf child who swept the cathedral steps; he has never forgotten the wharf, '
                    'and the wharf has never forgotten him.',
          greeting_line='Peace of the Iron Saint upon you. Sit, if your legs are tired.')
    for label, where, job in (('hearing confessions', nave, 'a priest of the Iron Saint'),
                              ('leading the choir', nave, 'the cathedral\'s choirmaster'),
                              ('keeping the crypt', crypt, 'the sexton of the crypt'),
                              ('lighting the candles', nave, 'a cathedral acolyte'),
                              ('copying the chapter\'s books', B['The Chapter House']['rooms'][0]['id'],
                               'a cathedral scribe')):
        bed = P.bed(clergy)
        if not bed:
            bed, _ = home_in(houses)
        P.add(role='civilian', work_label=label, home=bed, work=P.floor(where), evening=P.floor(nave, share=True),
              hours=(6, 19), job=job, greeting_line='The Saint\'s blessing on you.',
              age=rng.randint(16, 22) if 'acolyte' in job else None)
    for label in ('nursing the sick', 'nursing the sick'):
        bed, _ = home_in(houses)
        P.add(role='civilian', work_label=label, sex='female', home=bed, work=P.work(B['The House of Saint Chiara']),
              evening=P.floor(nave, share=True), hours=(7, 19), job='a sister of Saint Chiara in white linen',
              greeting_line='Are you hurt? No? Then someone else needs this bed.')
    # -- The Heights: the elite families --------------------------------------------------------------------------
    for family in ELITE_FAMILIES:
        palazzo = B.get(f'Palazzo {family}')
        if not palazzo:
            continue
        salon, upper = (r['id'] for r in palazzo['rooms'])
        for role_text, sex, age in (('the head of the {f} family', 'male', rng.randint(45, 68)),
                                    ('of the {f} family, who runs its alliances', 'female', rng.randint(40, 62)),
                                    ('the {f} heir', rng.choice(['male', 'female']), rng.randint(16, 30))):
            bed = P.bed(palazzo)
            if not bed:
                break
            P.add(role='civilian', work_label='receiving guests' if age > 35 else 'idling in the salon', sex=sex,
                  age=age, family=family, home=bed, work=P.floor(salon), evening=P.floor(
                      B["The Merchants' Guildhall"]['rooms'][0]['id'], share=True) if rng.random() < .3 else
                  P.floor(salon, share=True), hours=(10, 19), job=role_text.format(f=family), purse=600,
                  greeting_line=f'Welcome to Palazzo {family}. Do you have an appointment?')
        for _ in range(2):
            bed = P.bed(palazzo)
            if not bed:
                break
            P.add(role='civilian', work_label='serving the household', home=bed, work=P.work(palazzo),
                  evening=P.floor(salon, share=True), hours=(6, 21), job=f'a servant of the {family} family',
                  greeting_line='The family is not at home. They are never at home to strangers.')
    # -- The guilds, the shops and the middle town ---------------------------------------------------------------
    for building, label, job in (("The Merchants' Guildhall", 'presiding over the guild', 'the guildmaster of the '
                                  'Merchants\' Guild'),
                                 ("The Merchants' Guildhall", 'keeping the guild books', 'the guild\'s clerk'),
                                 ('The Wool Guild Hall', 'arguing the price of wool', 'the master of the Wool Guild'),
                                 ('The Wool Guild Hall', 'grading fleeces', 'a wool grader'),
                                 ('The River Office', 'tallying barges', 'the river harbourmaster')):
        if building not in B:
            continue
        bed, _ = home_in(houses)
        P.add(role='civilian', work_label=label, home=bed, work=P.work(B[building]), evening=evening(), hours=(8, 18),
              job=job, greeting_line='Business first. Always business first.')
    for building, label, job in (('The Sunlit Cup', 'keeping the taproom', 'the keeper of the Sunlit Cup'),
                                 ('The Three Bells', 'keeping the taproom', 'the landlord of the Three Bells'),
                                 ('The Golden Sheaf Inn', 'keeping the inn', 'the innkeeper of the Golden Sheaf'),
                                 ('The Muddy Oar', 'keeping the taproom', 'the keeper of the Muddy Oar'),
                                 ('The Fish Market', 'selling river fish', 'a fishwife of the wharf')):
        if building not in B:
            continue
        bed, _ = home_in(houses if 'Muddy' not in building and 'Fish' not in building else tenements + houses)
        P.add(role='merchant', work_label=label, home=bed, work=P.work(B[building]),
              evening=P.floor(B[building]['rooms'][0]['id'], share=True), hours=(10, 23.5) if 'tap' in label else (6, 20),
              job=job, greeting_line='Sit, sit! Wine? Bread? Both?')
    for b in [b for b in manifest if b.get('city') == 'ser_ferro' and b['kind'] == 'shop']:
        bed, house = home_in(houses)
        keeper = P.add(role='merchant', work_label=f'keeping {b["name"]}'[:40], home=bed, work=P.work(b),
                       family=family_of(house),
                       evening=P.floor(house['rooms'][0]['id'], share=True), hours=(7, 19), job=f'who keeps {b["name"]}',
                       greeting_line=f'Welcome to {b["name"]}! Everything here is the finest in the city.')
        family = keeper['name'].split()[-1]
        for _ in range(rng.choice([0, 1, 1, 2])):
            kin = P.bed(house)
            if not kin:
                break
            young = rng.random() < .35
            if young:
                P.add(role='civilian', work_label='playing by the fountain', family=family, home=kin,
                      work=P.at((city.spots['plaza'][0] + rng.randint(-12, 12), city.spots['plaza'][1] + rng.randint(4, 9)),
                                share=False), evening=kin, hours=(9, 16), age=rng.randint(5, 11), paid=False, purse=0,
                      job='a child of the household')
            else:
                P.add(role='civilian', work_label=f'helping at {b["name"]}'[:40], family=family, home=kin,
                      work=P.floor(b['rooms'][0]['id']), evening=evening(), hours=(8, 18), job='who helps in the '
                      'family shop')
    crafts = [('painting on the cathedral square', 'plaza', 'a painter who sells views of the cathedral'),
              ('playing the lute on the square', 'plaza', 'a street musician'),
              ('selling flowers on the square', 'plaza', 'a flower seller from the golden fields'),
              ('trading at the market', 'market', 'a market trader'),
              ('trading at the market', 'market', 'a market trader'),
              ('buying for a palazzo kitchen', 'market', 'a cook from the Heights with a basket'),
              ('carrying water', 'market', 'a water carrier'),
              ('sweeping the Processional Way', 'plaza', 'a street sweeper of the city')]
    for label, where, job in crafts:
        bed, _ = home_in(houses)
        spot = city.spots[where]
        P.add(role='civilian', work_label=label, home=bed,
              work=P.at((spot[0] + rng.randint(-12, 12), spot[1] + rng.randint(-8, 8)), share=False),
              evening=evening(), hours=(8, 18), job=job, greeting_line='A fine day in Ser Ferro, is it not?')
    # -- The city guard ------------------------------------------------------------------------------------------
    barracks = B['The City Guard Barracks']
    px, py = city.spots['plaza']
    mx, my = city.spots['market']
    wx, wy = city.spots['wharf']
    lower_round = P.route_at('lower_round', 'Lower town round', [(mx, my), (mx - 30, my - 40), (mx + 20, my - 60),
                                                                 (px - 20, py), (mx + 30, my + 30)])
    cathedral_round = P.route_at('cathedral_round', 'Cathedral round', [(px, py + 12), (px + 25, py), (px, py - 30),
                                                                         (px - 25, py)])
    wharf_watch = P.route_at('wharf_watch', 'Wharf watch', [(wx, wy), (wx + 50, wy - 10), (wx + 90, wy - 30),
                                                              (wx - 40, wy + 4)])
    P.add(role='guard', work_label='commanding the city guard', home=P.bed(barracks), work=P.work(barracks),
          evening=P.floor(barracks['rooms'][0]['id'], share=True), hours=(7, 19), age=rng.randint(42, 55),
          job='the Captain of the City Guard', greeting_line='Ser Ferro is a city of order. Keep it that way.')
    for route, hours in ((lower_round, (6, 18)), (lower_round, (18, 6)), (cathedral_round, (6, 18)),
                         (cathedral_round, (18, 6)), (wharf_watch, (6, 18)), (wharf_watch, (18, 6)),
                         (wharf_watch, (6, 18)), (lower_round, (6, 18))):
        bed = P.bed(barracks)
        P.add(role='guard', work_label='on night patrol' if hours[0] == 18 else 'on patrol', route=route, home=bed,
              work=P.at(city.spots['plaza']), evening=bed, hours=hours, job='a city guard in a red tabard',
              greeting_line='Move along, citizen. Or visitor.')
    for gate in ('north_gate', 'east_gate'):
        spot = city.spots.get(gate)
        if spot:
            bed = P.bed(barracks)
            P.add(role='guard', work_label=f'holding the {gate.replace("_", " ")}', home=bed,
                  work=P.at(spot, share=False), evening=bed, hours=(6, 18), job='a gate guard of the city',
                  greeting_line='Welcome to Ser Ferro. Keep your blade sheathed within the walls.')
    # -- The wharf -------------------------------------------------------------------------------------------------
    for label, count, job in (('unloading barges', 6, 'a bargeman of the wharf'),
                              ('mending nets on the wharf', 3, 'a river fisher'),
                              ('hauling for the warehouses', 3, 'a wharf porter'),
                              ('begging by the cathedral steps', 1, 'a beggar of the wharf'),
                              ('washing linen at the river stairs', 2, 'a laundress of the wharf'),
                              ('running about the wharf', 3, 'a wharf child')):
        for _ in range(count):
            bed, _ = home_in(tenements + houses)
            child = 'child' in job
            spot = city.spots['plaza'] if 'cathedral' in label else city.spots['wharf']
            P.add(role='civilian', work_label=label, home=bed,
                  work=P.at((spot[0] + rng.randint(-30, 30), spot[1] + rng.randint(-4, 4)), share=False),
                  evening=evening() if not child else bed, hours=(6, 18), paid='beg' not in label and not child,
                  purse=rng.randint(0, 10), age=rng.randint(6, 12) if child else None, job=job,
                  sex='female' if 'laundress' in job else None,
                  greeting_line='Spare something, for the Saint\'s sake?' if 'beg' in label else 'Mind your feet, '
                  'the planks are rotten.')
    return P


def populate(project, manifest, world, seed=SEED):
    """Both cities' people and routes, added to the project's own."""
    rng = random.Random(seed)
    by_city = {c.region: c for c in world.cities}
    people, routes = [], []
    base = dict(project)
    for build in (ridgemere, ser_ferro):
        city = by_city['ridgemere' if build is ridgemere else 'ser_ferro']
        P = build({**base, 'people': base.get('people', []) + people}, manifest, city, world, rng)
        people += P.people
        routes += P.routes
    return people, routes
