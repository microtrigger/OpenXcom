<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original project documentation, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md. -->
# One controlled in-engine hidden-occupant pair

This default-off test fixture advances one part of stage 5. It is a controlled
render/animation test, **not a normal gameplay trace or full fairness gate**.
Normal battle simulation and timer-driven thinking are deliberately not run
inside its sampling interval. No controller, new public observation, audio
interface, network listener, credentials or persistent registration are added.

## Recorded result

The [bounded source/build/runtime evidence](hidden-occupant-evidence.json) records
one final-binary run each for UFO and TFTD. Both exited successfully: eight exact
hidden/empty full-frame matches, 24 non-null responses across three scenarios,
and four odd-phase full-frame/crop positive controls per game. The source baseline
is `e97676bc5b9bf9b432e4be3c9f5c2d119beaccf6` plus the recorded source hashes.
Opt-in and default-OFF builds pass, with no gate/observer symbols or activation
strings in the OFF binary. Existing native/structural suites, 15 UI/transport
paired tests, two existing killed mutants and 125 Python tests also pass.
Only documentation/evidence changed after the final runtime checks. There is no
hosted CI, repeatability distribution, timing or broader gameplay certification.

## Build and run

Use the usual OpenXcom dependencies and separately installed, legally obtained
UFO or TFTD resources. The test neither provides nor copies those resources.
Build a separate executable with:

```sh
cmake -S . -B build-pair -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_CURSOR_OBSERVER=ON -DEXPERIMENTAL_BATTLE_PAIR_GATE=ON
cmake --build build-pair

tests/battle-observer/run.sh /absolute/path/build-pair/bin/openxcom \
  /absolute/path/data xcom1
# Repeat with xcom2 for TFTD's resource/rule profile.
```

The build option defaults to OFF and requires the existing cursor observer.
The wrapper sets the second opt-in, `OPENXCOM_BATTLE_PAIR_GATE=1`, creates temporary
configuration/user directories and removes them on exit. It does not alter
normal game launchers, configuration or saves. Do not use a normal user directory
when activating the gate manually. A real graphical SDL 1.2 software window must
be focused with the physical pointer inside it when resource loading finishes.
Unsupported presentation/focus conditions fail the gate instead of being mocked.
Window-close events remain available; other external input is excluded during
this fixed-input sampling interval. Existing `/1` and `/2` production behavior
outside the enabled test is unchanged.

## Actual engine dependency

After ordinary StartState resource and language loading, the runner constructs
fresh SavedGame/SavedBattleGame, 40×40×1 real Tile maps, pathfinding/TileEngine and
two real rule-backed BattleUnits. One selected player-faction unit and one
hostile use the active vanilla master's one-tile Sectoid/Aquatoid soldier rule.
The roster and construction order are identical in all scenarios, with both
engine RNG and std::rand pinned at setup. No campaign save is loaded.

The same hostile occupies the actual current-hover tile in the hidden case and
a distant real tile in the empty-hover case. Position, Tile occupancy and unit
back-links are established by the normal resetUnitTiles method and asserted.
Both sides remain alive. This fixture uses a discovered, otherwise empty map;
it does not test terrain semantics, map generation or natural scenario reachability.

Each real BattlescapeState goes through its ordinary constructor and init,
including map caching, ordinary init-time FOV work, HUD update and camera setup.
The hidden occupant must remain invisible after that setup. The positive control
then sets its initial stored visibility to visible before any observation request.
No observer request computes FOV or changes world state. The controlled stored
visibility is not evidence that a particular preceding gameplay history produces
these worlds.

An ordinary mouse-motion Action passes through Cursor and BattlescapeState/Map
handling to select the same hover. Debug is off, CT_NORMAL is active, the camera
level matches, the mouse is outside the icon panel and the top state is the real
BattlescapeState. A test-only assertion at Map's actual cursor-back branch checks
that it visits the intended Tile and expected animation phase. Renderer decisions
and production visibility logic are unchanged.

The exact sampling boundary starts with the first request after that setup.
There are eight complete presented frames per scenario, phases 0–7. Each later
frame is driven by one call to the existing BattlescapeState::animate; normal
think/timers and simulation advancement are skipped. All composition still goes
through the existing Game state/surface/FPS/cursor composition and Screen::flip.
There are no unrecorded extra fixture presentations inside that interval. Fresh
scenario setup uses an intervening unpresented blank State: Game first deletes
the old BattlescapeState and only then replaces/destroys its save and battle.

## Oracle and transport

The internal test client owns a connected Unix socketpair. No test-control or
hidden-world channel is exposed to MCP or any external process. Its request is
queued before the existing beginObserverFrame. Responses come only from the
production PresentationObserver after the existing successful SDL_Flip.

At that final boundary the fixture independently palette-resolves every pixel of
the complete 640×400 display into RGB, retaining the complete histories only in
process memory. It also records cursor geometry and actual HUD-candidate geometry
and foreground eligibility. It requires successful non-null HUD and cursor on
every frame, exact epoch/challenge binding, strict HUD fields and bounds, exact
produced cursor bytes, one response per request, no extra packet and an open
channel after every response.

Only after all eight hidden/empty complete RGB frames and eligibility records
match does it compare the full request/response transcript, including packet
lengths, order, count and all nonrandom bytes. Only the three identified random
transport token fields are normalized, with a width-preserving, consistent
bijection across each complete trace. Raw epoch continuity, admission chaining
and fresh-token uniqueness are first checked globally across all three scenarios;
normalization cannot conceal a reused admission or changed epoch.

The visible positive control must change both the complete display and actual
produced crop on every odd phase, where the ordinary normal cursor can change.
All eight phases are retained. Separate comparator self-checks alter recorded
pixels and one HUD value and require rejection. These self-checks are explicitly
not production hidden-state leak mutants; existing UI/transport mutation tests
remain separate evidence.

## Audio, output and limits

Before resource loading, the gate closes the actual SDL_mixer device and shuts
down SDL's audio subsystem, then pins mute. It verifies mute, no open mixer and
no initialized SDL audio subsystem throughout every scenario. The scoped game
audio output is therefore disabled. This is **not** mixed-PCM capture, audible-cue
parity, speaker verification or an audio-observer implementation.

Only a fixed PASS aggregate record or generic assertion label leaves the test.
No RGB histories, raw transport nonces, hidden unit values, saves or game resources
are written or published. Ordinary resource-loader diagnostics are not battle
state evidence. Preserve failures and unsupported profiles in any test report.

This pair leaves simulation noninterference, hidden smoke/fire densities,
occlusion/hidden movement, death/projectiles/audio, history/load/reconnect,
latency distributions, physical side channels, other platforms/scales/assets
and future controller availability tests open. Its native producer checks do not
rerun the Python adapter against this battle; the existing adapter/UI suites
cover that separate boundary. It does not close stage 5 or authorize a controller.

The structural check is asset-free and does not constitute runtime evidence:

```sh
python3 tests/battle-observer/check_boundary.py
```
