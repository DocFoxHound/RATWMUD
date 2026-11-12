# Doc 79, Phase 10: the play guide

This is for you to play the staged economy and say what feels right and what doesn't. It has three parts: how to start
the test server, the checklist (T1 to T17), and the numbers you may want changed.

Everything here was checked against DEV as it stands (build 71, at migration 0061) on 2026-11-12, after the wagons and
farms were fixed (section 6). DEV now has 773 residents: 590 in Ridgemere, 51 at the farmhouses and 132 in the seven
Country villages. Where something was read from the code or from a probe and not tried by hand in a browser, it says
so.

## 1. Start the test server

Open a terminal and run these three lines:

    cd /home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/r2-staged
    set -a; . /home/martinb/Documents/GitHub/MUDProject/Database/.env; set +a
    bash tools/scratch.sh dev --dev-identity --dev-tools --for 14400

- It reads DEV and writes nothing. It can run beside any other server.
- It prints `RATW server listening on port N`. Open `http://127.0.0.1:N/?identity=tester&name=Tester` in a browser.
  A second tab with another `identity=` gives you a second wolf.
- `--for 14400` keeps it up for four hours. Without it the server stops after one hour.
- Stop it with Ctrl-C when you are done.
- Nothing is saved. Each start is the same world: **Dawnday, the first day of Spring, a little after noon (12:12).**
  The farms are staffed and the Country villages lived in from the first moment.

### Moving the clock

One game day is four real hours, so you will want to jump. Press **SETTINGS** in the top bar. With `--dev-tools` it has:

- **WORLD CLOCK: dawn, day, dusk, night.** These set the hour to 6:00, 12:00, 18:00 or 0:00 on the same day. They can
  move the clock back.
- **CALENDAR: Day +1.** One day on, same hour. The economy runs the hours it missed (shops restock, households buy
  food, the night's purses), once each.

There is no button for an exact hour. To reach 9:00, press "dawn" and wait, or start the server with `--speed 6`
(add it to the third line): then a game hour takes under two minutes. Walking speeds up too, so use it only for waiting.

The top bar shows the weekday, the day and the hour, and says "MARKET DAY · STALLS OUT" or "REST DAY".

### What this server cannot do

- **No Dev Console.** The console needs a wolf marked Dungeon Master, and DEV has no saved wolves.
- **No DM tool.** A scratch server ignores everything the DM tool sends. The Prosperity and Economy tabs would show and
  change the real DEV database, not this server. Don't use them for this test.
- So there is no slider to drag, no "call a storm", no "add an event", and no teleport.

### Hard times and booms, without the DM tool

The slider's starting target is one number in a file. This was read from the code, not tried by hand.

1. Stop the server.
2. Open `Data/Economy/market.json`. Near the end of the `rules` block, change `"prosperity": 0` to `-100` (hard times)
   or `100` (boom).
3. Start the server again. The slider walks 10 points a day toward the target, so press **Day +1 ten times**.
4. On the first day a cause is posted on the notice board and cried in the square (the Crown's war levy, rot in the
   granaries, a hard winter; or silver in the hills, peace with the north, a rich harvest).
5. When you are done, put the number back to 0.

The poor need time to feel it. Their purses move halfway each night, and a household changes band only after a week.
For the beggars' table below to show, press Day +1 about **three weeks' worth** after the slider arrives.

### If you want the real DM tool

That needs the normal DEV server (`bash tools/live.sh server dev --dev-identity --dev-tools`) and the DM host. It saves
to DEV and only one may run. Say the word and it can be set up, or a throwaway copy of DEV made for it. It was not
started for this guide.

### Useful tricks

- **Read the notice board:** stand within two tiles of the board by the square. A "Read the board" button appears.
- **Open a shop's list:** click the keeper, choose "trade". Cheap goods are listed by the dozen. A gold ▲ or green ▼
  means the price moved a penny or more. Hover the arrow to read why.
- **Get goods to sell** (for T3): press F12 in the browser, and in its console type
  `ratw.connection.submit({type:'grant', item:'iron_ore', quantity:9})`. It gives nine pieces. Repeat it.
- **Where am I:** the map's title is the place's name. The four open quarters are named in the checklist.
- **Wagons and carts after a jump:** a wagon or a farm cart whose hour has already passed today sets out as soon as you
  press Day +1. So press it while standing where you want to watch (the Carters' Yard, Market Square). This was read
  from the code and run in a probe, not watched in a browser.
- **Getting to a Country village:** there is no teleport. Oakhaven is the nearest, about 1,150 tiles from Market
  Square in a straight line: leave by the South Gate and the road past Timberland Court, about ten minutes at a walk.
  Coldwater is about twice as far. Each village's notice board and wagon stand are on its green, beside the well.

## 2. The first two weeks, as this world rolls them

A fresh start always gives the same calendar. Day 1 is the start (Dawnday). The events and ships at Ridgemere are the
same as before the wagons were fixed; the wagons are new.

| Press Day +1 | Day | What happens |
|---|---|---|
| once | Hearthday | 8:00: the mill wheel stops for two days. Flour and bread dearer. |
| 3 times | Riverday | 8:30 to 17:00: the Long Tide is in, with a rich cargo. **The first wagons:** 8:00 to 9:00 three leave the Carters' Yard (for Oakhaven, Frosthollow and Coldwater); about 10:40 one is in from Coldwater. |
| 4 times | Emberday | About 11:10: a wagon in from Coldwater. Five wagons on the Country's roads. |
| 5 times | Marketday | Stalls out, beggars 8:00 to 18:00. 12:30: the brewers' glut, cider and mead cheap. 8:30: a wagon leaves for Coldwater; it and the wagon from Frosthollow are robbed on the road at about 9:40. |
| 6 times | Restday | Beggars 8:00 to 18:00. The dole at the chapel. 8:30: a wagon leaves for Coldwater. About 9:00 and 9:40: wagons in from Oakhaven and Coldwater. |
| 7 times | Dawnday | 9:30: the drovers are in. Meat, mutton and hides cheap. |
| 9 times | Stoneday | The notice of the House wedding goes up. |
| 11 times | Emberday | 11:00: the House wedding, three days. Wine, mead, brandy and cheese dearer. |
| 12 times | Marketday | 8:30 to 17:00: the Heron is in. Blankets and herbs cheaper, tallow dearer. |
| 13 times | Restday | 10:00: the salt pans flood. Salt fish and smoked fish dearer. |

Every morning farm carts come in: they leave their farms between 5:30 and 7:00 and reach Market Square between 6:30
and 11:00 (four to eight a day at Ridgemere, 14 to 20 in the whole realm).

The wagon hours above are from a probe that ran DEV's own save at normal speed for two weeks. If you jump with Day +1
the same wagons leave, but at the moment you press (see "Useful tricks"), so a raid's hour moves with them. The
Country villages have their own events too (a brewers' glut at Oakhaven on Marketday, a fire at the Southmeadow works
on Stoneday, and so on): read the board on the village's green.

## 3. The checklist

The four open quarters of Ridgemere: **the Commons and the Rise** (Market Square; you start here), **the Wharf and
the Sump** (the piers, the Rag Market), **Northgate**, and **the North Headland**.

"Seen today?" is what the probes found on DEV. Fill in the last column as you play.

| # | What to do | Where | When | What you should see | Seen on DEV today? | Verdict |
|---|---|---|---|---|---|---|
| T1 | Open the pie baker's list. Buy bread by the dozen, several times. | Oda Marrowby's stall, Market Square | Any day, 8:00 to 18:00 | "12 for 13p" climbs, with ▲. The baker says bread is dear. | Yes. Without buying anything, Hearthday's stopped mill already shows it: bread 13p to 14p a dozen, flour 26p to 29p. | |
| T2 | Buy a shop's whole shelf of one thing. Look again an hour later, and next morning. | Any small shop. Most wares have 4 on the shelf. | Before noon | The count falls to 0 and stays there till morning. | Partly. Small shelves do thin by evening as households buy (mead and wine down to 1 to 3). Bread never sells out unless you buy it: the pie stall holds 160 to 230 loaves. | |
| T3 | Sell 36 iron ore, six at a time. | The Timberland Foundry (Runa Coalhand), or a smith: Anchor Forge, Nailers' Forge, Eastgate Ironmongers, Eastgate Farriers, Forge Row, the Timberland Ironworks | Any working day | Each lot pays a little less. After 30 pieces: no room for more today. Next morning the ore's price is down with ▼. | The rule is there (30 pieces a day, 60p budget at the Foundry). Not played by hand. "The next wagon carries ore" cannot show: see T4. | |
| T4 | Read the wagon notices, then watch a wagon load and leave, and one come in. | Leaving: the Carters' Yard, just inside the East Gate (the Commons and the Rise). Arriving: Market Square, by Ivo Tarmarsh's sausage stall. In the Country: the green of any village. | Wagons leave between 7:30 and 9:30 on the day their notice names. The first are on Riverday of week 1 (Day +1 three times). | On the board two days ahead, at both ends: "Wagon to Coldwater, Riverday: plaster and flour for the general store... Delphine Brackwick driving." The wagon in the yard, "loading", its carter beside it; then on the road with a guard. Wagons for Frosthollow go out by the East Gate, for Oakhaven and Coldwater by the South Gate. When one comes in, its notice comes down and the shelves of what it carried refill. | **Yes.** 23 wagons ran between the eight markets in the first week, 61 in two weeks (a probe on DEV's own save). On Riverday at Ridgemere: Percival Thornham for Oakhaven at 8:00 (back by 10:00), Orm Carlisle for Frosthollow at 8:30, Delphine Brackwick for Coldwater at 9:00, and Willa Coldwick in from Coldwater at 10:40 with peat and herbs. Not watched in a browser. | |
| T5 | Read the board after a wagon has been robbed. Ask at the shops what is dear. | The notice board on Market Square (its work side), or on a village green | After a raid. In the first week: Emberday about 9:00 (the Saltwell wagon, in the Red Waste), Marketday about 9:40 (Ridgemere's wagon for Coldwater in the Rainwood, and Frosthollow's for Ridgemere in the Greenholt). | The robbed wagon limping home, its carter robbed or bruised (never killed). A bounty on the board: "the bandits robbing the road in the Rainwood". "Escort wanted" on the next wagon's notice. What it carried a little dearer where it was going (▲). The Watch goes out after a camp and may clear it. | **Yes.** Seven bandit camps sit on the roads from the first day. Three of the first week's 23 wagons were robbed, eight of 61 in two weeks: about one in seven. Not watched in a browser. | |
| T6 | Read the board, then go to the Wharf. Open the Rag Market dealer's list (Gerrit Sootgate). | The Wharf and the Sump | Riverday of week 1, or Marketday of week 2, 8:30 to 17:00 | The timetable on the board, signed "the Wharf office". Hands unloading. Cargo cheaper with ▼, and what the ship takes dearer with ▲. | Yes. After this pass's change: crates 4p to 3p, cushions 6p to 5p, blankets 9p to 7p, herbs 13p to 11p a dozen, hides 5p to 6p, tallow 13p to 15p a dozen. | |
| T7 | Walk the squares and the chapel steps. | All four quarters | Marketday and Restday, 8:00 to 18:00 only | A few beggars. More in hard times, one in a boom. | Yes: five in ordinary times (two at most in one place). See the table in section 4. Other days: none. | |
| T8 | Stand by a food stall for a while. | Market Square | 8:00 to 20:00 | Now and then a theft, and the Watch after the thief. | Barely. About one theft a day in the whole city, one or two in hard times. You will probably not see one. | |
| T9 | Ask keepers how business is. | Any shop | After a few days | "Thriving", "steady" or "failing", and it fits their shelves. | Not checked by probe. For your ear. | |
| T10 | Listen to rich and poor wolves talk about prices. | The Rise (rich), the Sump (poor) | When a price has moved (Hearthday on) | The rich shrug. The poor complain. | Not checked by probe. For your ear. | |
| T11 | Look for farm carts in the morning. Then open a stall's list for what the farms bring. | Market Square, by the sausage stall; or a village green | Carts leave the farms 5:30 to 7:00 and reach Ridgemere 6:30 to 11:00 | A small cart with its carter (the farm's keeper or a hand), "being loaded" at the farm, then at the square. Its goods refill. On Marketday the stalls are stocked from the morning's carts. In a poor harvest fewer carts come and their goods are dearer (▲, "harvest"). | **Yes.** All 42 farms and works are staffed (132 posts). 14 to 20 carts a day in the realm, four to eight of them to Ridgemere. This spring fruit, fuel, honey and iron are poor, salt is good, the rest fair: at Ridgemere apples went from 14p to 15p a dozen by Riverday, cider 40p to 43p, firewood 13p to 15p. Not watched in a browser. | |
| T12 | Visit the Wharf, a workshop, a farm and a village. | The Wharf; any forge; a farm near the city (Southgate Dairy Farm); Oakhaven | Working hours | Wolves at their posts. | **Yes.** All 38 Wharf posts are held, all 132 farm and works posts, and in the villages 14 shop and alehouse keepers, the workshops' crews and eight carters. | |
| T13 | Read the board on the days in section 2. | The notice board, Market Square | See section 2 | A notice for each event, a crier as it starts, the prices moving as it says. | Yes. Six events in the first two weeks, about two a week. | |
| T14 | Buy all of a ship good (blankets, crates, cushions, oil lamps, herbs, mead). Come back each day. Or buy all of something a village store gets by wagon (nails, cord, lavender soap at Oakhaven Stores). | The Rag Market; herb sellers; a village's general store | Just after a ship has sailed, or a wagon has come | The shelf stays bare until the next ship or wagon, then fills. The notice on the board says when the next wagon is due. | Partly. Sixteen goods wait for ships at Ridgemere, about one ship a week. Wagons now run, so a village's goods from Ridgemere come with them (the rule was read from the code; the shelves were not bought out by hand). | |
| T15 | Give a beggar food worth a week or more, or help with their trouble. Look for them on Restday. | Wherever they beg | Marketday, then Restday | They are not begging the next day. Later they thank you. | The rule works on this server. Not played by hand. | |
| T16 | Follow "Hard times and booms" in section 1. | The notice board and the square | The first day after the restart | A cause on the board and from the crier. Wolves speak of it. | Only by the file change. Not tried by hand. | |
| T17 | When a wolf names a price, open that shop's list. | Anywhere | When a price has moved | The same number. | Covered by the tests. For your ear. | |

**A Country village:** all seven are now lived in and are markets of their own: Coldwater (45 living there), Larkfield
(31), Fenshore (17), Southmeadow (13), Saltwell (9), Frosthollow (9) and Oakhaven (8), with 51 more at the farmhouses.
Each has a general store, an inn or alehouse (Coldwater and Larkfield a bakehouse and workshops too), a carter at the
wagon stand on the green (Coldwater has two), and a notice board on the green. T1, T2, T4, T5, T11 and T13 show there
as in the city, in small. There is no Watch in the villages, so their wagons go out with the carter alone.

## 4. What changed in this pass

Each is one small number. All can be changed back.

| What | Before | Now | Why |
|---|---|---|---|
| How far a ship moves a price | About 5% | About 15% | Before, most goods did not move a whole penny. Now a ship's cargo is visibly cheaper and what it takes visibly dearer. |
| A storm's step on the missing cargo | 10% | 15% | The same reason. |
| Ships lost at sea | 11 a year | 1 a year | A lost ship should be rare news. |
| House weddings | 13 a year | 10 a year | Fewer, so they don't follow week on week. |
| The poor's purse | 3p to 12p a head | 2p to 12p | A day's bread is about 4p. At 3p almost no poor household was ever short. |
| Who is judged hungry on Restday | Only wolves with a job got the Restday test | Everyone | Before, a poor household's old and idle begged on Marketday but never on Restday. |
| Talk: a lost ship | Always the top subject for four days | Usually, not always | So hard times are still heard. |
| Talk: hard or lean times | Mostly drowned out by any event | Hard times draw level with an event; lean times a step under | The same. |
| One line of the rich about dear goods | Said very often | Said about a quarter as often | It was taking most of its turn. |

### The wagons and farms pass (2026-11-12)

| What | Before | Now | Why |
|---|---|---|---|
| The farms' people on a test server | Never came: the test server could not write their rows, and DEV's old save said the day's hiring was already done | They come in the first second | So farms are worked, carts come, and the villages have keepers. |
| The same on the real DEV server | Would have waited for the first midnight | At start. DEV's roster now holds them: 214 placed | DEV shows what the game does. |
| Carters in the villages | None | One each, two at Coldwater: eight, from the wolves waiting in the Country | Without a carter a village could send no wagon, and four of the seven got none at all. |
| Where a village's wagons load | Inside whichever shop had the most keepers (a bakehouse, an alehouse) | On the green, by the well | A wagon should be seen in the open. |
| A village's notice board | Outside one shop's door, and it could not be read: the ground there counted as wild country | On the green, and it reads | Wagon notices are posted at both ends. |
| Bandit camps | None until one gathered, a week in | Seven on the roads from the first day | Raids can happen in the first week. |
| A carter out of sight | Fell behind his wagon, and cost the server time | Rides with it | Cheaper, and he is where his wagon is. |
| A bounty's words | "at w_ridgemere_4_6" | "in the Rainwood" | A board should not show a map id. |

**Beggars in Ridgemere, before and after** (Marketday, then the Restday after; "working" = wolves with a job, begging
on their day off):

| Slider | Before: Marketday | Before: Restday | Now: Marketday | Now: Restday |
|---|---|---|---|---|
| −100 (hard) | 9 | 11 (2 working) | 10 | 12 (6 working) |
| −50 (lean) | 8 | 9 (2 working) | 9 | 9 (5 working) |
| 0 | 5 | 5 (0 working) | 5 | 5 (2 working) |
| +50 (good) | 1 | 1 | 1 | 1 |
| +100 (boom) | 1 | 1 | 1 | 1 |

## 5. Numbers you may want changed

Tell me the number and the value. Those marked "file" you can also change yourself and restart the server.

**Prices** (`Data/Economy/market.json`)

| Number | Now | What it does | Sensible range |
|---|---|---|---|
| `groups` (depth) | staple 2.0, food 1.0, cloth 0.5, tools 0.4, luxury 0.3 | How much buying it takes to move a price. Bigger = harder to move. | Halve to double |
| `rules.depthScale` | 100 | The same, for everything at once, in percent. | 50 to 200 |
| `rules.halfLifePerishable`, `halfLifeKeeping`, `halfLifeDurable` | 20, 50, 80 (2, 5 and 8 days) | How long a price stays moved. | 10 to 150 |
| `rules.playerPushMost` | 15 | The most one player can move a price, in percent. | 10 to 25 |
| `rules.takeAllowance` | 50 | How much of one good a shop will buy in a day. | 25 to 100 |
| `times.staple` | 8% dearer in hard times, 4% in lean | Bread and staples in bad times. | 0 to 10% |
| Ship baselines (in the code) | 15% from rest | How far a ship moves its goods. | 10 to 17%. Past 17% the price hits its floor. |

**Ships and events** (`Data/Economy/market.json`)

| Number | Now | What it does | Sensible range |
|---|---|---|---|
| `rules.shipsPerWeek` | 2 (one to three a week) | How often a ship comes. | 1 to 3 |
| Storm odds (in the code) | 3% a ship; 6% in autumn; 15% in winter. About six a year. | Ships kept out by storms. | Winter 8 to 15% |
| `events.perWeek` | 2 | Events a week at each market. | 1 to 3 |
| `house_wedding` weight | 0.5 (about 10 a year) | How often it is picked. | 0.25 gives about 5 a year |
| `lost_ship` weight | 0.1 (about 1 a year) | The same. | 0.05 to 0.3 |
| Each event's `goods` | 10 to 20% | How far the event moves its goods. | 10 to 30% |
| `wharf.imports` | 16 goods wait for ships | Butter is one of them, and households eat it. | Your pick of the list |

**The roads** (`Data/Economy/market.json`, `rules`; the carters in `Data/Economy/communities.json`)

| Number | Now | What it does | Sensible range |
|---|---|---|---|
| `caravanRate` | 100 | How often wagons run, in percent. City to a village every two to four days, village to village every six to ten. | 50 to 200. 0 stops them. |
| `wagonsOnRoad` | 16 | The most wagons out at once. | 8 to 24 |
| `cartsOnRoad` | 48 | The most farm carts out at once. | 24 to 60 |
| `campStrengthMost` | 12 | How strong a bandit camp may grow. Stronger camps rob more wagons. | 6 to 16 |
| `bountyMost` | 60 | The most a bounty pays, in pennies. | 30 to 120 |
| Carters in a village (`square.posts`) | 1, Coldwater 2 | A village sends at most one wagon a day for each carter. | 1 to 3 |
| Raid odds (in the code) | About one wagon in seven | A village's wagon has no guard but its carter; Ridgemere's has one of the Watch. | Say if it is too many. |

**Rich and poor** (`Data/Economy/status.json`)

| Number | Now | What it does | Sensible range |
|---|---|---|---|
| `shares` | At 0: 5% destitute, 25% poor, 60% comfortable, 10% rich. At −100: 18, 45, 32, 5. | How many households are in each band. | Each row must add to 100 |
| `rows.*.meals` | 1, 2, 3, 3 | Meals a day by band. | 1 to 3 |
| `rows.*.short` | 45%, 12%, 1%, 0% | The chance of a hungry day. | Poor 5 to 25% |
| `rows.poor.purse` | 2p to 12p | Lower floor = more working poor short of food. | Floor 1 to 3 |
| `timesShift` | 25 | How far a market's own good or bad times move its bands. | 0 to 40 |

**The street** (`Data/Economy/status.json`, `street`)

| Number | Now | What it does | Sensible range |
|---|---|---|---|
| `beggarsInAMarket` | 12, 8, 5, 2, 1 | The most beggars in the city, from hard times to boom. | Hard 8 to 20 |
| `beggarsInACell` | 5, 3.5, 2, 1, 0.5 | The most in one quarter. | Hard 3 to 8 |
| `beggarsPosted` | 0.5 | How readily the working poor beg on Restday. 0 turns it off. | 0 to 1 |
| `theftBase` | 0.03 (about one theft a day) | Thefts in the city. | 0.08 gives about three a day |
| `rules.theftCap` (market.json) | 4 | The most thefts a day. | 2 to 8 |
| `doleDays` | 1 | Days of bread the Restday dole gives. | 1 to 2 |
| `rows.*.alms` | Comfortable 15%, rich 30% | The chance a passer-by gives a beggar a penny. | 5 to 50% |

**Talk** (in the code): how often each subject wins the hour. Tell me what you hear too much or too little of.

## 6. What was missing, and what is still open

The last version of this guide said wagons, raids, farm carts and harvests could not show, because nobody worked a
farm and no village had a keeper. That was not the world's data. It was two faults in the game:

- **DEV's save said the day's hiring was done.** The save was taken at noon on the first day, by a server from before
  the farms had people. The game hires for the farms once a day, at the day's first hour, so every server started
  from that save waited for midnight. A test server starts from the same save every time, so it never got there.
- **A test server could not place anyone.** It may not write to the database, and placing a farm wolf began with
  writing their row. Each one was refused.

Both are fixed, and tested. The real DEV server was then run for a minute, so DEV's own roster now has the 214 wolves
at their farms and villages (they came from the list of wolves waiting in the Country; nobody was made up).

Three things were added so the villages work as markets: a carter in each, a wagon stand and notice board on each
green, and bandit camps on the roads from the first day.

**Still open, for you to say:**

- **What the villagers say about themselves.** The wolves at the farms and in the villages still greet you with their
  old lines from the old map, and many are broken: "I am farms the south fields today", or the keeper of The Felled
  Cedar saying "I am keeps The Snow Chapel today". 157 of the 206 have a line like that. Their jobs, homes and prices
  are right; their own words are not. This needs a text pass like the one the city's wolves had.
- **Raids:** about one wagon in seven is robbed. A village's wagon goes with the carter alone, because the villages
  have no Watch. If that is too many: lower `campStrengthMost`, or give the villages a guard.
- **Village events:** each village rolls its own events from the same twelve kinds. Some do not fit the place: a
  broken mill wheel at Saltwell, which has no mill; orchard blight at Frosthollow, a mining village.
- **Walking there:** the villages are a long walk and there is no quick way to reach them on a test server.
- **A hitch in a village:** when a player is in a village and the wolves there leave home at 7:00, the server loads
  the village's houses in one step, which took 40 to 50 ms once in a test. It is a single late step, not a slowdown.
- **A purpose-built carters' yard:** the carters stand on the green. A yard building in each village would need the
  map generator to lay one.
