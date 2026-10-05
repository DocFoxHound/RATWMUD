# Calendar, moon, seasons, and aging

September 21, 2026 follow-up. Calendar math and the first aging policy are
authoritative simulation rules, not client decoration. The four-hour day and
365-day year are user decisions. The numerical weather, light, and stat curves
below are first-version tuning, not final balance or real-world biology.

## One shared clock

One game day takes 14,400 seconds: four real hours of running simulation.
Morning midpoint is 06:00 and evening midpoint is 18:00. Each direction between
those points takes exactly two real hours in every season. Daylight smoothly
rises from 05:00 to 07:00 and falls from 17:00 to 19:00; the ramps do not add
extra hours to the cycle. Seasonal day-length changes are intentionally absent
because they would contradict the requested equal halves.

The calendar counts fractional absolute game days from a saved epoch. Day zero
is Year 1, Spring 1, at midnight. Human-facing year, day-of-year, and
day-of-season are one-based; internal completed-day indices are zero-based.
The year always has 365 days and never inserts a leap day.

| Season | Days of year | Length | Initial climate character |
| --- | --- | --- | --- |
| Spring | 1–92 | 92 days | More rain; occasional lingering snow |
| Summer | 93–184 | 92 days | Mostly clear; no temperate summer snow |
| Autumn | 185–275 | 91 days | Wetter and foggier |
| Winter | 276–365 | 90 days | Snow much more likely; weak herb recovery |

At continuous uptime, a year takes 1,460 real hours, about 60.83 real days.
This is important for roleplay longevity: a year does not pass every real week.
An 18-year-old would reach 65 after about 7.8 real years at continuous uptime
under this pace. Developer clock jumps are test tools, not normal player powers.

The current clock advances while the authority is running. It does not derive
elapsed days from an operating-system date on load. Whether server downtime
should advance the calendar is an explicit future policy choice. Legacy saves
without an absolute calendar are migrated without retroactive age rewards.

## A natural lunar sequence on game days — confirmed

The first version uses the mean Earth-like synodic cycle of 29.530588 **game
days**, with a new moon at the game epoch. The changing visible fraction is
computed continuously, not by alternating fixed 29-day and 30-day months.
Names follow new moon, waxing crescent, first quarter, waxing gibbous, full moon,
waning gibbous, last quarter, and waning crescent. This period models the phase
cycle, not the shorter orbital period. [NASA's lunar phase catalog](https://eclipse.gsfc.nasa.gov/phase/phasecat.html)

The user confirmed that lunar phases follow game days. This matches the existing
implementation: the moon advances with the authoritative world calendar, not
today's actual lunar phase or a client computer's date. Actual-date synchronization
is not a pending requirement. The phase epoch and lighting curves remain tuning
choices; the game-day clock choice is settled.

```text
phase = fractional remainder of absoluteGameDays / 29.530588
illuminated fraction = (1 − cos(2π × phase)) / 2
night light = .08 + .32 × illuminated fraction × weather transmission
outdoor sky light = night light + (1 − night light) × daylight
```

| Weather | Moon transmission | Full-moon night light |
| --- | --- | --- |
| Clear | 1.00 | 0.400 |
| Rain | 0.35 | 0.192 |
| Fog | 0.20 | 0.144 |
| Snow | 0.45 | 0.224 |

New moon reaches the provisional 0.08 close-awareness floor in every weather.
These are gameplay visibility factors, not measured lux. The existing
rain/fog/snow obstruction multipliers still apply separately, so clouds reduce
moonlight while weather also interferes with seeing through the scene. Moon
altitude, eclipses, exact orbital perturbations, and cloud cover independent of
precipitation are not simulated yet.

Interior lighting continues to respect shelter and the authored whole-cell
lighting profile. A lit tavern remains legible while its warm glow increases
outside daytime; a sealed unlit cellar remains dark. An indoor cell does not
inherit outdoor precipitation merely because the same calendar drives both.

## Seasonal weather with repeatable outcomes

The first climate profile is temperate. Percentages are authored tuning:

| Season | Clear | Rain | Fog | Snow |
| --- | --- | --- | --- | --- |
| Spring | 45% | 35% | 18% | 2% |
| Summer | 70% | 20% | 10% | 0% |
| Autumn | 40% | 35% | 20% | 5% |
| Winter | 30% | 12% | 18% | 40% |

Forecasts use four six-game-hour slots per day. A stable hash of world seed,
cell ID, absolute day, and slot selects weather from the current season's
weights. Repeating a query, changing query order, or restoring the same seed and
epoch cannot reroll the forecast. The pure calendar module provides forecasts;
the world owns their application and explicit developer overrides. Outdoor
cells currently start in seasonal mode; an authored weather value is an initial
condition, not a permanent lock. Selecting a development weather preset pauses
seasonal updates for that cell until Seasonal is selected again. That mode is
saved. Atlas does not yet expose a permanent-weather-mode authoring field.
Changing weights or hash behavior later is a world-generation version change.

This is cell-specific climate, not a moving regional weather-front simulation.
Biome/altitude profiles, regional coherence, weather transition blending,
drought duration, storm strength, and wind-front coupling remain extensions.

## Annual character milestones

Players and NPCs use the same birthday math. Each actor stores age and a saved
last-birthday day. A birthday occurs after another 365 elapsed game days from
that actor's anchor, not simply when the world's year number changes. Annual
rewards are server-owned and idempotent: replaying a checkpoint or asking for a
snapshot again cannot award another stat point.

Birthdays award no statistics (doc 44, 2026-10-04: +1 strength and dexterity a
year to 34, then +1 wisdom, outweighed a character's whole level curve in a
fight). Gains already had are kept. Age alone does not unlock Gifted or Quickened status.

From the 65th birthday, age affects effective capability without destructively
rewriting the base statistic or injury state:

```text
old-age years = max(0, age − 64)
vision factor = max(.45, 1 − .015 × old-age years)
hearing factor = max(.50, 1 − .012 × old-age years)
effective dexterity = base dexterity × max(.55, 1 − .010 × old-age years)
```

Vision and hearing age factors combine with existing skill, injury, weather,
and light rules. Dexterity's effective value influences movement as before.
The floors preserve useful older characters and leave room for wisdom-focused
roles. These numbers are fictional design choices for RATW wolves, not a claim
about real wolves' or humans' lifespans. The current build does not yet implement
death; the confirmed natural-lifespan rule below is a separate pending feature.

Players receive a persisted pending birthday notice. A catch-up of multiple
years should be communicated clearly rather than silently changing the sheet
or flooding roleplay with one line per missed year. NPCs receive equivalent
stat changes without player notification spam. Existing dialogue memory should
reference the authoritative age; an NPC's language-model description never
decides its birthday or stat awards.

The first client delivery uses a reliable RPC and marks the pending notice sent
when queued. It is verified across ordinary reconnects and graceful restarts,
not a durable exactly-once acknowledgment protocol: a disconnect at delivery or
a crash before the next checkpoint can lose or repeat the message. Annual stat
rewards themselves remain replay-safe. Unsolicited notices cannot overwrite a
previous command's retry receipt.

The user confirmed that player characters age while logged out. The existing
implementation advances saved characters during authoritative checkpointing and
reconciles age again at login against the shared game calendar. Catch-up rewards
are idempotent; absence does not pause a character's birthday clock. A pending
notice reports missed birthdays on return without replaying a flood of lines.

Being logged out is distinct from the server being shut down. The current world
calendar pauses during server downtime. No wall-clock downtime catch-up is
implied by the confirmed logged-out aging rule.

## Natural lifespan — confirmed direction, not implemented yet

- A player may choose the timing of their character's natural death before the
  mandatory lifespan deadline. This is distinct from injury/combat death rules.
- Begin gentle natural-death/legacy prompts at an age still to be confirmed.
  Proposed default: age 90, at most once per game birthday, not every login.
  This proposed threshold is not the age-65 stat-decline threshold and is not
  active in the current build.
- Mandatory natural death occurs at age **100 + a random interval from 0 to 20
  game years**: never before 100 through this rule and never later than 120.
  A zero interval permits the 100th birthday itself. The interval may be
  fractional; whole-year versus continuous sampling and its distribution remain
  tuning details, not an instruction to reroll on every birthday.
- Draw the authoritative lifespan deadline once and persist it. Logout, restart,
  reconnect, repeated commands and restore must not reroll or postpone it. A
  recommended implementation is one uniform fractional offset in the inclusive
  permitted range, stored as an absolute game-day deadline.
- The voluntary-death flow must describe the consequence, require explicit
  confirmation and be replay-safe. Death is a persistent lifecycle transition,
  not deletion of the player account, historical character, social contributions
  or NPC memories. Ownership/inheritance and successor-character rules need
  separate design before enabling it.
- Whether a deadline reached while logged out finalizes death immediately or
  offers a final scene on return is still open. A final-scene policy must define
  retrospective storytelling versus bounded grace explicitly; it cannot become
  an unlimited lifespan extension. Aging while absent is already confirmed.
- NPCs continue to age by the same calendar. How their natural-death scheduling,
  replacements and economic succession should work requires an explicit NPC
  lifecycle adapter; NPCs do not have a player-confirmation prompt.

Prompts should live in a character/legacy notice surface, not interrupt a pose,
force a modal during combat or flood the narrative feed. Natural-death prompts
can be deferred until a safe moment; that must not silently change the final
lifespan deadline. If several years pass while absent, coalesce the reminder.
Whether the exact sampled date is shown to the player is still a design choice.

At the established four-hour day, the 0–20-year window spans up to about 1,217
real days of continuous server uptime. Starting at age 18, age 100 is about 13.7
real years away. These conversions describe the existing clock, not a proposed
change to lifespan or calendar speed.

Implementation gates: explicit alive/dead state and command guards; strict save
schema/legacy migration; one persisted deadline; voluntary confirmation;
coalesced prompts; offline-deadline policy; safe NPC/party/economy cleanup; and
boundary/restart/retry tests. Do not enable forced death against existing saves
by merely adding an age check to `advanceAge`.

## Verification and next decisions

`Tests/calendar_tests.cpp` checks exact cycle lengths, every seasonal boundary,
leapless year rollover, continuous twilight, fractional lunar recurrence,
weather attenuation, malformed numeric inputs, deterministic forecast replay,
and distribution agreement across 40,000 forecast samples. Aging and world
integration have separate tests; consult the current test report for executed
native and packaged verification rather than treating this design as evidence.

Before final balance: confirm calendar downtime behavior, prompt starting age,
offline-deadline handling, lifespan sampling/disclosure, birthday reward bands,
late-life floors, legacy/inheritance, named months/festivals, and additional
climates. The lunar game-day clock and logged-out aging are no longer open.
