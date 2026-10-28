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

Pages use `openxcom-player-bases/3`, a connection epoch, a campaign epoch,
content generation and a frozen snapshot token. A new first page captures
current state; continuation pages belong to that same detached snapshot.
Changed captures replace the snapshot; expired continuation tokens fail
explicitly. Bounds fail explicitly rather than silently omitting records.
No snapshot ID or entity ID grants action authority.

## Record coverage

Every record has `kind`, `id`, and `base_id`. The campaign record has an empty
`base_id`; each base record refers to itself. Type, rank, status, progress,
category, and item identifiers are engine identifiers, not promises of localized
on-screen text.

| Kind | Collected fields beyond the common envelope |
| --- | --- |
| `campaign` | Funds, campaign date/time, monthly country income and total monthly base maintenance. |
| `base` | Name, longitude/latitude, grid size; total and available soldiers/scientists/engineers, allocated scientists/engineers; capacity/used/free for quarters, stores, laboratories, workshops, hangars, psi labs and containment; craft/personnel/facility/total maintenance; expense-ledger craft rentals and soldier-type/engineer/scientist salaries with type, unit cost, quantity and total; defense and short-/long-range detection capability. |
| `facility` | Type, x/y, size, remaining/total construction days, build/monthly cost; quarters/stores/laboratories/workshops/hangars/psi-lab/containment capacities; radar range/chance, defense, hit ratio, lift/hyperwave/mind-shield/grav-shield flags. |
| `soldier` | Native ID, type, name, rank identifier/value, armor, gender/look, missions/kills, recovery days, psi-training flag and improvement values; craft reference; home/in-transit location and transfer reference; initial/current stats, a non-consuming recent-promotion flag, and nullable death status/time. Stats contain TU, stamina, health, bravery, reactions, firing, throwing, strength, psi strength/skill and melee. |
| `soldier_diary` | Soldier reference; own aggregate missions, service months, wounded days, shots fired/landed, accuracy, kills, stuns, panics and mind controls. No individual kill or mission records. |
| `soldier_commendation` | Soldier reference, award type, decoration level and unconsumed new-award flag. No enemy-specific award noun. |
| `equipment_layout` | Soldier reference, item type, slot, slot x/y, ammo item and fuse timer. This is the saved equipment layout, not tactical inventory. |
| `craft` | Native ID, type, name, status, home-roster/in-transit location and transfer reference; weapon-slot count/capacity; own longitude/latitude/speed; fuel/max fuel, damage/max damage, maximum speed; soldier/vehicle/item capacities and used/available space; monthly cost, low-fuel and mission-complete flags; crew references. |
| `stock` | Owner reference/kind, item type and quantity; separate records for base and craft stores. |
| `vehicle` | Owner reference/kind, type, ammo and size. Aliased vehicle objects are emitted once under their first collected owner. |
| `weapon` | Craft reference, type, slot index, ammo/max ammo, rearm rate/rearming flag, launcher item and clip item. Empty slots have no weapon record; the craft slot count preserves their extent. |
| `research` | Project type, progress identifier, assigned scientists, spent effort and actual project cost. |
| `production` | Type/category, total/produced amounts, time spent, assigned engineers, unit time/cost, required space, infinite/sell flags, required-item and produced-item quantities. |
| `transfer` | Item/craft/soldier/scientist/engineer type, remaining hours, quantity and nullable item/craft/soldier references. Incoming owned soldiers and craft have their own records and associated equipment. |

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
serialize tactical objects. Craft destinations, enemy retaliation/detection state,
battle-state graphs and fog-of-war information are excluded. Radar capability
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

## Qualification status

The native collector test suite passes 71 checks. Its real-object fixture covers
22 records across all 12 record kinds, two bases and all five transfer types.
Controller routing/pagination tests are a separate seam and must not be treated
as substitute evidence for that native collector coverage.

UFO and TFTD live qualification each captured 52 records over four pages from the Options
window, without navigating individual bases. Repeated unchanged paused reads
retained the snapshot token and generation. Selling one pistol clip in UFO and one dart clip in TFTD through the
actually visible Sell window changed its stock from 8 to 7 and increased funds
by 52; the subsequent snapshot advanced its generation from 2 to 3 while retaining
all entity IDs. The previous continuation token was rejected as expired.

These live runs each used one newly created test base; multi-base coverage is
provided by the separate native fixture, not claimed from those live runs. No
gameplay visual fallback was used. Both also verified a read before any UI read
at the main menu returns `no_campaign`, and a draft sale does not mutate stock
or generation. Facility-placement preview work has
78 native checks and a successful build, but is not live-qualified; it is separate
from the read-only snapshot evidence above. These results do not claim exhaustive
base-state coverage or acceptance of every UI mutation.

Regression comparison against `baseline-f7566924` is scoped, not a full-suite
pass. The common 191-test sample has the same two failures and one error in
baseline and current code:

- `test_agent_control.ListValidationTests.test_closed_list_action_matrix_kinds_and_sources`
  fails the Geoscape action-catalog expectation in both.
- `test_agent_events.EventClientTests.test_history_retained_independently_and_exact_public_ring_gap`
  errors when event reading is unavailable and also fails during worker teardown
  in both.

The 14 new focused tests pass, producing the selected 205-test current sample;
that sample still has the two failures and one error above. Separately, the
18-test standalone gameplay sample has the same two errors in baseline and
current code:

- `GameplayClientTests.test_hidden_hand_and_event_only_handles_cannot_broaden_action_grant`
- `GameplayValidationTests.test_action_catalog_requires_the_corresponding_visible_set`

These comparisons establish matching outcomes for the named samples, not that
all tests pass or that every other regression has been excluded.

Schema 2 adds the owned-soldier history slice. Its native fixture passes 156
checks with a frozen unchanged input closure, including native death transition,
MIA/KIA, stable memorial identities, unconsumed promotion/award flags and
exclusion canaries. The strict adapter accepts all 28 fixture records and all 27
post-transition records; nine focused adapter tests pass normally and optimized.
The production build passes.

Short UFO and TFTD live runs each read 60 records, including eight soldiers and
eight diary aggregates. Snapshot content and generation stayed unchanged across
Basescape, facility selection, General Stores preview and Cancel/Close. New
campaigns have no medals or memorial entries: those populated-history cases
are native-fixture qualification, not live death/award acceptance. Gameplay used
MCP with passive video. One read-only visual diagnostic in TFTD identified
missing visibly displayed facility rows; no visual gameplay action was used.
That list-projection gap was subsequently repaired and qualified separately; see [ordinary coverage](ordinary-coverage-status.md).


Schema 3 adds the owned expense-ledger slice described in
[Monthly Costs](monthly-costs-contract.md). It preserves the native researched,
nonzero-cost rule selection, including eligible zero-quantity rows and counts
that include applicable incoming craft/personnel. Campaign monthly income and
maintenance are explicit, not reconstructed from visible text. Locked rules
remain excluded. This independent read never authorizes a hidden UI action.
The focused native ledger fixture passes 206 checks; prior schema-2 history
results above remain historical qualification, not current schema literals.
