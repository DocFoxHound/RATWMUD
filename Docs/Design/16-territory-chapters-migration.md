# Territory, Chapters, population and political consequences

## The intended experience

A Chapter's town should change the surrounding region. Good jobs, food, safety
and sustained roleplay attract wolves; those wolves have homes, obligations,
skills and existing loyalties. They are not an unlimited spawn resource. A
successful town can enrich its neighbors through trade while angering a ruler
whose farmers and skilled workers are leaving. Losing population is a political
consequence, not an automatic declaration of war.

## Keep three concepts separate

- **Faction claim:** a faction says this land belongs within its jurisdiction.
  Several claims may overlap. A claim does not prove effective control.
- **Chapter settlement:** a Chapter occupies/builds a site. It can exist inside
  claimed territory, with permission, disputed rights or tolerated autonomy.
- **Standing:** a directed faction opinion of a Chapter or individual. Building
  rights, treaties, tax obligations and diplomatic agreements are distinct from
  a single friendship score.

The first Atlas extension authors faction/Chapter catalogs and per-cell region,
claims and Chapter site metadata. Cuts/splits inherit metadata; incompatible
merges/recuts are blocked. This political overlay never changes player fog,
terrain, elevation, cell traversal or the separate local map. Claims are cell
granular in this version, not freeform sub-cell polygons.

Storykeeper can administratively bootstrap Chapter records, members and sites
over otherwise unassigned cells. These are clearly labeled operator overlays,
not player construction or an implicit rewrite of the Atlas source. A conflicting
authored Chapter assignment takes precedence. Housing/jobs entered by the DM
are declared planning capacities, not measured buildings or actual vacancies.

## Migration model

The eventual autonomous pipeline should be:

```text
regional candidates → compare viable destinations → reserve housing/job
                    → travel physically → settle → account for source losses
```

Economic attraction should use actual food access, affordable housing, funded
wages, filled/unfilled work, demand and security. Player activity should use
bounded evidence of active presence and completed social participation, not
unlimited chat length or idle connections. Neither ingredient alone should
create residents. Supply chains must support the new population before it grows.

Candidates need distinct preferences and costs: family ties, faction loyalty,
profession, personal safety, moving distance and willingness. Existing crucial
workers should not all abandon a region at once. Capacity reservations, migration
budgets, cooldowns and a meaningful improvement threshold prevent oscillation
between two towns, duplicate arrivals and opinion farming. Future temporary
visits, job-seeking trips and permanent relocation should be separate states.

### Implemented first adapter

Storykeeper generates deterministic, expiring previews using existing resident
NPCs, the shared region, declared capacity/attraction, observed Chapter activity,
and authoritative purse/food data. An operator approves a candidate. Preview
approval rechecks eligibility and available capacity. There is no autonomous
population-growth loop yet; preview scoring is an initial policy, not proven
economic modeling or player-facing settlement gameplay.

The native authority accepts only existing, non-recruited resident-role NPCs.
The cook, keeper and forager remain protected. The NPC uses ordinary navigation,
doors, posture changes and collision. Its home changes only after arriving
within 0.35 tile of the requested point; accepting the command is not arrival.
The new home governs rest/free time. Existing demonstration jobs and food
procurement remain commutes to their old workplaces/market. General NPC/job
authoring, new businesses, Chapter construction and local provisioning are the
next prerequisites for a genuinely self-sufficient new town.

Atlas-exported custom worlds currently contain no authored NPCs. The existing
three-cell/six-resident demonstration is labeled `demo_reach` to exercise the
adapter without inventing canonical faction claims. Migration in regions marked
`unassigned` is not inferred. No source claim means no invented faction penalty.

## Resentment and diplomacy

The recommended long-term loss calculation is based on the fraction of the
source population leaving, scarcity of the worker's profession, recent losses,
and treaty terms. Small ordinary movement should not count the same as losing
the last miller. Neighboring factions without a source claim should not all
receive a penalty merely because they are nearby.

The first service applies a bounded loss to source-claim factions only after a
fresh native snapshot confirms arrival. Failed/expired commands do not create
resentment. A Chapter treaty modifier can reduce the provisional penalty.
Relations and their reasons are persisted in the separate political service;
native combat, NPC hostility, taxes and access checks do not yet consume them.
The DM can set Chapter/player standing manually, with a reason and audit entry.

Later, expose explanations such as “lost three households this season” alongside
trade benefits, treaties and manual adjustments. Separate those contributions
instead of treating a single overwritten integer as the complete diplomacy
history. Retain dissolved factions and displaced residents as historical records.

## Decisions to revisit

Confirm overlapping claims/sites, loss weighting, treaty rules, regional travel
limits, how declared capacity becomes actual funded housing/jobs, voluntary
return migration, and the social policy for events that target absent players.
Autonomous migration should be enabled only after a multi-season economy probe
with real Chapter provisioning and safeguards against depopulating essential
services. A successful two-good tavern loop is not sufficient evidence.
