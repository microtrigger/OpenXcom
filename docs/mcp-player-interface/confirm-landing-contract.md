# Landing confirmation: explicit Yes and No

`landing_confirmation` has exact visible fields `message`, `prompt` (nullable
current logical captions), and `buttons` (admitted `yes`/`no` in UI order, with
actual labels). Both actions take empty arguments; neither is selected automatically.

- `decline_landing_return_to_base` invokes native No: request craft return, then
  close this popup. This is **not Cancel**, arrival proof, or a no-mutation dismissal.
- `accept_landing_begin_mission` invokes native Yes: pop the confirmation, attach
  a new battle game, run the ordinary generator and, if it returns, push Briefing.
  The callback can mutate before throwing. Dispatch is not generation success or
  Briefing readiness; an uncertain result must never be replayed.

Text comes only from current `_txtMessage`/`_txtBegin`, including the prompt color
token, not reconstructed craft/target names. No world texture/shade, coordinates,
race, deployment, target model data or hidden eligibility is projected. Yes binds
privately to the constructor-selected destination pointer and refuses a different
or null current destination. This is selected-operand freshness, not an eligibility
oracle; it is not a proof against same-address object-lifetime ABA. No remains
independent of that Yes-only binding.

Current top/modal ownership, presentation/text revisions, current destination
paint, viewport allocation and actual later-widget/FPS occlusion remain required.
Logical captions do not require glyph/ink/fit completeness or pointer readability;
each button still requires its ordinary action-overlay guard. Resource limits
are protocol bounds, not layout tests. Native `init` can remove the popup if the
craft is already returning to its own base; removed states cannot authorize actions.

A known receipt establishes only dispatch. Read the actual continuation. Existing
`continue_briefing` may lead through Inventory/NextTurn or an ordinary alternative;
`close_debriefing` can enqueue other reports, storage/containment or save screens,
not a guaranteed Geoscape. Shared capability freshness, ledger and unknown-stop
rules are unchanged.

Native fixtures extract literal constructor/provider/init/Yes/No with actual SDL
widgets and explicit Game/Mod/Craft/Target/generator/Briefing/render seams. A
mutation-before-generator-exception case is separate from the Python synthetic
unavailable-after-send case; neither proves real exception propagation end to end.
Real tactical generation, campaign battle/debrief continuation and live coverage
remain separate qualification. Free-globe destination selection is a separate gate.
