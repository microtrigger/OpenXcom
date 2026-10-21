# Bounded public dogfight controls

This milestone extends the contact → craft selection → confirmation route with
one **already-open, expanded** dogfight window. It does not create encounters,
select hidden targets, accelerate combat invisibly, or modify combat rules.

## Composite ownership

DogfightState is drawn by GeoscapeState, not pushed on the top-level state stack.
The strategic provider delegates only when there is exactly one dogfight and no
queued dogfight or queued popup. The child verifies its own visible expanded
presentation. Modal, preview, minimized, ambiguous multiple-window, stale-paint
and clipped presentations do not grant combat control. The strategic HUD is not
exposed behind an unsupported combat window.

## Actions and public data

`dogfight` offers `choose_dogfight_mode` with the closed choices `standoff`,
`cautious`, `standard`, `aggressive`, and `disengage`. Choice IDs identify known
public icon controls; they are not enemy identifiers. Adapter arguments use
`{"mode":"standoff"}` (or another currently offered mode). Selection goes through
ordinary ImageButton radio-group selection and ordinary combat handlers, not a
callback-only shortcut. Selecting disengage is a request to retreat under normal
game mechanics, not an immediate forced return to base.

Displayed status, distance, ammunition and interception-number texts are sourced
from the actual current UI using the existing generic `agentTextJson` helper.
Its existing generic text-admission restrictions remain: a text field can be
`null` even when the mode controls are available. The separate article-body
layout-text contract is not broadened to every Text widget by this change.
No raw UFO health, hit probability, destruction flag,
or unseen outcome is returned. Each offered button’s selected flag describes its currently painted radio
selection. If the selected button is not admitted (for example it is clipped),
the offered subset may contain no `selected: true`; that means the current mode
is unavailable, not that the game has no selected mode or defaults to standoff. Disappearance of the window does not prove victory, destruction or a
completed return to base; later public UI must establish the next state.

Craft damage is a graphical silhouette, not a displayed numeric percentage.
This bounded milestone reports `damage: null`: it does **not** return the model's
precise health percentage as if it were shown text. Graphical damage telemetry
requires a separate qualified projection. No new glyph-pixel readability gate
is introduced as a condition for recognizing combat-mode icons.

## Remaining route qualification

Native provider/controller tests use explicit widget/game seams and are not live
UFO/TFTD campaign evidence. Full contact → attack → disengage/outcome → return
needs separate real-game qualification. Multiple simultaneous or minimized
windows, UFO preview, individual weapon toggles and graphical damage remain out
of this first bounded control scope. Existing visible status may describe an
outcome while the window remains open; it must be reported verbatim, without
inferring hidden destruction flags or treating a successful action receipt as a
successful attack.

## Public return check using existing controls

After the combat window closes, `open_bases` → `open_crafts` exposes the ordinary
craft status column. Ordinary `CraftsState::lstCraftsClick` refuses to open craft
info while the craft is `STR_OUT`; that refusal must not be bypassed. After
ordinary travel and a freshly displayed returned status, `select_craft` opens
`craft_info`, whose existing provider exposes the actual rendered Damage%, fuel,
and ammunition text. This is a viable public post-return damage check, not a
substitute for graphical in-combat damage telemetry. Return timing, damage and
outcome still need real UFO/TFTD route evidence.

## Current source/native qualification

- Release full build with experimental agent control enabled; changed UI files
  also pass feature-disabled syntax checks.
- Adapter suite: 1,224 tests run, two pre-existing skips.
- Closed native protocol: 48 checks, normal and ASan/UBSan.
- Dedicated provider/controller fixture: 209 checks, normal and ASan/UBSan,
  using real SDL ImageButton/Surface/Text with actual font PNGs. Combat domain,
  owning Game and range-helper seams are explicit; no game window is created.
- Matrix includes all five mode handlers; zero/one/two/null weapon slots;
  ordinary refusals; radio selection before callback; unknown/hidden controls;
  an offered subset with no selected button; nullable text; destination clipping;
  Text X/Y and hide/restore freshness; preview/minimized/modal/queued/multiple
  windows. Text and ImageButton presentation generations are retained along with
  text-content revisions, so geometry/visibility restoration is not accepted
  without repaint.

The fixture's unregistered FileMap INFO messages are not missing raster errors:
FileMap returns the original relative font path, and SDL loads it under the
configured common-data working directory. Recorded asset hashes identify those
inputs. These native results do not qualify real-time socket-action latency,
combat outcomes, graphical damage, or an end-to-end campaign route.
