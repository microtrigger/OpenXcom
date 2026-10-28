# Memorial and archived soldier navigation

The Memorial route is distinct from the living personnel roster. Its structured
UI state and action grants bind to the current saved game's dead-soldier list,
the selected archived soldier, and the actual visible, freshly rendered window.
The independently authorized owned-base snapshot remains read-only and does not
authorize an action in a hidden window.

## Intended bounded route

Personnel roster → Memorial → archived Soldier Info → Diary → Back, including
ordinary previous/next navigation. The Memorial displays reverse chronological
roster order; row selection performs the native conversion exactly once. The
archived information and Diary screens retain the native reversed arrow bindings.

Archived Soldier Info permits only previous/next, Diary and Close when their
real controls are available. It never grants armor, rename, dismiss, assignment
or training changes. A death-status label is presentation data, not a mutation
capability. The living personnel path keeps its own existing contract.

Freshness, selected-object/roster identity, single-use action grants, top-state,
actual overlay and clipping checks remain mandatory. A changed/reordered roster
invalidates previous row grants. Unavailable rows are not guessed from hidden
model positions. Memorial Statistics is outside this bounded route.

## Qualification boundary

The bounded route has focused native and strict adapter qualification, plus
short synthetic live runs in both games through Memorial, archived Info, Diary,
mission detail and Back. Reverse ordering and arrow behavior are checked; reads
and navigation preserve owned state. Scroll overflow and Awards are not claimed
as live-qualified by those short runs. Statistics remains unsupported; malformed
registry construction is not qualified. The broader regression suite is not
fully passing. Populated synthetic
native/UI scenarios are not evidence of actual combat deaths or mission success.
A fixture bootstrap is default-off, operates only on a new isolated campaign,
and is not an MCP mutation action or an editor for existing saves.
