<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Combined official-SDK → production-game smoke

Date: 2026-10-12. Production source: `34de1647b3fa63906f2ab0d20fe62a78c7bec16b`.
See the [aggregate evidence](combined-sdk-live-evidence.json) and the earlier
[selection](SUPERVISED-SELECTION-EVIDENCE.md),
[right-click](SUPERVISED-TURN-EVIDENCE.md),
[left-click](SUPERVISED-MAP-LEFT-CLICK-EVIDENCE.md) and
[inventory-opening](SUPERVISED-INVENTORY-OPEN-EVIDENCE.md) evidence.

## Result and scope

One complete observed selection → turn → visible movement → inventory opening/view
→ manual close → fresh tactical view chain passes in each game: UFO attempt 01
and TFTD attempt 04. These are real official-SDK sessions through the production
launcher and nongate game, rather than synthetic transport peers or native gate
fixtures. No production source or executable changed for these runs.

All five attempts are retained. TFTD attempt 01 selected another soldier at its
left-click target; attempts 02 and 03 showed no visible movement. They are not
counted as movement passes. Completing transport calls does not establish the
requested gameplay outcome, and these separate attempts are not action replays.

The bounded result adds actual combined integration evidence to the earlier
individual slices. It does not close the original full Stage 5, full controller,
campaign, timing or model-token comparison gates.

## Explicit sequence and evidence boundary

Official Python MCP SDK **1.29.0**, protocol **2025-11-25**, uses its real
`ClientSession` over a reviewed custom nonterminating stdio transport. Each step
requires one explicit command file. The driver rejects queued commands and permits
only one proposal of each action type per run. It does not choose targets, approve
dialogs, poll action status automatically, retry proposals or query hidden game,
path or save state. Grants and command IDs stay in memory; only the newest
validated observation grant is used, including after an ordinary epoch change.

The complete chain has 14 recorded tool calls followed by one explicit driver stop:

1. Read a fresh selected-soldier panel (`read_stance_context`, O).
2. Propose ordinary next-unit selection once.
3. After local confirmation, explicitly read its dispatch status.
4. Read a fresh tactical image (`read_tactical_context`, V).
5. Propose one ordinary map right-click at a chosen screen pixel.
6. After local confirmation, explicitly read its dispatch status.
7. Read the mandatory fresh V and inspect the visible turn outcome.
8. Propose one ordinary map left-click at a chosen screen pixel.
9. After local confirmation, explicitly read its dispatch status.
10. Read the mandatory fresh V and inspect the actual left-click outcome.
11. Propose ordinary inventory opening once, using that fresh view.
12. After local confirmation, explicitly read its dispatch status.
13. Read the mandatory capless inventory image (`read_inventory_view`, I).
14. After ordinary manual closing, read a fresh V.
15. Stop observation explicitly, without sending a game termination signal.

The local Y confirmations, derivation of choices from preceding images, manual
closing gestures and post-stop game/window observations are operator-attested
from visible UI use. The archived received inventory and subsequent tactical PNGs
corroborate those endpoints, but there are no independent stored UI-event captures
or post-stop screenshots for these gestures and lifecycle claims. Stop receipts
correctly retain `game_liveness: unverified`; operator observation separately
reports each game remained visible afterwards and was closed normally.

The independent read-only audit inspected the original returned pixels in all
five runs and checked every numbered receipt against the aggregate and processed
command shape. All 26 PNGs match their recorded byte counts and SHA-256 hashes and
pass the reviewed driver's PNG-format checks. Every action has one pending receipt
and an explicit latest-ID-matched dispatched receipt, followed by its required
fresh observation. The archive contains no failed tool result or duplicate
proposal. Raw IDs/grants were intentionally not retained: matching is recorded by
the reviewed driver, rather than reconstructible from raw tokens in the archive.
Hashes establish integrity; visible sprite displacement establishes movement.

## Per-attempt outcomes

| Attempt | Visible result | Combined movement chain |
| --- | --- | --- |
| UFO 01 | Giuseppe Martini → Hugo Dupre; turn changes orientation and TU 56 → 52; left-click visibly moves Hugo relative to craft/terrain, TU 52 → 38, energy 43 → 36; Hugo inventory TU 38 and tactical return | Pass |
| TFTD 01 | Hugo Garcia → Toma Krasimirov; turn TU 64 → 63; left-click selects Jaroslav Bosansky, TU 58, energy 54; Jaroslav inventory and tactical return | Not demonstrated: selection instead of movement |
| TFTD 02 | Hugo → Toma; turn TU 64 → 62; left-click leaves visible position and TU 62 unchanged; energy 65 throughout | Not demonstrated; stopped before inventory |
| TFTD 03 | Previous selection gives Rostislav Slezak; TU 69 and energy 77 unchanged; no turn-orientation change or movement established | Not demonstrated; stopped before inventory |
| TFTD 04 | Hugo → Toma; turn TU 64 → 60; left-click visibly moves Toma, TU 60 → 52, energy 65 → 61; Toma inventory TU 52 and tactical return | Pass |

For UFO 01, the right-click target was (144, 103), and the left-click target was
(192, 72). The claim is displacement relative to fixed craft/terrain features;
no ground level, destination terrain type or hidden route is inferred from the
projected view.

TFTD 03 also retains a nonideal setup event: the first ordinarily generated battle
ended automatically when the game reported that all aliens had been killed in
the submarine crash, before any MCP observation or gameplay action request.
The operator returned through normal UI and generated a second battle in the
same session. This operator-attested setup event is not an additional SDK attempt
or a tool failure; it does not change the subsequent recorded no-movement outcome.

For TFTD 04, ordinary Crew UI setup reduced the assigned crew from 14 to two,
retaining Hugo Garcia and Toma Krasimirov, without fixture or save editing. Those
setup steps are operator-attested; the returned tactical images corroborate the
much emptier cabin. Both right- and left-click targets were (208, 72). Toma visibly
moves relative to the cabin floor/walls and the other soldier, rather than merely
changing selection, TU or image hash. The inventory and post-close tactical views
retain Toma and TU 52; the tactical view retains energy 61.

UFO 01 and TFTD 01/04 each contain 14 tool calls, six images and explicit stop 15.
TFTD 02/03 each contain ten tool calls, four images and stop 11. Across all attempts:
**62 recorded tool calls, 18 action proposals, 26 images and five explicit stops**.
These are protocol counts, excluding initialization/tool listing; they are not
performance or resource measurements. Each complete chain contains four locally
confirmed action proposals. The three nonmovement outcomes remain in the totals
and in the aggregate; no cause is inferred from undisplayed game state.

## Provenance

- Production source revision: `34de1647b3fa63906f2ab0d20fe62a78c7bec16b`.
- Executable SHA-256: `dda22420cd7134f127e458aa42bb72eb61ece1983a9d91dde42b7efbfc908b6b`.
- Reviewed driver SHA-256: `b9d3ae58cefb17bb6da4a7043220f41d5773647b16130d5d46aebbd87147d75a`.
- Inspected production build metadata: Release, all five supervised features ON,
  inspected native-gate options OFF. Launch scripts target the production launcher
  and that on-disk executable. This is provenance consistency, not a fresh rebuild
  or an independently captured runtime executable attestation.
- Relevant source SHA-256 values are recorded in the aggregate and match the
  preceding inventory milestone. The private driver passed 19 pure checks normally
  and 19 with Python optimization; those preparation checks are separate from the
  real-game evidence above.

This publication adds documentation and sanitized metadata only. The driver and
raw run archive remain unpublished. Images, game resources, saves, binaries,
private paths and command tokens are excluded. Hashes alone do not reproduce or
independently expose the private run. New documentation/metadata inherit
GPL-3.0-or-later; existing upstream and third-party notices remain unchanged.

## Still open

- Broader terrain, doors, interruption/recovery and completed reaction outcomes;
  one successful movement chain per game does not establish general reliability.
- Inventory item manipulation and automated closing; both remain manual.
- Full hidden-state non-disclosure, residual timing channels, full controller and
  campaign acceptance, firing, end-turn control and safe sequential batches.
- Actual model-token, latency and resource comparisons. No model or token
  benchmark ran, and no efficiency or fairness conclusion is drawn.

The combined runs did not exercise stance changes or audio validation. Earlier
native parity, mutation and independent-slice evidence retains its own scope;
this smoke is not a new controlled UI-parity gate.
