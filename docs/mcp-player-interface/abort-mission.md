# Ordinary Abort Mission UI — agent-control/42

The ordinary Battlescape Abort control and its confirmation dialog are supported
by three no-argument actions (`args: {}`):

- `open_abort_mission`: invokes the visible toolbar's normal `btnAbortClick`.
  Opening the dialog does **not** confirm an abort.
- `confirm_abort_mission`: invokes the fresh visible OK button's `btnOkClick`.
- `cancel_abort_mission`: invokes the fresh visible Cancel button's `btnCancelClick`.

Confirm and cancel are offered only on `state: "abort_mission"`. Confirmation is
a separate explicit action; never automatically follow opening with confirmation.
The ordinary game rules still apply: confirming can end a mission, but a TFTD
exit may instead advance a stage. No debriefing destination, survival, recovery,
or mission-success outcome is promised by dispatch.

## Visible payload

The exact keys are `prompt`, `entrance`, `exit`, `outside`, and `buttons`.
The four caption fields are nullable displayed Text strings, bounded to 1024
UTF-8 bytes by the adapter. They are not reconstructed counters, private model
fields, unit lists, or eligibility predictions. Null means unavailable displayed
text, not zero or absence of units.

`buttons` is an ordered subset of:

```json
[{"id":"ok","label":"OK"},{"id":"cancel","label":"Cancel"}]
```

Labels are the actual displayed localized captions, not fixed English values.
Button entries have exactly `id` and nonempty `label`. A confirmation/cancel
action requires its corresponding visible button and the same label with
`label_source: "visible_text"` and `argument_kind: "none"`. The toolbar opener
uses `visible_control_semantics`. No extra arguments or payload fields are
accepted.

## Authority and evidence

Existing painted-widget admission, top-state guards, freshness, single-use
capabilities, command receipts, and uncertainty handling remain unchanged.
A read or visible caption does not authorize a hidden button. Obtain a fresh
state before each new action. A dispatch receipt proves handler dispatch only;
inspect its post-action observation and a fresh read to establish the actual
result. Never blindly retry a mutating action after an uncertain outcome.

`tools/mcp-observer/test_agent_abort_mission.py` checks the exact nullable shape,
caption/label validation, button ordering, screen scoping, malformed arguments,
and synthetic socket freshness/single-use behavior. These checks are not a live
game qualification or proof of abort/next-stage outcomes.
