# Visible soldier diary navigation

UI schema 39 supports the living soldier's actual Diary overview, Combat,
Performance, Awards and individual mission report windows when the native UI
makes them available. Stock rules without commendations hide Awards; the adapter
does not manufacture that button or grant its action.

`open_soldier_diary`, `open_diary_combat`, `open_diary_performance` and
`open_diary_awards` use ordinary visible controls. The performance state's
`mode` distinguishes `missions`, `combat` and `awards`. Reports provide shown
headings, cells and totals; absent or genuinely clipped captions remain null.
Previous/Next and OK retain the native selected-soldier return behavior.

An actually shown selectable overview row may expose a fresh opaque `project_row`
handle for `select_diary_mission`. Native mission reports support
`previous_diary_mission`, `next_diary_mission` and `close_diary_mission` when
available. Private row bindings validate the current own-history identity and
record mapping before dispatch; history replacement or reordering invalidates
old authority. The injury read does not insert a missing map entry.

Every action requires the corresponding actual open visible window, current
paint and viewport, fresh revisions/capability, and clear real action targets.
Clipped text alone does not remove an otherwise visible selectable native row.
The independent own-base/history snapshot grants no UI action authority and
never exposes current tactical enemy state through these history reports.

Empty-history live routes are locally qualified in both games. Populated Combat,
Awards and mission reports are checked separately in native fixtures, not claimed
as populated live qualification. Awards tooltip interaction, all medal sprite
semantics, Memorial, and every caption are not included. The empty mission no-record caption is projected when its own overlapping
list is currently painted and demonstrably empty; foreign overlays and stale
list state remain blocking. Full player-equivalent coverage and
the regression backlog remain open.

The scoped empty-list correction is native-fixture qualified in both games,
including overlay, stale-paint and added-row negatives. It is not a new populated
live-history acceptance claim.
