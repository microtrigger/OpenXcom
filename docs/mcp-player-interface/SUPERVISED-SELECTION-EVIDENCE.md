<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Bounded supervised selection verification

Date: 2026-10-12. Base revision: `b7399ba4adc322f4a45a72e09917db2397ee23c0`.
The [aggregate record](supervised-selection-evidence.json) pins the modified
source and final native binaries. This establishes a bounded next/previous
selection milestone under `practical-play/1`, not the full controller or Stage 5.

## Normal native runs

The final frozen gate binary passed one selection run and one separate stance
regression run for each of UFO and TFTD on a focused graphical software display.
Selection covers ten manual/controlled pairs and nineteen adversarial worlds
per game. Three separately eligible soldiers make next and previous independently
distinguishable. The pairs cover direction, ring wraparound, native do-not-reselect
and out-of-play skips, and a single eligible actor's no-op.

Every pair checks semantic battle/runtime state, engine RNG, selected actor,
current-action actor, normal camera behavior and unchanged TU/stance. Each
controlled pair also obtains a new post-dispatch panel, compares it to the manual
result and independently checks its pixels against the actual software display.
Local mouse or fresh-Y approval goes through the real displayed confirmation
modal and the ordinary selection handlers. The gate-only harness deliberately
approves its own finite test fixtures; this is not a public approval endpoint.

Adversarial cases cover same-looking selection changes, manual play, pause,
focus notifications, actual save/state replacement, TU/reservation changes,
missing capabilities, cancellation, stale/premature/held confirmation, failed
post-action observation, replay and disconnect. D↔N conflicts exercise the shared
command-ID ledger in both directions with a fresh valid capability, as well as
consumed-capability reuse. Receipts remain dispatch-only. Existing stance
regressions retain twelve pairs and nineteen adversarial worlds per game,
including the actual immediate reaction queue.

## Sensitivity controls

Four real-source mutations were separately compiled and linked against the same
325-object frozen build. Each mutant replaces exactly one object while retaining
324 verified unchanged objects, unchanged gate code, flags, generated headers
and libraries. The original source and binary remain unchanged.

Each control failed on its first run in both games at its intended assertion:

- Next dispatched as previous: normal handler direction, line 713
- Previous dispatched as next: normal handler direction, line 713
- Duplicate next dispatch: callback count, line 402
- Selection charged one TU: independent unchanged-TU check, line 413

All eight runs produced bounded assertion failures, not crashes or watchdog
failures. The mutant sources, objects and binaries have separate hashes in the
aggregate record. No assets or executables accompany this publication.

## Other checks

- Default-OFF, selection-enabled production, and selection-enabled gate builds
  linked successfully. The default-OFF binary has no controller/selection gate
  symbols or selection activation/prompt strings.
- Enabling selection without its stance prerequisite is rejected by CMake.
- Observer Python suite: 274 discovered tests, 272 executed, two expected
  native-logger skips. New selection coverage includes 24 tests and an actual
  launcher subprocess with a synthetic protocol peer.
- Stance/selection wrapper and mutation-setup tests: 20 passed.
- Native HUD, cursor, paired observer, diagnostic-isolation and mixed-audio
  regression scripts passed, including their existing sensitivity controls.
- Whitespace/diff checks passed; GPL-3.0-or-later notices and upstream licenses
  are retained.

The new selection adapter's subprocess test uses a synthetic peer, while the
native gates use the actual game. This milestone does not claim a new official
SDK→adapter→game selection demonstration, real model usage or token savings.
Historical stance SDK/game demonstrations retain their original scope.

## Earlier attempts and limits

The first selection and stance runs in each game timed out at a generic gate
watchdog with the pointer outside the game window. Those four attempts remain
failed in the aggregate record. An explicit new run after ordinary pointer
placement inside the game passed all four profiles on unchanged code. The
watchdog alone did not identify the expired stage or prove the causal diagnosis.
Gate-only diagnostics were then improved to report fixed assertion-line labels
for an expired stage/readiness predicate, without changing timeouts, focus
requirements or production logic; the final source was rebuilt and all four
profiles passed again.

The finite land fixtures do not establish underwater/multi-stage coverage,
full campaigns, every eligible unit/mod, complete reaction outcomes, physical
user visibility, timing equality or full non-disclosure. Cycling onto a unit
outside the narrow context profile can stop subsequent observation. Movement,
turning, inventory, firing, end-turn control and sequential batches remain
unimplemented. No hidden-state output, target eligibility query or automatic
retry after uncertainty is added.
