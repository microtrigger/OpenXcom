<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-13. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Ordinary inventory reload: bounded SDK evidence

The existing default-OFF [CLICK](SUPERVISED-INVENTORY-CLICK-PILOT.md) and
[Unload](SUPERVISED-INVENTORY-UNLOAD-PILOT.md) controls complete a finite visible
unload/reload cycle in stock UFO and TFTD tactical TU inventory. Each actual
production-game path completes **32 official-SDK tool calls, 16 returned images,
eight separately considered proposals and an explicit stop**. Seven actions are
dispatched; one reload proposal is locally declined. This adds integration
evidence for an existing ordinary click path, with no new native command, API,
observer field or production build.

Both games have accepted independent protocol/image reviews, successful
before/after checkpoints and independently verified terminal integrity records.

The [machine-readable evidence](INVENTORY-RELOAD-SDK-EVIDENCE.json)
records the bounded result and provenance. Earlier
[CLICK](SUPERVISED-INVENTORY-CLICK-EVIDENCE.md),
[release correction](INVENTORY-CLICK-RELEASE-FIX-EVIDENCE.md) and
[Unload](SUPERVISED-INVENTORY-UNLOAD-EVIDENCE.md) evidence retains its own scope
and failures. The original full Stage 5/controller gate remains open.

## Existing control boundary

Reload is the ordinary result of dropping compatible ammunition on a weapon
through the existing inventory LEFT-click handler. The normal game decides
whether to reload, refuse or leave the situation unchanged. There is no hidden
ammunition list, compatibility/fit/TU preflight, automatic weapon selection or
outcome field. Coordinates come from the latest presented image and stay within
the admitted stock hand/displayed-ground regions; belt/backpack targets are not
added by this result.

Each E proposal needs fresh H context, its own local confirmation and a new
capless I after dispatch. Unload uses its existing Q/F grant and the same exact
surviving-inventory observation obligation. Cancellation preserves the held item;
CLOSE/Ground retain their held-item refusal. Type-bound, one-use authority,
shared command deduplication, input invalidation and fail-closed lifetime checks
are unchanged. A receipt describes dispatch, not native success or eligibility.

The earlier native CLICK fixtures already cover reload in hand and on ground,
wrong ammunition, an already loaded weapon, insufficient TU, held-safe
interruption and negative mandatory-I/send-failure paths. This live sequence
checks positive integration and explicit post-action observation compliance.
It does not rerun those native tests or independently re-prove barrier enforcement.

## Fixed finite sequence

Ordinary visible setup prepared a loaded removable-ammunition weapon in the right
hand, empty left hand, no held selection, a compatible spare clip on the current
ground page and at least 23 displayed TU. Preparation used the normal UI; the
measured sequence began in tactical TU mode. Ordinary prebattle preparation is
not evidence of SDK control in prebattle/non-TU inventory.

Each block below was submitted separately after inspecting the preceding result
and relevant UI. S denotes one status read; it is not a polling loop.

1. V → B → S → I: open inventory with local confirmation.
2. H → E1 → S → I: pick up the loaded weapon.
3. Q → F1 → S → I: ordinary Unload.
4. H → E2 → S → I: pick up the detached clip from the left hand.
5. H → E3 → S → I: propose dropping it on the weapon, then decline locally.
6. Fresh H → E4 → S → I: propose the drop afresh and confirm ordinary reload.
7. H → E5 → S → I: pick up the spare compatible clip from displayed ground.
8. H → E6 → S → I: confirm a drop on the now-loaded weapon and inspect its refusal.
9. Explicit stop: ordinary transport EOF after the final I, with the spare held.

The status receipts are dispatched for B/E1/F1/E2/E4/E5/E6 and unavailable for
E3. All seven dispatched-action I reads are present. I after E3 is a planned
visibility checkpoint, not evidence of a native dispatch barrier. No status
retry, action replay, automatic approval, outcome-driven branch or observation
fallback occurs in either completed sequence. There is one completed live SDK
attempt per game and no in-flight retry.

The driver records the complete protocol sequence but leaves every native-outcome
flag false and game liveness unverified. Separate manual observations and image
review provide the visible outcome evidence; those records are not promoted into
automatic hidden-state assertions.

## Visible outcomes

- **UFO:** Joakim Claussen starts with an Auto-Cannon loaded with 14 AC-AP rounds
  and an AC-HE spare on ground. Weapon pickup preserves 68 TU. Unload clears the
  held selection, leaves the weapon right and AP clip left, and changes TU
  **68 → 60**. Picking up the detached clip and declining E3 preserve the held
  clip and 60 TU. Fresh E4 clears the held clip, removes its separate hand
  presence and changes TU **60 → 45**. The HE spare is then picked up; E6 visibly
  reports “Weapon is already loaded!” and preserves the held spare and 45 TU.
- **TFTD:** Andrei Tanase starts with a Gas Cannon loaded with eight GC-AP bolts
  and a GC-Phosphorous spare on his displayed ground. Weapon pickup preserves
  49 TU. Unload gives the same right-weapon/left-ammunition arrangement and changes
  TU **49 → 41**. Detached-clip pickup and declined E3 preserve the held clip and
  41 TU. Fresh E4 clears the held clip and changes TU **41 → 26**. E6 shows the
  same ordinary already-loaded warning and preserves the held spare and 26 TU.

The operator records that TFTD ordinary setup included moving the spare from belt to the selected soldier's
current ground tile, spending six TU before the measured sequence. Its initial
49 TU is the actual prepared starting value; setup cost is not attributed to
reload. Visible ground must belong to the current inventory context, not merely
to a previously inspected preparation screen.

In both games the reload observation combines disappearance of the separate
clip, cleared held selection and a 15-TU decrease, supported by the later
already-loaded warning. The weapon sprite and stale hover ammunition text alone
are insufficient. No hidden ammunition ownership or complete internal state is
queried or established by this evidence.

Each transient warning was captured immediately in a separate live screenshot.
It had faded before the returned I32 image; the report does not claim I32 shows
it. No action replay, timer change or pause was used to recover the warning.
After final I and orderly EOF, ordinary pointer interaction with the held spare
remained responsive according to the operator, followed by ordinary cleanup of
the disposable battle. This is not a disconnect test during pending consent or
an unsatisfied post-action observation.

Independent image inspection can check visible warning text, item placement and
TU progression. Local consent, the causal timing of the warning, post-EOF
responsiveness and cleanup remain separately operator-attested, without an
independent stored input/event trace. A changed held-sprite position supports
the responsiveness account but cannot by itself prove liveness or capture timing.

## Reused source and build

Published comparison base:
`d5f2c106ce71baa71d579f04065157dbae04bfe7`.
Its tree is `dcc76eb19c8bf76abc7bbddd92734662fb85f6a6`.
Comparison against all 1,402 files of the original Unload r2 freeze finds only
five changed documentation files plus two added public evidence documents.
The original build/source identities remain intact; this is not a new freeze
or rebuild at the publication commit.

| Identity | SHA-256 |
| --- | --- |
| Original Unload r2 source digest | `6ee35bf2e16b02805a6e74d21551db0876f3343df8296cd81d12be8a1ea97fe0` |
| Reused non-gate production executable | `1b4cdab2247d229e3251aaccebfa46a48740952c6eec09372e57c21138176779` |
| Original production-build provenance | `b684150f7e304e31998db4d8ef210dd1f5953cf806ff0a55c8c34679d62854c7` |
| Reused launcher | `97aee12b231233bcdfc802e07f3091036457226ef966b6669dcc5c222af7def1` |
| Reload SDK driver | `7d2f583c6d8051b7f07ac37697ede8c6202409ee269afc0185151d36a7ebae8c` |
| Unchanged transport section | `db23ac33ab9061c6ea0b2e31cfbf7075e4eaf207f470946104488b452822a4d6` |
| Frozen finite runtime contract | `da74be0deb0668bbddb6e0658694a8f64e2db9fbe50ad17cd37abf9da53bc46f` |
| Published-source comparison | `c91309cc9a07e40a7e4a3dde25d4adff8f1aa4d7107e2c9982dfa440bfec3fa7` |

Actual runs use official MCP SDK 1.29.0 and the reused production executable
with all native test gates disabled. No new native, adapter or API implementation
is claimed. The reload driver pure suite passes **83 checks in both normal and
optimized Python modes**. Its initial preparation-directory creation failure occurred before
any candidate or tests were written or run and was corrected before the frozen candidate; the earlier failure remains retained.
Integrity-helper pure suites pass **126 checks in each mode**. These synthetic
checks are not game runs. The helpers retain the original reviewed source-contract
schema; changes to these preparation helpers are reload-specific schema labels only.

## Checkpoints, review and terminal integrity

Each run has explicit successful before/after checks of **1,424 selected files**
and the exact **1,540-entry frozen source tree**, with no expanded source
exclusions. Mutable ordinary display preferences remain separate. These named
checkpoints establish equality at their recorded boundaries, not uninterrupted
immutability or a complete SDK/interpreter/OS/game-data closure.

| Record | UFO SHA-256 | TFTD SHA-256 |
| --- | --- | --- |
| Driver final manifest | `f4a6fb861c117c8d6b92a212cc97d9ff61b71d889882be3b110946c498481d93` | `d3654b8c8d01c53f0ef780e910aa0d68607a07185898c41a5f7b5b6fe296cfa2` |
| Before checkpoint | `17859052eb7de13870fb0d2227735621c5aee1667db9993575572b97a906bf9f` | `9ffb45d90986b72b9bf5639a4b4202222c2bbceb614c8cba6e8b3de8767babf5` |
| After checkpoint | `f8a5f4df6fb3316147a6c492378ff16d82f77091ac660db84c65477b8ea6ccad` | `fcea3a11e20122edb51278d47f7f043195b1dfbba0972617d247d551fafa08af` |
| Independent runtime review | `421257b30f7e05f36c993b457e4f59cdc920c0c90e26298de54c42cb47a8921a` | `7ebf340e7321a038634bc327a1a239511fe76005df680cc9004e72e083a71a65` |
| Terminal seal | `fa4fa4158ab0e2769c2470461270168fa3f7a1657d2e8ea2a52c3d92fff761ab` | `27471010d5f469743f0919df2f9416d387f2d6f7c225cf6d2c743846a72cc8fa` |
| Independent terminal-integrity review | `5b4af028854628205a10ad1d3ab936cc14d752c6c14f559be7fc1f5dd172a816` | `935904e607aa6e7387edb37ee8d1d706d3c03227822fee15316b709e5d3f64cb` |

Each terminal integrity check binds all 91 run entries, seven role references
and 1,442 input pins. The 18 supplementary pins in each are finalization-only and
do not retroactively become before/after runtime coverage. Terminal sealing preserves final bytes, raw streams and
negative/unverified fields. It does not independently witness process exit,
validate the call sequence, establish native success or upgrade acceptance.
Protocol/image review and recorded exit-zero process results are separate evidence.

Pre-runtime preparation and launch interruptions remain recorded separately.
They are not SDK gameplay attempts or discarded action failures. No private
runtime paths, original game resources, screenshots or binaries accompany this
public documentation; hashes identify retained evidence without redistributing
those assets.

## Practical inventory readiness and remaining work

Together with the earlier separate OPEN/CLOSE/Ground/CLICK/RIGHT/Unload results,
this closes the narrow missing live SDK reload cycle for stock tactical TU
inventory. Within that scope, a supervised caller can open and inspect inventory,
use admitted visible hand/ground targets, unload and reload through ordinary
handlers, inspect refusals, preserve a held item on decline, and use the separately
verified cancellation/closing routes when their conditions are met. Those are
individually bounded milestones, not one newly rerun all-controls scenario.

This provides a practical inventory baseline for a separately scoped next tactical
action slice. It does not require a dedicated reload capability. Priming, broader
inventory modes and weapon/mod permutations remain outside this result; firing
and end-turn control remain unimplemented.

No existing native regression or source-fault matrix was rerun for this milestone.
Their qualifications remain unchanged: CLICK has 64 mapped detections, four
survivors and two inconclusive outcomes in 70 fault runs; RIGHT has 46 detections,
two survivors and two inconclusive outcomes in 50; Unload has 70 detections and
two survivors in 72. The Unload palette-fixture correction and the individual
redundant-guard sensitivity limits remain historical evidence, not new reload
passes. Successful integration does not eliminate surviving faults or complete
fault sensitivity.

No prebattle/non-TU SDK control, general mod compatibility, full controller,
full-fairness or timing acceptance, campaign completion, model execution, token
benchmark or resource-savings result is claimed.
