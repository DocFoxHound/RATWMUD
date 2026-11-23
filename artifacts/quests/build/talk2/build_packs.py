# The quest packs' talk lines: packs_raw.json (extract_packs.py) with the fixes this part made (no season or weather
# word without its gate, no "boat shed" outside a fishing town, each id-gated line in its speaker's own speech) and the
# lines the design gives only in prose (the thin purse, the stage pack's five large quests). Writes packs.json.
import json
from pathlib import Path
HERE = Path(__file__).resolve().parent
r=json.load(open(HERE / 'packs_raw.json'))
FIX={
 "at the boat shed":"at the Upper Mere shed",
 "to the Boat Shed":"to the Upper Mere shed",
 "by the Boat Shed":"by the Upper Mere shed",
 "Fog like this, watch the North Headland, not the Mole.":None,
 "No eels this spring.":"No eels running this season.",
 "it's a fair-dry spring after all":"it's a fair-dry season after all",
 "Fair-priced wine this autumn":"Fair-priced wine this year",
 # review fixes (2026-11-22): vocatives the grammar keeps for pups or intimates go through #address# (below); the pup
 # is the old pawnshop wolf's littlest, not a sister; no place id in a recap; the Sump-lamps thief is a he, so he isn't
 # mistaken for the headland's lamplighter (decisions §2.2); a stage key lasts days, so no "tomorrow"; the logger is
 # 40, not young; no season word on a past event; an innkeeper's greeting asks no question.
 "Somebody walked the pawnbroker's little sister home from the piers.":"Somebody walked the old pawnshop wolf's littlest home from the piers.",
 "The shepherd from Cottage 3 wants help.":"One of the shepherds there wants help.",
 "An old lamplighter, they say. Forty years of lighting, and that's how it ends.":"One of the lamplighters himself, they say. Forty years he lit that round, and that's how it ends.",
 "The salt wagon comes home across the Waste tomorrow.":"The salt wagon's due home across the Waste.",
 "The young logger can't sleep before a storm.":"One of the loggers can't sleep before a storm.",
 "Somebody rendered the autumn tallow and sold the oil in the city.":"Somebody rendered the tallow meant for the list and sold the oil in the city.",
 "You've heard? Our cook's brother is on the Chapel's roll at last.":"Our cook's brother is on the Chapel's roll at last.",
 # committer's cross-piece fixes (2026-11-22): the packs' programs as reviewed (nh-pack.small.south.notes.md "Talk"):
 # a badger runs and bites only when cornered (its temper in Data/Wild), and Saltwell's farm cart comes most mornings.
 "they don't run, eh? They turn round.":"they'll run, eh? Corner one and it turns round and bites.",
 "Fair warning: they're top-weight and they don't back off.":"Fair warning: they're top-weight, and a cornered one bites.",
 "The wagons come in by the square. Wait in the shade":"The farm cart comes in early, most mornings, and the caravans more seldom. Wait in the shade",
 "The wheel? The post sat empty and the rule gave it to whoever was free. Same rule as put a patrolman in the Palace, no?":"The potter's wheel sat empty, and the rule gave it to whoever was free. Fen-sure, the same rule put a patrolman in the Palace.",
}
for x in r:
    for a,b in FIX.items():
        if a in x['line']:
            if b is None: x['gates'].append('here.weather=fog')
            else: x['line']=x['line'].replace(a,b)
def add(quest,node,gates,line): r.append(dict(src='manual',quest=quest,node=node,gates=gates,line=line,ln=0))
# The thin-purse lines (quests-small.md §1.8, P3; doc 86 §6.4), news, priority 3, improved to each dialect.
THIN={'plain':('ridgemere',"The town purse is thin this week. Errands are still paid, in bread if not in coin."),
 'coldwater':('coldwater',"Purse is thin as a holed net, eh? The errands still pay, in bread if not coin."),
 'larkfield':('larkfield',"The purse is short-weighed this week, #address#. Errands pay in bread till it's full weight again."),
 'southmeadow':('southmeadow',"The purse is blighted thin this week, #address#. You'll get bread, not coin, till it fills."),
 'mirebridge':('fenshore',"Purse is boggy-low, no? Errands still pay, in bread if not in coin."),
 'saltwell':('saltwell',"The purse is dry this week, #address#. The work is paid in bread until it fills."),
 'oakhaven':('oakhaven',"The purse is down to the bark, mm? Work's still paid, in bread if not in coin."),
 'frosthollow':('frosthollow',"Purse is thin as a worked seam, #address#. You'll be paid in bread till it fills.")}
for d,(t,l) in THIN.items():
    add('nh-purses','news',[f'here.region={t}',f'self.dialect={d}','world.mem.nh_coin=thin'],l)
# The stage pack's large quests (quests-large.md §12b), written in prose there.
add('st-patrol-round','news',['here.region=ridgemere','self.dialect=plain','self.quarter=sump','world.mem.st_round=open'],"If you walk the Sump round of a night, keep to the posts and carry a light. The cutpurses wait in the dark between.")
add('st-patrol-round','gossip',['here.region=ridgemere','self.dialect=plain','self.quarter=sump','world.mem.st_round=open'],"Somebody's walking the night round since the patrolman went up the Hundred Steps. Not the Watch. Just somebody.")
add('st-camp-bounty','news',['self.dialect=plain','world.mem.st_bounty=open'],"The broken camps come back where the roads cross the wild: Alder Mere, Glasswater, the Frosthollow road. The Watch pays for each one broken again.")
add('st-camp-bounty','news',['here.region=coldwater','self.dialect=coldwater','world.mem.st_bounty=open'],"Camp's back by Glasswater, eh? The Watch in the city pays for it broken. Net and line, go in a pack.")
add('st-midsummer-road','news',['here.region=ridgemere','self.dialect=plain','world.mem.st_fair=open'],"Fair-goers on the north road this week. The gate guard wants it walked. Bandits know the fair days too.")
add('st-twelve-bounds','gossip',['here.region=oakhaven','self.dialect=oakhaven','world.mem.st_bounds=walked'],"Somebody walked all twelve bounds of the hills in a day, mm? Old Oak, the old folk did that.")
add('st-twelve-bounds','gossip',['here.region=ridgemere','self.dialect=plain','world.mem.st_bounds=walked'],"Somebody walked every bound of the Rainward Hills in one day. Let the Council argue with that.")
add('st-post-night','news',['self.dialect=plain','world.mem.st_posts=probed'],"Bands are testing the Line's posts of a night. Stand one from dark till first light, and take a friend.")
add('st-post-night','gossip',['here.region=frosthollow','self.dialect=frosthollow','world.mem.st_posts=probed'],"Somebody stood the Pass post the whole night through, cousin. Warm hearth to them. Deep quiet, that.")
# Lines in a resident's own window carry that resident's speech too (live.npc_voice, read only 2026-11-22).
SPEECH={'cty_tristram_rainridge':'oakhaven','cty_ebba_coalgate':'frosthollow','cty_willa_wheatcombe':'southmeadow',
        'cty_ingram_ashleaf':'oakhaven','rm_rosamund_tarridge':'ridgemere','cty_nell_plummeadow':'fenshore'}
for x in r:
    for g in list(x['gates']):
        if g.startswith('self.id=') and g[8:] in SPEECH and not any(h.startswith('self.from') or h.startswith('self.dialect') for h in x['gates']):
            x['gates'].append('self.from='+SPEECH[g[8:]])
import re
for x in r:
    # st-fog-lanterns runs in s56 (letters and lights), so its helpful line plays in both.
    if x['quest']=='st-fog-lanterns':
        x['gates']=[('world.mem.nh_stage=letters|lights' if g=='world.mem.nh_stage=lights' else g) for g in x['gates']]
    x['line']=re.sub(r', (lambkin|tadpole|whelp|my apple|honoured guest)\b', ', #address#', x['line'])
json.dump(r,open(HERE / 'packs.json','w'),indent=0)
print(len(r))
