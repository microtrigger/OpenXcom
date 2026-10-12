<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised inventory OPEN: finite evidence

Date: 2026-10-12. Base: `c701a13b24d0555e560622739bd5be19950d5793`.
See the [pilot contract](SUPERVISED-INVENTORY-OPEN-PILOT.md) and
[native fixture](../../tests/inventory-open-control/README.md) and
[machine-readable evidence](supervised-inventory-open-evidence.json).

## Native baseline

Both **UFO and TFTD** pass all 14 isolated profiles. Per game, the core contains
12 independently reconstructed ordinary-UI/controlled opening pairs and 24
additional worlds. Six equipment/ground configurations run with information-screen
maximization both off and on: zero-TU and ordinary empty inventories, a loaded
weapon, occupied carried slots, multipage initially noncanonical ground items,
and a primed grenade.

The ordinary inventory handler runs once. Literal TU/energy/position/facing
expectations, normal initialization counts, canonical ground-slot expectations,
and semantic save/action/visibility/engine-RNG comparisons agree with manual opening.
Repeated capless reads do not initialize or arrange inventory again. Every full
view is compared pixel by pixel against the independently decoded physical
indexed display, including canonical first-seen RGB colors and zeroed unused
palette entries. Natural primer animation is witnessed without forced timers.

The core covers ordinary hover, page changes, selected-soldier changes, held
items, close refusal while holding an item, ordinary item return and manual
closing. Two worlds vary undisplayed ammunition and an off-page medikit charge;
actual RGB equality is established before normalized wire-view equality is
required. This is a bounded knowledge check, not a general hidden-state theorem.

Other profiles cover delivered focus-event, overlay/pop/state/display interruption, safe combined
state-and-save replacement, the shared non-evicting 64-command ledger, mandatory
post-R/L V, disabled inventory/turn activation and unsupported non-TU/debug views.
A lost B destination retains its mandatory I obligation. O/V cannot bypass it,
and I grants none of B/D/N/R/L. The save-replacement profile does not isolate a
save-only change beneath a live inventory's raw save pointer.

Delayed-input worlds use an ordinary low frame-rate setting to create a pending
read. A separate 1600ms main-thread stall checks the fixed 1500ms native deadline;
this is artificial starvation, not a natural-play timing result. A deliberately
partial deferred palette is rejected before another flip; restoring a full
palette through the normal display API and ordinary flip permits a fresh I.
An actual graphics-driver palette-application failure was not induced.

## Official SDK through a real production game

One complete, first-attempt integration run passes in each game with the
nongate production executable and official MCP SDK 1.29.0, protocol 2025-11-25:

1. Normal New Battle/deployment UI reaches an ordinary tactical screen.
2. `read_tactical_context` returns a visually verified 320×200 PNG and fresh V.
3. One `request_open_inventory` returns `pending_confirmation`.
4. The actual readable local modal is inspected and confirmed through ordinary
   local input: Y in UFO, the Confirm button in TFTD.
5. `read_stance_status` returns `dispatched`; this alone is not outcome proof.
6. `read_inventory_view` returns the actual, visually verified capless inventory
   PNG, with no epoch/capability or hidden item fields.
7. Ordinary manual closing is followed by a successful fresh tactical PNG.

All five public calls use the real SDK session and production launcher/native
transport. There is no synthetic native peer, hidden engine/save inspection,
automatic approval endpoint, action retry, model inference or token benchmark
in these two runs. The SDK session uses a nonterminating stdio transport: observer
shutdown closes observation only. Both games remained visible afterwards and
were then closed through their normal window controls.

The UFO initial/post-close PNGs are identical. TFTD's ordinary animation changes
pixels; equality is not claimed there. PNG/schema integrity checks and visual
inspection supplement, rather than replace, the native pixel oracle. No game
images, saved games, resources or private command tokens are published.

## Transport and build checks

- 390 Python tests run normally and with optimization: 388 pass and two expected
  native-only checks are skipped in each run; 38 are focused inventory tests.
- 29 wrapper and mutation-setup checks pass. Setup checks are not runtime
  sensitivity results.
- All seven official-SDK synthetic-peer smoke programs pass normally and with
  optimization. The inventory smoke verifies the exact 65,063-byte indexed PNG,
  strict three-token proposal, B99/I33, capless schema, shared five-action ledger,
  mandatory I and unchanged prior-action interoperability.
- Production, isolated native-gate and default-OFF Release builds pass.
- CMake rejects inventory without TURN, the aggregate gate without prerequisites,
  and specifically the inventory gate without its map-left-click dependency.

The default-OFF binary has no experimental controller/gate/inventory-helper
symbols. It is not byte-identical to the preceding milestone: adding guarded
source lines moves an existing OpenGL screenshot diagnostic's source-line
constant from 652 to 666. Recompiling and relinking old/current Screen objects
with identical other inputs reproduces both binaries exactly; the only code-byte
difference is that diagnostic constant, plus the resulting GNU build ID.

## Earlier failures and corrections

All earlier failed attempts remain failures and are excluded from passing totals.
Both games initially rejected the immediate inventory read in the first
maximized-information-screen world. Ordinary construction resets display-palette
qualification, and normal initialization defers the full palette until flip.
The fix permits this narrowly qualified I waiting stage; final capture still
requires the exact applied palette after successful flip. O/V/action/modal guards,
lifetime invalidation and fixed deadlines remain unchanged.

Subsequent fixture corrections preserved production semantics: wait for the
ordinary restored tactical flip before post-close V; give a selected comparison
soldier visibly distinct setup text before asserting changed pixels; wait for an
ordinary turn to finish before the single manual inventory opening; and use a
first-color-zero one-color palette update to establish a genuinely partial pending
palette rather than the display API's normal full-range merge. Additional static
fixture review drained held-key release events and used the currently selected
soldier's map location for typed-ledger conflict packets.

The initial Python aggregate also exposed invocation-directory assumptions and
queued-response test scheduling. The new inventory queue fixture now waits until
both peer packets are actually queued before checking the production extra-packet
rejection. Its original failing output was recovered and labeled as such; final
normal/optimized aggregates pass from their documented working directory. No
production behavior or existing left-click test was changed for that fixture race.

## Sensitivity and prior-control regression

All prior stance, selection, right-click and left-click native regressions pass
in both games on the final inventory gate binary. Observer, cursor, paired-observer,
mixed-audio and diagnostic-isolation native scripts also pass within their
existing scope.

All 15 actual-source faults are detected at their intended semantic assertions
in both games, 30 first-attempt detections in total. No crash, process timeout or
watchdog substitutes for a semantic failure. The faults cover duplicate/wrong
handler dispatch, extra TU spending, wire corruption, leaked capability, lost
B/I and R/L barriers, unwanted input cancellation, forced initialization, stale
camera/pose, unseen consent, deadline extension, conflicting IDs and the first-flip
palette regression. Source fingerprints, binary hashes and exact assertion
labels are recorded in the machine-readable evidence.

Each disposable source copy changes one production file and recompiles one
translation unit. All 328 baseline link objects, 922 dependency inputs and six
toolchain files were fingerprinted; 327 other link objects remain unchanged per
mutant. Fifteen distinct binaries were built and verified before execution.
An earlier build set was superseded, without runtime use, when the gate added an
immediate wrong-handler assertion to avoid relying on a watchdog. The final
baseline passes both games with that assertion in place.

## Limits

This is OPEN-only plus presented observation. Inventory closing remains manual;
item movement, equipment/reload/prime/drop actions, automatic batches, other
languages/scales/mods, broad campaign acceptance and model/token benchmarking are
not established. Ordinary inventory opening can rearrange ground display slots;
it is not described as a side-effect-free read. No full-fairness or timing-equality
claim is made, and the overall player-interface plan is not complete.
