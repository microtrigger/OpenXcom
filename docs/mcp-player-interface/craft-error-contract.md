# Craft maintenance warning acknowledgements

Schema26 additively adds `craft_error`, `acknowledge_craft_error` and
`acknowledge_craft_error_5_seconds`:156 actions,67 semantic states.

The exact visible object is `{message,buttons}`. `message` is the nullable
current logical `_txtMessage` value; it is not reconstructed from craft, base,
fuel, ammunition or storage state. The two native controls are `ok` and
`ok_5_seconds`, with their current labels. Both actions take empty arguments.

`acknowledge_craft_error` invokes native OK: one popup close, prior speed
unchanged. `acknowledge_craft_error_5_seconds` invokes native OK–5 seconds:
`timerReset()` and one close. No choice is automatic. The normal maintenance
attempt occurred before the warning was queued. Neither acknowledgement
rearms, refuels, repairs, supplies items or proves maintenance success.

The initial scenario is rearming shortage in each stock game. Refuelling
shortage is a separate pending trigger; sharing a provider is not live coverage.
Ordinary setup can equip an empty weapon, remove available clips via Sell, then
advance to the hourly maintenance tick. Such setup must be verified through UI,
not injected saves or private model reads.

Active top/modal, current complete paint, widget revisions, allocation bounds,
genuine later-widget occlusion and action ownership stay guarded. The current
logical message is not gated by glyph ink, character completeness, line count,
font choice or fitting. Cursor overlap alone does not suppress the message;
control overlap still prevents the affected action. No game layout or
maintenance rule is changed.

Use fresh authority for each action. A dispatch receipt does not prove the next
screen or maintenance outcome; read the actual resulting UI. A queued event may
replace Geoscape. Exact duplicates only query a receipt. Uncertain outcomes
stop without replay. Native fixtures use literal constructor/provider/callback
bodies and native widgets with explicit Game/domain/render seams; synthetic
adapter tests are not live or full-game qualification.

Actual destination selection remains a separate OPEN/BLOCKED design item:
Cancel-only navigation is not retargeting, and no hidden-dependent map-click
admission is introduced by this independent warning provider.
