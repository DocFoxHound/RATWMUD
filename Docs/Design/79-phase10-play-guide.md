# Doc 79, Phase 10: the play guide

This is for you to play the staged economy and say what feels right and what doesn't. It has three parts: how to start
the test server, the checklist (T1 to T17), and the numbers you may want changed.

Everything here was checked against DEV as it stands (build 70, migrated to 0061) on 2026-11-12. Where something was
read from the code and not tried by hand, it says so.

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
- Nothing is saved. Each start is the same fresh world: **Dawnday, the first day of Spring, at noon.**

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

## 2. The first two weeks, as this world rolls them

A fresh start always gives the same calendar. Day 1 is the start (Dawnday).

| Press Day +1 | Day | What happens |
|---|---|---|
| once | Hearthday | 8:00: the mill wheel stops for two days. Flour and bread dearer. |
| 3 times | Riverday | 8:30 to 17:00: the Long Tide is in, with a rich cargo. |
| 5 times | Marketday | Stalls out, beggars 8:00 to 18:00. 12:30: the brewers' glut, cider and mead cheap. |
| 6 times | Restday | Beggars 8:00 to 18:00. The dole at the chapel. |
| 7 times | Dawnday | 9:30: the drovers are in. Meat, mutton and hides cheap. |
| 9 times | Stoneday | The notice of the House wedding goes up. |
| 11 times | Emberday | 11:00: the House wedding, three days. Wine, mead, brandy and cheese dearer. |
| 12 times | Marketday | 8:30 to 17:00: the Heron is in. Blankets and herbs cheaper, tallow dearer. |
| 13 times | Restday | 10:00: the salt pans flood. Salt fish and smoked fish dearer. |

## 3. The checklist

The four open quarters of Ridgemere: **the Commons and the Rise** (Market Square; you start here), **the Wharf and
the Sump** (the piers, the Rag Market), **Northgate**, and **the North Headland**.

"Seen today?" is what the probes found on DEV. Fill in the last column as you play.

| # | What to do | Where | When | What you should see | Seen on DEV today? | Verdict |
|---|---|---|---|---|---|---|
| T1 | Open the pie baker's list. Buy bread by the dozen, several times. | Oda Marrowby's stall, Market Square | Any day, 8:00 to 18:00 | "12 for 13p" climbs, with ▲. The baker says bread is dear. | Yes. Without buying anything, Hearthday's stopped mill already shows it: bread 13p to 14p a dozen, flour 26p to 29p. | |
| T2 | Buy a shop's whole shelf of one thing. Look again an hour later, and next morning. | Any small shop. Most wares have 4 on the shelf. | Before noon | The count falls to 0 and stays there till morning. | Partly. Small shelves do thin by evening as households buy (mead and wine down to 1 to 3). Bread never sells out unless you buy it: the pie stall holds 160 to 230 loaves. | |
| T3 | Sell 36 iron ore, six at a time. | The Timberland Foundry (Runa Coalhand), or a smith: Anchor Forge, Nailers' Forge, Eastgate Ironmongers, Eastgate Farriers, Forge Row, the Timberland Ironworks | Any working day | Each lot pays a little less. After 30 pieces: no room for more today. Next morning the ore's price is down with ▼. | The rule is there (30 pieces a day, 60p budget at the Foundry). Not played by hand. "The next wagon carries ore" cannot show: see T4. | |
| T4 | Watch the gate for a wagon. | The North Gate | — | A named carter leaving with a guard. | **No.** Ridgemere is DEV's only market. No Country town has a shop or a carter, so no wagon ever leaves. | |
| T5 | Look for a bounty notice. | The notice board | — | A raid, a bounty, salt dear. | **No.** No wagons, so nothing to raid. | |
| T6 | Read the board, then go to the Wharf. Open the Rag Market dealer's list (Gerrit Sootgate). | The Wharf and the Sump | Riverday of week 1, or Marketday of week 2, 8:30 to 17:00 | The timetable on the board, signed "the Wharf office". Hands unloading. Cargo cheaper with ▼, and what the ship takes dearer with ▲. | Yes. After this pass's change: crates 4p to 3p, cushions 6p to 5p, blankets 9p to 7p, herbs 13p to 11p a dozen, hides 5p to 6p, tallow 13p to 15p a dozen. | |
| T7 | Walk the squares and the chapel steps. | All four quarters | Marketday and Restday, 8:00 to 18:00 only | A few beggars. More in hard times, one in a boom. | Yes: five in ordinary times (two at most in one place). See the table in section 4. Other days: none. | |
| T8 | Stand by a food stall for a while. | Market Square | 8:00 to 20:00 | Now and then a theft, and the Watch after the thief. | Barely. About one theft a day in the whole city, one or two in hard times. You will probably not see one. | |
| T9 | Ask keepers how business is. | Any shop | After a few days | "Thriving", "steady" or "failing", and it fits their shelves. | Not checked by probe. For your ear. | |
| T10 | Listen to rich and poor wolves talk about prices. | The Rise (rich), the Sump (poor) | When a price has moved (Hearthday on) | The rich shrug. The poor complain. | Not checked by probe. For your ear. | |
| T11 | Look for farm carts in the morning. | The gates, Market Square | Early morning | Carts in from the farms, heavy or light. | **No.** None of DEV's 42 farms and works has anyone working it, so no cart comes. | |
| T12 | Visit the Wharf and a workshop. | The Wharf; any forge | Working hours | Wolves at their posts. | Yes in the city: all 38 Wharf posts are held. No on the farms (nobody there). | |
| T13 | Read the board on the days in section 2. | The notice board, Market Square | See section 2 | A notice for each event, a crier as it starts, the prices moving as it says. | Yes. Six events in the first two weeks, about two a week. | |
| T14 | Buy all of a ship good (blankets, crates, cushions, oil lamps, herbs, mead). Come back each day. | The Rag Market; herb sellers | Just after a ship has sailed | The shelf stays bare until the next ship, then fills. | Partly. Sixteen goods wait for ships here. Ships come about once a week. "The wagon's due" cannot show (T4). | |
| T15 | Give a beggar food worth a week or more, or help with their trouble. Look for them on Restday. | Wherever they beg | Marketday, then Restday | They are not begging the next day. Later they thank you. | The rule works on this server. Not played by hand. | |
| T16 | Follow "Hard times and booms" in section 1. | The notice board and the square | The first day after the restart | A cause on the board and from the crier. Wolves speak of it. | Only by the file change. Not tried by hand. | |
| T17 | When a wolf names a price, open that shop's list. | Anywhere | When a price has moved | The same number. | Covered by the tests. For your ear. | |

**A Country town:** there is nothing to see. All 559 residents live in Ridgemere. The Country's towns have no people,
no shops and no market, so none of T1 to T17 shows there. This is the world's data, not the code.

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

## 6. What the world needs before the rest can show

These are not numbers. They are missing people.

- **Wagons, raids and "the wagon's due" (T4, T5, part of T14):** a second market. One Country town needs a household
  with a merchant and a carter.
- **Farm carts and harvests (T11):** someone working at least one farm.
- **A Country town with any life:** residents there.
