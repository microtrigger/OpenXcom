# Tactical input context

Every battlescape `visible` object includes required `input_context` alongside `tactical`. It describes current own-player input controls, not hidden AI orders, weapon identity, targeting validity, movement eligibility or future effects.

- `mode`: `normal`, `targeting`, `busy`, or `unavailable`.
- `selected_action`: normal mode is exactly `none`; targeting is `snap`, `aimed`, `auto`, `throw`, `launch`, `psi_panic`, `psi_control`, `use`, or `other`.
- `cursor`: normal/targeting modes use `normal`, `aim`, `throw`, `waypoint`, `psi`, `none`, or `other`.
- Busy/unavailable always have null action and cursor. Non-map tactical presentation always has unavailable context. Debug, foreign actor, non-player-turn and other native authority gates do not expose hidden input values.

The native safe enum mapping emits `other` for unexpected targeting actions/cursors; it does not expose raw enum integers. No action/cursor consistency beyond this contract is invented by the client. The field is included in the native control authority binding. Old battlescape payloads missing it fail closed.

`tactical.mode == map` means a map viewport is presented, **not** that a primary map action means movement. A retained targeting mode can fire again after a shot. Read `input_context` before choosing a map action. Even normal mode does not guarantee movement. A secondary action can remove a preview/waypoint rather than leave targeting; obtain a fresh read after every cancel and inspect the new context. A dispatched receipt is not proof of a shot, hit, movement or completed cancellation.

The current private player driver exports the whole public visible object and compares fresh semantics before map dispatch; input-context changes therefore invalidate the old choice without a driver runtime change. It must not reconstruct mode from screenshot pixels, selected weapon names, hidden state or an earlier receipt.

`test_agent_input_context` has strict schema/hidden-presentation negatives. Its native integration method requires `NATIVE_INPUT_CONTEXT_PAYLOADS` pointing to a JSON array of actual native helper output; absence explicitly skips only that method and is not native integration qualification. Native fixture/domain seams and synthetic transport envelope limits remain separate from live gameplay evidence.

## Scoped qualification

The implementation was qualified with 22 normal and 22 ASan/UBSan native checks of literal projection/cursor/cancel/post-shot bodies with explicit synthetic dependencies, plus four normal and four optimized Python/driver methods consuming 16 actual emitted helper contexts. This is not a whole-renderer or live-context qualification. No live run using this new field is claimed here. The prior full aggregate remains NOT PASS; these focused checks do not replace that history.
