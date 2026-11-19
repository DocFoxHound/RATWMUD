# Cleanup before the city batches (city-a, 2026-11-18)

Lead: Claude Opus 5.5 (the r3-mantles-city-a session). Branch: r3-mantles-city-a, on ridgemere2 3c039b11 (304 mantles).

## The older mantles: doubled determiners and trade words (39 mantles reloaded)
- `Saved/Mantles/dets.py` counted 133 over the 304; 29 of them were possessive values after the/a/that ("the
  #thunder#" = "the storm's low rumble", "a #batch#" = "a day's run"), which read right: the checker no longer counts
  them. The other 104, in 36 mantles (cw103 alone 50: "the #board#" = "the my draughts board"), fixed symbol by symbol
  (`Saved/Mantles/cleanup/detplan.py`): the determiner before the symbol dropped where every value carries its own
  (OUTER), or the values' own dropped where every use has one (cw103 names/warp/berries, cw158 shop, lf102 flies,
  lf137 family); five "a #saying#" / "that #saying#" lines meant the word, not the habit (cw119, cw143, lf133 x2,
  rm106, which also asked in its opener), and lf110's "The #work#" the word. One left by choice: cw123 "your
  #voice#" ("your voice" / "your teller's voice").
- `#self.trade_words#` after "I'm", "I'm a", "I'm the", "I keep a" (12 window lines: lf101, lf102, mb110-mb114,
  oh101, rm101, sw106, sw107, sw108) made a fragment of its own ("#self.trade_words.capitalize#. ..."); sw106/sw107's
  second alternatives re-worded so no two start alike; the personas of mb101-mb103 ("#self.first# is
  #self.trade_words#") read "#self.first# (#self.trade_words#):".
- Reloaded with each mantle's own writer kept (`--written-by`): claude-p11-pilot 22, claude-codex-cw-2 9,
  claude-codex-cw-1 4, claude-codex-lf-1 4, batch `cleanup-city-a`; backups
  `Saved/Backups/mantles-ridgemere3-backup-20261118-193603.jsonl` (all) and `...-cleanup-city-a-*.jsonl`; exported;
  `mantle_export --check`: the rows and the files agree (but the city drafts not yet loaded).
- G1 over the 304: 0 refused; G2 `--strict`: 0 refusals, 1 warning (cw197's, as before); `dialect_check --world
  ridgemere3`: 0 failures; `talk_lint`: 0 new.

## The shared library
- `Data/Talk/rules/shared8_callbacks.talk:222` (`s8cb_kin_spouse`): "There's my #dear#." said "There's my my pearl."
  in every dialect whose `#dear#` begins with "my" (six of eight): now "Hello again, #dear#." (validated; talk_lint 0
  new; talk_migrate --check clean).

## The allow-lists (G2's capitals)
- `tools/dialect_allow.json` places: "Rainwood" (Oakhaven's forest) and "Market Hall" (the Commons' hall, doc 67).
- `tools/mantle_rules.json` allow: the weekdays' plurals ("Restdays", "Marketdays"...).
- "Chapel of the Tide" was on the list but G2 matched only one- or two-word places: `tools/mantle_check.py` now reads a
  place of three words or more as a phrase. "Market Square" and "Rain Hall" were already allowed and pass G2 in every
  form tried (mid-sentence, possessive, before a comma); the refusals the town batches met could not be reproduced.

## G1 and `self.house` on a graft choice: not changed, and why
G1 reads a graft choice's `[criteria]` with the card's narrow reader (the `criteria` lambda in
`Library::compileMantle`, Core/RatwMantle.cpp, also used for `fits` and `prefers`): `key=value`, or a bare sex,
stage, role, job, band, time or weather word. A when-line (and so a `dlg`) also takes a dotted fact alone as a
presence test (`self.house`, `!world.news`: the `test` lambda in Core/RatwTalk.cpp). So it is a checker gap, but
G1 is not the only reader: the window code Phase 13 is building on `r3-talk-p13` (`Library::criteriaOf`, uncommitted
there) reads a choice's criteria the same narrow way and **drops** what it can't read. Teaching G1 alone would pass
a `[self.house]` choice that the window then shows to everyone. The fix belongs in one helper used by both (the
when-line's presence test), on P13's branch; until then writers gate House choices on `self.class=upper`.
