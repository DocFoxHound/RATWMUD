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
 # locked-door re-casts (2026-11-22, doc 86 "Givers behind locked doors"): each line points at the resident who now
 # gives or takes the errand, where a wolf can reach them in their working hours.
 "Even the nurse smiled, and she never smiles.":"The porter at the Stairs says even the nurse smiled, and she never smiles.",
 "The abbey's healer pays for herbs.":"The apothecary on the square pays for herbs, for the House of Mending's draughts.",
 "Yarrow's the one the healer wants most.":"Yarrow's the one the apothecary wants most.",
 "Someone brought the healer an armful.":"Someone brought the apothecary on the square an armful.",
 "Upstairs at the Records Office they pay for copying. You read the old count while you do it.":"The Council Press is printing the old rolls. They pay for paper and ink brought in, and you hear the old count read off the proofs.",
 "If you want to know how the city was counted, sort a drawer at the Records Office. They'll even pay you for it.":"If you want to know how the city was counted, take the Council Press some paper. The printer reads the old rolls aloud as she sets them.",
 "There was a new paw at the copying desk today. Neat letters, they say.":"Somebody brought the Council Press a stack of good paper today. The old rolls are nearly set.",
 "A letter for this street? Bring it to the door, not the gate. The doorkeepers here don't come down for anyone.":"A letter for the Vances? Don't climb for it. Their doorkeeper doesn't come down for anyone, but the one who runs the Rag Mill takes the House's letters.",
 "A City Office letter came to one of our doors today. The servant took it like it might bite.":"A City Office letter came up to one of our houses today. The servant took it like it might bite.",
 "The cathedral's sexton takes candles any day, but she wants you at the service on Restday too.":"Wickham's chandler takes candles for the cathedral any day, but wants you at the cathedral door on Restday too.",
 "The young almoner from the lake country sends bread down here. Some wolf carries it, and doesn't ask questions.":"The young almoner from the lake country leaves bread at the abbey brewhouse for us. Some wolf carries it down, and doesn't ask questions.",
 "The abbey's almoner is lake-born, eh? She'll find you work if you've a strong back and a closed mouth.":"The abbey's brewer hands out the almoner's bread. He'll find you work if you've a strong back and a closed mouth.",
 "The Greatly stables pay for oats and straw. Don't expect the steward to thank you; the stable wolf will.":"The Greatly Yard pays for oats and straw for the stables. Don't expect the steward to thank you; the yard hand will.",
 "The lamplighter wants somebody to ring the morning bell. You climb the Clock Tower at first light; he can't, at his age.":"A porter at the Sump Stairs wants somebody to ring the morning bell. The rope comes down by the Clock Tower door; pull it at first light.",
 "The wolf who visits our sick swears by willow bark for stiff joints. Says it beats any physician on the Commons.":"The Mortar and Pestle's hand who brings our physic up swears by willow bark for stiff joints. Says it beats any physician on the Commons.",
 "Somebody brought the healer bark enough for every stiff knee on the street.":"Somebody brought the Mortar and Pestle bark enough for every stiff knee on the street.",
 "The Fellhall kitchen is always a basket short. The hall servant pays for eggs and cheese brought round the back.":"The Fellhall kitchen is always a basket short. The Lamp and Wick hand from the Fell cottages pays for eggs and cheese, and carries them up.",
 "Fellhall's kitchen buys eggs off the Commons most days. Go in by the kitchens; the front door's for the family.":"Fellhall's kitchen buys eggs off the Commons most days. Nobody goes in; the Lamp and Wick hand who lives in the cottages takes them up.",
 "Grayrock Hall buys linen when the sea damp gets into the sheets. The young hall servant pays, and talks loud enough for two.":"Grayrock Hall buys linen when the sea damp gets into the sheets. The Bridge Stores hand from the Grayrock cottages pays, and carries it up.",
 "Looking for the Court's chamber keeper? He takes his supper below stairs, and you'll hear him humming before you see him.":"Looking for the Court's chamber keeper? He never comes out. Wenna Timberland takes things in for the Court; she's in the yard with the iron carts all day.",
 "The Larkdowns shepherd pays for straw for his pens. Calm sort, eh? He hums to the ewes.":"The Looms carts straw up to the Larkdowns pens, and pays for it, eh? The shepherd hums to his ewes, they say.",
 "The fen-born shepherd at Larkdowns loses ewes up the downs, eh? They leave wool on the thorns; a good nose follows it.":"A Larkdowns shepherd loses ewes up the downs, eh? The Looms finds him a nose to follow the wool on the thorns.",
 "If both his strays are being tracked, the other Larkdowns shepherd wants straw for the pens, eh?":"If both his strays are being tracked, the Looms wants straw for the Larkdowns pens, eh?",
 "The Longmead miller wants the reeds pulled from his race. Wet work, but he pays in flour, eh?":"The Longmead mill race wants its reeds pulled, and the baker's asking: his flour comes short. Wet work, but it pays in flour, eh?",
 "Somebody waded in for the miller.":"Somebody waded in for the baker.",
 "The Larkholm farmhand's stringing garlands round the square for the festival.":"The clay digger from the Sallow pits is stringing garlands round the square for the festival.",
 "The rope's round the square. Ripe, every bunch of it, and not one of them city-bought.":"The rope's round the square, every bunch of it, and not one of them city-bought.",
 "The Fenside salt worker has a grandmother story for everything, no? Ask her about Mirebridge, if you've the stomach for it.":"The Fenside salt workers say the old names over the pans, no? Ask the peat cutter about Mirebridge, if you've the stomach for it.",
 "Somebody hauled her wood, fen-sure.":"Somebody hauled them wood, fen-sure.",
 "The deckhand who mends the boardwalks takes planks and nails, no?":"The Heron's pot hand mends the boardwalk, and takes planks and nails, no?",
 "The City Office is sending the Moot's summons to every farmstead on the roll. The clerk pays carriers who don't mind hills.":"The carters are taking the Moot's summons to every farmstead on the roll. One of them pays wolves to leave a summons where a farm's cart calls.",
 "The Rainward orchard keeper sends bread up to the wolves walking the bounds.":"A woodcutter at the Rainwood coppice sends bread up to the wolves walking the bounds.",
 "Bread comes up from the Rainward orchard for the bounds-walkers, mm? Good bark, that keeper.":"Bread comes up from the Rainwood coppice for the bounds-walkers, mm? Good bark, that woodcutter.",
 "The orchard keeper's bread, carried up into the hills.":"The coppice's bread, carried up into the hills.",
 "The Longmead farmhand's giving out bread for the burned houses, eh? Find him at the granary and he'll load you up.":"The Print House vat hand's giving out Longmead's bread for the burned houses, eh? Find her at the press and she'll load you up.",
 "A shepherd took his rope to the Upper Mere shed.":"A still hand took his rope to the Upper Mere shed.",
 "The nurse at the Sump Infirmary wants willow bark and legs.":"The honest apothecary at the Mortar and Pestle wants willow bark for the infirmary, and legs.",
 "A skipper at North Cove says somebody lights a lamp there.":"A crane hand on the Wharf says somebody lights a lamp there.",
 "A pup, they say, no Gift at all.":"A young Sump wolf, they say, no Gift at all.",
 "The lender at Vance House wants a plain answer.":"The Vance at the Rag Mill wants a plain answer.",
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
# The large lines' lines "for wave G" (quests-large/*.notes.md, "Talk lines for wave G"; doc 86, "Content, today-only
# waves as built", open problem 2), word for word. A target's or giver's own greeting goes to the hub of its tree
# (node 'hub': resident.tree; 'watch.hub': watch.tree), priority 4 like the cast's (gen.py); the rest to news, gossip and
# inn.greet, with the generator's priorities (gossip none, so a real claim still competes; news 2; recaps 1). Each is
# gated on its line's talk key (C8, or the DM by hand), so none plays until the key is set. A speaker whose accent isn't
# `none` and whose words are their town's carries self.from (live.npc_voice home_region, read only 2026-11-22); the
# others speak plain. Four lines the notes reworded from the design join the design's line as a second alternative
# (ALSO, below) rather than say the same thing twice. Four are trimmed to the library's 160 characters (Iris,
# Arne, Stone and the weir's directions), as the notes trimmed Harl's and Corwin's.
def gate(*g): return list(g)
NOTES = [
 # lq-black-hearths (lq-black-hearths.notes.md): the four witnesses, the carter, the contract's thanks, the thin purse.
 ('lq-black-hearths','hub',gate('self.id=cty_ebba_coalgate','self.from=frosthollow','world.mem.lq_hearths=undersold'),"Half, cousin. Half, and a face at the store like I'd brought them mud. Those sacks were dry when I loaded them. Cold iron, I'd swear it on the top bend."),
 ('lq-black-hearths','hub',gate('self.id=cty_iris_fernhaven','world.mem.lq_hearths=undersold'),"I came up late, the way I always do, and the east clamps were open on the side and steaming. Wet ash, all of it. We raked it out as waste. By noon it was gone."),
 ('lq-black-hearths','hub',gate('self.id=rm_arne_ashton','world.mem.lq_hearths=undersold'),"Strangers bring sacks to the yard door at half price, dry as a good song. I didn't ask where from. I should have. The hill carter gets what's left of our coin."),
 ('lq-black-hearths','hub',gate('self.id=cty_stone_acorndell','self.from=frosthollow','world.mem.lq_hearths=undersold'),"'Arth, cousin. Twelve sacks of waste raked out, twelve gone by noon. I counted. Drag marks north, deep, to the tarn. Black seam, somebody's drying our char."),
 ('lq-black-hearths','hub',gate('self.id=cty_leif_driphand','self.from=frosthollow','world.mem.lq_hearths=undersold'),"A cart on the quarry road at night, no lamp, black sacks to the tail. Going west, to the city. Nothing good goes that way in the dark, cousin."),
 ('lq-black-hearths','hub',gate('self.id=rm_arne_ashton','world.mem.lq_hearths=contract'),"A price by the sack, and dry sacks from the hill. No more strangers at the yard door. I'd sing about it if you stood still long enough."),
 ('lq-black-hearths','hub',gate('self.id=cty_ebba_coalgate','self.from=frosthollow','world.mem.lq_hearths=contract'),"Marketday, down the pass road with a full cart and a paper that says what they'll pay. Warm hearth, cousin. That's all a carter wants."),
 ('lq-black-hearths','hub',gate('self.id=cty_marl_craghearth','world.mem.lq_hearths_thin=yes'),"The stores are scraped bare this week. Bread, and my thanks, and a riddle owed. Sit long enough and you'll get the riddle."),
 # lq-eel-famine (lq-eel-famine.notes.md): where the weir is, Ivo's thin purse (the Fenshore inn's recap is in ALSO).
 ('lq-eel-famine','news',gate('here.region=fenshore','self.dialect=mirebridge','world.mem.lq_eels=weir'),"Take friends and a strong back to the weir below Rivermouth. The guards come out after dark and don't like being outnumbered. The stakes pull easy in the mud."),
 ('lq-eel-famine','hub',gate('self.id=cty_ivo_sootridge','self.from=fenshore','world.mem.lq_eels_thin=yes'),"#thanks#, fen-friend. The stores are sunk this week, so it's bread and my word. The eels'll pay you back, mark me."),
 # lines 5-8 (lq-lines-5-8.notes.md): the conversation targets' own words. Rupert's key is the npc key the design gives
 # (quests-large.md, runnel_draught: npc key lq_rupert=paid), so self.mem. Theodora's needs the Residence's cough not yet
 # tended (campaign nh_b_cough off): its talk key is nh_cough=tended (cast-and-talk §4), so nh_cough!=tended.
 ('lq-blight-and-boar','hub',gate('self.id=tn_oh_harl_oakbough','self.from=southmeadow','world.mem.lq_vines=blight'),"Down from the Southwood, along the stream, under the old fence. I was lost three days in that wood once. I know its paths. Good harvest if you find the gap."),
 ('lq-blight-and-boar','hub',gate('self.id=tn_oh_harl_oakbough','self.from=southmeadow','world.mem.lq_vines=reckoning'),"Where I was raised they paint a sick root with oak bark and lime, and pray to the Bright One it takes. Three of bark and two of lime, and I'll do the painting."),
 ('lq-runnel-fever','hub',gate('self.id=rm_liv_slagstack','world.mem.lq_runnel=fever'),"Thyme syrup? Bring me herbs and I'll brew it the old way, with honey, not sugar. A spoon at night and one at dawn. I cough in the damp myself."),
 ('lq-runnel-fever','hub',gate('self.id=rm_halvard_ashley','world.mem.nh_stage=howls|letters|lights|harvest|bell|moot','world.mem.nh_cough!=tended'),"Boil the Runnel water. And they say the cough's up at the Residence too. Damp doesn't care whose lungs it gets."),
 ('lq-runnel-fever','hub',gate('self.id=rm_rupert_fogton','self.mem.lq_rupert=paid'),"Come to count my bottles again? Count away. Every penny went back to the courts, and I'll thank you not to mention it."),
 ('lq-wreckers-light','hub',gate('self.id=rm_dorcas_rainford','world.mem.lq_wreck=lamp'),"Full measure to the Lamp Store, and one of their lamplighters buys a jar more of her own on blowy days. Says the wind snuffs her wicks. I've said nothing."),
 ('lq-wreckers-light','watch.hub',gate('self.id=rm_bettany_nailley','world.mem.lq_wreck=watch'),"A wrecker of Grayrock ships, and I'm the one who locks her up. The Bright One has a sense of humour. Thank you. I mean it."),
 # lq-vanishing-thief (lq-vanishing-thief.notes.md): the House's thin purse, in the voice of the Vance at the Rag Mill.
 ('lq-vanishing-thief','hub',gate('self.id=rm_eleanor_vance','world.mem.lq_thief_thin=yes'),"The treasury's short this week, so it's bread and my thanks. Bright One knows I've had my fill of counting coin."),
]
for q,n,g,l in NOTES:
    r.append(dict(src='notes',quest=q,node=n,gates=g,line=l,ln=0))
# The notes' rewordings of four design lines (black-hearths 2, eel-famine 2), kept as the line's second alternative.
ALSO={
 ("lq-black-hearths","Upper hearths charcoal at half price in the city. Dead rock luck"):"Upper hearths charcoal at half price in the city. That's no cracked seam, cousin. That's somebody's hand in it.",
 ("lq-black-hearths","Frosthollow charcoal on the Frosthollow square, Marketday."):"Frosthollow charcoal on the Frosthollow square, Marketday. Sound sacks at our own price, the way it should be, stone.",
 ("lq-eel-famine","The eels are up Withy Creek again, thick as reeds."):"The eels are up Withy Creek again, thick as reeds. Mud and eels, there's supper in every pot.",
 ("lq-eel-famine","No eels running this season. The skipper at Fen Shore landing"):"The eels never came up Withy Creek. The skipper at Fen Shore landing thinks somebody's stopped them on the river.",
}
for (q,a),b in ALSO.items():
    hit=[x for x in r if x['quest']==q and x['line'].startswith(a)]
    if len(hit)!=1: raise SystemExit(f'ALSO: {len(hit)} lines of {q} start {a!r}')
    hit[0]['line']+=' | '+b
# Lines in a resident's own window carry that resident's speech too (live.npc_voice, read only 2026-11-22).
# (The garland line moved from Willa Wheatcombe to Briar Sedgebridge, who speaks plain: doc 86, givers behind locked doors.)
for x in r:
    x['gates']=['self.id=cty_briar_sedgebridge' if g=='self.id=cty_willa_wheatcombe' else g for g in x['gates']]
SPEECH={'cty_tristram_rainridge':'oakhaven','cty_ebba_coalgate':'frosthollow',
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
