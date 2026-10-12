<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised inventory CLOSE: evidence and limits

**Bounded verification complete.** The final native suite and two production
SDK paths pass. Sensitivity is established for 17 of 18 fault recipes; the
surviving single-guard fault is explicitly limited below. This is not full
controller or fairness acceptance.
See the [pilot contract](SUPERVISED-INVENTORY-CLOSE-PILOT.md) and
[machine-readable evidence](supervised-inventory-close-evidence.json).

## Revision and isolation

The source baseline is `69849b4ca56167140e004ec5b0aba3260e7924e6`, plus this
CLOSE contribution. The production-game smoke used a nongate binary with SHA-256
`f609ee3957a1fe3b744e7f9cb5b7b5c1bf61eed28fba9283254c83eda9f1179c`.
A subsequent rebuild including the gate-only Map header declaration produced the
same bytes. No test gate was enabled in this production binary.

The fresh default-OFF binary is byte-for-byte identical to the preserved preceding
default-OFF binary:
`5f9dd82f5ff0af34146da6602f0ca27ff307dfa319209ccdda26b7a43c446056`.
All 21 experimental options are OFF; experimental control/observer/gate symbols
and activation/protocol strings are absent. This identity result applies to
these recorded builds, not every compiler or platform.

No original game resources, images, generated native binaries, credentials or
private runtime artifacts are included. New code and documentation retain
GPL-3.0-or-later and existing upstream/third-party notices.

## Client, launcher and synthetic SDK

- Python discovery ran 426 tests: **424 passed, 2 expected native-only skips**,
  both normally and with `-O`.
- The new 36-test CLOSE suite covers exact J33/C97 encoding, strict context/image
  validation, the O/V/I/J-to-action matrix, shared ledger limits, status-only
  duplicate behavior, mandatory post-C V, deadlines and opt-in isolation.
- All eight distinct official-SDK synthetic smoke scripts passed normally and
  with `-O`. Their fixture transports are not native-game evidence.
- The private explicit-step live driver passed 42 pure checks normally and with
  `-O`; the main operator independently reran its normal suite and hash manifest.
  Its nonterminating transport was unchanged from the previously reviewed
  combined driver.
- All 28 native-wrapper/mutator pure tests passed normally and with `-O`: 17
  wrapper tests and 11 source-mutator tests. They do not substitute for runtime
  sensitivity checks.

One initial Python-discovery invocation used the wrong working directory and
failed an existing cwd-sensitive subprocess test. The documented invocation
passed without changing that unrelated test. This invocation failure was retained.

## Actual official SDK → production game

One normal-close sequence passed on its first attempt in each game, using the
official MCP SDK 1.29.0 and protocol `2025-11-25`:

1. Fresh tactical V, one B proposal, actual local confirmation, explicit status.
2. Mandatory capless I, then fresh close-capability J.
3. One C proposal, actual in-place overlay, fresh local Y, explicit status.
4. Mandatory fresh tactical V, followed by explicit SDK stop.

There were **16 tool calls and two stop receipts**, with **eight stored PNGs**.
No action was retried. The driver did not approve the game overlay, poll status,
query hidden state or run a model. Normal New Battle/deployment UI prepared each
disposable mission; no saved-world fixture or item manipulation was used.

| Game | Visible soldier | TU before opening / after closing | Observed return |
|---|---|---:|---|
| UFO | Cai Kong | 50 / 50 | Same Skyranger tactical scene |
| TFTD | Henk van der Veen | 65 / 65 | Same Triton tactical scene |

Both opening and closing proposals initially returned `pending_confirmation`.
Their explicit status reads returned `dispatched`. Closing was verified by the
subsequent actual tactical image, not inferred from that receipt alone.
All I/J/V images were inspected. The J image and real overlay displayed the
same named soldier and TU; no input intervened between J and C.

Independent review checked both complete receipt sequences, all eight PNG hashes
and byte counts, and visually read both J/final-V pairs. Separate PNG validation
checked canonical chunk order, CRCs, indexed 320×200 shape, palette and row data.
This is review of retained evidence, not a second live execution.

Fresh desktop observations showed the game still alive after each SDK EOF; it
was then closed normally. Local consent and post-EOF liveness are **operator
attestations**, not independently replayable event captures or separately stored
post-stop screenshots. The driver deliberately retains `visually_verified=false`
and `game_liveness=unverified`; the separate operator record supplies those
observations. Exact parent identity is enforced natively, not independently
proven by the public SDK response.

The optional cancel-only live-driver scenario was prepared but not run. This
normal-close smoke does not prove arbitrary campaigns, items, mods or transients.

## Prior-control regressions

All ten native regression invocations passed: stance, selection, map right-click,
map left-click and inventory opening, each in UFO and TFTD. The separate regression
binary includes CLOSE production code and all those prior gates; the CLOSE gate
itself is disabled. Binary SHA-256:
`ab1474724c05c56f618c9a222b306aada0d0da1f9f06c59a6fd7ee6364a5493e`.
Earlier milestones retain their original limitations, including timing inequality
and no full-fairness claim.

## Native CLOSE acceptance

The final frozen native binary is
`20ca28a7b0c1f2bf57db3e651ecd4398d2657e3003130ad076db24f30cac8139`.
All **34 profiles per game, 68 total, pass**. Each core run reconstructs 14
manual/C pairs: seven fixtures with `maximizeInfoScreens` off and on. They cover
zero/ordinary TU, a loaded weapon, filled slots, paged ground, a primed grenade,
and positive flare/terrain-FOV consequences. The gate uses actual ordinary input,
normal game loops and an independent physical SDL display oracle.

Semantic saved-world/runtime comparisons and simulation RNG snapshots accompany
pixel checks. The snapshot reads `RNG::getSeed()`; libc/seedless RNG state was not
measured. The earlier wording incorrectly included libc RNG and is corrected
here without changing the recorded runtime outcomes. A manual result is observed
through all eight naturally occurring
map-animation phases; C's full RGB frame must exactly equal an actual manual
frame at the same phase. No pixel mask, synthetic reference rendering, forced
timer advancement or animation-phase reset is used. **126 physical frames per
game, 252 total**, are retained privately with hashes/readiness records. A separate
file audit validates all hashes/PPM shapes and all 28 whole-frame pair matches.
Timing equality is not claimed.

The 33 non-core profiles include:

- Later-ground-page and noncanonical-layout cancellation, no extra init/ground
  arrangement, surface/page/state preservation, then a fresh successful J/C.
- X1/X2, wheel, inventory hotkey, outside click and modifier dismissal without
  underlying input fall-through; queued pre-presentation Y, held Y and held mouse.
- Held-item refusal without dropping or clearing the item; ordinary manual
  unload/reload, exact TU/ammunition identity/ownership and capless I observation.
- Same-looking actor selection away/back, ordinary input and reinit invalidation;
  focus, display, overlay, inventory/parent/save lifetime loss and disconnect.
- Real 120-second consent expiry in each game, shared-ledger exhaustion,
  conflicting IDs, cross-kind grants, capless-I schema and disabled/non-TU/debug
  rejection. Exact duplicate C/status does not redispatch or rearm fulfilled V.
- Natural deferred destruction/display restoration, mandatory exact-parent V,
  and ground-item/actor gravity followed by ordinary falling completion.

The positive lighting fixture drops a flare normally and requires a changed
terrain shade after closing. The FOV fixture turns an unselected friendly during
inventory setup; a north terrain tile is proven undiscovered/invisible before
closing and discovered/visible afterward. These are isolated test fixtures,
not hidden-state fields or setup operations exposed by the MCP tools.

## Actual-source sensitivity

Eighteen separately modified source binaries compiled and linked. Each replaced
one production translation unit/object; the normal source, gate, wrapper,
compiler/linker dependencies and baseline objects were hash-checked. Predeclared
profile/assertion expectations were frozen before running each mutant in both
games. **34 of 36 runs reached their intended behavioral assertion: 17 of 18
fault recipes detected in both games.** There were no crashes, watchdogs or
build failures counted as detections.

Detected faults cover missing actual overlay drawing, duplicate ordinary OK,
direct-pop bypass, cancellation init/ground rearrangement, dismiss-event leakage,
unseen approval, stale actor binding, missing gravity/lighting/FOV, premature V,
post-close barrier bypass, conflicting-ID acceptance, capless-I authority,
held-item admission and J-pixel corruption. The unseen-approval fault is
**composite**: it removes three independent consent guards. It does not establish
individual sensitivity of every guard separately.

The `stale-reinit` mutation removed only `uiRevision` equality and **survived in
both games with exit 0**: the stale request was still rejected. Source inspection
explains a redundant path: `InventoryState::init` invokes `_refreshMouse`, whose
ordinary `SDL_WarpMouse` calls queue motion events; `StanceController::beforeEvent`
invalidates the grant independently. This is a **source-based explanation, not an
additional causal experiment**. The baseline stale-init refusal passes, but
sensitivity of the UI-generation guard alone is not established. Its original
surviving outputs and unmodified predeclared expectations remain retained.

## Earlier attempts, corrections and exclusions

No failed attempt was relabeled as a pass. Retained native attempts are:

| Attempt | Profiles run | Passed | Failed |
|---|---:|---:|---:|
| Initial full suite | 66 | 55 | 11 |
| Revised full suite | 68 | 64 | 4 |
| Focused animation/FOV attempt | 10 | 8 | 2 |
| Focused terrain-oracle attempt | 10 | 8 | 2 |
| Corrected focused suite | 10 | 10 | 0 |
| Final frozen full suite | 68 | 68 | 0 |

Corrections concerned fixture readiness/pointer focus, item-drop sprite centering,
first-overlay bookkeeping, an extra Escape after already rejected consent,
deterministic pre-blit consent queueing, the UFO flare rule ID, and the ordinary
falling queue's lifecycle. Earlier hostile-FOV fixtures failed exact pixel parity
because the visible-enemy button has independent blink/startup behavior. Retained
pixels established an immediate black pre-blink background; an earlier phase-offset
hypothesis alone was incomplete. A longer natural joint cycle did not establish
initial-frame equality. That fixture was replaced by the narrower positive
friendly terrain-discovery check above; **hostile-discovery pixel parity is not
claimed**.

One subsequent oracle incorrectly treated `BattleUnit::visibleTiles` as general
terrain visibility. Source inspection shows it stores opposing-unit tiles;
terrain LOS instead updates `Tile` discovery/visibility. The erroneous assertion
failed even ordinary manual closing. The corrected source-grounded terrain
oracle passes both manual and C paths. Earlier raw results/frames remain retained.
Some original early build/setup diagnostic logs were overwritten before retention
was added; no claims depend on reconstructing those missing logs.

## Limits

This slice proposes ordinary OK closing only. Item manipulation remains manual.
The native gate qualifies 640×400 (2×) software presentation; it does not establish
other physical scales, OpenGL, arbitrary mods or every inventory configuration.
Deployment/non-TU inventory, debug mode and held-item close proposals are excluded.
Full controller acceptance, Stage 5, campaign coverage, sequential batches and
actual model/token benchmarking remain open. No token-saving result is claimed.
