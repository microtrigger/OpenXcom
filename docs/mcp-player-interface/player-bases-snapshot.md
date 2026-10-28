# Read-only state of all player bases

`read_player_bases` is a separately authorized read-only engine interface,
independent of the selected base, current window, focus, and rendered cells.
It does not replace normal UI actions. Every base mutation must occur in the
corresponding actually open, visible game window through its normal UI handler,
visible on screen and passive recording. A native handler must never be invoked
against a hidden base or closed window; snapshot records grant no such authority. Tactical/enemy state and fog-of-war
remain outside its scope.

The native game-thread collector emits detached, deterministic typed records
for all owned bases and associated facilities, personnel, soldiers, craft,
stock, weapons, vehicles, equipment layouts, research, production and incoming
transfers. In-transit owned soldiers/craft are included. The schema uses positive
field allowlists, not recursive save-game dumps. Enemy retaliation/detection
state and enemy destination graphs are excluded.

Pages use `openxcom-player-bases/6`, a connection epoch, a campaign epoch,
content generation and a frozen snapshot token. A new first page captures
current state; continuation pages belong to that same detached snapshot.
Changed captures replace the snapshot; expired continuation tokens fail
explicitly. Bounds fail explicitly rather than silently omitting records.
No snapshot ID or entity ID grants action authority.

## Record coverage

Every record has `kind`, `id`, and `base_id`. Campaign-wide knowledge, waypoint,
completed-mission and owned-item definition records have an empty `base_id`;
each base record refers to itself. Memorial soldier/history records also have
an empty base reference because deceased personnel no longer occupy a base. Type, rank, status, progress,
category, and item identifiers are engine identifiers, not promises of localized
on-screen text.

| Kind | Collected fields beyond the common envelope |
| --- | --- |
| `campaign` | Funds, campaign date/time, monthly country income and total monthly base maintenance. |
| `base` | Name, longitude/latitude, grid size; total and available soldiers/scientists/engineers, allocated scientists/engineers; capacity/used/free for quarters, stores, laboratories, workshops, hangars, psi labs and containment; craft/personnel/facility/total maintenance; expense-ledger craft rentals and soldier-type/engineer/scientist salaries with type, unit cost, quantity and total; defense and short-/long-range detection capability. |
| `facility` | Type, x/y, size, remaining/total construction days, build/monthly cost; quarters/stores/laboratories/workshops/hangars/psi-lab/containment capacities; radar range/chance, defense, hit ratio, lift/hyperwave/mind-shield/grav-shield flags. |
| `soldier` | Native ID, type, name, rank identifier/value, armor, gender/look, missions/kills, recovery days, psi-training flag and improvement values; craft reference; home/in-transit location and transfer reference; initial/current stats, a non-consuming recent-promotion flag, and nullable death status/time. Stats contain TU, stamina, health, bravery, reactions, firing, throwing, strength, psi strength/skill and melee. |
| `soldier_diary` | Soldier reference; own aggregate missions, service months, wounded days, shots fired/landed, accuracy, kills, stuns, panics and mind controls. Wins and score plus bounded native neutralization and completed-mission breakdowns. These counts describe recorded events, not unique victims; hostile weapon/ammo totals have a different native filter from race/rank totals. |
| `soldier_commendation` | Soldier reference, award type, decoration level and unconsumed new-award flag. Localized earned-award name, noun, description and decoration label; no future award criteria. |
| `mission_history` | Completed campaign archive only: native ID, title/type, date, region/country, nullable shown UFO/race labels, success/rating/score, day/night and recorded loot/award outcome. No active targets or tactical links. |
| `soldier_mission` | Own soldier and completed-mission references, with that soldier’s recorded wounded days; no whole injury map. |
| `soldier_encounter` | Archived Diary mission-row status, unit-name and weapon presentation strings, linked to the own soldier and completed mission. No raw victim ID, coordinates, turn, faction graph or current BattleUnit. |
| `item_definition` | Metadata for own stock, transit and remembered equipment references only: separate store/inventory labels and identification, logistics, physical dimensions/nominal capacities, and nullable research-gated technical article fields. No full rule catalog. |
| `equipment_layout` | Soldier reference, item type, slot, slot x/y, ammo item and fuse timer. This is the saved equipment layout, not tactical inventory. |
| `craft` | Home-base reference, native fuel consumption and nullable return-to-home fuel/distance estimates, own dogfight/battlescape flags and interception order, safe tagged destination; Native ID, type, name, status, home-roster/in-transit location and transfer reference; weapon-slot count/capacity; own longitude/latitude/speed; fuel/max fuel, damage/max damage, maximum speed; soldier/vehicle/item capacities and used/available space; monthly cost, low-fuel and mission-complete flags; crew references. |
| `stock` | Owner reference/kind, item type and quantity; separate records for base and craft stores. |
| `vehicle` | Owner reference/kind, type, ammo and size. Aliased vehicle objects are emitted once under their first collected owner. |
| `weapon` | Craft reference, type, slot index, ammo/max ammo, rearm rate/rearming flag, launcher item and clip item. Empty slots have no weapon record; the craft slot count preserves their extent. |
| `research` | Project type, progress identifier, assigned scientists, spent effort and actual project cost. |
| `production` | Type/category, total/produced amounts, time spent, assigned engineers, unit time/cost, required space, infinite/sell flags, required-item and produced-item quantities. |
| `transfer` | Item/craft/soldier/scientist/engineer type, remaining hours, quantity and nullable item/craft/soldier references. Incoming owned soldiers and craft have their own records and associated equipment. |
| `research_knowledge` | Actually discovered research type and localized name; campaign-owned, no undiscovered technology or dependency/unlock graph. |
| `research_offer` | Per-base currently known ordinary research-list type/name, base cost and item requirement/destruction flags. |
| `manufacture_offer` | Per-base currently known ordinary manufacture-list type/name/category, required space, unit hours/cost and item input/output quantities. |
| `own_waypoint` | Player-owned waypoint native ID, name, longitude and latitude. |

Aggregate values retain native game semantics, including reservations and incoming
transfers where the native capacity/personnel calculation includes them. A craft's
`home_roster` location means membership in that base's roster, not physical presence
at the base. Construction values are native counters, not a separately calculated
completion forecast.

## Scope and identity limits

This is an explicit field allowlist, not an exhaustive serialization of every
owned object's internals. Own diary aggregates, award type/level/new flags and the recent-promotion flag
are read without consuming notifications. Campaign memorial soldiers are
explicitly separate from base rosters: they have an empty `base_id` and
`location=memorial`, since their former base is not retained reliably. Their
death timestamp and KIA/MIA status use only the same cause-presence distinction
as the memorial UI; cause contents are not inspected. Individual kill records,
mission IDs/locations, enemy identities and enemy-specific award nouns are
not collected. Reads must not consume
promotion flags, advance simulation, change allocations/funds, invoke RNG or
serialize tactical objects. Craft destinations are tagged as none, an owned base, a player-owned waypoint,
or other. Only pointer equality against owned registries classifies them; the
collector never dereferences a foreign destination or exports its identity,
coordinates or type. Enemy retaliation/detection state, battle-state graphs and
fog-of-war information remain excluded. Radar capability
numbers describe owned facilities, not actual enemy detection results.

Campaign replacement resets identity bindings even when allocation addresses
are reused. Base and craft bindings additionally track object lifetimes; soldier
identifiers use native soldier IDs. Other record IDs are observational bindings,
not durable universal identities: a non-Target object removed and replaced at the
same allocation address between captures can reuse its apparent ID (the ABA
limitation). An absence observed in a capture retires its old binding. Consumers
must use snapshot content and epochs, not assume an unchanged ID proves object
continuity across an unobserved replacement.

Stable IDs are never setters or UI capabilities. Ordinary UI actions retain their
separate freshness, visibility and overlay requirements. The all-base read does
not authorize a native action against any hidden base or closed window.

## Knowledge and craft semantics

The research offer query and final filtering match the ordinary New Research
list. Manufacture offers use the ordinary available-production query. An offer
means it belongs to that known list, not that funds, space, materials or other
native action conditions will permit execution. The read does not unlock
research, allocate staff, create projects, consume items or reveal future rules.
Discovered helper flags already recorded in campaign knowledge are not a dump
of the undiscovered rule graph.

Craft home calculations use only an owned base. The cached maximum-speed
radian denominator and computed estimate must be finite, positive where needed,
and within integer bounds before the native return-fuel calculation is used;
otherwise estimates remain null. No altitude, meetpoint, UFO target or foreign
target method is called. Own operational flags do not grant tactical visibility.

## Qualification and remaining scope

Earlier read-only, ledger and bounded base-action qualifications remain
historical evidence, including independent reads from other windows and later
two-base live scenarios in both games. They do not by themselves qualify every
new schema field. Native fixtures, wire/adapter seams and live runs are distinct
forms of evidence; none is a claim that the full regression suite passes.

Schema 4 has focused native model, wire and adapter qualification. Short live
checks in both games read two owned bases from Geoscape and an ordinary options
window, with unchanged owned state and stable repeated snapshots. Populated
waypoint, foreign-destination and operational-flag edge cases remain native-fixture
evidence rather than live qualification of those conditions. The broader test
suite is not fully passing. These earlier checks do not qualify schema 5 additions. Item metadata remains
an explicit player-known allowlist rather than exhaustive raw-rule serialization. This is broader owned state, not exhaustive serialization or a relaxation
of visible-action and enemy-information boundaries.

## Detailed owned-history boundary

History is the campaign’s completed record and the same earned-award presentation
used by the soldier Diary. A recorded enemy name or race in that history is not
permission to inspect present tactical enemies. No live BattleGame graph, target
lookup, future award criteria or mutation is used. Missing or invalid historical
references fail explicitly rather than silently binding to another mission.
Schema 5 has focused native model, wire and strict adapter qualification.
Short live checks in both games cover owned item definitions and empty histories
from two bases and another ordinary window, without state changes. Populated
mission/encounter/award and research-gate edge cases have synthetic native
qualification, not populated live acceptance. The full regression suite remains
not fully passing; unchanged baseline diagnostics are not treated as success.

## Owned-item definition boundary

An item reference does not automatically unlock technical knowledge. Remembered
equipment-layout references do not prove the item remains physically in stock. Store labels
are distinct from the inventory’s unidentified-artifact label. Technical fields
require a matching, available item article in the ordinary UFO/TFTD Ufopaedia;
compatible-ammo links additionally require that ammo’s independently available
article and an owned reference. The link list follows the native article’s first
three slots, not an exhaustive mod compatibility graph. TFTD standalone-ammo
pellets remain null even where a firearm ammo panel could show a multiplier. Research requirements,
future unlocks, AI parameters and hidden simulation fields are not serialized.

Physical dimensions, weight and nominal clip/medikit capacities are definition
values, not current tactical rounds or charges. Purchase delivery hours, when
available, describe nominal purchase delivery, not a base-to-base transfer quote.
A purchase price or offer never grants action authority or bypasses native checks.
Technical damage and firing-mode values are article baselines, not the current
soldier’s hit chance or a prediction about an unseen target.

## Psionic visibility (schema 6)

All home, in-transit and memorial soldier records now apply the same native
SoldierInfoState visibility test to both initial and current stats. Psi strength
is numeric only after positive current psi skill, or when the active ruleset's
psi requirements are researched **and** psi-strength evaluation is enabled.
A missing active Mod cannot grant research evaluation. Psi skill is numeric
only with positive current skill. Otherwise the corresponding stat and its
improvement field are `null`, not zero or an inferred value. Ordinary stats
remain mandatory numbers. Owning all bases does not authorize unrevealed stats.
The adapter rejects older sub-schemas and invalid/non-psi nulls. These are
read-only eligibility changes; they grant no action or hidden tactical access.
