# Coverage holes at Phase 7 (`talk_check --coverage --holes 3`, Docs/Design/84 §15.4)

Move lines (no concept, found by the move they make) that answer each owed move, for every topic x replier kind (Data/Talk/profiles.json) x band; the replier did not know the news (knows=no). Phase 8 fills the holes (its gate: none in the core moves).

```
coverage (doc 84 §15.4): 55 moves a line can owe x 36 topics x 150 replier kinds (716 residents) x 4 bands = 1188000 cells; 663264 holes (55.8303%), 64800 in the core moves
core greet.return: 1 move lines, 0 of 21600 cells a hole
core react.news: 4 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
core react.complaint: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
core answer.detail: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
core accept_preclose: 2 move lines, 0 of 21600 cells a hole
core farewell.return: 6 move lines, 0 of 21600 cells a hole
core accept_decline: 3 move lines, 0 of 21600 cells a hole
     accept: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     accept_apology: 3 move lines, 0 of 21600 cells a hole
     accept_invite: 1 move lines, 0 of 21600 cells a hole
     ack: 3 move lines, 0 of 21600 cells a hole
     agree: 5 move lines, 0 of 21600 cells a hole
     answer.after: 7 move lines, 0 of 21600 cells a hole
     answer.check_in: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     answer.how: 3 move lines, 9936 of 21600 cells a hole; worst topics work 276 weather 276 travel 276; by band acquaintances 2484 family 2484 friends 2484 strangers 2484
        gossip / k006 adult merchant poor plain / strangers
        gossip / k006 adult merchant poor plain / acquaintances
        gossip / k006 adult merchant poor plain / friends
     answer.plan: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     answer.whereabouts: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     apologise: 3 move lines, 0 of 21600 cells a hole
     ask.detail: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     assess: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     clarify: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     concede: 4 move lines, 0 of 21600 cells a hole
     congratulate: 1 move lines, 0 of 21600 cells a hole
     counter: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     decline: 3 move lines, 0 of 21600 cells a hole
     decline_apology: 1 move lines, 5328 of 21600 cells a hole; worst topics work 148 weather 148 travel 148; by band acquaintances 1332 family 1332 friends 1332 strangers 1332
        gossip / k018 adult merchant comfortable larkfield / strangers
        gossip / k018 adult merchant comfortable larkfield / acquaintances
        gossip / k018 adult merchant comfortable larkfield / friends
     deflect_praise: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     deflect_thanks: 2 move lines, 0 of 21600 cells a hole
     disagree: 5 move lines, 0 of 21600 cells a hole
     extend: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     farewell: 7 move lines, 0 of 21600 cells a hole
     give.advice: 1 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     give.opinion: 14 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     go_ahead: 3 move lines, 0 of 21600 cells a hole
     grant: 1 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     insist: 3 move lines, 0 of 21600 cells a hole
     introduce.return: 2 move lines, 0 of 21600 cells a hole
     keep_confidence: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     laugh: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     offer: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     praise: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     put_off: 2 move lines, 0 of 21600 cells a hole
     react.news.doubt: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     react.self: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     react.self.glad: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     react.story: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     reassure: 4 move lines, 0 of 21600 cells a hole
     refuse: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     sympathise: 3 move lines, 0 of 21600 cells a hole
     take_offence: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     tease: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     tease.laugh: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     tease.return: 0 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     thank: 1 move lines, 21600 of 21600 cells a hole; worst topics work 600 weather 600 travel 600; by band acquaintances 5400 family 5400 friends 5400 strangers 5400
        gossip / k001 adult merchant comfortable plain / strangers
        gossip / k001 adult merchant comfortable plain / acquaintances
        gossip / k001 adult merchant comfortable plain / friends
     welcome_join: 2 move lines, 0 of 21600 cells a hole
```
