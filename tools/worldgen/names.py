"""Names and scene prose for Upper Accord's buildings. Wolf-made places: plain, old, a little proud."""
from __future__ import annotations

import random

SHOP_NAMES = {
    'general': ['The Long Shelf', 'Crag & Candle', 'Oldstone Sundries', 'The Honest Pack', 'Greywall Goods'],
    'smith': ['The Ringing Anvil', 'Cinderpaw Forge', 'Ironhowl Smithy', 'The Cold Hammer'],
    'armorer': ['Wardhide Armory', 'The Riveted Collar', 'Stoneguard Armor'],
    'baker': ['The Warm Crust', 'Hearthloaf Bakery', 'Morning Rise Ovens', 'The Crag Oven'],
    'butcher': ['Keenfang Butchery', 'The Hanging Haunch', 'Redclaw Meats'],
    'tanner': ['Brinevat Tannery', 'The Stretched Hide', 'Soakpit Leathers'],
    'apothecary': ['The Bitter Root', 'Stillwater Apothecary', 'Nightbloom Remedies'],
    'herbalist': ['Mossbank Herbs', 'The Dry Bundle', 'Snowsage Herbalist'],
    'tailor': ['Needle & Nap', 'The Warm Mantle', 'Fairweather Tailor'],
    'weaver': ['The Tight Weft', 'Loomsong Weavers', 'Grey Yarn Hall'],
    'carpenter': ['Pinecut Joinery', 'The Square Peg', 'Timberline Carpentry'],
    'potter': ['Kilnmouth Pottery', 'The Cracked Jar', 'Clayfoot Wares'],
    'jeweler': ['The Quiet Gem', 'Starfleck Jewels', 'Moonglint & Band'],
    'scribe': ['Inkpaw Scriptorium', 'The Faithful Copy', 'Quillstone Scribes'],
    'chandler': ['Tallowlight Chandlery', 'The Steady Wick', 'Brightwax'],
    'fletcher': ['The Straight Shaft', 'Greyfeather Fletchery', 'Nockpoint'],
    'provisioner': ['Trailhead Provisions', 'The Full Pack', 'Passes & Provisions'],
    'cooper': ['Hoopwright Cooperage', 'The Sound Stave'],
    'mason': ['Oldcut Masonry', 'The True Block'],
    'fishmonger': ['Tarnwater Fish', 'The Cold Catch'],
    'cartographer': ['Ridgeline Maps', 'The Folded Road'],
    'brewer': ['Bitterfrost Brewery', 'The Deep Cask'],
    'tinker': ['Oddments & Mending', 'The Tinker\'s Bench'],
    'moneychanger': ['Counted Coin', 'The Fair Scale'],
}
TRADE_PROSE = {
    'general': 'Rope, candles, salt, twine and wrapped trail food crowd the shelves behind a scarred counter.',
    'smith': 'Heat rolls off the forge; hammers, tongs and half-finished blades hang in sooty rows.',
    'armorer': 'Harness plates, riveted collars and padded coats hang from pegs, each tagged for a Warden.',
    'baker': 'The ovens breathe warmth and the smell of crust into the street from before dawn.',
    'butcher': 'Salted haunches hang from hooks over a wide block worn hollow by years of cleavers.',
    'tanner': 'Vats of bark-brown water and stretched hides give the place a sharp smell no door can hold in.',
    'apothecary': 'Shelves of stoppered jars climb the walls; the air is bitter with drying roots.',
    'herbalist': 'Bundles of mountain herbs hang drying from every beam, sage and snowbloom and thistle.',
    'tailor': 'Bolts of heavy mountain cloth lean against the walls beside half-sewn cloaks.',
    'weaver': 'Looms clack behind the counter; skeins of undyed grey wool fill the baskets.',
    'carpenter': 'Sawdust drifts across a floor stacked with seasoned pine and tools on every wall.',
    'potter': 'A kiln ticks as it cools; jars, bowls and lamps wait in rows to be sold.',
    'jeweler': 'Small, careful and quiet: rings and clasps glint in cases under a narrow window.',
    'scribe': 'Lecterns and ink pots, stacked vellum and the patient scratch of copying.',
    'chandler': 'Wax and tallow in every shape; the rendering pot never quite goes cold.',
    'fletcher': 'Arrow shafts dry in bundles; feathers are sorted by length into clay pots.',
    'provisioner': 'Everything a traveller up the mountain road forgot: packs, flints, oil and dried meat.',
    'cooper': 'Barrels in every stage of making, hoops and staves stacked to the rafters.',
    'mason': 'Blocks of the same grey stone as the city wall, dressed and waiting for a use.',
    'fishmonger': 'Fish from the mountain tarns packed in cold wet moss on a stone slab.',
    'cartographer': 'Maps of passes and ridges cover the walls, most of them corrected more than once.',
    'brewer': 'Casks line the walls and something dark and bitter ferments in the back.',
    'tinker': 'Buckles, hinges, lamps and locks in pieces; almost anything can be mended here.',
    'moneychanger': 'Scales, ledgers and a heavy strongbox; every coin is weighed twice.',
}
TAVERN_NAMES = ['The Howling Stair', 'The Last Ridge', 'The Warden\'s Rest', 'The Split Crag', 'The Frost Cup']
INN_NAMES = ['The Gatehouse Inn', 'The Summit Lodge', 'The Road\'s End Inn']
HOUSE_PROSE = [
    'A plain stone home: a hearth, a worn table, sleeping mats along the walls.',
    'Old stone walls and a low ceiling; the hearth has blackened the same corner for generations.',
    'A crowded family home, warm and cluttered, smelling of smoke and bread.',
    'A narrow house wedged between its neighbours, tidy and spare.',
    'Thick walls keep out the mountain cold; the family\'s bedding is rolled neatly by the hearth.',
]
FAMILY_NAMES = ['Ashfall', 'Greymantle', 'Stonepaw', 'Frostborn', 'Ridgewalker', 'Cragfoot', 'Emberhide', 'Slatecoat',
                'Pinecrest', 'Snowmere', 'Hollowell', 'Brackenrun', 'Moorstep', 'Windhollow', 'Duskfell', 'Ironbark',
                'Thornback', 'Fallowmark', 'Rimeclaw', 'Oakshadow', 'Coldbrook', 'Harrowgate', 'Mistvale', 'Bramblefoot',
                'Shalewood', 'Stormcoat', 'Graniteheart', 'Loamridge', 'Swiftwater', 'Heathrow', 'Kestrelmoor', 'Quarrystone']


class Namer:
    def __init__(self, seed):
        self.rng = random.Random(seed)
        self.used = set()

    def pick(self, options, fallback):
        free = [o for o in options if o not in self.used]
        name = self.rng.choice(free) if free else f'{fallback} {len(self.used) + 1}'
        self.used.add(name)
        return name

    def shop(self, trade):
        return self.pick(SHOP_NAMES[trade], trade.title())

    def house(self):
        return self.pick([f'{f} House' for f in FAMILY_NAMES], 'House')

    def tavern(self):
        return self.pick(TAVERN_NAMES, 'Tavern')

    def inn(self):
        return self.pick(INN_NAMES, 'Inn')
