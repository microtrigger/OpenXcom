# Initial CraftPatrol OK-only contract (experimental MCP)

Historical incremental scope: the button/action set below is extended by the
[bounded navigation contract](craft-patrol-navigation-contract.md).

Schema 26 adds `craft_patrol` and one empty-argument action,
`acknowledge_craft_patrol`. The initial OK-only increment had 150 actions/64 states; see the subsequent
[bounded navigation contract](craft-patrol-navigation-contract.md) for current additions.

The exact visible object is `{message, patrolling, buttons}`. The two nullable
strings come only from the active popup's current logical Text widgets.
`buttons` contains only the admitted `ok` control and its current label.
The original OK-only increment omitted Redirect. The subsequent bounded
navigation contract exposes Redirect and a safe close only; this is not full
navigation or craft-status support.

OK invokes the existing handler once, closing the popup. It does not set a
destination, start patrol, change time speed or prove later movement. The game
constructs the reached-destination caption before clearing the destination;
the provider never reconstructs that caption from current craft/target data.

Active-screen/modal, current paint, widget/text revisions, allocation geometry,
later-widget/FPS occlusion and action-control ownership checks remain enforced.
Glyph ink, character completeness, caption width overflow and cursor overlap
are not logical-caption readability gates. Actual control overlap still affects
OK admission. Private binding identities are not serialized as model values.

Use fresh authority, explicit choice and empty arguments. A dispatch receipt
is not proof of a resulting screen. Read the fresh observation; never replay
after an uncertain outcome. Unsupported transitions remain unsupported.

Qualification is separate: native extracted constructor/provider/OK-handler
tests use real widgets with explicit Game/domain/overlay seams, not a full game
or live flight. Adapter tests use synthetic native-channel fixtures. SelectDestination navigation remains deferred; the subsequent contract scopes
the limited GeoscapeCraft close path.
