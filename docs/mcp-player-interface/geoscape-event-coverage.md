# Geoscape public-event and modal coverage (UFO / TFTD)

Source inventory dated 2026-10-21, based on `mcp-player-interface` baseline
`7dac39f3dad63433bae0b229be0ea2c6a597cde3`, rechecked against
`26c2b8cc4d0d9e701e78a7f4e88a5ddff9a8c2ec`. The inventory remains a **source audit**;
the bounded runtime ledger below separately links completed qualification slices.
Readability policy is owned by its checkpoint/contract, not redefined here.

## Status vocabulary and boundary

- **checked**: source constructor/transition and provider presence inspected; never means a test passed.
- **pending**: runtime trigger, rendered evidence, action and return-to-Geoscape proof still required for both games.
- **not applicable**: the feature is disabled/not defined by the chosen game's rules/options, with the reason recorded per run. It is not a substitute for a missing provider.

Every row below is source-checked; **pending** means some or all listed branches
still lack live acceptance, not that no bounded route has ever run. The runtime
ledger records the exceptions. A listed test is an existing
source reference only: it may be synthetic, native-with-stubs, or depend on historical
fixtures. No new native suite was executed for this inventory refresh. Missing direct test references do
not prove that no indirect test exists. An `agentUiState` override is only a candidate
provider: its paint/clip/overlay and current-state guards may correctly make it unavailable.
No hidden simulation values, unavailable calendar fields, or occluded text may be exported.

Scope is exhaustive for the 34 `src/Geoscape/*State.cpp` classes and the explicitly
listed immediate cross-directory boundaries, not all of Basescape/Battlescape or
arbitrary mod-created event types. Non-State `Globe` is included through Geoscape's
visible globe/navigation boundary. All 34 states are shared source for UFO/TFTD;
actual availability depends on game rules, craft capability, options and campaign state.

## Timer and public event families

`GeoscapeState::timeAdvance` advances five-second ticks; trigger switch deliberately
falls through month → day → hour → 30 minutes → 10 minutes → 5 seconds.
`popup` queues and pauses time; dogfight/zoom transitions have their own pause handling.
Test queues and the complete return stack, not only the first modal.

| Trigger / family | Public event and continuation | Coverage / required variations |
|---|---|---|
| 5 seconds: contact movement | `UfoLostState`, `MissionDetectedState` | Lost airborne/landed contact; mission conversion/arrival. Pending each visible route. |
| 5 seconds: base attack | `BaseDefenseState`; `handleBaseDefense` → `BriefingState` or `BaseDestroyedState` | Defensive weapons / repulsion / invasion / base destruction; pending. |
| 5 seconds: interception | `DogfightState`, `DogfightErrorState` | Start, multi-interceptor window, minimize/restore, disengage, abort, UFO escape/crash/destruction, craft loss; pending. |
| TFTD interception constraints | unable-to-engage depth / airborne branches in `GeoscapeState` and `DogfightState` | Record capability and target conditions from public UI. Mark individual rule-inapplicable branches N/A only with reason; never silently skip TFTD. |
| 5 seconds: destination / patrol | `GeoscapeCraftState`, `CraftPatrolState`, `ConfirmLandingState` | Waypoint arrival, UFO crash/landing, mission site and alien base; accept/cancel and return paths pending. |
| 10 minutes: fuel | `LowFuelState` | Forced return notification and continuation pending. |
| 30 minutes: maintenance/contact | `CraftErrorState`, `UfoDetectedState`, `UfoLostState` | Refuelling shortage, radar/hyperwave contact, contact lost; provider contact dismissal does not imply interception supported. |
| Hour: maintenance | `CraftErrorState` | Rearming shortage distinct from refuelling; pending. |
| Hour: arrivals | `ItemsArrivingState` → selected `BasescapeState` or dismiss | Items/personnel/craft, multiple bases, nullable/clipped rows, no arrivals, repeated queue; pending live route. |
| Hour: manufacturing | `ProductionCompleteState` → `ManufactureState` | Three-unit completion and public stores +3 passed in both games (ledger below); insufficient funds/materials and construction remain separate pending branches. |
| Hour: storage enforcement | `ErrorMessageState` then `SellState` | `storageLimitsEnforced && storesOverfull`; errors alone do not provide resolution. Storage-limit option off is N/A for this trigger, not a pass. |
| Hour: mission alert | `MissionDetectedState` | Undetected mission site becomes public; intercept/centre/cancel UI routes pending, typed provider may expose only subset. |
| Day: facility construction | `ProductionCompleteState(PROGRESS_CONSTRUCTION)` → `BasescapeState` | Distinct destination from production; pending. |
| Day: research | `ResearchCompleteState`, `ResearchRequiredState`, `NewPossibleResearchState`, `NewPossibleManufactureState`; optional `CutsceneState` | Laser/Gauss starter→pistol completion/unlock/article route passed in both games; bonus research, required alien/item notices and optional cutscenes remain pending. |
| Day: persistence | autosave / ironman `SaveGameState` | Save/error modal and recovery; no silent success claim. |
| Month: report | `MonthlyReportState` | Funding, rating, warning/defeat; may lead to commendations, psi training, save, lose-game cutscene. No dedicated provider found; pending. |
| Month: alien base discovery | `AlienBaseState` | Discovered base alert and return; pending. |
| Mission outcome: debrief | `DebriefingState` and continuation stack below | Success/failure/abort, recovery, casualties, promotions, missing equipment, storage/containment; pending even when debrief provider exists. |
| Campaign end | `CutsceneState`, save / main-menu boundaries | Win/lose and terminal transitions; pending. |
| Player-driven navigation | funding/graphs, intercept, target chooser, craft/destination, build/name base, psi allocation, pause/save/load | Not timer events, but must be covered to continue a public event. See exhaustive state rows. |

Simulation-only updates (mission countdowns, damage calculations, alien plans, repair
progress without a public modal) are **not applicable as independently exposed MCP
events**. Only their actually rendered public consequences qualify for this inventory.

## Bounded runtime ledger (not all-event acceptance)

See [production qualification](production-qualification.md) and
[readability/allocation qualification](readability-qualification.md) for the
accepted scopes and test-layer distinctions. Private run evidence was re-read
for this refresh; no private assets, logs or screenshots are published here.

| Slice | UFO | TFTD | Remaining boundary |
|---|---|---|---|
| Fresh campaign setup / scientist allocation | live PASS | live PASS | Ten available staff; not workshop/lab-space-bound or every input method. |
| Starter and pistol research completion / unlocks / article return | live PASS | live PASS | Specific titles only; no all-research, bonus, alien, or cutscene claim. |
| Ordinary contact dismissal during research wait | live observed | live observed | Does not qualify Intercept, Center, radar-detail variants, loss or combat. |
| Mission alert dismissal in accepted production prerequisite route | live observed | pending in that route | UFO observation alone does not qualify TFTD or all mission-site variants. |
| Three-unit production completion → manufacture → stores +3 → Pause | live PASS | live PASS | Funds/material shortage, construction, arrival, hangar and full warehouse branches remain pending. |
| Queued completion replacing incidental/speed view during fresh-read check | runner regression PASS; route remains bounded | failed attempt preserved, corrected retry live PASS | Not a general guarantee of all event queue interleavings. Unknown command outcome still stops. |
| Interception, mission-debrief loot overflow, monthly/defeat stacks | pending | pending | Separate implementations/fixtures are not an accepted live route. |

The two accepted production runs reported unchanged source/runner/binary inputs;
binary SHA-256 was `7c712379c6f75dea9307c37844ec97c96e7d08eaaca28a967ee947edeeb40508`.
Their scope does not make all rows in the inventory PASS. Earlier silent videos
also do not establish real-game audio recording; audiovisual qualification must
be reported separately from semantic route proof.

Native tests referenced below remain a distinct layer: arithmetic domain seams,
widget/provider seams, protocol/socket fixtures and actual renderer tests cannot
be silently promoted to campaign outcomes.

## Exhaustive Geoscape state inventory

Provider column gives the exact wire screen when present. Tests are direct textual
references to the class in the two current test roots (not inferred PASS results).
All source paths are relative to repository root.

| State/source | Provider | Existing direct test source references | Status |
|---|---|---|---|
| `src/Geoscape/AlienBaseState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/AllocatePsiTrainingState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/BaseDefenseState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/BaseDestroyedState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/BaseNameState.cpp` | `base_name` | `tests/agent-control/test_strategy_widget_architecture.py` | checked / pending |
| `src/Geoscape/BuildNewBaseState.cpp` | `build_base` (first-base route only) | `tests/agent-control/test_projects_fixture_architecture.py`; `tests/agent-control/test_strategy_globe_architecture.py` | checked / pending |
| `src/Geoscape/ConfirmCydoniaState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/ConfirmDestinationState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/ConfirmLandingState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/ConfirmNewBaseState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/CraftErrorState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/CraftPatrolState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/DogfightErrorState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/DogfightState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/FundingState.cpp` | `funding` | `tests/agent-control/test_list_architecture.py`; `tests/agent-control/AgentGeoscapeControlsTest.cpp` | checked / pending |
| `src/Geoscape/GeoscapeCraftState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/GeoscapeState.cpp` | `geoscape` | `tests/agent-control/AgentCompletionProvidersTestV19.cpp`; `tests/agent-control/AgentArrivalsProviderTestV20.cpp`; `tests/agent-control/test_strategy_widget_architecture.py`; `tests/agent-control/test_list_architecture.py`; `tests/agent-control/AgentResearchPopupTestV22.cpp`; `tests/agent-control/test_projects_fixture_architecture.py`; `tests/agent-control/test_strategy_globe_architecture.py`; `tests/agent-control/AgentGeoscapeControlsTest.cpp`; `tests/agent-control/run-research-article-tests.py`; `tests/agent-control/run-calendar-layout-tests.py` | checked / pending |
| `src/Geoscape/GraphsState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/InterceptState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/ItemsArrivingState.cpp` | `items_arriving` | `tests/agent-control/AgentCompletionProvidersTestV19.cpp`; `tests/agent-control/AgentArrivalsProviderTestV20.cpp`; `tests/agent-control/test_completions_architecture_v19.py`; `tests/agent-control/test_arrivals_architecture_v20.py` | checked / pending |
| `src/Geoscape/LowFuelState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/MissionDetectedState.cpp` | `mission_alert` | `tests/agent-control/AgentResearchPopupTestV22.cpp` | checked / pending |
| `src/Geoscape/MonthlyReportState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/MultipleTargetsState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/NewPossibleManufactureState.cpp` | `new_possible_manufacture` | `tests/agent-control/AgentResearchPopupTestV22.cpp` | checked / pending |
| `src/Geoscape/NewPossibleResearchState.cpp` | `new_possible_research` | `tests/agent-control/AgentResearchPopupTestV22.cpp` | checked / pending |
| `src/Geoscape/ProductionCompleteState.cpp` | `production_complete` | `tests/agent-control/AgentCompletionProvidersTestV19.cpp`; `tests/agent-control/test_completions_architecture_v19.py` | checked / pending |
| `src/Geoscape/PsiTrainingState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/ResearchCompleteState.cpp` | `research_complete` | `tests/agent-control/AgentResearchPopupTestV22.cpp`; `tests/agent-control/run-research-article-tests.py` | checked / pending |
| `src/Geoscape/ResearchRequiredState.cpp` | `research_required` | `tests/agent-control/AgentResearchPopupTestV22.cpp` | checked / pending |
| `src/Geoscape/SelectDestinationState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/TargetInfoState.cpp` | No dedicated override | No direct class reference found | checked / pending |
| `src/Geoscape/UfoDetectedState.cpp` | `ufo_contact` | `tests/agent-control/AgentCompletionProvidersTestV19.cpp`; `tests/agent-control/test_completions_architecture_v19.py` | checked / pending |
| `src/Geoscape/UfoLostState.cpp` | No dedicated override | No direct class reference found | checked / pending |

## Cross-directory outcome / resolution boundary

Debrief's close callback stacks commendations (including late awards), promotions,
`CannotReequipState`, containment and storage resolution, then save as applicable.
Stack push order is not display order: validate actual top-state sequence. The
no-containment-facility alien-death error is another distinct branch. `SellState`
is not `StoresState`: an inventory display provider does not resolve overfull stores.

| Source | Provider | Status |
|---|---|---|
| `src/Battlescape/BriefingState.cpp` | `briefing` | checked / pending |
| `src/Battlescape/DebriefingState.cpp` | `debriefing` | checked / pending |
| `src/Battlescape/PromotionsState.cpp` | No dedicated override | checked / pending |
| `src/Battlescape/CannotReequipState.cpp` | No dedicated override | checked / pending |
| `src/Battlescape/CommendationState.cpp` | No dedicated override | checked / pending |
| `src/Battlescape/CommendationLateState.cpp` | No dedicated override | checked / pending |
| `src/Basescape/SellState.cpp` | No dedicated override | checked / pending |
| `src/Basescape/ManageAlienContainmentState.cpp` | No dedicated override | checked / pending |
| `src/Basescape/BasescapeState.cpp` | `basescape` | checked / pending |
| `src/Basescape/PlaceLiftState.cpp` | No dedicated override | checked / pending |
| `src/Basescape/ResearchState.cpp` | `research` | checked / pending |
| `src/Basescape/ManufactureState.cpp` | `manufacture` | checked / pending |
| `src/Menu/ErrorMessageState.cpp` | `error_message` | checked / pending |
| `src/Menu/SaveGameState.cpp` | No dedicated override | checked / pending |
| `src/Menu/LoadGameState.cpp` | No dedicated override | checked / pending |
| `src/Menu/ListSaveState.cpp` | `save_list` (delegated) | checked / pending |
| `src/Menu/PauseState.cpp` | `pause_menu` | checked / pending |
| `src/Menu/CutsceneState.cpp` | No dedicated override | checked / pending |

## Specific support gaps (source checked)

- `UfoDetectedState` publishes painted Intercept/Center captions but deliberately
  grants neither action; only `dismiss_ufo_contact` is offered. UFO detail tables
  remain unavailable. This is not interception coverage.
- `BuildNewBaseState` provider is restricted to first-base placement with hidden
  Cancel; later base placement remains pending, not covered by that provider.
- No dedicated `agentUiState` override was found for 22 of 34 Geoscape states,
  including interception/dogfight, low fuel, craft errors, monthly report, psi
  allocation, base defense/destruction, landing and destination confirmation.
- Cross-directory Sell and alien-containment resolution lack dedicated overrides;
  their preceding error heading's provider cannot complete those stacks.

## Existing test families to reuse (not executed here)

- Geoscape/globe/navigation: `AgentGeoscapeControlsTest.cpp`, `AgentGeoscapeBudgetControllerTest.cpp`, `AgentGeoscapeHudProjectionTest.cpp`, `AgentGlobeProjectionTest.cpp`, `AgentGlobeRasterTest.cpp`; Python `test_agent_geoscape.py`, `test_geoscape_navigation_architecture.py`.
- Public events and reports: `AgentPublicEventsTest.cpp`, `AgentDisplayedMessagesTest.cpp`, `AgentReportWidgetTest.cpp`, `AgentCompletionProvidersTestV19.cpp`, `AgentErrorHeadingTestV19.cpp`; `test_agent_events.py`, `test_agent_reports.py`, `test_agent_completions.py`.
- Arrivals: `AgentArrivalsProviderTestV20.cpp`, `AgentArrivalsProtocolTestV20.cpp`, `test_agent_arrivals.py` (explicitly synthetic socket fixtures), `test_arrivals_architecture_v20.py`.
- Research: `AgentResearchPopupTestV22.cpp`, `AgentResearchArticleTestV22.cpp`, `AgentResearchProviderTest.cpp`, `test_agent_research_completion.py`, `test_agent_research_controls.py`; `research-live/test_route.py` and `test_allocation_route.py` test the runner, not proof that a real game passed.
- Debrief: `AgentDebriefControllerTest.cpp`, `AgentDebriefWidgetTest.cpp`, `test_agent_debrief.py`, `test_debrief_architecture.py`.
- Storage-related contracts: `test_agent_capacity.py`, `test_agent_outfitting.py`, `AgentOutfittingProviderTestV21.cpp`; do not equate stores display with Sell/containment handling.

Native filenames are under `tests/agent-control/`; Python `test_agent_*` files are
under `tools/mcp-observer/`; architecture tests are under `tests/agent-control/`.
`src/Observer/AgentController.cpp`, `AgentProtocol.cpp`, `AgentUi.cpp` and
`AgentPublicEvents.h` are common admission/protocol/event boundaries, not proof of
individual modal support. Check offered action IDs rather than dispatching guessed ones.

## Remaining acceptance ledger to fill during live qualification

For **each game and applicable branch**, record: starting public screen; visible trigger;
exact build/commit and ruleset; before screenshot; offered state/action snapshot;
selected visible action; resulting screenshot/state; return-to-Geoscape or expected
terminal screen; whether time resumed safely; all intermediate modals; PASS/FAIL and
artifact references. Include cancel/back, clipped/occluded text, stale action rejection,
queued multiple events, and resource shortage branches. Do not read hidden state to
manufacture a pass. If a provider is missing, retain fail-closed behavior and mark pending.

This document intentionally neither modifies the gameplay plan nor declares a milestone
complete. Counts describe source inventory, never passed tests.

## Next event-stack priorities

1. Preserve completed research/production slices; add resource-shortage and
   construction branches without re-labelling them as already covered.
2. Complete bounded interception detection→launch→approach→combat modes→visible
   outcome→return in each game. An unmerged development provider is not support
   at this document's baseline; audit offered actions on the actual tested build.
3. Exercise causal tactical mission→debrief→loot→storage enforcement/resolution→
   outbound/inbound transfer. Zero-capacity transfer refusal is not this route.
4. Monthly report→optional commendations/psi/save/defeat, then base defense,
   low fuel, rearming/refuelling errors, lost contacts and remaining event rows.

For each applicable branch check ordinary modal pause, available button semantics,
Cancel/Back only where present, full stack unwinding, expected resumed/paused time,
and repeated/queued occurrences. Do not add a fictitious Cancel to notifications
that expose only OK; do not infer a time value from hidden simulation state.
