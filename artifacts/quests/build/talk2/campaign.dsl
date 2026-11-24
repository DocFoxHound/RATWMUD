# The Ninth Howl: gossip and helpful knowledge (cast-and-talk.md §4), as the generator reads it.
# "= <stages>" opens a stage block (none: no stage gate; after: the epilogue, no stage gate). "- <dialect>" a dialect.
# A line: KIND [tokens] :: text. Tokens: @town (here.region), stage=<v> (overrides the block's stage), weather=fog,
# time=dusk|night, fig=<figure> (the figure must be set), quarter=<q>, any key=value (world.mem.<key>), any a.b=c as is.
# KIND: R rumour, W where to go, A who to ask, V villain, ! warning, H thanks, X recap (plain, no dialect gate).
# Changes from the design (decisions §6.4): no line dates the Moot; "Midwinter" only under nh_moot=midwinter.

= none
- plain
R nh_debts=grace :: The factors gave a moon's grace. Generous of them, they say. It only means the interest runs a month longer.
- coldwater
R nh_debts=grace :: A moon's grace from the factors, eh? Like letting a fish run with the hook still in. Deep and dark, it comes in all the same.
- larkfield
R nh_debts=grace :: A moon's grace on every note, #address#. Fair price, they call it. Weigh the interest, then tell me it's fair.
- southmeadow
R nh_debts=grace :: The factors gave the debtors a moon's grace. Chaff and promises, #address#. The note's still due, only later.
- mirebridge
R nh_debts=grace :: A moon's grace, no? Boggy sort of grace. The water's still rising, only slower.
- saltwell
R nh_debts=grace :: The factors gave a moon's grace, #address#. A grace you pay interest on is not a grace. It is a longer leash.
- oakhaven
R nh_debts=grace :: A moon's grace on the notes, mm? Bark and branch, the rot keeps working under the bark.
- frosthollow
R nh_debts=grace :: The factors gave a moon's grace. Dead rock talk. The note weighs the same, only colder.

= king|moot_called
- plain
X stage=king :: The old King's gone up to the Bright One, and a Sump patrolman keeps the Palace lamp. The herald's sending word to every hearth.
X stage=moot_called :: For anyone new on the roads: the old King's gone up, a Sump patrolman keeps the Palace lamp, and the herald's called a Moot of the whole realm.
R :: The old King's gone up to the Bright One. And they say the one keeping the Palace lamp still walks his Sump round.
A @ridgemere :: Looking for the wolf with the lamp? Ask on the Sump Stairs. The porters always know who's passed.
W @ridgemere :: The Palace wants its word carried to every hearth. The herald hands out the Summons at the Antechamber.
V :: There's a camp on the Oakhaven road that won't starve out. Somebody's feeding it.
! :: The notice on the Oakhaven board was signed with a grey paw-mark. Nobody signs like that who means well.
H :: Whoever's carrying the Summons out to the far towns, blood and bread, the whole city owes them a cup.
H nh_alder=cleared :: Whoever broke the Alder Mere camp, the carters drink to them now.
W @ridgemere :: The baker on Market Square is sending bread to the Soup Kitchen for the wake. Carry a basket if your paws are free.
W @ridgemere :: The City Office has more Summonses than clerks. A clerk there wants one walked out to a Rainward farmstead.
A @ridgemere :: A Watch guard means to walk the Sump round the lamp-keeper left. Ask at the Watch Office if he wants company.
R stage=moot_called :: A Moot! Blood and bread, I never thought I'd see one called.
A @ridgemere stage=moot_called :: The herald takes pledges at the Palace door. Pledge to a town and your deeds carry its howl.
R stage=moot_called nh_purse=greatly :: The Counting House paid for the old King's wake. Fine wake. Long account.
R stage=moot_called nh_purse=household :: The Queen's household paid for the wake from its own purse, they say. Bright One bless a house that pays its way.
- coldwater
R :: A patrolman keeping the Palace lamp, eh? Still water runs strange in the city.
W :: The carter's cart went out on the Oakhaven road and hasn't floated home. Net and line, somebody look below Alder Mere.
A @coldwater :: The loom master takes the city's word for the town. If you carry a Summons, it's hers to hear, eh?
V :: Wet ash on the wind from the Alder Mere side. That's no fisher's fire, eh?
! :: Don't take the Oakhaven road alone. Something's camped under the oaks, eh?
H :: Whoever walks the city's word up to the lake, calm water to them. We're not often asked, eh?
H nh_willa=home :: Our carter's home! A wolf of the road walked her in. Calm water to them.
R nh_willa=alone :: Our carter walked home alone. Nobody came for her. Muddied, that.
R stage=moot_called :: A Moot, eh? The old folk speak of one. Said it was the only time the city asked the lake anything.
- larkfield
R :: A Watch guard keeping the Palace lamp? By the market cross, the Houses will have that weighed and short-weighed by Marketday.
W :: The Summons came on the cart. Carry one on to the looms if you're going: Larkfield wants its own reading, #address#.
A @larkfield :: The linen master's the one to hear the city's word. She'll weigh it fair.
V :: Somebody's buying the Oakhaven road off the carters. Fair price? Not to the carters.
! :: Wagons to the Sound Road go in pairs now. Scales and silver, I'd not send one alone.
R :: The market beam weighs light, some say. Weights and measures, and the factors still want paying by the new moon.
H nh_alder=cleared :: The Alder Mere camp's broken. Full weight to whoever broke it.
R stage=moot_called :: A Moot! Every town a voice. That's the first fair market the city's ever held.
- southmeadow
R :: The old King's gone up, they say. Sheaf and sickle, I hope the next one likes apples.
W :: We've had no city word in a long while. Blight take the roads. Bring the Summons south and you'll be stood a cup.
A @southmeadow :: The Larkholm farmer walked to the city once. She'll take the Summons for the town.
V :: Carters say the Oakhaven road's got a toll on it now, and no tollhouse.
! :: Don't send a cart north alone, neighbour. They wait for the lone ones.
H :: Whoever walked the Summons all this way south, they'll have a cup of the new wine from me.
R stage=moot_called :: A Moot of every town? Ripe days for talkers, then.
- mirebridge
R :: That lamp-keeper's got fen in his mouth, I hear. Talks like the old ones on the boardwalk, no?
R :: Mirebridge-born, they say, the wolf with the lamp. Dark One's mire, the drowned get a king at last.
W :: Fen-sure, the city boards are slow. Walk the Summons down the Fen Road yourself and you'll be the first.
A @fenshore :: The stores keeper takes the city's word for the boardwalks. Mind, he'll argue with it.
V :: Grey paw-marks on the Oakhaven board, no? Boggy business, signing with a paw.
! :: Keep off the Oakhaven road alone, #address#. Something's sitting on it.
H stage=moot_called :: A Moot called by a fen wolf's lamp. Dry feet to the herald who said it out loud.
- saltwell
R :: The Summons reached the well, #address#. The well remembers who drew from it. We will see whether the city does.
R :: A guard keeping the Palace lamp. In the oasis we would say: a well is kept by whoever draws the water.
W :: If you carry the city's word, #address#, the far towns want it most. Saltwell has heard nothing since the last caravan.
A @saltwell :: The keeper of our stores hears the city's word for us. Bring it to her shade.
V :: Caravans say the north road has a keeper who is not the Watch. By salt and water, I mistrust a keeper with no face.
H :: Whoever walked the Summons to the pool, may their well never fail.
! :: Caravans that walk the north road alone come in short, #address#. Walk in company, or wait for one.
- oakhaven
R :: Slow news, the city. A king gone up a fortnight before the Sound Road hears it, mm?
W :: Camp's on the Alder Mere side, under the windthrown oaks. You'll smell it before you see it, mm?
A @oakhaven :: The logger's not the thief they say. Ask her about the wool, and smell her paws, mm?
V :: Grey paw-mark on our board. Proper strange. Wolves who sign with a paw don't sign with a name.
! :: Don't walk the Oakhaven road alone. Bark and branch, we've lost a wagon already.
H :: Shade on whoever carries the city's word down the Sound Road, mm? Slow news, but welcome.
H nh_logger=cleared :: The logger's cleared. Somebody found the wool smelling of smoke, not sap. Good bark, that wolf.
R nh_logger=believed :: Still a thief, the logger, if you ask the store. I don't ask the store, mm?
- frosthollow
R :: Patrolman keeping the Palace lamp. Deep strange. Keep your lamp lit, they say. Well, someone is.
W :: Carters down from the Pass say there are lamps on the High Road that aren't ours. Cold iron. I'd walk in a pair.
A @frosthollow :: The miner who watches the road takes the city's word for us. Short with it, but he hears.
V :: Somebody's paying carters for their days. In Frosthollow we call that buying a road.
! :: Don't take the Pass Road alone, #address#. Lamps where there's no house.
H :: Whoever walked the Summons up the Pass Road, lamp's thanks. The valley's not used to being asked.
R stage=moot_called :: A Moot. The city remembered the valley. Lamp and pick, it'll forget again by spring.
H nh_alder=cleared :: The Alder Mere camp's broken, the carters say. Lamp's thanks to whoever went in.

= reckoning
- plain
X :: You've missed a moon of it. The herald called a Moot, and now the Counting House wants every debtor's howl by the new moon.
R :: Chalk on the doors in the Sump. The Counting House wants every debtor's howl for the Moot.
R nh_debts=eased :: A wolf of the road worked off a neighbour's note and asked nothing back. That's the Sump I remember.
W @ridgemere :: Want the notes? The Pawn and Ledger buys and sells them. Smell the ink: Counting House ink smells of lamp oil.
A :: The Greatly daughter reads in the gardens. They say she's found a sum that won't sit still.
V :: The Counting House master gives every debtor a moon's grace. Grace, he calls it.
V fig=villain :: #world.figure_villain# gives every debtor a moon's grace. Grace, he calls it.
! :: Don't sign anything the clerks bring round. Chalk washes off. Ink doesn't.
H nh_debts=eased :: The lamplighter's note is paid. A wolf of the road did it. Blood and bread, the streets are brighter.
R nh_debts=called :: They called in every note at the new moon. Half the Sump's howl belongs to the Counting House now.
R nh_ledger=shown :: A ledger went to the Antechamber, and the Sump's doubled debts are struck. Bright One keep whoever carried it.
R nh_ledger=ash :: Somebody burned a Counting House ledger, they say. Ash doesn't testify, but it doesn't lie either.
R nh_loan=taken :: The Crown took the Counting House's loan. Every small note paid, and the Crown in the book instead.
R nh_loan=refused :: The Queen's household turned down the Counting House's loan. The notes stand, but the Crown's its own.
W @ridgemere :: The City Office still has Summonses to walk out to the Rainward farmsteads. Ask a clerk there; they're short of feet.
H :: Blood and bread to whoever lends a debtor a paw at her work and won't take a penny for it.
- coldwater
R :: Factors chalking doors in the city, eh? Out here we'd call that marking a net that isn't yours.
W :: The carters' yard in the city is buying wagon days by night. Somebody's paying to know where our carts go, eh?
A :: Ask a debtor what work wants doing. A day's work for a neighbour pays a note in the old way, eh?
V :: Counting House pennies at the carters' yard. Deep and dark, those coins swim upstream.
! :: Don't lend your wagon days to a guard with his badge cut off, eh?
H nh_debts=eased :: A wolf of the road paid off a city note with work. Calm water, there's hope in the city yet.
A @coldwater :: The loom master takes Coldwater's pledges for the Moot. Howl for the lake and she'll keep your name, eh?
- larkfield
R :: Short-weighed, the lot of it. Fair price to you, they say, and the scale's got a thumb on it.
W @larkfield :: Weigh a sack at Longmead and weigh it again at the press. The printer wants the difference in ink, #address#.
A @larkfield :: The printer's making handbills: a debt is not a voice. Ask him for a stack to carry.
V :: The Counting House lends with one paw and weighs with the other. Weights and measures, I know a cheat's scale.
! :: If a clerk asks you to sign for a neighbour, read it twice and sign nothing.
H nh_handbill=up :: The handbill's on every board. A debt is not a voice. Sold, to whoever wrote it.
A @larkfield :: The stores keeper weighed his own sack on the market beam and it came up short. Ask him, #address#. He's quiet, but he's counting.
- southmeadow
R :: Notes due at the new moon? Chaff and promises, #address#.
W :: Help a debtor with a day's work and the city counts it paid. A good harvest from a little sweat.
A :: The city's herald owes a note herself. Free hers and the whole Moot owes you a cup.
V :: The Counting House master wants every howl in the realm. Blight take him, he can't carry a tune.
! :: Don't let them chalk your door, neighbour. Scrub it before the moon's up.
H nh_debts=eased :: The new moon came up on paid doors, they say. Ripe days.
A @southmeadow :: The Larkholm farmer takes Southmeadow's pledges for the Moot, #address#. Pledge to us and your good deeds carry our howl.
- mirebridge
R :: Owe a House a penny and they own your boots. Lucky we go barefoot in the fens, no?
W :: The Fenshore boats won't carry Counting House chests. Boggy coin, that.
A :: Want to know who sold whose note? The cells in the city. The keeper there remembers everything.
V :: A clerk in the city smells of smoke. Fen-sure, some fires aren't for warming.
! :: Don't take the House's coin for your howl, #address#. Mirebridge sold its say once, and look where it got us.
H :: Whoever works the city's lamplighter free of his note, dry feet to that one.
A @fenshore :: The stores keeper takes Fenshore's pledges, no? Pledge to the boardwalks and every deed you do howls for us.
- saltwell
R :: A loan the borrower cannot refuse is not a loan, #address#. It is a leash.
W :: Bring bread and salt to a debtor's door, #address#. In the old way, a guest's gift cannot be called in.
A :: The pawnbroker in the city knows every note, #address#. Bring her a gift; trust is bought slowly there.
V :: The master of the Counting House has never once been thirsty. That is how you know a dangerous wolf.
! :: Do not pledge your voice for coin, #address#. The well remembers who drew from it.
H :: May the well never fail whoever frees the city's herald from her note.
A @saltwell :: The keeper of our stores takes Saltwell's pledges, #address#. Pledge in her shade and your deeds carry the well's voice.
- oakhaven
R :: They chalk doors in the city, mm? We'd mark a tree that way before we felled it.
W :: A debt's lighter carried by two. Lend a paw at a debtor's work and see, mm?
A :: The Grayrock wharf office knows which wagons go out by night. The harbourmaster says little, slowly.
V :: Bark and branch, a lender who wants your howl wants your whole tree.
! :: Don't sell your wagon days, old root. You'll want the road back.
H :: A wolf who frees a debtor and asks nothing back, mm? Good bark, that one.
A @oakhaven :: Ask at the store counter about pledging for the Moot, mm? Somebody there will point you right.
A @oakhaven nh_logger=cleared :: The logger takes Oakhaven's pledges now, mm? Sap on her paws, not smoke. Howl for us there.
A @oakhaven nh_logger=believed :: The store takes Oakhaven's pledges while the logger's name stays muddy, mm? Ask at the counter.
- frosthollow
R :: Captain's quiet. Too quiet. Dead rock talk, that note on the mine.
W @frosthollow :: The mine's short of ore and the note's due. Bring a load down to the mine house and it's one less hold on us.
A @frosthollow :: The miner who watches the road knows the quota. Ask him, short as he is.
V :: The Counting House rider comes up the Pass Road. Lamp and pick, somebody wants his letters lost.
! :: If the mine captain offers you a share in anything, count your claws after.
H :: Lamp's thanks to the road wolves hauling ore with us. The note's lighter for every load.
H nh_seam=honest :: The quota's met. A road wolf hauled ore with us. Lamp's thanks.
R nh_seam=crooked :: The quota's met, they say, with ore that isn't ours. Black seam. Somebody'll pay for that.

= signals
- plain
X :: Lamps on the ridges, a captain in grey taking a mark a wagon, and the Watch's own lists going round. That's how things stand.
R :: Somebody's gathering the lists of who pays the Watch. Mind whose name you say aloud in a taproom.
R nh_lists=press :: It's printed. The whole list, on every board. Half the Watch can't meet your eye.
R nh_lists=sold :: The lists went to the Counting House, they say. The sergeant's on the Harbour Gate now. Funny, that.
R nh_lists=captain :: The Watch Captain burned the lists in front of the sergeants, they say. Nobody on them pays twice now.
R nh_lists=lost :: The Watch lists went into the river, they say. Half the Watch slept easy that night.
A @ridgemere :: They say the abbot keeps a list too, and shares it only after the Restday service.
W :: The fish-hall papers smell of fish oil. A real Watch list smells of a Watch House: wet cloaks and old ink.
V :: Three Houses on every list, they say. One on none. Ask yourself why it needn't pay.
! :: The Cinder Gate looks away for a grey paw-mark. Don't trust that gate after dark.
H nh_lists=bettany :: The sergeant took the list to the magistrate. Door open, at noon. Blood and bread.
W @ridgemere :: The Masons' Yard wants stone carried out to the Line posts. The Yard's keeper calls it a gift to the realm.
A @ridgemere :: The Carters' Yard wants a wolf to walk beside a wagon to Coldwater, past the lit ridges. Ask in the yard.
A @ridgemere :: A Watch guard walks the Sump round the lamp-keeper left, and wants company of a night. Ask at the Watch Office.
H nh_dark=1 :: No lamps on the ridges last night, the carters say. Blood and bread to whoever sat up there.
- coldwater
R :: Lamps on the downs, two long and one short. That's no fisher's light, eh?
W :: Stand on the ridge where they burn and they won't light it. They don't like being seen, eh?
A :: The shepherd at Greyteeth counts lamps from his barn. Ask him why he counts, eh?
V :: A captain in grey taking a mark a wagon, eh? Deep and dark, that's a toll with no tollhouse.
V fig=villain nh_captain=named :: #world.figure_villain#, they're calling him. Keeps the roads, he says. Keeps them shut.
! :: The camp above Glasswater comes back in threes. Don't go in alone, eh?
H nh_dark=1 :: No lamps on the downs last night. Calm water to whoever sat on that ridge.
A @coldwater :: The Greenholt carters bring their wagons in by the square. Ask them which nights they saw the lamps, eh?
- larkfield
R :: A Watch you can buy is a scale you can lean on. Scales and silver, I'd like to see that list.
W :: The camp above Glasswater comes back in threes. Take a friend, and then another, #address#.
A :: The Glasswater road's half ours. Ask the carters which nights the lamps burned.
V :: Two long and one short. Somebody's selling our wagon days, and the price is a lamp on a hill.
! :: Don't buy a list from the fish hall. It's short-weighed paper.
H nh_glasswater=open :: The Glasswater road's open. Wagons in, wagons out. Full weight.
A @larkfield :: The Longmead farmer knows which nights the wagons didn't come. He counts sacks, and he counts carts, #address#.
- southmeadow
R :: The fish-hall wolf sells papers, and they smell of fish oil. Blight take it, don't pay her a penny.
W :: Two long and one short means a wagon going out. Get to it before they do, #address#.
A :: Ask a carter who's lost a wagon lately. Every one of them saw the lamps first.
V :: Somebody in grey keeps the roads now, they say. Wormy sort of keeping.
! :: If you see two long and one short, don't wait to count again.
H :: A wolf sat on a ridge all night so a wagon of ours came through. Sheaf and sickle, I'll stand them a cup.
A @southmeadow :: Our carters wait by the press house. Ask one which ridge lit up when they went north, #address#.
- mirebridge
R :: In the fens the Watch never came at all, so nobody paid. Boggy sort of honesty, no?
W :: Lamps on a ridge want oil. Follow the smell of it uphill, #address#.
A :: The old ones in Frosthollow say there's a list of who shared lamp oil one bad winter. Ask the city's records.
V :: Grey paw-marks on every board now. Fen-sure, he's proud of it.
! :: A lamp you can't see the house behind is a lamp you don't follow.
H nh_dark=1 :: Somebody put the ridge lamps out. Dry feet to them, wherever they're drying.
H :: A wolf of the road sat up on the ridges so the wagons could pass. Dry feet to them, no?
A @fenshore :: The Fenshore carter takes the north road twice a week, no? Ask him which ridge he'd not pass these days.
- saltwell
R :: A Watch that is paid by a House guards that House's well, not yours, #address#.
W :: Four ridges, four lamps. One wolf on each and the company is blind, #address#.
A :: By salt and water, the shepherd on the Greyteeth counts every lamp. Ask him why he counts.
V :: A road kept by no one is kept for someone, #address#. Ask who the grey keeper keeps it for.
V nh_captain=named :: Caravans called him the keeper with no face. Now he has a name, they say, and a grey cloak.
! :: Do not buy secrets that smell of fish, #address#. Sweet water does not smell of anything.
H :: The road to the well runs clear this week. May the well never fail whoever cleared it.
A @saltwell :: The keeper of our stores counts every caravan in off the north road, #address#. Ask her which ones came in short.
- oakhaven
R :: Rot starts under the bark, mm? You don't see it till the tree's down.
W :: There's a lamp-stone on the Alder Mere knoll now. Sit by it of a night and see who comes, mm?
A @oakhaven :: The printer counts every stake and every lamp. Ask her the tally, slow.
V :: A captain of the roads with no road of his own. Windthrown sort of wolf, mm?
! :: Don't walk under a lit knoll, old root. They're watching from up there.
H nh_dark=1 :: The knoll was dark last night. Somebody sat on it. Good bark.
W @oakhaven :: The Line post by Alder Mere wants stone and timber, mm? Carry them out and gift them at the post.
- frosthollow
R :: A harness turned up on the Pass Road, old and sooty. Deep strange.
V :: Somebody's lighting lamps on our ridges and it isn't us. Lamp and pick, I'd like a word with him.
V fig=villain nh_captain=named :: #world.figure_villain#, they're saying. Knows the dark like a good seam.
W :: Lamps burn on the Upper Mere ridge after the second bell. Go up with your own lamp out.
A @frosthollow :: Ask the miner who watches the road about lamp-black. He knows the smell of it better than he'd like.
! :: If you hear two long and one short from a ridge, #address#, you're already seen.
H :: The Upper Mere post's going up. Cold iron, there's a light on the road that's ours.
W @frosthollow :: Our Line post wants stone and timber, #address#. Gift them at the post; the city yards send some, never enough.

= howls
- plain
X :: The herald says a Moot needs nine howls, and half the towns sang the burying one at the feast. They're finding the old ones now.
R :: Eight towns howling at dusk and one silent. The old folk say the ninth was the loudest.
A :: Want the old howl? Ask the oldest wolf in town, then ask her again. She'll give you half the first time.
W @ridgemere :: The herald stands on the square at dusk. Howl with her and the city's howl comes back.
V :: The Counting House is buying howls, they say. A penny a throat. Ask yourself what a bought Moot sounds like.
V nh_relay=ran|broke :: The ridge howled back two long and one short on the relay night. That wasn't ours.
! :: The Counting House says a bought howl still counts. Buy yours back if you sold it.
H :: Somebody's carrying the old howl from square to square. Blood and bread, it's been years since I heard it right.
H nh_howl=found :: The square came out at dusk and howled the old one right. The Bright One heard that, I'd say.
R nh_cough=tended :: The King's daughter climbed the Hundred Steps without stopping. Somebody sat with her.
W @ridgemere :: The guard holding the north gate wants the road kept safe for the fair-goers in the feast days. Ask him at the gate.
- coldwater
R :: A gathering howl? We've one for the drowned, one for a catch and one for supper, and I'd not swear which, eh?
A @coldwater :: The loom master knows the old howl. Catch her on the square at dusk and she'll start it if you'll carry it, eh?
W :: The mine captain at Frosthollow wants rope for Highmere. Something under that ice besides the big thing, eh?
V :: Somebody wants the old howls lost, eh? Deep and dark, a Moot with no howl can't sit.
V nh_relay=ran|broke :: False howls off the ridge, two long and one short. Deep and dark, he even steals our singing.
! :: Don't howl the burying one at a wedding. It's been done. It was awful, eh?
H :: Somebody sat with the old ones and caught the howl off them. Calm water, that's patience.
H nh_howl=found :: Coldwater found its howl! Calm water, it went up like a rising fish.
- larkfield
R :: Larkfield's howl goes up at the end like a price. Scales and silver, it's the best one.
A @larkfield :: The pot hand at the Sheaf sings the old Mirebridge turn when he's had two. Buy him the second, #address#.
W @larkfield :: The linen master starts the chorus at dusk. Stand by her and it counts.
V :: A howl from the ridge, they say, and it wasn't from any town. Somebody's put a price on our song.
! :: If you've sold your howl to the Counting House, buy it back. It's worth more than a note.
H :: A road wolf stood in our chorus and didn't miss a turn. Full weight, that.
H nh_howl=found :: Full weight! Larkfield howled the old one right and half the Downs came to hear.
- southmeadow
R :: Our gathering howl? Blight take it, I've only ever known the harvest one.
R nh_howl=found :: A good harvest of a night! Half the vale came out to howl.
A :: The cellar hand says the old stone on the Sunfields remembers. She says a lot of things, #address#. Some are true.
W :: The bee-keeper's honey's what the cellar hand wants. Don't ask why. Nobody ever knows why.
V :: Somebody chiselled a name off the Sunfields stone. Wormy business, that.
! :: Don't sing at the stone of a night unless the sky's clear. The cellar hand says so, and she's odd but right.
H nh_stone=read :: The stone's read, nine names, one chiselled out. Somebody went up four nights for it. Ripe days.
W @southmeadow :: The Larkholm farmer starts our chorus at dusk on the square. Stand by her and lift it round and warm, #address#.
A @southmeadow :: A young Larkholm farmhand sang the burying howl at a wedding. Now he wants mead and honey for the families. Ask him, #address#.
- mirebridge
R :: Ninth howl's gone under, #address#. Mirebridge got drowned, and the drowned don't howl. Or do they, no?
A :: Ask the eel-trappers who came out of Mirebridge. Every one of them has a scrap of the old howl, fen-sure.
W :: The creek at dusk. The eel-trapper sings the first turn there, twice round the reeds.
V :: The Houses struck Mirebridge off the rolls. Struck it off so the Moot could never sit, they say.
! :: Don't sing the drowned howl on the boardwalk of a night. The old ones don't like it.
H :: Somebody's carrying the old howl from town to town. Dry feet to them. The drowned would thank them.
W @fenshore :: The stores keeper howls on the boardwalk at dusk. Bring him the scraps you've gathered, no? Ours went thin when Mirebridge went under.
- saltwell
R :: The old one at the salt pans keeps the well's roll, #address#. Every caravan that ever drank is in it.
A :: Sit with her on Restday, #address#, three times. The well remembers who drew from it, and the howl remembers who sat still.
W @saltwell :: The keeper of our stores starts the howl at the pool when the water is still.
V :: A howl that answers from a ridge with no town under it is a mirage, #address#. Do not follow it.
! :: Do not hurry an old wolf's song. Saltwell begins nothing in a hurry.
H :: A wolf of the road sat three Restdays with the old one at the pans. The well remembers patience, #address#.
H nh_howl=found :: The pool heard its own howl again. May the well never fail whoever sat still for it.
- oakhaven
R :: Our howl's slow and it climbs, like good bark. Come by the square at dusk, mm?
A @oakhaven :: The keeper starts it low. Don't rush in. A sapling that rushes falls, mm?
W @oakhaven :: Three old roots in town know a piece each. Ask slow, and twice.
V :: Somebody's teaching false howls from the ridges, mm? Rotten at the root, that.
V nh_relay=ran|broke :: False howls on the relay night, they say. Somebody's got lamps and a throat.
! :: The relay night wants a wolf at every post. One gap and it limps, mm?
H :: A road wolf asked three old roots for the howl, slow and twice. Good bark, that one.
H nh_relay=ran :: Oakhaven answered on the relay night. Proper, it did.
- frosthollow
R :: In Frosthollow the warning howl goes low, so it carries down the shafts. The gathering one's lower still.
A :: If the mine captain asks for rope, he's after the old King's silver under Highmere. Lamp and pick, go in a pair.
W :: Highmere's iced at the edges even now. Haul together or don't haul.
V :: Deepvein wolves knew a howl too, once. Nobody's heard it since the deep winter.
! :: Don't go onto Highmere alone, #address#. The ice keeps what it takes.
H nh_silver=crown :: The silver came down the Pass Road and the Crown has it. Cold iron, the mine's got its charter.
R nh_silver=greatly :: The silver went to the Counting House. Black seam. The mine's charter is a Greatly note now.
W @frosthollow :: The miner who watches the road starts our howl low on the square at dusk. Ask three old ones for the pieces first, #address#.

= letters
- plain
X :: A Moot called, debts fought, lamps put out, old howls found, and now forty letters chasing a lord who's never home. And black cloaks on the Pass.
R :: Someone's carrying every letter for the Grayrock lord out to his road at last. Forty of them.
R nh_grayrock=home :: The Grayrock lady walked out to the Wharf gate. Didn't even put her hood up.
A @ridgemere :: Want the Grayrock lord? Ask at the Wharf Office which road he took. He's always on one.
W :: Every wharf wolf has a letter for him. Do them a kindness and they'll give it you gladly.
V :: Some of those letters are toll receipts, they say, signed with a grey paw. Grayrock pays the roads to stay shut.
V nh_toll=cried :: The herald cried a Grayrock toll receipt on the square. The Sump cheered. The Wharf didn't.
! :: Fell wants the Rainward Hills, and Fell's green is on the wolves driving stakes. Mind the cut.
H nh_grayrock=home :: The Grayrock lord's in the Antechamber, and the Council's whole. Blood and bread, the letters did that.
H nh_wharf=heard :: The wharf drowned were read out at the Chapel of the Tide, with the Grayrock lord standing. About time.
W @ridgemere :: The bounds-walkers in the Rainward Hills want water and bread carried up. The orchard keeper out there sends for it.
- coldwater
R :: Bounders in green on the Oakhaven road, eh? Fishers don't drive stakes into the ground.
W :: If you're walking the bounds, mark the alders too. The old line runs along the water, eh?
A :: The harbourmaster at the city's Wharf Office grew up on the lake. Ask her kindly and she'll say which road the lord took.
V :: Green cloaks and grey lamps, together. Deep and dark, who's paying whom?
! :: Black cloaks on the Pass Road, eh? Wardens, they say. Fair winds to whoever they've come for.
H nh_hills=oakhaven :: The cut's still Oakhaven's, I hear. Calm water.
H :: Somebody walked the Grayrock lord's letters out to him, eh? Calm water, the wharf can sleep.
W @coldwater :: A Coldwater carter wants wolves to scout the Pass Road stage before the black cloak walks it. Ask at the carts by the square, eh?
- larkfield
R :: Fell wants the hills. Fell wants everything with a price on it, #address#.
W :: The Grayrock lady's a Larkfield wolf, did you know? Bring her something from the downs and see her face.
A @larkfield :: Thyme grows on the downs where the sheep don't crop. The linen master will know the swell.
V :: Grayrock pays one mark a wagon on the High Road. Scales and silver, to whom?
! :: Don't take a Fell errand without reading it. Fellhall has more rooms than doors.
H :: The Grayrock lady smiled at the gate, they say. Full weight, that wolf who brought the thyme.
- southmeadow
R :: A lord who's never home is like a vine never pruned. All runners, no grapes, #address#.
W :: Thyme grows on the Larkdowns swells where the sheep don't crop. A windfall, if you know where.
A :: The younger Timberland son's a Southmeadow wolf at heart. He rides out our way. Ask him what Timberland wants.
V :: Fell's green cloaks are driving stakes in Oakhaven's cut. Blight take them.
! :: Don't trust a wager in the Fell distillery. The house always drinks first.
H nh_hills=oakhaven|split :: The hills are settled and nobody's dead. Ripe days, that's a fair end.
H :: Somebody's walking the twelve bounds for Oakhaven. Sheaf and sickle, I'd stand them a cup.
W @southmeadow :: Thyme for the Grayrock lady grows on the Larkdowns, not here. Take the Downs Road north, #address#, where the sheep don't crop.
- mirebridge
R :: Forty letters and not one answered. In the fens we'd call that a very wet postbag, no?
W :: Black cloaks on the Pass Road, they say. Fen-sure they'll want walking down.
A :: The Chapel of the Tide's priest wants names of the wharf drowned. Ask the wharf; it's all wet grief there.
V :: Grey paw-marks on the lord's toll receipts. Boggy coin, that.
! :: Don't step between Fell and Oakhaven, #address#. Both sides have axes.
H nh_grayrock=home :: The Grayrock lord came home. Dry feet to whoever dragged him.
H :: Somebody carried the drowned's names to the Chapel of the Tide. Dry feet to them, no?
W @fenshore :: A deckhand on the boardwalk wants lamp oil for the fog lamps, no? Honest lamps, these. Not the warm ones in the reeds.
- saltwell
R :: The well remembers who drew from it. The lord has drawn for six years and never come back to the well.
W :: A letter carried is a debt paid, #address#. Do the carrier a kindness first, and they will give it gladly.
A :: The healer at the Sump Infirmary grew up at our well. They say he wrote to the black cloaks. Ask him why.
V :: A House that pays the roads to stay shut buys a thirst for everyone else.
! :: Black cloaks on the Pass Road, #address#. If you carry a Gift, carry it quietly.
H nh_wharf=heard :: The names of the wharf drowned were read before the Grayrock lord. May every well remember them.
H :: Somebody carried forty letters to a lord who never drinks at home. May their well never fail.
A @saltwell :: Our stores keeper still takes Saltwell's pledges, #address#. A wolf pledged to the well carries its voice on every road.
- oakhaven
R :: Walk the bounds with us? Mark every ninth cedar and the old boundary stone, mm?
R nh_hills=oakhaven :: Good bark, those wolves. The cut's ours yet. Root well.
R nh_hills=fell :: Fell's hills now. Windthrown, the lot of us. Oakhaven howls against Fell at the Moot.
W @oakhaven :: Pull the green stakes and bring them to the square. Old Oak, they burn well enough.
A :: The quarrymaster's saving for the common right. Ask her what she's short, mm?
V :: Wolves in Fell green on the cut, and the Captain's lamps behind them. Same paws, different cloaks, mm?
! :: Don't argue with a bounder alone, old root. They come in threes.
H :: Shade on every wolf who walks the bounds with us. The cut remembers who stood on it.
H @oakhaven nh_cut=ours :: The cut's ours for good. Good bark to every wolf who walked the bounds with us, mm?
- frosthollow
R :: Three black cloaks came down the Pass on Stoneday. Didn't take their boots off at the Pass House. Deep strange.
W :: The cloaks want walking down to the city. Go in a pair, #address#. The Captain's men watch the High Road.
A @frosthollow :: The Pass House keeps the cloaks a room. Ask there when they move.
V :: The Captain's bounders wear Fell green in the west, they say. Grey up here. Same wolves.
! :: Don't lie to a black cloak, cousin. Don't tell them more than's true, either.
H :: Lamp's thanks to whoever walks the black cloaks' road with them. The Pass is long and the Captain's watching.
H nh_envoy=walked :: The black cloak walked down with an escort. Lamp's thanks to whoever kept her road.

= lights
- plain
X :: Now the Houses are at each other, the fens are full of lights, and a black cloak wants one howl from the Pass to the Waste on the night the Moot sits.
R :: The Houses are at each other's throats, and a black cloak sits in the Antechamber listening to all of it.
R nh_quarrel=fell_timber :: The Timberland works burned in the night. Every smith in the Cinders swears it wasn't them.
R nh_quarrel=houses_greatly :: There's a run on the Greatly Bank. The masons have walked off the Yard.
R nh_quarrel=greatly_grayrock :: Grayrock's closed the Wharf to Greatly stone. The Wharf's idle and the Yard's shouting.
R nh_quarrel=against_crown :: The Council wants the lamp-keeper gone before the Moot sits. Petitions on every desk.
W :: A House at odds still eats. Bring its steward bread and salt. It's an old custom and nobody refuses it.
A @ridgemere :: The Records Office rolls have Mirebridge's names, they say, under the Houses' own. Ask the roll keeper.
V :: A black cloak at the Antechamber, and every House shouting. Somebody's glad of the noise.
! :: Cudgels in House colours on the roads out of the city. Walk round them.
H :: Somebody carried bread and salt to a House steward and the door stayed open. Blood and bread, the old ways.
H nh_quarrel=peace :: A wedding at the Greatly salon and every House at one table. Blood and bread, somebody kept the peace.
- coldwater
R :: Houses at each other in the city and lights in the fens. A good season to mend nets, eh?
W :: The fen lights smell of lamp oil, they say. No drowned wolf ever carried a lamp, eh?
A :: The city's Chapel of the Tide priest says names over water. Bring her one and she'll say it.
V :: Lamp oil in the reeds. Deep and dark, the same lamps as on the ridges.
! :: Don't follow a warm light in the fens, neighbour. Cold ones only, they say.
H nh_lights=laid :: The fen lights stopped. They say the priest said every name. Calm water.
H nh_cache=home :: Our stillmaster got her wine back from the drowned causeway. A road wolf carried it. Fair catch.
A @coldwater :: Our stillmaster lost her wine to the fen boats. Ask at the Still what went missing, eh? She'll know it by the smell.
- larkfield
R :: A black cloak on the Fen Road, and the price of a room on the Long River just doubled.
W :: The fen boats come up past the ford with no lamps lit. Sold goods, #address#, and not theirs to sell.
A :: The Rivermouth skipper's always a little late. Ask her where she's been, and watch her paws.
V :: Plunder up the river by night. Weights and measures, somebody's fencing the Captain's take.
! :: Don't buy cheap wool from a fen boat. You'll be buying it twice.
H nh_cache=home :: A wolf brought the cache from the drowned causeway home, piece by piece. Full weight, that.
W @larkfield :: Fen boats tie up below the ford after dark. Watch the bank from the mill, #address#, and count what comes off.
- southmeadow
R :: Fen boats on the Long River by night, no lamps lit. Blight take them, a Larkholm carter lost a cart of honey.
W :: The stone's names are nine, and one chiselled out. Take that to the fen priest, #address#.
A :: The bee-keeper lost her honey to the fen boats. Ask her what it smelled of.
V :: The Captain's southern company wades the fens with float lamps. Wormy trick.
! :: If you walk the fens by night, take a friend and a lamp you won't light.
H nh_cache=home :: The bee-keeper got her honey back! Some road wolf found it at the drowned causeway. Ripe days.
A @southmeadow :: The cellar hand at the press house read the Sunfields stone. Ask her for the chiselled name; the fen priest will want it.
- mirebridge
R :: Lights in the reeds every night now, no? The old ones say the drowned want their names said.
R weather=fog :: Dry feet. Mizzle's up. If you see a light, follow it, but only the cold ones, no?
R nh_lights=laid :: Dark marsh and dry feet. The priest from the city said every name.
R nh_lights=turned :: They used the drowned for bait. Dark One's mire, we'll pay for that.
W :: The path with no note, #address#: start at Withy Creek, keep the reeds on your left, and never step where it shines warm.
A :: The old one at Withy Creek knows which lights are which. Don't sit with your back to the reeds when you ask.
V :: Float lamps in the reeds, fen-sure. The drowned never needed lamps to walk.
! :: The potter's post went to a carpenter and the boardwalks whisper. Ask the records before you whisper too.
H nh_potter=cleared :: The carpenter's cleared. The records say what they say, and he carves birds for the pups. Dry feet.
A @fenshore :: The Rivermouth skipper ties up at our boardwalk at dawn, late as ever. Ask her what she carried, no? Watch her paws.
- saltwell
R :: In the old way, #address#, you bring bread and salt to a House at war. Even a House needs to eat.
W :: The healer at the Sump Infirmary writes to the black cloaks. If a friend carries a Gift, speak for them there, #address#.
A :: The black cloak speaks only through the healer. Ask him what she asks.
V :: Lights in the fens, fire in the city, cudgels on the roads. A mirage of three, and one paw behind it.
! :: Do not hide a Gift from the black cloak and then use it in the open, #address#.
H nh_quarrel=peace :: Bread and salt reached every House. By bread and salt, the old way still works.
W @saltwell :: Bread and salt for a House at odds? Saltwell's stores have the salt, #address#. Carry a measure to the city's stewards.
- oakhaven
R :: The Houses are windthrown in a row this season, mm? One falls and takes the next.
W :: The quarrel's carts go by the East Road. Ride with one and nobody touches it, mm?
A :: The carpenter at the Fenshore pottery's an Oakhaven root. Ask him about the potter's post, slow.
V :: Cudgels in House colours at the Rainward camps. Proper strange that a House needs a camp.
! :: Don't take sides in the city's quarrel, old root. Take bread.
H nh_potter=cleared :: The carpenter at Fenshore's cleared, they say. He carved a bird for whoever did it.
W @oakhaven :: The board on our square still asks for green stakes pulled from the cut, mm? Read it, then pull one.
- frosthollow
R :: Lights in the fens, fire in the city. Black seam of a season.
W :: The black cloak's healer asks after the Gifted. Say what's true and no more, #address#.
A :: If you want the black cloak's question, ask the healer in the city. She won't say it herself.
V :: The Captain's company's gone south to the fens. Cold iron, we're glad and we're not.
! :: One howl from the Pass to the Waste, the black cloak wants. Our post's first. Don't let it be our fault.
H nh_lights=laid :: The fen lights are laid, they say. Lamp's thanks to whoever walked the path.
W @frosthollow :: Our Line post still wants stone and timber before the Moot, #address#. Gift them at the post.

= harvest
- plain
X :: Now the Captain's at the Larkfield ford with every wolf he has, and the harvest with him.
R :: Every wolf who can stand is at the Larkfield ford. The city's quiet as a crypt.
R nh_ford=held :: They held the Larkfield ford three nights. Blood and bread, I'd have liked to see it.
R nh_ford=burned :: The ford burned, and Larkfield's houses with it. The realm will be short of bread before spring.
W :: The ford wants every paw. Take the Downs Road south, and take bread; they're short at the walls.
W nh_toll=bargained :: The Wharf says two Grayrock ships are loading bread for the ford. Somebody bargained well.
A nh_desmond=held :: The magistrate's court hears the dismissed guard's case. Sit in if you've an opinion.
V :: The Counting House is buying Longmead grain at siege price. Somebody always profits.
! :: Wolves carrying sacks from Longmead to the Counting House. Don't be one.
H :: Every wolf who went down to the Larkfield ford, blood and bread, the city's proud of you.
H nh_desmond=redeemed :: The dismissed guard stood the granary door all the third night. Mercy paid, for once.
A @ridgemere :: The sergeant at the Watch Office rides with the ford escorts. Ask her when they leave, and bring a lamp you're willing to lose.
- coldwater
R :: The Captain's at the ford, eh? Then the downs are quiet. Fair catch, for once.
W :: Bring blankets to the ford. Wolves who stand all night want warm backs, eh?
A :: The Larkfield looms are sending blankets to the ford. Ask a loom hand there; they're quiet, but fair.
V :: Lamp-black at the ford too, they say. Same lamps as on our ridges.
! :: If the ford burns, the whole realm's short of bread. Help it hold, neighbour.
H nh_ford=held :: The ford held. Calm water to every wolf who stood in it.
W @coldwater :: The Glasswater camp creeps back while every paw's at the ford, eh? The Watch in the city pays for it broken again.
- larkfield
R :: Every sack in Longmead's gone to the ford walls. Scales and silver, I never thought I'd see grain for a rampart.
R nh_ford=burned :: Short-weighed, all of it. The year, the bread, the lot.
W @larkfield :: They come up the shallows below the mill. The current's slack there after dark, #address#.
A @larkfield :: The smith knows a path across any bog. Ask her for the flank path before the river company finds it.
V :: The Captain's companies took the Downs Road barns. Weights and measures, he's counted our grain before we have.
V nh_captain=parley :: The Captain parleyed at the truce lamp. A wolf with a reason, and a reason's worse than a price.
! :: Don't cross the ford after dark unless you're standing it, #address#. Both banks are watching.
! nh_truce=broken :: Somebody struck under the truce lamp. Weights and measures, the whole Downs is talking.
H nh_ford=held :: Three nights held! Full weight, every wolf who stood.
W @larkfield :: The looms are sending blankets down to the ford. A loom hand wants them carried, #address#.
A @larkfield nh_ford=burned :: A Longmead farmhand is sharing grain with the burned-out houses. Ask at Longmead what wants carrying, #address#.
- southmeadow
R :: Every cart in the vale's gone north with something for the ford. Sheaf and sickle, the press house is empty.
R nh_runner=came :: The runner came at midnight, howling the new howl. We knew it! Sheaf and sickle, we went.
W :: If the Downs Road's held, go round by the vine lanes. Blight take the Captain, he doesn't know them.
A @southmeadow :: The Vine and Sickle keeper knows who the Longmead farmer's word is for. Ask her quietly.
V :: The Captain's companies took every barn on the Downs Road. Wormy wolves.
! :: Don't wait for a runner if you see smoke from the ford. Go.
H nh_ford=held :: Southmeadow went to the ford. Ripe days, we'll tell it at every harvest.
W @southmeadow :: A relief wagon goes from Larkholm to Larkfield in the siege nights. Walk beside it, #address#.
- mirebridge
R :: The river company comes up through the reeds. Boggy fighting, that.
R nh_sloe=turned :: Fen boats bringing grain upriver now, no? Not plunder. That's new.
W :: The Larkfield smith knows a path across any bog. Ask her before the river company does, #address#.
A :: Ask the Rivermouth skipper what moves on the river by night. She'll answer, late.
V :: The Captain's a Deepvein wolf, they say, from the high valley. Long way to come for a ford, no?
! :: Keep off the Long River banks by night. They're all fighting there.
H :: Somebody walked the river company into the shallows. Dry feet to them.
W @fenshore :: The ford's up the Long River from us. Take the river road north from the boardwalk, no? You'll smell the smoke first.
- saltwell
R :: Salt and water for the wolves at the ford, #address#. The well gives to those who stand.
W :: A runner from the ford wants the caravan arc, #address#. It is the shortest way to the pool.
A @saltwell :: Ask the keeper of our stores when the runner came. She keeps the hour.
V :: A captain who parleys has a grievance, not a greed. By salt and water, that is harder to buy off.
! :: Do not leave the pool unwatched while the south is fighting.
H :: Salt and water went north on every caravan this week. May the well never fail those who carried it.
H nh_runner=came :: The runner reached the pool before midnight. May the well never fail that wolf.
- oakhaven
R :: Half the cut's gone south to the ford, mm? The cedars can wait. Wheat can't.
R nh_captain=parley :: Old Oak, a captain who parleys. That's a wolf with a reason, mm?
W :: A truce lamp's a truce lamp. Strike under one and the whole of Cedar Sound will know by morning, mm?
A @oakhaven :: The Felled Cedar keeper hears the carters. Ask her which roads are open.
V :: Deepvein, they say. Turned out in the deep winter. Rotten thing to do to a wolf, mm?
! :: Don't go to the ford without bandages, old root.
H :: Shade on every wolf at the ford. Oakhaven's sending what it can.
H nh_ford=held :: Larkfield held. Good bark, every one of them.
- frosthollow
R :: The Captain's a Deepvein wolf. Lamp and pick, I could have told you. Nobody shared oil with Deepvein.
W :: If he names the mine house, ask the mine captain what he barred, #address#.
A @frosthollow :: The miner who watches the road went quiet when he heard. Ask him why, and wait.
V :: Deepvein. The deep winter. The mine house door. Black seam, we all knew.
! :: Don't say Deepvein in the Lamp and Pick unless you mean it.
H :: Lamp's thanks to the wolves at the ford. The valley sent rope and lamps, what we had.
H nh_ford=held :: They held the ford in Larkfield. Cold iron, the valley's proud.

= bell
- plain
X :: The Chancellor's tears, a writ the Houses buried, and a bell under the Upper Mere ice. The Moot's not far off now.
R :: Everyone's asking what the Chancellor wept over in the Rain Hall. Turns out half the city saw him do it.
R nh_writ=read :: The old King wrote it down: the Sump's own peace-keeper keeps the lamp until a Moot. So it was no accident.
R nh_oskar=confessed :: The Chancellor stood before the Council and said it all. The Counting House's debt, the order, the lot.
R nh_bell=lost :: They lost the bell through the ice. The Upper Mere kept it. Some say that's where it belongs.
A @ridgemere :: Want the Chancellor's story? Ask the porters on the Sump Stairs. One of them saw the seal on the paper.
W @ridgemere :: The Records Office rolls, the cathedral crypt and the Greatly Bank flat. One of them hides it, they say.
V :: The Counting House master told the Chancellor to lose it, they whisper. Lose a King's writ!
! :: An old rag-mill hand is selling what she saw. Get there before she gets to the City Office.
H :: Every wolf hauling on the Upper Mere ropes, blood and bread, the city's waiting to hear that bell.
H nh_writ=read :: A wolf of the road found the writ. The herald read it at noon. Blood and bread, I wept too.
- coldwater
R :: Hear that? Under the ice. Bright One's tide, it's ringing.
R nh_bell=rung :: Net and line, they raised it! Green as a pike and loud as a gale, eh?
R nh_mere=still :: The whole mere's still. I've never seen it so. Calm water to her.
W :: Take a tug rope and oil your coat, eh? And lie flat on the ice. It carries the sound.
A @coldwater :: The Upper Mere skipper takes bearings. Fish with him first; he doesn't trust city paws.
V :: The black cloak watches the mere all day. Deep and dark, what's she looking for?
! :: Don't walk the black lane on the ice, neighbour. It never freezes, and nobody says why.
W @coldwater :: The Bakehouse wants bread and smoked fish carried out to the haulers at the Upper Mere shed, eh? Cold paws want feeding.
H nh_bell=rung :: The bell's up and the haulers are home. Calm water to every wolf on the ropes, eh?
W @coldwater :: A Coldwater shepherd wants a wolf to sit a night on the Upper Mere ice and listen, eh? Something rings under there.
- larkfield
R :: A writ lost for a debt. That's the dearest paper ever bought in Ridgemere, #address#.
W :: The bell'll want a yoke of good iron. Frosthollow's the only seam deep enough. Scales and silver.
A :: Ask the mine at Frosthollow for ore. They count everyone down and up, so go as a party.
V :: The Counting House hid the old King's writ. Weights and measures, there's a cheat's scale for you.
! :: Don't go out on the Upper Mere ice alone, #address#. Weights and measures, ice doesn't bargain.
! nh_ford=burned :: Send bread to Larkfield before you send iron to the bell.
H nh_bell=rung :: The bell came up out of the Upper Mere. Full weight of a bell!
W @larkfield :: The looms still send blankets down to the ford houses. Ask a loom hand what wants carrying, #address#.
A @larkfield nh_ford=burned :: A Longmead farmhand is still sharing grain with the burned-out houses. Ask at Longmead, #address#.
- southmeadow
R :: The old King wanted all of us to choose? Sheaf and sickle, I'd best think what I'll say.
W :: If you're hauling on the mere, take something hot. A windfall for whoever's next to you.
A :: The cellar hand says the stone remembers the last Moot. Now there's a writ that remembers it too.
V :: Somebody bought a King's writ with a debt. Wormy, wormy business.
! :: Don't go out on the Upper Mere ice unless the skipper says it holds.
H nh_writ=read :: The writ's found and read. Ripe days for the whole realm.
W @southmeadow :: A Larkholm wagon still carries relief north to Larkfield. Walk beside it, #address#; the roads aren't safe yet.
- mirebridge
R :: Our bell, under all that ice. Mud and eels, I'd walk the whole Long River to hear it, no?
R nh_oskar=fled :: The Chancellor ran down the Fen Road and stopped where Mirebridge used to stand. Even the drowned didn't want him, no?
W :: Bring the scattered something from the bell, #address#. A keepsake off the old ferry-raft, and they'll give you a name.
A :: The eel-trapper at Withy Creek knows the bell's old note. Ask her to hum it.
V :: The Houses struck us off the rolls and hid the King's writ. Same paws, fen-sure.
! :: Don't stamp on the ice when you listen, #address#. The bell's not the only thing under it.
! nh_bell=cracked :: They rang it cracked. Don't laugh. It's still ours.
H nh_bell=rung :: Our bell's out of the ice. Dry feet to every wolf on the ropes.
A @fenshore :: The stores keeper was a pup when the bell last rang, no? Ask him how it went. He'll hum it, badly.
- saltwell
R :: A King writes "let the realm choose", and the Houses hide it in the dark. The Houses hid it, #address#, not the King.
W :: The keeper of our stores lost a brother at a mine. She has asked which one for years, #address#. Ask Frosthollow.
A :: The mine captain drinks at the Lamp and Pick on Restday. Bring a friend if you mean to ask him things.
V :: A Chancellor weeping over a paper means someone made him hold it.
! :: A pup who calls water is a gift and a danger, #address#. Do not hide her carelessly.
H nh_fenwick=confessed :: The mine captain said it aloud at last. By salt and water, the well remembers, and now so does the valley.
A @saltwell :: Our stores keeper has waited years to hear which mine, #address#. When the mine captain speaks, carry it to her shade.
- oakhaven
R :: Old Oak. The old King planted something deep before he fell, mm? Now we see the shoot.
W :: A bell under ice wants a slow pull. Rush it and it cracks, like green wood, mm?
A :: Ask the Glasswater Inn's cook about the ice. He's seen the big thing move too.
V :: The Counting House paid for a writ to be lost. Rotten at the root, mm?
! :: Don't trust a sale of secrets on the Sump Stairs. Somebody's paying both ends.
H nh_writ=read :: The writ's read. Shade on whoever found it.
W @oakhaven :: The road post by Alder Mere wants stone and timber before the Moot, mm? Gift them there. The printer counts who comes.
- frosthollow
R :: Our iron's in the bell's yoke. Cold iron for a cold bell. Lamp and pick.
W @frosthollow :: The mine captain drinks at the Lamp and Pick on Restday. Bring him there if he's got something to say.
A @frosthollow :: The miner who watches the road wants the lamp-oil list read out. Ask him what it says.
V :: On the Moot night the Pass is his, the notice says. The Captain writes like he means it.
! :: Nobody goes down alone, and nobody comes up without counting the others.
H :: Lamp's thanks to every wolf who went down the mine and was counted up again.
H nh_fenwick=confessed :: He said it, at the Lamp and Pick. Deep quiet after. Then we drank to Deepvein.

= moot
- plain
X :: The Moot night's close. Every post on the Line wants a keeper, and the Moot sits on Market Square.
X nh_moot=midwinter :: The Midwinter Moot's close. Every post on the Line wants a keeper, and the Moot sits on Market Square.
R :: Every town's sending its howl to the Moot. Ours goes to whoever kept the peace best.
R nh_moot=midwinter :: The Midwinter Moot! Every post on the Line wants a keeper, and every keeper wants a coat.
R nh_moot=nine_howls :: The Moot of the Nine Howls, they're calling it. Nine towns, one night, one howl.
R nh_dagfinn=stands :: The lamp-keeper will stand at the Moot. A Sump wolf, standing!
R nh_dagfinn=steps :: The lamp-keeper's stepping back, they say, and asking the Moot to choose. A Sump wolf who doesn't want a crown.
W :: On the Moot night, listen for the Bay Light bell, then count a hundred breaths. That's when the far howls come in.
A @ridgemere :: The herald's posted how the voices are counted. It's on the board, plain as bread.
V :: The Counting House master says he's already counted his voices. Make sure he's counted wrong.
! :: The Captain says the Pass is his on the Moot night. If the north arm fails, the trunk starts short.
H :: Every post on the Line has a keeper now. Blood and bread, I never thought the realm could agree on a howl.
W @ridgemere :: The carpenter in the Market Hall wants planks and benches for the Moot on the square. Carry some; the Houses won't.
A @ridgemere :: A Watch guard means to stand a Line post all night before the Moot night. Ask at the Watch Office if she wants company.
- coldwater
R nh_bell=rung|cracked :: The big thing's going to the city for the Moot. It rang once on the road, they say. On its own.
R nh_moot=midwinter :: A Midwinter Moot, eh? The mere's froze hard, and a howl carries over ice.
W :: The relay's at the old Glasswater camp. Both arms meet there. Bring oil, eh?
A @coldwater :: The skipper keeps the Upper Mere post. Ask him where to stand.
V :: The Counting House has bought howls. A bought howl's a muddied one, eh?
! :: If the howl stops at the mere post, it's not over. The next post can take it from two back, eh?
H :: Every wolf standing a post on the Line, calm water to you, eh? We'll hear you from the mere.
H nh_voice=raised :: Coldwater's voice is raised. We'll answer even if no wolf of the road is here. Calm water.
W @coldwater :: The Looms want wolves to walk the bell a stage on its road to the city, eh? Ask a loom hand.
- larkfield
R :: Our voice to the Moot is bought by deeds, not silver. Scales and silver, that's a market I like.
R nh_moot=midwinter :: A Midwinter Moot. Weights and measures, the coldest market the realm's ever held.
W @larkfield :: The ford post is ours. Stand with the linen master and lift it at the end like a bid.
A @larkfield :: The board says how Larkfield howls and why. Read it before you back anyone.
V :: Bought howls go to the Purse, three at most. Weights and measures, that's three too many.
! :: Don't go to the Moot empty-pawed. Bring oil for a post.
H :: Every wolf who held the ford, Larkfield howls for you on the Moot night.
- southmeadow
R :: Southmeadow's voice is ready. A windfall of a Moot, #address#.
W @southmeadow :: Our post is the square, second on the south arm. After the pool answers, it's us.
A @southmeadow :: The Larkholm farmer keeps our post. Ask her for a horn.
V :: The Counting House would crown a debt if it could. Blight take it.
! :: If the pool's silent, we can still take the howl from two back. Don't give up on it.
H :: Every wolf who carried a howl south, Southmeadow sings for you. Sheaf and sickle.
H nh_stone=read :: Whoever read the stone, Southmeadow howls with Mirebridge's name back in it.
- mirebridge
R :: Mirebridge has a voice again, no? Dry feet to every wolf who carried it.
W :: Lamp oil to the crossing three nights running. The true lights answer, fen-sure.
A :: Ask on the boardwalk who keeps the crossing post. Somebody always does, no?
A nh_lights=laid :: The eel-trapper at Withy Creek is our Keeper now. Ask her where the crossing post stands.
V nh_lights=faded :: The Council would cast our voice if we're silent. The Council struck us off. Boggy justice.
! :: The crossing post's out in the open. Don't stand it alone if there are cudgels about.
H :: The drowned heard their names this season. Now they'll hear their howl. Dry feet to you.
W @fenshore :: The boardwalk post's ours on the Moot night. Stand it with the stores keeper, oil and a horn, no?
- saltwell
R :: Stand by the pool when the night falls, #address#. Saltwell starts the south road's howl.
W @saltwell :: Our post is the pool. Oil, a horn, and a still heart.
A @saltwell :: The keeper of our stores starts it. Stand where she points.
V :: A crown bought with debts is a well with salt in it, #address#.
! :: If Saltwell is late, the whole south arm is late. Do not be late, #address#.
H :: The well's roll has your names in it now, every wolf who sat still. Peace on you.
- oakhaven
R :: A handful of wolves and one voice. That's a lot of shade for a small grove, mm?
W @oakhaven :: Our square's on the trunk, after the road post. Slow and climbing when it comes.
A @oakhaven :: The printer keeps the road post. She counts every wolf who comes. Be counted, mm?
V :: The Counting House would buy every voice in the grove, mm? Rotten at the root.
V nh_hills=fell :: Oakhaven howls against whoever Fell backs. The hills went to Fell, and we've long roots.
! :: If the road post goes quiet, the square can't carry. Fill both, old root.
H :: Shade on every wolf who walked the bounds. Oakhaven remembers.
- frosthollow
R :: Frosthollow starts the howl at the Pass. Lamp and pick, we'll be heard.
W @frosthollow :: The Pass post's by the Pass House. Stand it with the miner who watches the road.
A @frosthollow :: Ask the miner who watches the road what the Captain means to do. He knows the wolves.
V :: The Captain's band gathers at the Pass the night before. Cold iron, there'll be a fight.
! :: If the Pass falls silent, the north arm's two posts short. Don't let it, #address#.
H :: Every wolf who stands at the Pass, Frosthollow keeps your lamp lit.

= after
- plain
R nh_crown=ottilie :: Bright One keep the Queen. And the lamp-keeper beside her, bless his flat feet.
R nh_crown=lamp :: A Sump wolf at the lamp, by the realm's own howl. Blood and bread, I howled for him.
R nh_crown=regency :: No crown at all, and a Moot every year. I'll believe it when it sits twice.
R nh_crown=regency nh_moot=midwinter :: No crown at all, and a Moot every Midwinter. I'll believe it when it sits twice.
R nh_crown=bought :: The Counting House has the realm now. They call it a Protectorate. I call it a debt with a crown on.
R nh_concord=answers :: The black cloak tore the Keeping at the Bay Light. The Wardens are going home.
R nh_concord=bends :: Wardens in every silent town by spring. We were too quiet, somewhere.
R nh_captain=turned :: The Grey Captain went back over the Pass with the coin. To fetch the Deepvein families down, they say.
R nh_captain=beaten :: They held the Grey Captain a day in the Keep's cells and judged him at the Moot's end. It's on the Market Square board.
H fig=hero nh_crown=ottilie|lamp|regency|bought :: They say #world.figure_hero# carried the realm's howl further than anyone. The Lantern, folk are calling that wolf.
- coldwater
R nh_howl=ran :: We answered, eh? Glasswater answered! A howl over the water.
R nh_crown=ottilie :: A Queen who coughs in the damp. She'll want a lake wolf's soup. Calm water.
R nh_crown=lamp :: The lamp-keeper's a fen wolf. Close enough to the lake. Fair catch.
R nh_crown=regency :: A Moot every year, eh? The city asking the lake what it thinks. Deep and dark, I'll get used to it.
R nh_crown=bought :: Bought, eh? Muddied water all the way up.
R nh_concord=holds :: One black cloak at the Grey Gull till spring. Fair winds to her.
- larkfield
R nh_howl=ran :: Larkfield answered on the dot. Sold, to the realm.
R nh_crown=ottilie :: A Queen who carries her own petitions. That's full weight.
R nh_crown=lamp :: A patrolman king. By the market cross, the Houses are still weighing it.
R nh_crown=regency :: No crown, and a market of voices every year. Fair price.
R nh_crown=bought :: Short-weighed realm. The Counting House kept the scales.
R nh_captain=beaten :: They led the Grey Captain down the Pass Road. Weights and measures, there'll be a judging worth watching.
- southmeadow
R nh_howl=ran :: Ripe days. We howled the burying howl by mistake and nobody minded.
R nh_crown=ottilie :: A Queen! Sheaf and sickle, I'll plant her an apple tree.
R nh_crown=lamp :: The lamp-keeper's king. Good harvest to him.
R nh_crown=regency :: A Moot every year. More talk than a harvest dance, and longer.
R nh_crown=bought :: Blight take it. Bought, like a cart of chaff.
R nh_concord=answers :: The Wardens left by the Pass. A good harvest of a night.
- mirebridge
R nh_bell=rung :: Our bell rang the Moot in. Fen-sure, the drowned heard it.
R nh_crown=lamp :: A Mirebridge wolf with the Palace lamp. Dry feet, the drowned have a king.
R nh_crown=ottilie :: A Queen, and the lamp-keeper beside her. Fen-sure, he'll keep her feet dry.
R nh_crown=regency :: A Moot every year, and Mirebridge has a voice in every one. Mud and eels!
R nh_crown=bought :: Bought. Boggy. We'll howl louder at the next Moot, no?
R nh_lights=faded :: The Council spoke for us at the Moot. The drowned won't forgive that.
- saltwell
R nh_howl=ran :: The howl came out of the dark to the oasis, #address#, and we sent it on. The well remembers.
R nh_crown=ottilie :: A Queen who answers petitions. Peace on her house.
R nh_crown=lamp :: A keeper of the peace with the lamp. By salt and water, that is what the old King meant.
R nh_crown=regency :: The realm draws from the Moot each year, like a well. Good.
R nh_crown=bought :: A crown bought with debt is a brackish crown, #address#.
R nh_concord=holds :: A black cloak at the Grey Gull, they say. Let her drink, and go in peace.
- oakhaven
R nh_howl=ran :: A Moot night to remember, mm? It came through the cedars slow and climbing.
R nh_crown=ottilie :: A Queen, mm? Root well, Majesty.
R nh_crown=lamp :: The lamp-keeper keeps it. Proper.
R nh_crown=regency :: No crown. A council of everyone. Old Oak, that's a big grove.
R nh_crown=bought :: Bought. Rotten at the root. We'll grow round it.
R nh_concord=answers :: The black cloaks went back over the Pass. Shade on the realm.
- frosthollow
R nh_howl=ran :: Held the Pass post. Cold iron and warm hearts, the both of it.
R nh_crown=bought :: Bought, then. Black seam. At the next Moot, we howl deeper.
R nh_captain=turned :: Deepvein's coming down in spring, they say. We'll share the oil this time.
R nh_captain=dead :: No quarter at the Pass. Lamp and pick, I'd rather we'd given one.
R nh_captain=fled :: The Captain went over the Pass with his coin. He'll be back. They come back for what they're owed.
R nh_captain=beaten :: They took the Captain down to the city's cells. Lamp and pick, I hope they judged him fair.
R nh_crown=ottilie|lamp|regency :: The realm howled from the Pass to the Bay Light. Deep quiet after. Then the bell.
