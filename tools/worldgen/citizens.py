"""The people of Ridgemere (Docs/Design/60): homes, work, evenings and a line of story for each, in the buildings the
city designer (cities.py) made for them.

The four great Houses (Greatly inside the walls; Fell, Grayrock and Mossgrave on their estates outside), with their
stewards, servants and estate staff; the Council of Houses, the City Office and the city's officers; the City Watch;
the wealthy households of the Heights; shopkeepers over their shops and stallholders on the three market squares;
the masters and hands of the works; the wharf's dockhands, fishers and sailors; and the poor of the Sump. Lord
Grayrock travels the High Road to the mountain mines and is almost never home; Lady Grayrock almost never leaves it.
More townsfolk fill the city's free beds afterwards (worldgen.towns.fill_city).
"""
from __future__ import annotations

import random

from . import residents as R
from .buildings import WORKS_TRADE, keeper_label
from .cities import HOUSES, MARKETS

SEED = 5151
FACTIONS = [
    {'id': 'council_of_houses', 'name': 'The Council of Houses', 'color': '#7d8a96'},
    {'id': 'ridgemere_watch', 'name': 'The Ridgemere Watch', 'color': '#5d6b4f'},
    {'id': 'house_greatly', 'name': 'House Greatly', 'color': '#2f4f6f'},
    {'id': 'house_fell', 'name': 'House Fell', 'color': '#4f7a4a'},
    {'id': 'house_grayrock', 'name': 'House Grayrock', 'color': '#8e8e8a'},
    {'id': 'house_mossgrave', 'name': 'House Mossgrave', 'color': '#7a8a3a'},
]

RM_FEMALE = ['Maren', 'Isolde', 'Greta', 'Hilde', 'Bryn', 'Edda', 'Ottilie', 'Sigrun', 'Tove', 'Wenna', 'Agna', 'Brenna',
             'Corra', 'Dagny', 'Elspet', 'Frida', 'Gudrun', 'Hanne', 'Ingrid', 'Jorunn', 'Kari', 'Liv', 'Magda', 'Nessa',
             'Orla', 'Petra', 'Runa', 'Solveig', 'Thyra', 'Ulla', 'Vigdis', 'Yrsa', 'Ada', 'Beatha', 'Clemence',
             'Dorcas', 'Ebba', 'Gwen', 'Hester', 'Mab', 'Aud', 'Berit', 'Disa', 'Grete', 'Ilse', 'Karin', 'Lotte',
             'Moll', 'Nan', 'Oda', 'Pim', 'Rika', 'Signe', 'Tilde', 'Una', 'Wynn', 'Agnes', 'Bettany', 'Cressida',
             'Delphine', 'Eleanor', 'Florence', 'Harriet', 'Imogen', 'Josephine', 'Lavinia', 'Millicent', 'Philippa',
             'Rosamund', 'Sybil', 'Theodora', 'Winifred']
RM_MALE = ['Aldric', 'Brand', 'Corwin', 'Dagfinn', 'Edric', 'Fenwick', 'Gunnar', 'Halvard', 'Ivo', 'Jarl', 'Kjell',
           'Leif', 'Magnus', 'Njal', 'Osric', 'Piet', 'Ragnar', 'Sten', 'Torvald', 'Ulf', 'Vidar', 'Wystan', 'Egil',
           'Hakon', 'Orm', 'Tobias', 'Ansel', 'Bertil', 'Caspar', 'Dunstan', 'Emmerich', 'Gerrit', 'Hollis', 'Jasper',
           'Lothar', 'Mathis', 'Nils', 'Oskar', 'Roald', 'Silas', 'Arne', 'Bo', 'Dirk', 'Erland', 'Finn', 'Gorm',
           'Hjalte', 'Ivar', 'Knut', 'Lars', 'Mogens', 'Olaf', 'Palle', 'Rune', 'Soren', 'Tage', 'Ambrose', 'Bartholomew',
           'Cedric', 'Desmond', 'Edmund', 'Frederick', 'Godfrey', 'Humphrey', 'Lionel', 'Montague', 'Percival',
           'Rupert', 'Septimus', 'Tristram']
RM_HEADS = ['Soot', 'Tar', 'Brine', 'Rust', 'Slag', 'Cinder', 'Wet', 'Grey', 'Mud', 'Kelp', 'Ash', 'Coal', 'Salt',
            'Rope', 'Plank', 'Nail', 'Mill', 'Rain', 'Drip', 'Fog', 'Gull', 'Pitch', 'Wick', 'Bilge']
RM_TAILS = ['wick', 'by', 'well', 'ford', 'hand', 'stack', 'worth', 'ley', 'gate', 'more', 'ridge', 'cot', 'ham', 'ton',
            'marsh', 'dale']
RM_QUIRKS = ['coughs in the damp and pretends not to', 'never goes out without an oilcloth hood', 'can name every ship '
             'in the Sound', 'keeps a pocketful of coal for luck', 'hums sea songs under the breath', 'counts the '
             'bells from the Chapel of the Tide', 'trusts no one from a rival House', 'remembers every debt, owed or '
             'owing', 'watches the rain as if it might stop', 'smells faintly of smoke whatever the weather',
             'swears by the forge and the sea', 'carries a wet-stone whetting knife everywhere',
             'knows which streets flood first', 'takes the long stairs two at a time']
RM_HOOKS = ['owes a House factor money and has been told to find it by the new moon',
            'lost a brother to the quarry and does not forgive House Grayrock for it',
            'dreams of a berth on a Greatly ship bound somewhere dry',
            'keeps a secret list of which Houses pay the Watch to look away',
            'was born in the Sump and means to die somewhere better, the Heights for preference',
            'has a cousin in the Fell logging camps who writes when he can',
            'believes the Houses will fall out with each other before the next winter',
            'once saw the Chancellor weep in the Rain Hall and has told no one',
            'is saving for a stall on the Commons Market, coin by coin',
            'carries a letter meant for Lord Grayrock, who is never home to take it',
            'grew up on a Mossgrave sheep run in the Folds and still misses the quiet',
            'has a petition lost somewhere in the City Office and goes back every week to ask']


class Folk(R.People):
    """The residents kit, with the city's own names, quirks and histories, and IDs that carry the city prefix."""

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
        from .realm import OX, OY
        x, y = int(point[0]) + OX, int(point[1]) + OY
        for r in range(0, 48, 4):
            for dx, dy in [(0, 0)] if r == 0 else [(r, 0), (-r, 0), (0, r), (0, -r), (r, r), (-r, -r), (r, -r), (-r, r)]:
                try:
                    return self.world(x + dx, y + dy, share)
                except ValueError:
                    continue
        raise ValueError(f'No open ground near {point}')

    def route_at(self, rid, name, points):
        from .realm import OX, OY
        return self.route(self.prefix + rid, name, [(int(x) + OX, int(y) + OY) for x, y in points])


def family_of(house):
    """A family house's family ("Bellandi House" -> Bellandi); tenements and halls have none."""
    return house['name'][:-6] if house['kind'] == 'house' and house['name'].endswith(' House') else None


def index(manifest, city):
    return {b['name']: b for b in manifest if b.get('city') == city}


def shop_household(P, b, rng, greeting, evening, play):
    """A shop's family in its flat upstairs (Docs/Design/39): the keeper, who sells there; often a partner who helps in
    the shop; then children (`play`: where they play) or an elder in the beds left."""
    family = P.family()
    home = P.bed(b)
    if home is None:
        return None
    keeper = P.add(role='merchant', work_label=keeper_label(b['trade'], b['name']), home=home, work=P.work(b),
                   family=family, evening=evening() or home, hours=(7, 19), job=f'who keeps {b["name"]} and lives over it',
                   greeting_line=greeting)
    first = True
    while True:
        bed = P.bed(b)
        if bed is None:
            break
        if first and rng.random() < .8:
            P.add(role='civilian', work_label=f'helping at {b["name"]}'[:40], family=family, home=bed,
                  work=P.floor(b['rooms'][0]['id'], share=True), evening=evening() or bed, hours=(8, 18),
                  job='who helps in the family shop')
        elif rng.random() < .7:
            P.add(role='civilian', work_label='playing in the streets', family=family, home=bed, work=play(),
                  evening=bed, hours=(9, 16), age=rng.randint(5, 14), paid=False, purse=0, job='a child of the shop')
        else:
            P.add(role='civilian', work_label='minding the shop door', family=family, home=bed,
                  work=P.floor(b['rooms'][0]['id'], share=True), evening=bed, hours=(10, 15), age=rng.randint(62, 82),
                  job='the eldest of the family')
        first = False
    return keeper


def stallholders(P, city, key, rng, home, evening):
    """A stallholder for each stall of one of the city's market squares (Docs/Design/39), selling its food there every
    day: standing behind the stall, living in the city's houses and tenements (`home()`: a bed)."""
    square, foods, call = MARKETS[key]
    for i, place in enumerate(city.spots.get(f'stalls_{key}', [])):
        food = foods[i % len(foods)]
        bed = home()
        P.add(role='merchant', work_label=f'{food} stall on {square}'[:40], home=bed, work=P.at(place, share=False),
              evening=evening() or bed, hours=(6, 16), job=f'who keeps the {food} stall on {square}',
              greeting_line=call.format(food=food, Food=food.capitalize()), age=rng.randint(20, 66))


# The Houses' families: (first name, sex, age, work label, where they work, who they are, personality, backstory).
HEADS = {
    'greatly': [
        ('Ambrose', 'male', 63, 'counting the House\'s money', 'counting',
         'Lord of House Greatly, the richest wolf in Ridgemere, who owns the ships, the wharf and half the city\'s debts',
         'Courteous, patient and utterly without mercy in business; he never raises his voice and never forgets.',
         'Ambrose made House Greatly the first of the Houses with loans the others could not refuse; he means his '
         'heir to inherit a city, not a fortune.'),
        ('Celestine', 'female', 58, 'keeping Greatly House', 'home',
         'Lady of House Greatly, who rules the Heights\' society as her husband rules its purse',
         'Gracious, sharp-tongued and the best-informed wolf in the city.',
         'Celestine hosts the only salon in Ridgemere that every House attends, and remembers everything said there.'),
        ('Julian', 'male', 29, 'keeping the wharf\'s books', 'wharf',
         'the Greatly heir, who runs the House\'s wharf and ships',
         'Clever, restless and a little too fond of the Sailors\' Rest.',
         'Julian wants to sail with the ships rather than count them, and his father knows it.'),
        ('Rosalind', 'female', 22, 'reading in the gardens', 'home',
         'the Greatly daughter, promised (she has heard) to a Fell',
         'Bookish, wry and quietly stubborn.',
         'Rosalind has read every ledger in the Counting House and found one that does not add up.')],
    'fell': [
        ('Isolde', 'female', 58, 'ruling House Fell', 'home', 'the Lady of House Fell, widowed and unbending',
         'Unbending, shrewd and cold until she decides you are useful.',
         'Isolde has ruled Fell since her husband drowned under a log raft, and has never let a debt go.'),
        ('Corwin', 'male', 31, 'driving the logging crews', 'works', 'the Fell heir, loud and broad as a cedar',
         'Loud, proud and generous with other people\'s coin.',
         'Corwin wants the Council to give Fell the whole of the Rainward Hills, and says so in every tavern.'),
        ('Gudrun', 'female', 29, 'keeping the Fell accounts', 'home',
         'Corwin Fell\'s wife, who does the sums he does not',
         'Quiet, precise and far more ambitious than her husband.',
         'Gudrun was a Mossgrave cousin once, and still writes to them more than Isolde knows.')],
    'grayrock': [
        ('Aldric', 'male', 52, 'travelling on House business', 'traveller',
         'Lord of House Grayrock, always on the road: to the mines above Frosthollow, to the quarries, to anyone who '
         'will buy iron. His family sees him a few days a year.',
         'Restless, charming and hard as his own stone; he trusts contracts more than people.',
         'Aldric has not slept a full week in Grayrock Hall in six years; he tells himself the House needs him on the '
         'road, and half believes it.'),
        ('Maren', 'female', 47, 'keeping Grayrock Hall', 'home',
         'Lady of House Grayrock, who runs the Hall, the ledgers and, some say, the House itself while her lord is away',
         'Patient, sharp-eyed and lonely in a way she would never admit; the Hall is her whole world.',
         'Maren married into Grayrock at nineteen and has not left the Hall\'s grounds in years; every letter from her '
         'husband is filed, and none are answered in kind.'),
        ('Tobias', 'male', 23, 'overseeing the ironworks', 'works', 'the Grayrock heir, soot to the elbows',
         'Earnest, anxious and better with iron than with people.',
         'Tobias would rather run the forge than the House, and knows he will have to do both.'),
        ('Ingrid', 'female', 16, 'at her lessons in the Hall', 'home', 'the Grayrock daughter',
         'Bright, bored and forever at the window watching the road.',
         'Ingrid writes to her father every week, and waits for a reply.')],
    'mossgrave': [
        ('Halvard', 'male', 66, 'walking the gardens', 'home',
         'Lord of House Mossgrave, whose sheep runs and warrens fill the Folds south of the city',
         'Gentle, slow-spoken and far sharper than he lets on.',
         'Halvard knows every shepherd in the Folds by name, and every one of them would lie to the Council for him.'),
        ('Signe', 'female', 61, 'keeping Mossgrave Court', 'home', 'Lady of House Mossgrave',
         'Warm, practical and the only one who can make Halvard rest.',
         'Signe keeps the House\'s seat on the Council and votes as Halvard never bothers to.'),
        ('Wenna', 'female', 34, 'running the wool shed', 'works', 'the Mossgrave heir, who runs the House\'s looms',
         'Brisk, ambitious and unafraid of the other Houses.',
         'Wenna means to sell Ridgemere grey in every port the Greatly ships reach, and on her own ships if need be.'),
        ('Osric', 'male', 27, 'riding out to the Folds', 'home', 'the younger Mossgrave son',
         'Easygoing, kind and forever muddy.',
         'Osric would rather live on a sheep run than in the Court, and very nearly does.')],
}
# The works whose hands make a raw good (Data/Items/crafts.json `producers`, by their work label): how many, the label.
PRODUCERS = {'The Salt Pans': (5, 'rakes the salt at the Salt Pans', 'a salt raker, cracked pads and all'),
             'The Charcoal Store': (4, 'burns charcoal at the Charcoal Store', 'a charcoal burner black to the eyes')}
HOUSE_WORK = {'greatly': 'The Greatly Counting House', 'fell': 'The Fell Sawmill', 'grayrock': 'The Grayrock Ironworks',
              'mossgrave': 'The Mossgrave Wool Shed'}
VILLA_HEADS = [('a Greatly factor', 'keeps the House\'s ships\' accounts'), ('a ship\'s master', 'between voyages'),
               ('an advocate of the Magistrate\'s Court', 'pleading cases'), ('a wool merchant', 'trading Mossgrave grey'),
               ('a timber factor', 'selling Fell cedar'), ('an iron merchant', 'buying Grayrock iron'),
               ('a physician', 'visiting the sick of the Heights'), ('a retired captain of the Watch', 'at leisure'),
               ('a councillor\'s secretary', 'drafting the Council\'s letters'), ('a banker of the Greatly Bank', 'lending')]


def ridgemere(project, manifest, city, world, rng):
    P = Folk(project, manifest, rng, 'rm_', RM_FEMALE, RM_MALE, RM_HEADS, RM_TAILS, RM_QUIRKS, RM_HOOKS)
    B = index(manifest, 'ridgemere')
    mine = [b for b in manifest if b.get('city') == 'ridgemere']
    district = lambda b: b['district']
    houses = [b for b in mine if b['kind'] == 'house' and not district(b).startswith('estate_')]
    tenements = [b for b in mine if b['kind'] == 'tenement' and not district(b).startswith('estate_')]
    rng.shuffle(houses)
    rng.shuffle(tenements)
    sump = [b for b in tenements if district(b).startswith('sump')]
    lodgings = [b for b in tenements if district(b).startswith(('works', 'docks', 'wharf'))]

    def home_in(pool):
        for b in pool:
            bed = P.bed(b)
            if bed:
                return bed
        for b in houses + tenements:                   # Anywhere at all, before giving up.
            bed = P.bed(b)
            if bed:
                return bed
        raise ValueError('Ridgemere has run out of beds')

    taverns = [b for b in mine if b['kind'] in ('tavern', 'inn')]

    def evening():
        if rng.random() < .55 and taverns:
            return P.floor(rng.choice(taverns)['rooms'][0]['id'], share=True)
        return P.at(city.spots[rng.choice(list(MARKETS))])

    def near(spot, r=10):
        return P.at((spot[0] + rng.randint(-r, r), spot[1] + rng.randint(-r, r)), share=True)

    rain = 'The rain has not stopped since morning. It never does.'
    # -- The great Houses --------------------------------------------------------------------------------------
    journey = None
    for house, family in HEADS.items():
        seat_name = HOUSES[house][0]
        seat = B[seat_name]
        hall, chambers, below = (r['id'] for r in seat['rooms'])
        sid = f'estate_{house}'
        surname = house.title()
        works_here = B.get(HOUSE_WORK[house])
        for first, sex, age, label, where, role_text, personality, backstory in family:
            person_name = f'{first} {surname}'
            bed = P.bed(seat) or P.floor(chambers)
            if where == 'traveller':
                journey = journey or grayrock_journey(P, world, city)
                P.add(role='civilian', work_label=label, sex=sex, age=age, family=surname, home=bed,
                      work=P.at(city.spots.get(f'{sid}_gate', city.spots['office'])), evening=bed, hours=(5, 21),
                      route=journey, name=person_name, job=role_text, paid=False, purse=400, meals=6,
                      greeting_line='I have an hour, perhaps less. The road does not wait.',
                      personality=personality, backstory=backstory)
                continue
            if where == 'home':
                work = P.floor(hall) if age > 20 else P.floor(chambers)
            elif where == 'counting' and works_here:
                work = P.work(works_here)
            elif where == 'wharf':
                work = P.work(B['The Greatly Bonded Warehouse']) if 'The Greatly Bonded Warehouse' in B else P.floor(hall)
            elif where == 'works' and works_here:
                work = P.work(works_here)
            else:
                work = P.floor(hall)
            P.add(role='civilian', work_label=label, sex=sex, age=age, family=surname, home=bed, work=work,
                  evening=P.floor(chambers, share=True), hours=(7, 20) if where == 'home' else (6, 18),
                  name=person_name, job=role_text, paid=False, purse=300 if house == 'greatly' else 250,
                  personality=personality, backstory=backstory,
                  greeting_line=f'You stand in the House of {surname}. Be brief.')
        # The steward and the servants of the seat.
        P.add(role='civilian', work_label=f'stewarding {seat_name}'[:40], home=P.bed(seat) or P.floor(below),
              work=P.work(seat), evening=P.floor(below, share=True), hours=(6, 20), age=rng.randint(40, 65),
              job=f'the steward of House {surname}', greeting_line='The family is not receiving. I can take a message.')
        for label in ('cooking for the House', 'serving in the hall', 'keeping the chambers', 'serving in the hall'):
            P.add(role='civilian', work_label=label, home=P.bed(seat) or P.floor(below, share=True),
                  work=P.floor(below if 'cooking' in label else hall if 'hall' in label else chambers),
                  evening=P.floor(below, share=True), hours=(6, 21), job=f'a servant of House {surname}',
                  greeting_line='Mind the floors, they have just been done.')
        # The estate's own people: gardeners in the grounds, grooms in the stables, the gatekeeper.
        estate_homes = [b for b in mine if district(b) == f'{sid}_works' and b['kind'] in ('house', 'tenement')]
        gate = city.spots.get(f'{sid}_gate') or city.spots.get('greatly_gate')
        seat_spot = city.districts.get(f'{sid}_seat')
        staff = [('tending the gardens', 'a gardener of House ' + surname, 'grounds', 3),
                 ('keeping the stables', 'a groom of House ' + surname, f'The {surname} Stables', 1),
                 ('keeping the gate', 'the gatekeeper of House ' + surname, 'gate', 1)]
        if house == 'greatly':
            staff = [('tending the gardens', 'the Greatly gardener', 'grounds', 2),
                     ('keeping the stables', 'the Greatly groom', 'The Greatly Stables', 1),
                     ('clerking at the Counting House', 'a Greatly clerk', 'The Greatly Counting House', 2)]
        for label, job, where, count in staff:
            for _ in range(count):
                bed = home_in(estate_homes) if estate_homes else home_in(houses)
                if where == 'grounds':
                    work = near(seat_spot, 18)
                elif where == 'gate':
                    work = P.at(gate, share=False)
                else:
                    work = P.work(B[where]) if where in B else near(gate, 4)
                P.add(role='civilian', work_label=label, home=bed, work=work, evening=bed, hours=(6, 18), job=job,
                      greeting_line=f'This is {surname} land. You\'ll want the house, up the drive.')
    # -- The Heights' households ---------------------------------------------------------------------------------
    for i, villa in enumerate([b for b in mine if b['kind'] == 'villa']):
        family = villa['name'][:-6]
        what, doing = VILLA_HEADS[i % len(VILLA_HEADS)]
        rooms = [r['id'] for r in villa['rooms']]
        upstairs = [bed for bed in P.beds(villa) if bed[0] == rooms[-1]]
        head_bed = P.spot(*upstairs[0]) if upstairs else P.floor(rooms[-1])
        P.add(role='civilian', work_label=doing[:40], family=family, home=head_bed,
              work=P.floor(rooms[0]), evening=evening(), hours=(9, 17), age=rng.randint(36, 64), job=what, paid=False,
              purse=rng.randint(150, 320), greeting_line='We do not buy at the door.')
        while True:
            bed = P.bed(villa)
            if bed is None:
                break
            if bed['cell'] == rooms[0]:                 # The servants sleep below.
                P.add(role='civilian', work_label=f'in service to the {family}s'[:40], home=bed, work=P.floor(rooms[0]),
                      evening=bed, hours=(6, 21), job=f'a servant of the {family} household', age=rng.randint(18, 60))
            elif rng.random() < .5:
                P.add(role='civilian', work_label='at home in the Heights', family=family, home=bed,
                      work=P.floor(rooms[0], share=True), evening=evening(), hours=(10, 17), age=rng.randint(30, 60),
                      job=f'of the {family} household', paid=False, purse=rng.randint(60, 160))
            else:
                P.add(role='civilian', work_label='at lessons', family=family, home=bed, work=P.floor(rooms[1], share=True),
                      evening=bed, hours=(9, 15), age=rng.randint(8, 17), job=f'a child of the {family}s', paid=False,
                      purse=rng.randint(5, 30))
    # -- The Council, the City Office and the city's officers -----------------------------------------------------
    officers = [('The City Office', 'running the City Office', 'the Chancellor of Ridgemere, who serves the Houses and '
                 'belongs to none', 'The City Office is open. The Chancellor is not.'),
                ('The City Office', 'clerking at the City Office', 'a clerk of the City Office', 'Name, ward and business?'),
                ('The City Office', 'clerking at the City Office', 'a clerk of the City Office', 'Form first. Then the queue.'),
                ('The City Office', 'taking the tolls and rents', 'a rent clerk', 'Your rent is due. It is always due.'),
                ('The City Office', 'issuing permits', 'the permits clerk', 'A permit for that? Third counter.'),
                ('The Rain Hall', 'keeping the Rain Hall', 'the Speaker of the Council of Houses', 'The Houses are in '
                 'session. Whether they agree on anything is another matter.'),
                ("The Magistrate's Court", 'hearing cases', 'the magistrate of Ridgemere, grey and tired',
                 'Guilty or not, you will be heard. That is all I promise.'),
                ("The Magistrate's Court", 'keeping order in court', 'the court bailiff', 'Quiet in the court.'),
                ('The Gaol', 'keeping the gaol', 'the gaoler', 'Visiting, or staying?'),
                ('The Hall of Records', 'copying the rolls', 'a records clerk', 'Every birth, every death, every debt.'),
                ('The Hall of Records', 'keeping the records', 'the keeper of records', 'It is written down somewhere.'),
                ('The Customs House', 'taxing the cargoes', 'the Customs officer', 'Declare it, or lose it.'),
                ('The Customs House', 'weighing bales', 'a customs tallyman', 'Stand clear of the scales.'),
                ("The Harbourmaster's Office", 'keeping the harbour', 'the harbourmaster', 'Berths are full. Always.'),
                ('The Chapel of the Tide', 'keeping the chapel', 'the chaplain of the Tide',
                 'Light a candle for them. The sea listens, sometimes.'),
                ('The Sump Infirmary', 'tending the sick', 'a Sump healer', 'Sit. Cough. Again.'),
                ('The Sump Infirmary', 'boiling rags', 'a Sump healer\'s helper', 'Mind the pots.'),
                ('The House of Mending', 'healing the sick', 'the healer of the Commons', 'Where does it hurt?'),
                ('The Commons School', 'teaching letters', 'the Commons schoolteacher', 'Sit down. Slates out.'),
                ('The Heights Watch Post', 'keeping the Heights post', 'a Watch sergeant', 'Move along, now.')]
    for building, label, job, line in officers:
        if building not in B:
            continue
        P.add(role='civilian', work_label=label, home=home_in(houses), work=P.work(B[building]), evening=evening(),
              hours=(8, 18), job=job, greeting_line=f'{line} {rain}' if rng.random() < .2 else line,
              age=rng.randint(30, 66))
    # The Soup Kitchen and the taverns sell food and drink.
    P.add(role='merchant', work_label='ladling soup', home=home_in(sump), work=P.work(B['The Soup Kitchen']),
          evening=P.floor(B['The Soup Kitchen']['rooms'][0]['id'], share=True), hours=(6, 20),
          job='the Soup Kitchen cook', greeting_line='One bowl. Back of the queue for another.')
    for b in taverns:
        inn = b['kind'] == 'inn'
        P.add(role='merchant', work_label=('keeping the inn' if inn else 'keeping the taproom'), home=home_in(houses),
              work=P.work(b), evening=P.floor(b['rooms'][0]['id'], share=True),
              hours=(6, 20) if inn else (10, 23.5), job=f'the keeper of {b["name"]}',
              greeting_line=f'Dry yourself by the fire. {rain.split(".")[0]}.')
    # Shopkeepers and their households, over their shops.
    for b in [b for b in mine if b['kind'] == 'shop']:
        play_at = city.spots['sump_market'] if district(b).startswith('sump') else city.spots['commons_market']
        shop_household(P, b, rng, f'{b["name"]}. Shut the door behind you, the rain gets in.', evening,
                       lambda at=play_at: near(at, 10))
    # The stallholders of the three markets: the Sump's live in the Sump, the others in the middle town.
    for key in MARKETS:
        pool = sump if key == 'sump_market' else houses
        stallholders(P, city, key, rng, lambda pool=pool: home_in(pool), evening)
    # -- The City Watch ------------------------------------------------------------------------------------------
    watch_house = B['The Watch House']
    wall_route = P.route_at('wall_walk', 'Wall walk', city.spots['wall_walk'])
    wharf_route = P.route_at('wharf_patrol', 'Wharf patrol', city.spots['wharf_walk'])
    street_route = P.route_at('street_patrol', 'Street patrol', city.spots['street_walk'])
    P.add(role='guard', work_label='commanding the Watch', home=P.bed(watch_house), work=P.work(watch_house),
          evening=P.floor(watch_house['rooms'][0]['id'], share=True), hours=(7, 19), age=rng.randint(40, 55),
          job='the Captain of the Ridgemere Watch',
          greeting_line='The Watch keeps the walls. The Houses keep the rest. Trouble?')
    for route, hours in ((wall_route, (6, 18)), (wall_route, (18, 6)), (wharf_route, (6, 18)), (wharf_route, (18, 6)),
                         (street_route, (6, 18)), (street_route, (18, 6)), (wall_route, (6, 18)), (street_route, (6, 18))):
        bed = P.bed(watch_house) or home_in(houses)
        P.add(role='guard', work_label='on night patrol' if hours[0] == 18 else 'on patrol', route=route, home=bed,
              work=P.routes[[r['id'] for r in P.routes].index(route)]['posts'][0], evening=bed, hours=hours,
              job='a Watch guard in a sodden cloak', greeting_line='Keep moving. Keep dry, if you can.')
    for gate in ('north_gate', 'east_gate', 'south_gate'):
        for hours in ((6, 18), (18, 6)):
            bed = P.bed(watch_house) or home_in(houses)
            P.add(role='guard', work_label=f'holding the {gate.replace("_", " ")}', home=bed,
                  work=P.at(city.spots[gate], share=False), evening=bed, hours=hours, job='a gate guard of the Watch',
                  greeting_line='Name and business.')
    # -- The works, the wharf and the docks --------------------------------------------------------------------
    for b in [b for b in mine if b['kind'] == 'works']:
        stations = sum(len(r['work']) for r in b['rooms'])
        word = WORKS_TRADE.get(b['trade'], 'master')
        crew = [('merchant', f'runs {b["name"]}'[:40], f'the {word} who runs {b["name"]}')] + \
               [('civilian', f'works at {b["name"]}'[:40], f'a hand at {b["name"]}')] * min(4, max(0, stations - 1))
        if b['name'] in PRODUCERS:                    # Hands whose work the game counts as making a good.
            count, label, job = PRODUCERS[b['name']]
            crew = crew[:1] + [('civilian', label, job)] * count
        for role, label, job in crew:
            pool = lodgings if not district(b).startswith('suburb') else houses
            P.add(role=role, work_label=label, home=home_in(pool + sump), work=P.work(b), evening=evening(),
                  hours=(6, 18), job=job, greeting_line='Mind the fires. And the vats. And the floor.')
    for label, count, where, job in [('unloading ships on the wharf', 8, 'wharf', 'a dockhand with rope-burned paws'),
                                     ('fishes the Sound off the piers', 4, 'piers', 'a fisher of the Sound'),
                                     ('mending nets on the wharf', 2, 'wharf', 'a net-mender'),
                                     ('hauling timber at the docks', 3, 'docks', 'a docks labourer')]:
        for _ in range(count):
            if where == 'piers' and city.spots.get('piers'):
                px, py = rng.choice(city.spots['piers'])
                work = near((px, py), 3)
            else:
                work = near(city.spots[where], 12)
            P.add(role='civilian', work_label=label, home=home_in(lodgings + sump), work=work, evening=evening(),
                  hours=(5, 17), job=job, greeting_line='Out of the way, unless you\'re carrying something.')
    # -- The Sump ------------------------------------------------------------------------------------------------
    sump_jobs = [('washing clothes by the wharf', 3, 'wharf', 'a washerwoman of the Sump'),
                 ('picking rags', 3, 'sump_market', 'a rag-picker'),
                 ('begging in the market', 3, 'sump_market', 'a beggar of the Sump'),
                 ('running errands', 4, 'office', 'a Sump child quick with messages'),
                 ('sweeping the Commons Market', 2, 'commons_market', 'a street sweeper')]
    for label, count, where, job in sump_jobs:
        for _ in range(count):
            child = 'child' in job
            bed = home_in(sump)
            P.add(role='civilian', work_label=label, home=bed, work=near(city.spots[where], 12),
                  evening=evening() if not child else bed, hours=(7, 19),
                  paid='beg' not in label and not child, purse=rng.randint(0, 8),
                  age=rng.randint(7, 12) if child else None, job=job,
                  greeting_line='Spare a copper? No? Then what do you want?' if 'beg' in label else
                  'What is it? I have work.', sex='female' if 'wash' in label else None)
    return P


def grayrock_journey(P, world, city):
    """Lord Grayrock's road: from his estate gate down the Grayrock Drive, along the High Road to Coldwater and up the
    Pass Road to the mountain mines at Frosthollow, and back again."""
    from .realm import OX, OY
    line = world.road_lines.get('The Grayrock Drive', []) + world.road_lines.get('The High Road', []) + \
        world.road_lines.get('The Pass Road', [])
    pts, last = [], None
    for x, y in line:
        if last is None or ((x - last[0]) ** 2 + (y - last[1]) ** 2) ** .5 >= 70:
            pts.append((int(x) + OX, int(y) + OY))
            last = (x, y)
    posts = pts + list(reversed(pts[1:-1]))
    places = []
    for x, y in posts[:64]:
        try:
            places.append(P.world(x, y))
        except ValueError:
            continue
    rid = P.prefix + 'grayrock_journey'
    P.routes.append({'id': rid, 'name': 'Lord Grayrock\'s road', 'posts': places})
    return rid


def populate(project, manifest, world, seed=SEED):
    """Ridgemere's people and routes, to add to the project's own."""
    rng = random.Random(seed)
    city = next(c for c in world.cities if c.region == 'ridgemere')
    P = ridgemere(project, manifest, city, world, rng)
    return P.people, P.routes
