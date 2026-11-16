# Residents' talk today: the Phase 0 baseline (a listen sheet)

Ridgemere 3, 2026-11-16. Docs/Design/84, Phase 0. Forty conversations between residents, as the game's own director
chose and spoke them, heard by listeners nobody plays (`talk_sim`) over the one ticking 25-game-day run that recorded
the fixture (day 27.8 to 52.8). The library is today's (`ridgemere2` a46cbdac): nothing in it has changed yet, so this is
what a player hears now, and what every later phase is measured against.

They were picked across the kinds of talk, not the commonest: by dialect, by who the two are to each other (kin,
friends, acquaintances, strangers), by news or everyday topic, and by length; from Ridgemere's four busiest places (L1),
a wanderer (L2), the towns (L3-L7) and a tavern regular (L10). Frosthollow and Oakhaven (L8, L9) are missing because a
listener there heard nothing in 100 hours: in both towns, nobody but the keeper is ever in the alehouse, store or press,
and the only place two stand together is a cottage.

**How to listen.** Read each one as overheard talk and mark it **fine**, **off** (it works, but something jars) or
**wrong** (it doesn't make sense, or isn't true), with a note on what. The same forty are on a page with buttons and
notes that saves your marks as a file: `artifacts/talk/listen/p0-baseline.html` (open it in a browser; "Export marks"
saves them, for `artifacts/talk/marks/`). Under each line, in small type, is the rule or scene it came from.

**What the scorecard heard** (`artifacts/talk/scorecard-baseline-fixtures.json`: 13,021 conversations, 39,073 lines, 8
listeners, 100 hours each; the shuffled control in brackets, the same lines put into other conversations of the same
topic):

| | Today | Shuffled control | Target |
|---|---|---|---|
| First word-for-word repeated conversation, the earliest listener | 2.2 h (Fenshore's listener, at its 6th exchange) | 2.3 h | 40 h+ |
| First repeated opener | 2.2 h | 2.2 h | 8 h+ |
| The most-spoken line ("Good.") | 3.6% of all lines | 3.6% | 0.4% |
| The same distinctive line from two speakers in one place within an hour | 189 per 100 h | 208 | 1 or fewer |
| Distinct replies per 1,000 reply beats | 53 | 49 | 500+ |
| Lines per conversation | 3.0 (88% are 3 lines; p90 4) | 3.0 | 5-7 |
| Questions answered by the other | 96% | 87% | 97% |
| Replies sharing a word with the line before (cohesion) | 27% | 1.6% | (shows the control works) |
| Gossip and crime | 71% of conversations (gossip 40%, crime 30%) | | 25% largest topic |
| Everyday topics | 3.8% | | 35%+ |
| The same news told twice to the same resident | 2,284 times | | 0 |
| Raw ids spoken ("camp_w_ridgemere_6_8 raids the road...") | 13 per 1,000 lines | 13 | 0 |
| A teller naming themselves as the victim | 3.3 per 1,000 lines | 3.4 | 0 |
| Lines in a light accent with any dialect word | 0.2% | 0.2% | 25-50% |
| A speaker's dialect guessed from 20 of their lines | 67% | 72% | 85%+ |

**What you'll hear** (each a Phase 1 fix or a later phase's): raw ids spoken; the victim telling of their own robbery in
the third person; "Good." and "A day like any other." again and again; three-line exchanges that end on nothing; gossip
and crime almost everywhere; the dialects barely showing.

---


## 1. gossip, strangers: tavern in `rm_the_grey_gull_inn` (ridgemere), day 28.8, hour 20, clear  
`L1` conversation 179; ended: done

- *Tobias Saltton, baker at Grist & Crust; from Southmeadow, no accent*
- *Lionel Lowe, working at The Eastgate Stables; from Ridgemere, broad accent*

> **Tobias Saltton:** Camp_w_ridgemere_6_8 raids the road at w_ridgemere_6_8, and still walks about with that nose in the air.  
> <sub>`et_gos_petty` alt 1</sub>  
> **Lionel Lowe:** That nose.  
> <sub>`et_gos_petty` alt 1</sub>  
> **Tobias Saltton:** Higher than ever.  
> <sub>`et_gos_petty` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 2. crime, acquaintances: tavern in `rm_the_grey_gull_inn` (ridgemere), day 33.7, hour 17, clear  
`L1` conversation 1115; ended: done

- *Rosamund Tarridge, keeping the inn; from Ridgemere, light accent*
- *Gunnar Farrow, none; from Ridgemere, broad accent*

> **Rosamund Tarridge:** My ears pricked at something that night. Thought it was the wind.  
> <sub>`h7_theft_night_noise` alt 2</sub>  
> **Gunnar Farrow:** Probably was the wind.  
> <sub>`h7_theft_night_noise` alt 2</sub>  
> **Rosamund Tarridge:** Wind doesn't take purses.  
> <sub>`h7_theft_night_noise` alt 2</sub>  
> **Gunnar Farrow:** Nor does it give them back.  
> <sub>`h7_theft_night_noise` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 3. gossip, strangers: work in `rm_the_commons_school` (ridgemere), day 37.6, hour 13, clear  
`L1` conversation 1766; ended: done

- *Silas Brinegate, at lessons; from Ridgemere, broad accent*
- *Elspet Ropewick, at lessons; from Ridgemere, no accent*

> **Silas Brinegate:** The big pups at the well say Jarl Greywell tried to steal from Josephine Wetmore.  
> <sub>`n7_go_young` alt 1</sub>  
> **Elspet Ropewick:** Mm.  
> <sub>`n7_ga2_plain2` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 4. crime, acquaintances: tavern in `rm_the_grey_gull_inn` (ridgemere), day 38.7, hour 16, clear  
`L1` conversation 1992; ended: done

- *Orm Blackwood, none; from Ridgemere, broad accent*
- *Gerrit Hollis, none; from Ridgemere, broad accent*

> **Orm Blackwood:** They knew just where the strongbox was. That wasn't luck.  
> <sub>`et_theft_inside_job` alt 0</sub>  
> **Gerrit Hollis:** Someone close, then.  
> <sub>`et_theft_inside_job` alt 0</sub>  
> **Orm Blackwood:** Someone close.  
> <sub>`et_theft_inside_job` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 5. gossip, acquaintances: work in `rm_the_commons_school` (ridgemere), day 39.6, hour 14, clear  
`L1` conversation 2146; ended: done

- *Greta Thornworth, at lessons; from Ridgemere, light accent*
- *Elspet Ropewick, at lessons; from Ridgemere, no accent*

> **Greta Thornworth:** I heard the grown-ups. Jarl Greyford is wanted for attempted theft! I'm not supposed to know.  
> <sub>`n7_go_young` alt 2</sub>  
> **Elspet Ropewick:** Mm.  
> <sub>`n7_ga2_plain2` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 6. gossip, strangers: shore in `w_ridgemere_0_1` (ridgemere), day 46.3, hour 6, overcast  
`L1` conversation 3335; ended: done

- *Gorm Tarton, works at The Wharf Tannery; from Saltwell, light accent*
- *Gwen Brineby, working the cranes; from Larkfield, light accent*

> **Gorm Tarton:** Lean in. Camp_w_ridgemere_7_6 raids the road at w_ridgemere_7_6. You didn't hear it from me.  
> <sub>`et_gos_whisper` alt 0</sub>  
> **Gwen Brineby:** I never hear anything from you.  
> <sub>`et_gos_whisper` alt 0</sub>  
> **Gorm Tarton:** Good.  
> <sub>`et_gos_whisper` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 7. crime, family: work in `rm_the_commons_school` (ridgemere), day 50.3, hour 8, clear  
`L1` conversation 4097; ended: done

- *Orm Fogmore, at lessons; from Ridgemere, light accent*
- *Josephine Wetmore, at lessons; from Ridgemere, light accent*

> **Orm Fogmore:** They'll blame the Sump for Fenwick Mossdale. Watch.  
> <sub>`h7_cr_open_sump` alt 1</sub>  
> **Josephine Wetmore:** Up the hill, that's news. Down here it's the weather.  
> <sub>`h7_cr_ans_sump` alt 2</sub>  
> **Orm Fogmore:** They'll not, this time.  
> <sub>`h7_cr_close_blame` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 8. prices, friends: work in `fs_grain_lark_granary` (southmeadow), day 31.5, hour 13, clear  
`L2` conversation 291; ended: done

- *Iris Meadowfield, farms Larkholm; from Southmeadow, light accent*
- *Alder Blossomcroft, farmhand at Larkholm; from Southmeadow, broad accent*

> **Iris Meadowfield:** Herbs at twenty-nine pence the dozen. We'll manage, but I noticed.  
> <sub>`e8p_up_comfortable` alt 0</sub>  
> **Alder Blossomcroft:** Ripe and right. We'll all go short together.  
> <sub>`prices_answer_poor` alt 0, southmeadow</sub>  

Mark: fine / off / wrong. Note:

## 9. family, spouses: wild in `w_ridgemere_4_4` (rainwood), day 32.5, hour 11, rain  
`L2` conversation 357; ended: done

- *Frost Colddrift, fells timber at the Oakhaven cut; from Oakhaven, light accent*
- *Ingram Ashleaf, logger at the Oakhaven cut; from Oakhaven, light accent*

> **Frost Colddrift:** Your father still doesn't like me.  
> <sub>`kn_mates_new_family_visit` alt 2</sub>  
> **Ingram Ashleaf:** He doesn't like anyone.  
> <sub>`kn_mates_new_family_visit` alt 2</sub>  
> **Frost Colddrift:** He likes you.  
> <sub>`kn_mates_new_family_visit` alt 2</sub>  
> **Ingram Ashleaf:** I'm his pup. He has to.  
> <sub>`kn_mates_new_family_visit` alt 2</sub>  
> **Frost Colddrift:** That doesn't count.  
> <sub>`kn_mates_new_family_visit` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 10. family, spouses: wild in `w_ridgemere_4_4` (rainwood), day 32.5, hour 12, rain  
`L2` conversation 358; ended: done

- *Frost Colddrift, fells timber at the Oakhaven cut; from Oakhaven, light accent*
- *Ingram Ashleaf, logger at the Oakhaven cut; from Oakhaven, light accent*

> **Frost Colddrift:** Do you remember the year we nearly lost the house?  
> <sub>`f7_sp_middle` alt 1</sub>  
> **Ingram Ashleaf:** Give me your paw.  
> <sub>`f7_sa_years_warm` alt 0</sub>  
> **Frost Colddrift:** Come here, you.  
> <sub>`f7_st_share_kind` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 11. gossip, strangers: work in `rm_the_last_lamp` (ridgemere), day 32.8, hour 20, clear  
`L2` conversation 402; ended: done

- *Osric Ropeworth, provisioner at Vane & Daughters; from Frosthollow, no accent*
- *Jorunn Cindermore, smith at Eastgate Ironmongers; from Ridgemere, light accent*

> **Osric Ropeworth:** I shouldn't say. But I will. Soren Ashcote tried to steal from Gudrun Wickby.  
> <sub>`n7_go_sly` alt 0</sub>  
> **Jorunn Cindermore:** Hm.  
> <sub>`n7_ga2_plain2` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 12. gossip, friends: work in `r2_the_abbey_school` (ridgemere), day 36.5, hour 11, clear  
`L2` conversation 784; ended: done

- *Mab Saltgate, at lessons; from Ridgemere, light accent*
- *Eleanor Wetby, teaching at the abbey school; from Ridgemere, light accent*

> **Mab Saltgate:** I've been saving this one for you. Philippa Pitchstack is wanted for theft.  
> <sub>`et_gos_delight` alt 1</sub>  
> **Eleanor Wetby:** You're a treasure. Go on.  
> <sub>`et_gos_delight` alt 1</sub>  
> **Mab Saltgate:** That's it. That's the whole of it.  
> <sub>`et_gos_delight` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 13. crime, strangers: work in `rm_the_bilge` (ridgemere), day 40.3, hour 6, clear  
`L2` conversation 1078; ended: gone

- *Hester Fogstack, provisioner at Harbour Provisions; from Ridgemere, light accent*

> **Hester Fogstack:** Somebody went through the smokehouse in the night.  
> <sub>`et_cr_coast_theft` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 14. crime, friends: hall in `rm_the_council_press` (ridgemere), day 40.4, hour 10, clear  
`L2` conversation 1093; ended: gone

- *Wynn Fogworth, printing at The Council Press; from Oakhaven, no accent*

> **Wynn Fogworth:** With road wolves on the roads, who's to say they're not in our streets too?  
> <sub>`h7_crime_bandits_in_town` alt 2, plain</sub>  

Mark: fine / off / wrong. Note:

## 15. crime, strangers: shore in `w_ridgemere_0_1` (ridgemere), day 40.5, hour 11, rain  
`L2` conversation 1100; ended: done

- *Agnes Millford, greens stall in the Rag Market; from Oakhaven, light accent*
- *Desmond Grantham, runs The Rainwater Brewery; from Ridgemere, broad accent*

> **Agnes Millford:** My neighbour nailed their shutters shut after Frida Marrowby.  
> <sub>`h7_theft_doors` alt 2</sub>  
> **Desmond Grantham:** And how do they get the air in?  
> <sub>`h7_theft_doors` alt 2</sub>  
> **Agnes Millford:** They don't. They sweat.  
> <sub>`h7_theft_doors` alt 2</sub>  
> **Desmond Grantham:** Better sweaty than robbed.  
> <sub>`h7_theft_doors` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 16. bandits, spouses: work in `cn_larkfield_bakehouse` (larkfield), day 44.6, hour 15, clear  
`L2` conversation 1518; ended: done

- *Rowan Chalkley, shop hand at Larkfield Bakehouse; from Larkfield, no accent*
- *Eskar Cornley, baker at Larkfield Bakehouse; from Larkfield, broad accent*

> **Rowan Chalkley:** They watch the road from The Greenholt, Glasswater. You can feel it.  
> <sub>`em_bd_camp_scared` alt 2</sub>  
> **Eskar Cornley:** I've felt it. Like eyes on your neck.  
> <sub>`em_bd_camp_scared` alt 2</sub>  
> **Rowan Chalkley:** That's it exactly.  
> <sub>`em_bd_camp_scared` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 17. family, spouses: wild in `w_ridgemere_5_9` (greenholt), day 44.7, hour 17, clear  
`L2` conversation 1534; ended: done

- *Ambrose Gullton, cuts wood at the Greenholt hearths; from Larkfield, light accent*
- *Kell Greenshaw, cuts wood at the Greenholt hearths; from Larkfield, light accent*

> **Ambrose Gullton:** I dreamt I was falling. All night.  
> <sub>`f7_home_dream` alt 3</sub>  
> **Kell Greenshaw:** You fell out of bed. That's why.  
> <sub>`f7_home_dream` alt 3</sub>  
> **Ambrose Gullton:** Maybe that's what it was.  
> <sub>`f7_home_dream` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 18. crime, friends: hall in `rm_the_council_press` (ridgemere), day 46.4, hour 10, clear  
`L2` conversation 1700; ended: done

- *Wynn Fogworth, printing at The Council Press; from Oakhaven, no accent*
- *Runa Ashridge, printing at The Council Press; from Ridgemere, light accent*

> **Wynn Fogworth:** Nobody's saying who robbed Kjell Saltton. Everybody knows not to.  
> <sub>`et_sf_theft_syndicate` alt 2</sub>  
> **Runa Ashridge:** The City Office's ledgers show crime falling. I have the ledgers.  
> <sub>`et_sf_theft_syndicate` alt 0</sub>  
> **Wynn Fogworth:** Very wise.  
> <sub>`et_sf_theft_syndicate` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 19. crime, strangers: work in `r2_the_knucklebones` (ridgemere), day 49.6, hour 14, clear  
`L2` conversation 2004; ended: done

- *Edmund Sootford, none; from Fenshore, broad accent*
- *Signe Dripworth, none; from Fenshore, light accent*

> **Edmund Sootford:** They always come looking in the Sump first.  
> <sub>`h7_crime_sump` alt 3</sub>  
> **Signe Dripworth:** Because it's usually here.  
> <sub>`h7_crime_sump` alt 3</sub>  
> **Edmund Sootford:** Usually isn't always.  
> <sub>`h7_crime_sump` alt 3</sub>  
> **Signe Dripworth:** Tell the watch that.  
> <sub>`h7_crime_sump` alt 3, plain</sub>  

Mark: fine / off / wrong. Note:

## 20. gossip, strangers: work in `rm_the_black_tun` (ridgemere), day 50.9, hour 21, clear  
`L2` conversation 2106; ended: done

- *Agnes Millford, greens stall in the Rag Market; from Oakhaven, light accent*
- *Dagfinn Gullham, tailor at Needle & Drip; from Ridgemere, light accent*

> **Agnes Millford:** The wharf's buzzing. Karin Wetridge tried to steal from Runa Gullley.  
> <sub>`et_rm_gossip_houses` alt 2</sub>  
> **Dagfinn Gullham:** The wharf always buzzes.  
> <sub>`et_rm_gossip_houses` alt 2</sub>  
> **Agnes Millford:** Like flies.  
> <sub>`et_rm_gossip_houses` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 21. crime, friends: work in `r2_the_penny_loaf` (ridgemere), day 52.6, hour 15, clear  
`L2` conversation 2213; ended: done

- *Jorunn Saltdale, helping at The Penny Loaf; from Frosthollow, no accent*
- *Gorm Saltstack, baker at The Penny Loaf; from Ridgemere, light accent*

> **Jorunn Saltdale:** Geese honking at the second hour. That was the thief.  
> <sub>`et_theft_night_noise` alt 1</sub>  
> **Gorm Saltstack:** They always honk.  
> <sub>`et_theft_night_noise` alt 1</sub>  
> **Jorunn Saltdale:** Not like that.  
> <sub>`et_theft_night_noise` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 22. player, family: work in `cn_coldwater_weavers` (coldwater), day 28.6, hour 16, clear  
`L3` conversation 51; ended: done

- *Fell Miremore, loom hand at Coldwater Looms; from Ridgemere, light accent*
- *Marl Cragpeak, loom hand at Coldwater Looms; from Coldwater, light accent*

> **Fell Miremore:** Would that grey stranger spare a penny, do you think?  
> <sub>`h7_st_open_poor` alt 1</sub>  
> **Marl Cragpeak:** I'd like to see what they've seen.  
> <sub>`h7_st_ans_ambitious` alt 0</sub>  
> **Fell Miremore:** The wind gets cold.  
> <sub>`h7_st_close_envy` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 23. prices, spouses: work in `cn_coldwater_weavers` (coldwater), day 28.7, hour 17, clear  
`L3` conversation 61; ended: done

- *Jory Rimepeak, loom hand at Coldwater Looms; from Coldwater, light accent*
- *Varg Acorndell, loom hand at Coldwater Looms; from Coldwater, light accent*

> **Jory Rimepeak:** Still thirty pence the dozen. I said it yesterday and I'll say it again.  
> <sub>`prices_complain_again` alt 0</sub>  
> **Varg Acorndell:** It'll float. We'll all go short together.  
> <sub>`prices_answer_poor` alt 0, coldwater</sub>  

Mark: fine / off / wrong. Note:

## 24. crime, family: work in `cn_coldwater_weavers` (coldwater), day 29.4, hour 10, clear  
`L3` conversation 92; ended: done

- *Jory Rimepeak, loom hand at Coldwater Looms; from Coldwater, light accent*
- *Fell Miremore, loom hand at Coldwater Looms; from Ridgemere, light accent*

> **Jory Rimepeak:** Sage Wheatridge's been telling it at every door.  
> <sub>`h7_theft_rumour_grows` alt 3</sub>  
> **Fell Miremore:** It'll be ten thieves by Restday.  
> <sub>`h7_theft_rumour_grows` alt 3</sub>  
> **Jory Rimepeak:** And a bear by market day.  
> <sub>`h7_theft_rumour_grows` alt 3</sub>  
> **Fell Miremore:** A bear with a crown on.  
> <sub>`h7_theft_rumour_grows` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 25. player, acquaintances: work in `cn_coldwater_weavers` (coldwater), day 36.8, hour 19, clear  
`L3` conversation 573; ended: done

- *Hob Cornwell, child; from Coldwater, light accent*
- *Marl Cragpeak, loom hand at Coldwater Looms; from Coldwater, light accent*

> **Hob Cornwell:** Keep a paw on your purse. That grey stranger is new.  
> <sub>`h7_stranger_thief_eye` alt 0</sub>  
> **Marl Cragpeak:** New's not guilty.  
> <sub>`h7_stranger_thief_eye` alt 0</sub>  
> **Hob Cornwell:** New's not innocent either.  
> <sub>`h7_stranger_thief_eye` alt 0</sub>  
> **Marl Cragpeak:** You'd hang your own shadow.  
> <sub>`h7_stranger_thief_eye` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 26. event, spouses: work in `cn_coldwater_weavers` (coldwater), day 37.6, hour 14, clear  
`L3` conversation 620; ended: done

- *Fell Miremore, loom hand at Coldwater Looms; from Ridgemere, light accent*
- *Marl Cragpeak, loom hand at Coldwater Looms; from Coldwater, light accent*

> **Fell Miremore:** It's all talk of the drovers and their herds at the market today.  
> <sub>`e8t_ev_any_now` alt 1</sub>  
> **Marl Cragpeak:** I saw the notice.  
> <sub>`e8t_ev_ans_news` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 27. player, acquaintances: work in `cn_coldwater_weavers` (coldwater), day 37.7, hour 17, clear  
`L3` conversation 633; ended: done

- *Jory Rimepeak, loom hand at Coldwater Looms; from Coldwater, light accent*
- *Hob Cornwell, child; from Coldwater, light accent*

> **Jory Rimepeak:** What do you think that grey stranger does for a living?  
> <sub>`ev_pl_wondering_job` alt 0</sub>  
> **Hob Cornwell:** Hard to say.  
> <sub>`ev_pl_wondering_job` alt 0</sub>  
> **Jory Rimepeak:** Something with walking.  
> <sub>`ev_pl_wondering_job` alt 0</sub>  

Mark: fine / off / wrong. Note:

## 28. bandits, spouses: work in `cn_coldwater_weavers` (coldwater), day 44.7, hour 16, clear  
`L3` conversation 1061; ended: done

- *Jory Rimepeak, loom hand at Coldwater Looms; from Coldwater, light accent*
- *Varg Acorndell, loom hand at Coldwater Looms; from Coldwater, light accent*

> **Jory Rimepeak:** Where do they get their beer, the The Greenholt, Glasswater lot?  
> <sub>`h7_camp_fed` alt 2</sub>  
> **Varg Acorndell:** Same place we do.  
> <sub>`h7_camp_fed` alt 2</sub>  
> **Jory Rimepeak:** Then they're drinking our beer.  
> <sub>`h7_camp_fed` alt 2</sub>  
> **Varg Acorndell:** And paying better for it.  
> <sub>`h7_camp_fed` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 29. times, family: work in `cn_coldwater_weavers` (coldwater), day 46.7, hour 18, clear  
`L3` conversation 1174; ended: done

- *Jory Rimepeak, loom hand at Coldwater Looms; from Coldwater, light accent*
- *Fell Miremore, loom hand at Coldwater Looms; from Ridgemere, light accent*

> **Jory Rimepeak:** A day like any other.  
> <sub>`` alt -1</sub>  
> **Fell Miremore:** So it is.  
> <sub>`` alt -1</sub>  

Mark: fine / off / wrong. Note:

## 30. player, friends: work in `cn_larkfield_weavers` (larkfield), day 28.8, hour 19, clear  
`L4` conversation 47; ended: done

- *Heather Reedwater, loom hand at Larkfield Looms; from Larkfield, light accent*
- *Aspen Marshholm, linen loom master at Larkfield Looms; from Larkfield, light accent*

> **Heather Reedwater:** Did you see the eyes on that grey stranger? Brightest I've ever seen.  
> <sub>`ev_pl_handsome` alt 1</sub>  
> **Aspen Marshholm:** Eyes like amber.  
> <sub>`ev_pl_handsome` alt 1</sub>  
> **Heather Reedwater:** Amber, exactly.  
> <sub>`ev_pl_handsome` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 31. caravan, family: work in `cn_larkfield_weavers` (larkfield), day 29.6, hour 15, clear  
`L4` conversation 82; ended: done

- *Quill Oakdell, loom hand at Larkfield Looms; from Larkfield, light accent*
- *Vale Miremore, loom hand at Larkfield Looms; from Larkfield, light accent*

> **Quill Oakdell:** My mate signed on as a guard for the trip to Coldwater.  
> <sub>`em_cv_out_missing` alt 2</sub>  
> **Vale Miremore:** Good coin, guarding.  
> <sub>`em_cv_out_missing` alt 2</sub>  
> **Quill Oakdell:** Dangerous coin.  
> <sub>`em_cv_out_missing` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 32. player, friends: work in `cn_larkfield_weavers` (larkfield), day 37.7, hour 16, clear  
`L4` conversation 422; ended: done

- *Tansy Sunmead, loom hand at Larkfield Looms; from Larkfield, light accent*
- *Heather Reedwater, loom hand at Larkfield Looms; from Larkfield, light accent*

> **Tansy Sunmead:** Let's say good day to that grey stranger. Watch them jump.  
> <sub>`h7_stranger_kind_word` alt 3</sub>  
> **Heather Reedwater:** Ha! Let's.  
> <sub>`h7_stranger_kind_word` alt 3</sub>  
> **Tansy Sunmead:** Watch.  
> <sub>`h7_stranger_kind_word` alt 3</sub>  
> **Heather Reedwater:** They jumped!  
> <sub>`h7_stranger_kind_word` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 33. family, family: work in `cn_larkfield_weavers` (larkfield), day 42.3, hour 7, clear  
`L4` conversation 577; ended: done

- *Tansy Sunmead, loom hand at Larkfield Looms; from Larkfield, light accent*
- *Quill Oakdell, loom hand at Larkfield Looms; from Larkfield, light accent*

> **Tansy Sunmead:** Vale Miremore has taken up the whistle. Badly.  
> <sub>`tell_spouse` alt 3</sub>  
> **Quill Oakdell:** And here's me with nobody to burn my bread.  
> <sub>`glad_family_single` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 34. player, friends: tavern in `cn_fenshore_alehouse` (fenshore), day 36.5, hour 13, clear  
`L5` conversation 180; ended: done

- *Nell Plummeadow, keeps the taproom at The Heron; from Fenshore, light accent*
- *Ludo Ashwood, pot hand at The Heron; from Fenshore, light accent*

> **Nell Plummeadow:** Keep an eye on that grey stranger. Shifty.  
> <sub>`ev_pl_suspicious` alt 2</sub>  
> **Ludo Ashwood:** You think everyone's shifty.  
> <sub>`ev_pl_suspicious` alt 2</sub>  
> **Nell Plummeadow:** Usually right, though.  
> <sub>`ev_pl_suspicious` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 35. player, strangers: tavern in `cn_southmeadow_alehouse` (southmeadow), day 32.7, hour 16, clear  
`L6` conversation 114; ended: done

- *Brisk Plumby, pot hand at The Vine and Sickle; from Southmeadow, broad accent*
- *Varg Meadowfield, child; from Southmeadow, light accent*

> **Brisk Plumby:** Travellers. No rent, no master, no debts.  
> <sub>`h7_stranger_poor_envy` alt 2</sub>  
> **Varg Meadowfield:** Debts, I'd bet. Everyone's got debts.  
> <sub>`h7_stranger_poor_envy` alt 2</sub>  
> **Brisk Plumby:** Not ones that knock on your door.  
> <sub>`h7_stranger_poor_envy` alt 2</sub>  
> **Varg Meadowfield:** Ones that wait at the next town, then.  
> <sub>`h7_stranger_poor_envy` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 36. player, apprentice: tavern in `cn_southmeadow_alehouse` (southmeadow), day 33.3, hour 8, clear  
`L6` conversation 123; ended: done

- *Rowan Downridge, keeps the taproom at The Vine and Sickle; from Southmeadow, light accent*
- *Varg Meadowfield, child; from Southmeadow, light accent*

> **Rowan Downridge:** Think that grey stranger has news from the road?  
> <sub>`ev_pl_newcomer_hope` alt 1</sub>  
> **Varg Meadowfield:** Ask them.  
> <sub>`ev_pl_newcomer_hope` alt 1</sub>  
> **Rowan Downridge:** I will, once they've eaten.  
> <sub>`ev_pl_newcomer_hope` alt 1</sub>  

Mark: fine / off / wrong. Note:

## 37. gossip, apprentice: tavern in `cn_southmeadow_alehouse` (southmeadow), day 36.3, hour 8, clear  
`L6` conversation 170; ended: done

- *Varg Meadowfield, child; from Southmeadow, light accent*
- *Rowan Downridge, keeps the taproom at The Vine and Sickle; from Southmeadow, light accent*

> **Varg Meadowfield:** The big pups at the well say Brisk Plumby tried to steal from Iris Meadowfield.  
> <sub>`n7_go_young` alt 1</sub>  
> **Rowan Downridge:** Is that so? Well, well.  
> <sub>`n7_ga2_plain` alt 3</sub>  
> **Varg Meadowfield:** Make of it what you like.  
> <sub>`n7_close_go` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 38. bandits, friends: tavern in `cn_southmeadow_alehouse` (southmeadow), day 44.3, hour 7, clear  
`L6` conversation 337; ended: done

- *Rowan Downridge, keeps the taproom at The Vine and Sickle; from Southmeadow, light accent*
- *Brisk Plumby, pot hand at The Vine and Sickle; from Southmeadow, broad accent*

> **Rowan Downridge:** They say there's a camp at The Sallow Fens, North-east, and the fire's never out.  
> <sub>`h7_camp_smoke` alt 3</sub>  
> **Brisk Plumby:** Then they mean to stay.  
> <sub>`h7_camp_smoke` alt 3</sub>  
> **Rowan Downridge:** That's what frightens me.  
> <sub>`h7_camp_smoke` alt 3</sub>  
> **Brisk Plumby:** It should frighten the Watch.  
> <sub>`h7_camp_smoke` alt 3</sub>  

Mark: fine / off / wrong. Note:

## 39. prices, friends: tavern in `cn_southmeadow_alehouse` (southmeadow), day 47.6, hour 15, clear  
`L6` conversation 410; ended: done

- *Rowan Downridge, keeps the taproom at The Vine and Sickle; from Southmeadow, light accent*
- *Brisk Plumby, pot hand at The Vine and Sickle; from Southmeadow, broad accent*

> **Rowan Downridge:** We grow it ourselves, mostly. Lucky.  
> <sub>`re_prices_country` alt 2</sub>  
> **Brisk Plumby:** That's the advantage of the soil.  
> <sub>`re_prices_country` alt 0</sub>  
> **Rowan Downridge:** Very.  
> <sub>`re_prices_country` alt 2</sub>  

Mark: fine / off / wrong. Note:

## 40. bandits, friends: tavern in `cn_saltwell_inn` (saltwell), day 39.5, hour 13, clear  
`L7` conversation 253; ended: done

- *Moss Downridge, pot hand at The Oasis Inn; from Saltwell, light accent*
- *Oriel Marshditch, keeps the taproom at The Oasis Inn; from Saltwell, light accent*

> **Moss Downridge:** There's smoke at The Sallow Fens, North-east. They're camped there.  
> <sub>`h7_camp_smoke` alt 0</sub>  
> **Oriel Marshditch:** How many?  
> <sub>`h7_camp_smoke` alt 0</sub>  
> **Moss Downridge:** Six. Twelve. Depends who's telling.  
> <sub>`h7_camp_smoke` alt 0</sub>  
> **Oriel Marshditch:** Six is enough.  
> <sub>`h7_camp_smoke` alt 0</sub>  

Mark: fine / off / wrong. Note:

