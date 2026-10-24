# Low-fuel notification (experimental MCP)

Additive schema 26 state `low_fuel` exposes only the currently open UI's
`title`, `message` (nullable strings), and `buttons` (`id`, `label`). It does
not expose craft fuel, fuel limits, coordinates, destinations, or model status.
Logical captions/labels are not gated on glyph ink, character completeness,
or text fitting its allocation. Active ownership, visible widget allocation,
current paint/revisions, modal state, and actual occlusion remain checked.
The pointer is not a caption-readability gate; control hitboxes retain the
existing action-overlay checks.

| Button ID | Action (empty arguments) | Ordinary handler effect |
|---|---|---|
| `ok` | `acknowledge_low_fuel` | Pop this notification; leave selected speed unchanged. |
| `ok_5_seconds` | `acknowledge_low_fuel_5_seconds` | Select 5 Seconds via `timerReset`, then pop this notification. |

Use a fresh `read_ui_state` authority and only its offered action. There is no
automatic choice or retry after an unknown outcome. Existing single-use command
and stale-frame rules apply. Other states do not admit these actions.

**Returning to base is not an MCP effect:** ordinary Geoscape fuel processing
has already called `returnToBase()` before constructing this popup. Popup
dismissal proves neither arrival nor successful return. The two acknowledgments
must not be described as return-to-base commands.

Tests separate actual extracted constructor/provider/callback bodies and native
widgets from explicit Game/Mod/Craft/Geoscape fixture seams. They are not a live
encounter, full game-loop or pixel-appearance qualification. A natural live
low-fuel occurrence and post-dismissal return remain unqualified by this change.
