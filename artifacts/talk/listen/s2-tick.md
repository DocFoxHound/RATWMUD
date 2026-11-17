# Residents' talk at Slice 2's end: the ticking 100-hour listen

Ridgemere 3, 2026-11-17. Docs/Design/84, Slice 2's end (§15.8, §18; the user's Q6 allows one ticking 100-hour listen
at each slice's end). The code is branch `r3-talk-s2`: Phases 2 to 8 (moves and tags, the turn engine, beats when
spoken, topics and the ledger, opinions and moods, the converted library, the shared lines), before the slice review's
fixes (§34: the traveller's pronouns, two eyewitness lines), which came after this run. Forty conversations from a world
that really ran, as the game's own director chose and spoke them, heard by four listeners nobody plays in Slice 1's four
places (profile L1: The Knucklebones, The Grey Gull Inn, The Rainbarrel, The Bilge).

**The run** (a world simulation, reported as Q6 asks): `talk_sim --world ridgemere3 --tick --profile L1 --places
r2_the_knucklebones,rm_the_grey_gull_inn,rm_the_rainbarrel,rm_the_bilge --hours 100 --seed 1 --ablate shuffle`, from
DEV's save as it was at 05:13 UTC (game day 32.05, 01:12 at night) to day 57.05: 100 world hours, 360,000 world seconds,
25 game days, 7,200,000 ticks of 0.05 s, on world build 81. Nothing was written to the world (a scratch game). 3,877 s
of wall time for the ticking (64 min 46 s with the load; speed 93, beside a card of twenty runs for most of it), 1.38
GiB peak memory. 7,449 conversations, 48,446 lines, 162 speakers heard. The places are named because the save was at
one in the morning: ranked by that moment, L1 chose two street cells where nobody stands to talk, and a first ticking
run there heard nothing in 16 world hours and was stopped after 6 minutes (talk_sim now says where it listens and warns).

**What the scorecard heard** (the ticking L1 run, its shuffled control in brackets; Slice 1's ticking L1 rescored with
today's scorer; L1 over P0's fixture and frozen at noon, from this slice's last card, after the review's fixes):

| | Metric | Ticking L1, Slice 2 (control) | Ticking L1, Slice 1 | Fixture L1 | Frozen L1 |
|---|---|---|---|---|---|
| R1 | first word-for-word repeated conversation, earliest listener (h) | 17.6 (99.5) | 2.54 | 42.9 | 8.29 |
| R2 | first repeated opener (h) | 10.1 (10.1) | 2.54 | 15.1 | 2.57 |
| R3 | distinct openers per 3,000 | 2,803 (2,803) | 2,026 | 2,933 | 2,148 |
| R4 | a distinctive line heard again within 10 h, per 100 h | 1,582 (1,851) | 3,534 | 358 | 6,086 |
| R5 | the same distinctive line from two speakers in one place within an hour, per 100 h | 0 (332) | 951 | 0 | 0 |
| R6 | the most-spoken line's share (%) | 0.815 (0.852) | 0.679 | 1.12 | 1.55 |
| R7 | distinct replies per 1,000 reply beats | 98.1 (90.0) | 13.8 | 182 | 55.1 |
| C1 | owed answers answered (%) | 98.5 (19.4) | - | 98.7 | 98.1 |
| C3 | talks ended by a farewell or a marked break (%) | 89.2 (32.2) | 1.29 | 83.0 | 92.6 |
| C4 | tone clashes per 1,000 beats | 0 (0.0826) | 0 | 0 | 0 |
| C6 | an entry or question twice in a talk, per 1,000 | 0 (179) | 0 | 0 | 0 |
| C7 | lines per conversation | 6.5 (6.5) | 3.21 | 4.93 | 6.12 |
| C8 | talks with a third speaker (%) | 2.56 (2.56) | 0 | 0.174 | 9.41 |
| C9 | callbacks per 10 talks | 4.07 (4.07) | - | 4.18 | 2.09 |
| T1 | raw ids per 1,000 lines | 0 (0) | 0 | 0 | 0 |
| T2 | untrue presuppositions per 1,000 lines | 0.0206 (6.98) | 4.15 | 0 | 0 |
| T3 | self-reference errors per 1,000 lines | 0.227 (0.433) | 0.536 | 0 | 0 |
| T5 | told what they already knew (%) | 0.786 (0.406) | - | 0.3 | 0 |
| T6 | "they" of one named wolf per 1,000 lines | 0.248 (0.206) | 9.96 | 0.616 | 0 |
| K1 | everyday topics (%) | 46.9 (46.9) | 0 | 43.8 | 88.5 |
| K2 | the largest topic (%) | 21.2 (21.2) | 75.2 | 18.9 | 12.5 |
| K3 | the same news told twice to the same resident | 1 (1) | 131 | 0 | 0 |
| D1L | light accents' lines with dialect words (%) | 0.999 (0.76) | 0.139 | 0.113 | 0 |
| D1B | broad accents' lines with dialect words (%) | 1.98 (1.61) | 0.274 | 0.157 | 0 |
| D3 | dialect guessed from 20 lines (%) | 78.7 (91.5) | 38.4 | 91.7 | - |
| U2 | look-alikes told apart (%) | 63.9 (60.1) | 65.6 | 64.8 | 56.8 |
| O1 | opinions aired per 10 talks | 0.801 (0.055) | 0 | 0.356 | 0.604 |

**What it says.** The engine holds up in a living world: owed answers are answered (98.5% against the control's 19%),
nine talks in ten close with a farewell, nobody's news is told to the same wolf twice but once (Signe Brineworth's
gossip about Karin Wetridge to Rune Wickwell, three weeks apart), no raw id, one untrue line ("My son rides with the
Frosthollow caravan." from a wolf with no son, the lint's old frozen finding), and the busiest rooms talk of the
everyday as often as of crime (K1 47%, crime the largest topic at 21%, against 75% at Slice 1). A word-for-word
repeated conversation came first after 17.6 hours (Slice 1: 2.5): "Good for Hollis Slagwick. A trade is a life." / "It
is." in The Grey Gull; The Knucklebones heard one again after 29.5 hours (the authored fallback, "A day like any other."
/ "So it is.", still said where no line fits), The Rainbarrel after 93, The Bilge never. Life moved the
talk: weddings were told by the ones who married ("I married Runa Ashridge.") and congratulated. Heard, and not yet
right: the traveller near was "them" ("I bet that grey stranger could lift a cart." / "Ask them to help unload.": fixed
since, §34); "I saw her marched off." and
"I saw someone near X's door that night." from wolves who saw nothing (fixed since); "We're both from the downs, I
think." said by a Southmeadow baker to a broad Coldwater voice (conversation 3: the accent topic's opener spoken by the
one whose voice is heard, open); the night's farewell "Sleep sound." is the most-said line (395 times, 0.8%); and a
light or broad accent is still mostly plain words in Ridgemere's rooms (D1L 1.0%, D1B 2.0%), which the mantles are for.

**How to listen.** Read each one as overheard talk and mark it **fine**, **off** (it works, but something jars) or
**wrong** (it doesn't make sense, or isn't true), with a note on what. The same forty are on a page with buttons and
notes that saves your marks as a file: `artifacts/talk/listen/s2-tick.html` (open it in a browser; "Export marks" saves
them, for `artifacts/talk/marks/`). Under each line, in small type, is the rule or scene it came from. They were picked
across the kinds of talk, not the commonest (`tools/talk_listen.py --packet 40 --seed 1`: dialect, who the two are to
each other, news or everyday, length).

## 1. memories, strangers: work in `r2_the_knucklebones` (ridgemere), day 32.4, hour 11, clear  
`tick-L1` conversation 9; ended: done

- *Signe Dripworth, none; from Fenshore, light accent*
- *Edmund Sootford, none; from Fenshore, broad accent*

> **Signe Dripworth:** Back again? The wharf was half the length then. Mud and a few boats.  
> <sub>`e3_m_ridgemere_growth` alt 1</sub>  
> **Edmund Sootford:** I thought it had always been this big.  
> <sub>`e3_m_ridgemere_growth` alt 0</sub>  
> **Signe Dripworth:** Nothing's always been anything.  
> <sub>`e3_m_ridgemere_growth` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 2. gossip, strangers: work in `r2_the_knucklebones` (ridgemere), day 32.8, hour 18, clear  
`tick-L1` conversation 86; ended: done

- *Soren Ropewick, helping at The Pawn and Ledger; from Coldwater, light accent*
- *Magnus Millley, working at Grayrock Shipwrights; from Ridgemere, light accent*

> **Soren Ropewick:** Good evening, neighbour. Over wine in the Heights, they say Finn Saltwell tried to steal from Ebba Netherby.  
> <sub>`et_sf_gossip_respectable` alt 0, plain</sub>  
> **Magnus Millley:** Wine loosens tongues.  
> <sub>`et_sf_gossip_respectable` alt 0</sub>  
> **Soren Ropewick:** And purses.  
> <sub>`et_sf_gossip_respectable` alt 0</sub>  
> **Magnus Millley:** Good night.  
> <sub>`cv_bye_night` alt 0</sub>  
> **Soren Ropewick:** Warm bed, now.  
> <sub>`s8t_return_night` alt 7</sub>  

Mark: fine / off / wrong. Note:

## 3. accent, strangers: tavern in `rm_the_grey_gull_inn` (ridgemere), day 34.3, hour 6, clear  
`tick-L1` conversation 476; ended: done

- *Tobias Saltton, baker at Grist & Crust; from Southmeadow, no accent*
- *Corra Pennock, runs The Greatly Lime Kilns; from Coldwater, broad accent*

> **Tobias Saltton:** Morning, neighbour. We're both from the downs, I think. Not the same town, though.  
> <sub>`s83a_south_pair` alt 0, plain</sub>  
> **Corra Pennock:** Coldwater. On Glasswater, under the mountains. You'd love it. Or freeze.  
> <sub>`m7_home_re_cold` alt 0</sub>  
> **Tobias Saltton:** So it goes.  
> <sub>`m7_close_any2` alt 0, plain</sub>  
> **Corra Pennock:** Right. Farewell.  
> <sub>`cv_bye_both` alt 1, plain</sub>  
> **Tobias Saltton:** Aye. Mind how you go.  
> <sub>`cv_return_any2` alt 2, plain</sub>  

Mark: fine / off / wrong. Note:

## 4. crime, friends: tavern in `rm_the_grey_gull_inn` (ridgemere), day 34.5, hour 11, clear  
`tick-L1` conversation 651; ended: apart

- *Wynn Rustworth, butcher in the Market Hall; from Ridgemere, broad accent*
- *Fenwick Mossdale, warden of the Market Hall; from Coldwater, broad accent*

> **Wynn Rustworth:** More trouble. We were on about it only lately. Even the chapel's not safe. Corwin Sootstack's not the only one robbed.  
> <sub>`et_theft_chapel` alt 2</sub>  
> **Fenwick Mossdale:** Nowhere's safe.  
> <sub>`et_theft_chapel` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 5. pastimes, strangers: work in `rm_the_bilge` (ridgemere), day 34.7, hour 17, clear  
`tick-L1` conversation 898; ended: done

- *Aud Dripgate, helping at Sump Sundries; from Fenshore, light accent*
- *Hanne Sootham, unloading ships at the piers; from Ridgemere, light accent*

> **Aud Dripgate:** Hello. I've been reading of an evening.  
> <sub>`e2_pt_reading` alt 0, plain</sub>  
> **Hanne Sootham:** Never learned. Who had the time?  
> <sub>`e2_pt_reading` alt 0</sub>  
> **Aud Dripgate:** I'll teach you. Your name first.  
> <sub>`e2_pt_reading` alt 0</sub>  
> **Hanne Sootham:** Right. Till next time.  
> <sub>`cv_bye_both2` alt 2</sub>  
> **Aud Dripgate:** Go carefully.  
> <sub>`s8t_return2` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 6. gossip, strangers: tavern in `rm_the_rainbarrel` (ridgemere), day 35.0, hour 1, clear  
`tick-L1` conversation 1082; ended: cell

- *Orm Wickwick, helping at Cinder Butchers; from Coldwater, no accent*

> **Orm Wickwick:** Good evening, neighbour. It's on every corner. Corwin Bilgestack tried to steal from Finn Dripwick.  
> <sub>`et_gs_city` alt 2, plain</sub>  

Mark: fine / off / wrong. Note:

## 7. memories, acquaintances: work in `r2_the_knucklebones` (ridgemere), day 35.5, hour 13, clear  
`tick-L1` conversation 1107; ended: done

- *Signe Dripworth, none; from Fenshore, light accent*
- *Edmund Sootford, none; from Fenshore, broad accent*

> **Signe Dripworth:** Can't keep away, then. I wrote a poem for a girl when I was twelve. It rhymed "moon" with "moon".  
> <sub>`e3_m_first_love` alt 1</sub>  
> **Edmund Sootford:** What did she say?  
> <sub>`e3_m_first_love` alt 1</sub>  
> **Signe Dripworth:** She said it was very round. Like the moon.  
> <sub>`e3_m_first_love` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 8. faith, friends: work in `r2_the_knucklebones` (ridgemere), day 36.7, hour 17, clear  
`tick-L1` conversation 1391; ended: done

- *Signe Dripworth, none; from Fenshore, light accent*
- *Edmund Sootford, none; from Fenshore, broad accent*

> **Signe Dripworth:** Back again, feet still dry? We were on about the gods lately, too. Back home you never call a name across the water after dark.  
> <sub>`e4_rf_tell_fen` alt 1, mirebridge</sub>  
> **Edmund Sootford:** I've heard of that. Never seen it done.  
> <sub>`e4_rf_rep_folk` alt 3</sub>  
> **Signe Dripworth:** Truth be told, fen-friend, I've not had a proper meal in days. It makes a wolf bog-tired.  
> <sub>`md_vent_hungry` alt 1, mirebridge</sub>  
> **Edmund Sootford:** You'll think straighter with something in your belly.  
> <sub>`s8o_advice_hungry` alt 3</sub>  
> **Signe Dripworth:** You're right. I know you are.  
> <sub>`s8s_thank_advice` alt 3</sub>  
> **Signe Dripworth:** The Bright One keeps us well, the priest says. I've had a fever. I've offered it up.  
> <sub>`e3_hr_devout` alt 1</sub>  
> **Edmund Sootford:** Could be worse. Could be me.  
> <sub>`e3_ha_dry` alt 1</sub>  
> **Signe Dripworth:** Thank you for your sympathy.  
> <sub>`e3_hb_rough` alt 0</sub>  
> **Edmund Sootford:** Well. Dry feet.  
> <sub>`cv_bye_both` alt 0, mirebridge</sub>  
> **Signe Dripworth:** See you.  
> <sub>`cv_return_any4` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 9. smalltalk, acquaintances: work in `rm_the_bilge` (ridgemere), day 37.8, hour 19, clear  
`tick-L1` conversation 1703; ended: done

- *Aud Dripgate, helping at Sump Sundries; from Fenshore, light accent*
- *Hanne Sootham, unloading ships at the piers; from Ridgemere, light accent*
- *Gwen Fogstack, minding the shop door; from Ridgemere, broad accent*

> **Aud Dripgate:** Back again? Back home rich and poor drink at the same well. Here they've a well each.  
> <sub>`n7_st_country_city` alt 1, plain</sub>  
> **Hanne Sootham:** Don't let me stop you. I'll just listen.  
> <sub>`s8j_favourite_smalltalk3` alt 3</sub>  
> **Aud Dripgate:** Be seeing you.  
> <sub>`cv_bye_any2` alt 0</sub>  
> **Gwen Fogstack:** Rest well.  
> <sub>`cv_return_night` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 10. accent, strangers: tavern in `rm_the_grey_gull_inn` (ridgemere), day 37.9, hour 21, clear  
`tick-L1` conversation 1790; ended: done

- *Dirk Langstead, crying wares in the Rag Market; from Coldwater, no accent*
- *Lothar Abbot, clerking at the Records Office; from Ridgemere, broad accent*

> **Dirk Langstead:** Evening, friend. You talk fast, like the city.  
> <sub>`s83a_city` alt 1, plain</sub>  
> **Lothar Abbot:** Ridgemere. The city. Don't hold it against me.  
> <sub>`s83a_home_city2` alt 0</sub>  
> **Dirk Langstead:** Tell me about it some time. Properly.  
> <sub>`m7_close_home` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 11. ships, friends: tavern in `rm_the_grey_gull_inn` (ridgemere), day 38.5, hour 13, clear  
`tick-L1` conversation 1845; ended: done

- *Maren Hatherley, none; from Ridgemere, broad accent*
- *Torvald Slagton, helping at The Grey Gull Inn; from Ridgemere, light accent*

> **Maren Hatherley:** You again? Butter off the Long Tide, by the crate.  
> <sub>`s83s_in_3` alt 2, plain</sub>  
> **Torvald Slagton:** Then the Wharf'll be loud till dark.  
> <sub>`e8r_sha_in` alt 0</sub>  
> **Maren Hatherley:** And then there's the wagons. First caravan of spring. Roads are mud, but they're open.  
> <sub>`em_season_spring_roads` alt 0</sub>  
> **Torvald Slagton:** Muddy is better than snowed in.  
> <sub>`em_season_spring_roads` alt 0</sub>  
> **Maren Hatherley:** Much better.  
> <sub>`em_season_spring_roads` alt 0</sub>  
> **Maren Hatherley:** Let's hope they keep clear of trouble. Bandits run from anyone who stands their ground.  
> <sub>`em_more_bd_old_soldier` alt 2</sub>  
> **Torvald Slagton:** Do they?  
> <sub>`em_more_bd_old_soldier` alt 2</sub>  
> **Maren Hatherley:** Mostly.  
> <sub>`em_more_bd_old_soldier` alt 2, plain</sub>  
> **Torvald Slagton:** Another time, then.  
> <sub>`cv_bye_any2` alt 2, plain</sub>  
> **Maren Hatherley:** Aye. Good day to you.  
> <sub>`cv_return_any2` alt 2, plain</sub>  

Mark: fine / off / wrong. Note:

## 12. memories, acquaintances: work in `r2_the_knucklebones` (ridgemere), day 38.7, hour 17, clear  
`tick-L1` conversation 1889; ended: apart

- *Edda Muddale, none; from Ridgemere, broad accent*
- *Rupert Cindergate, lighting the lamps at dusk; from Coldwater, no accent*

> **Edda Muddale:** We were hungry every winter. People forget that, when they go misty.  
> <sub>`e3_m_frank_bad` alt 0</sub>  
> **Rupert Cindergate:** I'm sorry.  
> <sub>`e3_m_frank_bad` alt 0, plain</sub>  

Mark: fine / off / wrong. Note:

## 13. travel, spouses: work in `rm_the_bilge` (ridgemere), day 38.8, hour 19, clear  
`tick-L1` conversation 1957; ended: done

- *Gunnar Mudstack, works at The Greatly Masons' Yard; from Larkfield, no accent*
- *Gerrit Ropewick, works at The Greatly Masons' Yard; from Ridgemere, light accent*

> **Gunnar Mudstack:** You again! You've been about a bit, haven't you? Tell me again.  
> <sub>`e5_ask_been_close` alt 0</sub>  
> **Gerrit Ropewick:** Why would I? Everything I want's in walking distance.  
> <sub>`e5_been_content` alt 0</sub>  
> **Gunnar Mudstack:** Look after yourself.  
> <sub>`cv_bye_any2` alt 3</sub>  
> **Gerrit Ropewick:** Sleep sound.  
> <sub>`cv_return_night` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 14. crime, friends: tavern in `rm_the_grey_gull_inn` (ridgemere), day 39.6, hour 14, clear  
`tick-L1` conversation 2110; ended: done

- *Orm Blackwood, none; from Ridgemere, broad accent*
- *Gerrit Hollis, none; from Ridgemere, broad accent*

> **Orm Blackwood:** Kjell Saltton lost a season's salt. Sacks gone from the shed.  
> <sub>`et_sr_theft_salt` alt 1</sub>  
> **Gerrit Hollis:** A season's work!  
> <sub>`et_sr_theft_salt` alt 1</sub>  
> **Orm Blackwood:** Gone in a night.  
> <sub>`et_sr_theft_salt` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 15. life, friends: work in `r2_the_knucklebones` (ridgemere), day 39.6, hour 15, clear  
`tick-L1` conversation 2121; ended: done

- *Edmund Sootford, none; from Fenshore, broad accent*
- *Signe Dripworth, none; from Fenshore, light accent*

> **Edmund Sootford:** A day like any other.  
> <sub>`` alt -1</sub>  
> **Signe Dripworth:** So it is.  
> <sub>`` alt -1</sub>  

Mark: fine / off / wrong. Note:

## 16. prices, acquaintances: work in `r2_the_knucklebones` (ridgemere), day 40.9, hour 21, clear  
`tick-L1` conversation 2495; ended: done

- *Karin Bilgeford, helping at Cinder Butchers; from Coldwater, no accent*
- *Rune Wickwell, none; from Ridgemere, broad accent*

> **Karin Bilgeford:** Can't keep away, then. Cheaper? Why? What's wrong with it?  
> <sub>`w7_pr_down_suspicious` alt 0</sub>  
> **Rune Wickwell:** Down? I'll believe it when I'm charged it.  
> <sub>`s8n_doubt_prices` alt 0</sub>  
> **Karin Bilgeford:** Rest well, then.  
> <sub>`s8t_bye_night` alt 11, plain</sub>  
> **Rune Wickwell:** Good night, friend.  
> <sub>`cv_return_night` alt 4, plain</sub>  

Mark: fine / off / wrong. Note:

## 17. player, acquaintances: tavern in `rm_the_grey_gull_inn` (ridgemere), day 40.9, hour 21, clear  
`tick-L1` conversation 2502; ended: done

- *Dirk Langstead, crying wares in the Rag Market; from Coldwater, no accent*
- *Lothar Abbot, clerking at the Records Office; from Ridgemere, broad accent*

> **Dirk Langstead:** Back again? Any trouble, and it's always the new face that's blamed. Poor that grey stranger.  
> <sub>`h7_stranger_thief_eye` alt 2, plain</sub>  
> **Lothar Abbot:** Poor them? Why?  
> <sub>`h7_stranger_thief_eye` alt 2</sub>  
> **Dirk Langstead:** Because they'll be blamed whatever.  
> <sub>`h7_stranger_thief_eye` alt 2</sub>  
> **Lothar Abbot:** Unfair, that.  
> <sub>`h7_stranger_thief_eye` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 18. caravan, acquaintances: work in `r2_the_knucklebones` (ridgemere), day 40.9, hour 22, clear  
`tick-L1` conversation 2543; ended: done

- *Una Plankton, cooking for the King's household; from Larkfield, light accent*
- *Soren Coaldale, portering in Market Square; from Ridgemere, light accent*

> **Una Plankton:** You again! Wagon day. I'll have a pot on.  
> <sub>`s83v_due_keeper` alt 2</sub>  
> **Soren Coaldale:** Long road, that. They'll be glad of a rest.  
> <sub>`e8r_cvd_carter` alt 0</sub>  
> **Una Plankton:** Aye, well.  
> <sub>`e8r_cv_close` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 19. crime, acquaintances: work in `rm_the_bilge` (ridgemere), day 41.4, hour 10, clear  
`tick-L1` conversation 2698; ended: done

- *Aud Dripgate, helping at Sump Sundries; from Fenshore, light accent*
- *Greta Gullton, working the cranes; from Larkfield, no accent*

> **Aud Dripgate:** The Watch took Jorunn Ropewick. Some House will have her out by morning.  
> <sub>`et_rm_arrest_houses` alt 0</sub>  
> **Greta Gullton:** If they're owned, they'll walk.  
> <sub>`et_rm_arrest_houses` alt 0</sub>  
> **Aud Dripgate:** They're always owned.  
> <sub>`et_rm_arrest_houses` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 20. caravan, spouses: work in `rm_the_bilge` (ridgemere), day 41.5, hour 13, clear  
`tick-L1` conversation 2810; ended: done

- *Gerrit Ropewick, works at The Greatly Masons' Yard; from Ridgemere, light accent*
- *Gunnar Mudstack, works at The Greatly Masons' Yard; from Larkfield, no accent*

> **Gerrit Ropewick:** Every time we meet, it's the carts. When the Coldwater wagon's in, I'll be at the yard.  
> <sub>`s83v_due_laborer` alt 1</sub>  
> **Gunnar Mudstack:** Willow bark. That'll help.  
> <sub>`s83v_ans_due2` alt 2</sub>  
> **Gerrit Ropewick:** I went to bed empty as a drum again. I'm not complaining. Well, I am.  
> <sub>`md_vent_hungry` alt 2, plain</sub>  
> **Gunnar Mudstack:** That's a sorry thing. You should have said sooner.  
> <sub>`md_sympathise` alt 4</sub>  
> **Gerrit Ropewick:** Wagons in, prices up. Or down, if we're lucky. Everything's dearer than last week. Everything.  
> <sub>`w7_pr_up_shrug` alt 1</sub>  
> **Gunnar Mudstack:** Aye. Nothing to be done.  
> <sub>`prices_answer` alt 1, plain</sub>  
> **Gerrit Ropewick:** Go well.  
> <sub>`cv_bye_any3` alt 1</sub>  
> **Gunnar Mudstack:** Aye, you too.  
> <sub>`cv_return_any3` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 21. faith, acquaintances: work in `rm_the_bilge` (ridgemere), day 41.8, hour 19, clear  
`tick-L1` conversation 3042; ended: done

- *Corwin Sootton, unloading ships at the piers; from Ridgemere, broad accent*
- *Hanne Sootham, unloading ships at the piers; from Ridgemere, light accent*

> **Corwin Sootton:** Say one for me.  
> <sub>`e4_fa_quip_pray` alt 0</sub>  
> **Hanne Sootham:** I'll say two.  
> <sub>`e4_fa_quip_pray` alt 0</sub>  
> **Corwin Sootton:** The priest says dreams are a kind of prayer. One day I'll sail somewhere nobody's ever drawn on a chart.  
> <sub>`e4_dr_fisher` alt 1</sub>  
> **Hanne Sootham:** Blue's bad luck on a hull. Paint her red.  
> <sub>`e4_dr_fisher` alt 0</sub>  
> **Corwin Sootton:** Red, then. Superstitious lot.  
> <sub>`e4_dr_fisher` alt 0</sub>  
> **Hanne Sootham:** Mind yourself, then.  
> <sub>`s8t_bye` alt 1, plain</sub>  
> **Corwin Sootton:** Good night.  
> <sub>`cv_return_night` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 22. land, housemates: tavern in `rm_the_rainbarrel` (ridgemere), day 41.8, hour 19, clear  
`tick-L1` conversation 3052; ended: done

- *Jarl Greyford, helping at Harbour Provisions; from Ridgemere, light accent*
- *Rosamund Coalford, runs The North Wharf Smokehouse; from Ridgemere, broad accent*

> **Jarl Greyford:** You can tell how rich a wolf is by how high up they live.  
> <sub>`e5_rg_rm_terraces` alt 1</sub>  
> **Rosamund Coalford:** And the rain runs downhill. Guess where it all ends up.  
> <sub>`e5_rg_rm_terraces` alt 0</sub>  
> **Jarl Greyford:** In your boots, by the sound of it.  
> <sub>`e5_rg_rm_terraces` alt 0</sub>  
> **Jarl Greyford:** But never mind that. Not that song again.  
> <sub>`st_tavern_song` alt 1</sub>  
> **Rosamund Coalford:** It's the only one they know.  
> <sub>`st_tavern_song` alt 1</sub>  
> **Jarl Greyford:** Then teach them another.  
> <sub>`st_tavern_song` alt 1</sub>  
> **Rosamund Coalford:** Till morning, then.  
> <sub>`cv_bye_night` alt 6</sub>  
> **Jarl Greyford:** Till next time.  
> <sub>`cv_return_any2` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 23. caravan, friends: work in `r2_the_knucklebones` (ridgemere), day 43.6, hour 15, clear  
`tick-L1` conversation 3486; ended: done

- *Edmund Sootford, none; from Fenshore, broad accent*
- *Signe Dripworth, none; from Fenshore, light accent*

> **Edmund Sootford:** Berries are dear in Coldwater, so the Coldwater wagon says.  
> <sub>`e8r_cv_news` alt 1</sub>  
> **Signe Dripworth:** I'll hear about it when it hits my supper.  
> <sub>`s83v_ans_news_poor` alt 2</sub>  
> **Edmund Sootford:** And whatever the wagons bring, it's dear. Ninety-nine pence! I can have theriac this week.  
> <sub>`e8p_down_poor` alt 0</sub>  
> **Signe Dripworth:** Cheaper's the best word I've heard all week.  
> <sub>`w7_cheap_poor` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 24. crime, strangers: work in `r2_the_knucklebones` (ridgemere), day 46.7, hour 17, clear  
`tick-L1` conversation 4036; ended: apart

- *Ebba Heath, crying wares in the Rag Market; from Southmeadow, light accent*
- *Karin Ashgate, keeping the taproom; from Ridgemere, light accent*

> **Ebba Heath:** Morning, friend. Eleanor Vance was robbed in the crowd by the hall.  
> <sub>`et_cr_city_theft` alt 2, plain</sub>  
> **Karin Ashgate:** Every crowd has a thief in it.  
> <sub>`et_cr_city_theft` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 25. prices, acquaintances: tavern in `rm_the_rainbarrel` (ridgemere), day 46.9, hour 22, clear  
`tick-L1` conversation 4204; ended: cell

- *Karin Wetridge, working at The Fish Hall; from Saltwell, no accent*
- *Rune Cinderwick, helping at The Hook and Cleaver; from Ridgemere, light accent*

> **Karin Wetridge:** Again! Theriac down. First good word all month.  
> <sub>`e8p_down_poor` alt 1</sub>  
> **Rune Cinderwick:** I'll believe it when I pay it.  
> <sub>`w7_cheap_plain` alt 3</sub>  
> **Karin Wetridge:** And it's not just that. Twenty-nine pence? For handbells? Daylight robbery.  
> <sub>`prices_complain_poor` alt 1, plain</sub>  
> **Rune Cinderwick:** Don't I know it.  
> <sub>`prices_answer_poor` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 26. season, spouses: tavern in `rm_the_rainbarrel` (ridgemere), day 47.9, hour 22, clear  
`tick-L1` conversation 4444; ended: cell

- *Maren Tarhand, printing at The Council Press; from Ridgemere, light accent*
- *Runa Ashridge, printing at The Council Press; from Ridgemere, light accent*

> **Maren Tarhand:** Nettle soup again. Third day.  
> <sub>`e5_se_spring_hungry_gap` alt 2</sub>  
> **Runa Ashridge:** Same at ours. We're eating the seed potatoes.  
> <sub>`e5_se_spring_hungry_gap` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 27. smalltalk, acquaintances: work in `r2_the_knucklebones` (ridgemere), day 48.3, hour 7, clear  
`tick-L1` conversation 4468; ended: done

- *Una Plankton, cooking for the King's household; from Larkfield, light accent*
- *Soren Coaldale, portering in Market Square; from Ridgemere, light accent*

> **Una Plankton:** Can't keep away, then. I've lost my good comb.  
> <sub>`st_lost_thing` alt 1</sub>  
> **Soren Coaldale:** Where did you see it last?  
> <sub>`st_lost_thing` alt 1</sub>  
> **Una Plankton:** Never mind.  
> <sub>`st_lost_thing` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 28. memories, friends: work in `r2_the_knucklebones` (ridgemere), day 51.5, hour 12, clear  
`tick-L1` conversation 5597; ended: done

- *Signe Dripworth, none; from Fenshore, light accent*
- *Edmund Sootford, none; from Fenshore, broad accent*

> **Signe Dripworth:** Back again, feet still dry? My mother never raised her voice. She didn't need to.  
> <sub>`e3_m_strict_mother` alt 2, mirebridge</sub>  
> **Edmund Sootford:** Those are the frightening ones.  
> <sub>`e3_m_strict_mother` alt 2</sub>  
> **Signe Dripworth:** I still sit up straight when I think of her.  
> <sub>`e3_m_strict_mother` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 29. crime, family: tavern in `rm_the_rainbarrel` (ridgemere), day 51.9, hour 22, clear  
`tick-L1` conversation 5830; ended: cell

- *Lionel Dripworth, merchant; from Larkfield, light accent*
- *Ivo Dripworth, helping at Gullgut Fish; from Ridgemere, no accent*

> **Lionel Dripworth:** More trouble. We were on about it only lately. Bo Gilbey got robbed by the Sump. No surprise there.  
> <sub>`et_rm_theft_sump` alt 0</sub>  
> **Ivo Dripworth:** The Sump eats purses.  
> <sub>`et_rm_theft_sump` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 30. accent, strangers: work in `r2_the_knucklebones` (ridgemere), day 52.7, hour 17, clear  
`tick-L1` conversation 5925; ended: apart

- *Ebba Heath, crying wares in the Rag Market; from Southmeadow, light accent*
- *Karin Ashgate, keeping the taproom; from Ridgemere, light accent*

> **Ebba Heath:** Good day, neighbour. I'm sorry, what was that?  
> <sub>`ac_what_did_you_say` alt 0, plain</sub>  
> **Karin Ashgate:** I said it's terribly cold.  
> <sub>`ac_what_did_you_say` alt 1, plain</sub>  
> **Ebba Heath:** Right. Well. Hello.  
> <sub>`ac_what_did_you_say` alt 1, plain</sub>  

Mark: fine / off / wrong. Note:

## 31. prices, acquaintances: work in `r2_the_knucklebones` (ridgemere), day 52.9, hour 21, clear  
`tick-L1` conversation 6034; ended: done

- *Ulla Greymore, washing at the Washhouse; from Ridgemere, no accent*
- *Knut Sootley, unloading ships at the piers; from Ridgemere, light accent*

> **Ulla Greymore:** Ivo Tarmarsh kept a loaf back for me. Didn't even ask.  
> <sub>`n7_ro_shopkeeper_kind` alt 0</sub>  
> **Knut Sootley:** Good sort, that one.  
> <sub>`n7_aa_shopgood` alt 0</sub>  
> **Ulla Greymore:** There's always something.  
> <sub>`n7_close_ans` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 32. crime, housemates: work in `rm_the_bilge` (ridgemere), day 52.9, hour 21, clear  
`tick-L1` conversation 6035; ended: done

- *Greta Gullton, working the cranes; from Larkfield, no accent*
- *Grete Gullhand, works at The Greatly Yard; from Ridgemere, light accent*

> **Greta Gullton:** You again? More trouble. We were on about it only lately. The Heights'll hear Rosamund Tarridge was robbed and blame the Sump.  
> <sub>`h7_crime_sump` alt 2, plain</sub>  
> **Grete Gullhand:** They do anyway.  
> <sub>`h7_crime_sump` alt 2</sub>  
> **Greta Gullton:** Let them. We've nothing.  
> <sub>`h7_crime_sump` alt 2</sub>  
> **Grete Gullhand:** And they still take it.  
> <sub>`h7_crime_sump` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 33. crime, acquaintances: work in `r2_the_knucklebones` (ridgemere), day 53.6, hour 15, clear  
`tick-L1` conversation 6156; ended: done

- *Rupert Cindergate, lighting the lamps at dusk; from Coldwater, no accent*
- *Florence Gullworth, none; from Ridgemere, broad accent*

> **Rupert Cindergate:** They knew just where the strongbox was. That wasn't luck.  
> <sub>`et_theft_inside_job` alt 0</sub>  
> **Florence Gullworth:** Someone close, then.  
> <sub>`et_theft_inside_job` alt 0</sub>  
> **Rupert Cindergate:** Someone close.  
> <sub>`et_theft_inside_job` alt 0</sub>  
> **Rupert Cindergate:** And it's not the first this week. The Heights'll hear Rosamund Tarridge was robbed and blame the Sump.  
> <sub>`h7_crime_sump` alt 2</sub>  
> **Florence Gullworth:** They do anyway.  
> <sub>`h7_crime_sump` alt 2</sub>  
> **Rupert Cindergate:** Let them. We've nothing.  
> <sub>`h7_crime_sump` alt 2</sub>  
> **Florence Gullworth:** And they still take it.  
> <sub>`h7_crime_sump` alt 2</sub>  
> **Rupert Cindergate:** Right. Farewell.  
> <sub>`cv_bye_both` alt 1, plain</sub>  
> **Florence Gullworth:** Off you go.  
> <sub>`s8t_return` alt 0, plain</sub>  

Mark: fine / off / wrong. Note:

## 34. gossip, strangers: work in `r2_the_knucklebones` (ridgemere), day 54.1, hour 3, clear  
`tick-L1` conversation 6343; ended: apart

- *Soren Ropewick, helping at The Pawn and Ledger; from Coldwater, light accent*
- *Yrsa Nailwell, washing at the Washhouse; from Ridgemere, light accent*

> **Soren Ropewick:** Good evening, friend. Quietly. Dagfinn Ropeby stole from Elspet Sootley.  
> <sub>`s83g_theft_work` alt 1, plain</sub>  
> **Yrsa Nailwell:** Not much to steal off me, at least.  
> <sub>`s8n_theft_poor` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 35. life, friends: tavern in `rm_the_rainbarrel` (ridgemere), day 54.9, hour 21, clear  
`tick-L1` conversation 6556; ended: done

- *Karin Wetridge, working at The Fish Hall; from Saltwell, no accent*
- *Rune Cinderwick, helping at The Hook and Cleaver; from Ridgemere, light accent*

> **Karin Wetridge:** The stalls are full today.  
> <sub>`` alt -1</sub>  
> **Rune Cinderwick:** So it is.  
> <sub>`` alt -1</sub>  
> **Karin Wetridge:** Lamps are lit. Off home.  
> <sub>`s8t_bye_night` alt 11, plain</sub>  
> **Rune Cinderwick:** Don't be a stranger.  
> <sub>`s8t_return` alt 0, plain</sub>  

Mark: fine / off / wrong. Note:

## 36. food, strangers: tavern in `rm_the_rainbarrel` (ridgemere), day 55.3, hour 8, clear  
`tick-L1` conversation 6651; ended: done

- *Jorunn Saltdale, helping at The Penny Loaf; from Frosthollow, no accent*
- *Josephine Dripgate, shopkeeper at Sump Sundries; from Ridgemere, broad accent*

> **Jorunn Saltdale:** Hello. Tobias Rainley cooked last night. Something new. I'm still not sure what.  
> <sub>`e1_fd_tell_spouse` alt 0, plain</sub>  
> **Josephine Dripgate:** Ha! Good.  
> <sub>`glad_plain` alt 0</sub>  
> **Jorunn Saltdale:** Another time, then.  
> <sub>`cv_bye_any2` alt 2, plain</sub>  
> **Josephine Dripgate:** Mind yourself.  
> <sub>`cv_return_any4` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 37. faith, acquaintances: work in `rm_the_bilge` (ridgemere), day 55.4, hour 9, clear  
`tick-L1` conversation 6681; ended: done

- *Greta Gullton, working the cranes; from Larkfield, no accent*
- *Finn Saltwell, unloading ships at the piers; from Ridgemere, broad accent*

> **Greta Gullton:** You again? I said the dawn prayer on the step this morning. It sets the day right.  
> <sub>`e4_rf_tell_devout` alt 0, plain</sub>  
> **Finn Saltwell:** I wish I had your faith.  
> <sub>`e4_rf_rep_keen` alt 1</sub>  
> **Greta Gullton:** Say what you like about the Restday dole; it fed my house through a lean week, and I'll not forget it.  
> <sub>`op_give_for` alt 1</sub>  
> **Finn Saltwell:** Just what I was thinking.  
> <sub>`s8o_agree_home2` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 38. gossip, strangers: work in `rm_the_bilge` (ridgemere), day 55.4, hour 10, clear  
`tick-L1` conversation 6709; ended: done

- *Gorm Tarton, works at The Wharf Tannery; from Saltwell, light accent*
- *Agna Bilgecot, working at The Fell Preserving House; from Larkfield, light accent*

> **Gorm Tarton:** Good day, neighbour. They'll blame the likes of us. But Magnus Wetherell tried to steal from Rosamund Tarridge.  
> <sub>`s83g_theft_poor` alt 2, plain</sub>  
> **Agna Bilgecot:** From Rosamund Tarridge? That's low.  
> <sub>`s8n_theft_victim` alt 3</sub>  
> **Gorm Tarton:** I'll see you.  
> <sub>`cv_bye_any3` alt 2</sub>  
> **Agna Bilgecot:** Till next time.  
> <sub>`cv_return_any2` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 39. animals, spouses: tavern in `rm_the_rainbarrel` (ridgemere), day 55.7, hour 18, clear  
`tick-L1` conversation 7031; ended: done

- *Jarl Greyford, helping at Harbour Provisions; from Ridgemere, light accent*
- *Rosamund Coalford, runs The North Wharf Smokehouse; from Ridgemere, broad accent*

> **Jarl Greyford:** Somebody's goose has taken the bridge. Nobody can cross.  
> <sub>`e2_an_mad_goose` alt 1</sub>  
> **Rosamund Coalford:** Not even the Watch?  
> <sub>`e2_an_mad_goose` alt 1</sub>  
> **Jarl Greyford:** The Watch went round the long way.  
> <sub>`e2_an_mad_goose` alt 1</sub>  
> **Jarl Greyford:** And what they graze on. From the Heights you can see the hills go blue, ridge after ridge.  
> <sub>`e5_rg_rm_hills_behind` alt 1</sub>  
> **Rosamund Coalford:** Pretty, if you've the climb in you.  
> <sub>`e5_rg_rm_hills_behind` alt 1</sub>  
> **Jarl Greyford:** I haven't.  
> <sub>`e5_rg_rm_hills_behind` alt 1</sub>  
> **Rosamund Coalford:** I'll see you.  
> <sub>`cv_bye_any3` alt 2</sub>  
> **Jarl Greyford:** Don't be a stranger.  
> <sub>`s8t_return` alt 0, plain</sub>  

Mark: fine / off / wrong. Note:

## 40. crime, friends: work in `r2_the_knucklebones` (ridgemere), day 56.7, hour 17, clear  
`tick-L1` conversation 7283; ended: done

- *Una Plankton, cooking for the King's household; from Larkfield, light accent*
- *Florence Gullworth, none; from Ridgemere, broad accent*

> **Una Plankton:** Fenwick Mossdale's been done. The Watch'll be down here looking by noon.  
> <sub>`h7_cr_open_sump` alt 0, plain</sub>  
> **Florence Gullworth:** If they took bread, I'd hold the door.  
> <sub>`h7_cr_ans_hungry` alt 2</sub>  
> **Una Plankton:** I know. Gods, I know.  
> <sub>`h7_cr_close_pity_poor` alt 1</sub>  
> **Florence Gullworth:** Between us, I've not had a proper meal in days. It makes a wolf tired.  
> <sub>`md_vent_hungry` alt 1, plain</sub>  
> **Una Plankton:** You don't deserve that.  
> <sub>`md_sympathise` alt 1</sub>  
> **Una Plankton:** It doesn't stop there, either. City thieves are quick as eels. Isolde Ropehand never felt a thing.  
> <sub>`et_cr_city_theft` alt 1</sub>  
> **Florence Gullworth:** Eels in a barrel.  
> <sub>`et_cr_city_theft` alt 1</sub>  
> **Una Plankton:** Slippery.  
> <sub>`et_cr_city_theft` alt 1</sub>  
> **Una Plankton:** Not all the news is bad, mind. Someone should look in on Bo Rainmarsh. After Aud Kemp.  
> <sub>`h7_death_mourner` alt 2</sub>  
> **Florence Gullworth:** I'll go. I'll take something.  
> <sub>`h7_death_mourner` alt 2</sub>  
> **Una Plankton:** Take bread. Nobody eats, but take it.  
> <sub>`h7_death_mourner` alt 2</sub>  
> **Florence Gullworth:** I'll take bread.  
> <sub>`h7_death_mourner` alt 2</sub>  
> **Una Plankton:** Take care.  
> <sub>`cv_bye_any` alt 3</sub>  
> **Florence Gullworth:** Mind yourself, you.  
> <sub>`s8t_return2` alt 0, plain</sub>  

Mark: fine / off / wrong. Note:

