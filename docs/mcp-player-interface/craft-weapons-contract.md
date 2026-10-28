# Visible craft armament controls

The `craft_info` surface offers `open_craft_weapon_1` and
`open_craft_weapon_2` only for currently available painted weapon-slot buttons.
These invoke the ordinary native slot handlers. A craft with no available slot
does not gain an action merely because an owned-base snapshot describes it.

The `craft_weapons` modal projects its displayed title, three column headings,
painted armament rows, native scroll availability and Cancel button. Row handles
are observation-local, not rule identifiers. Nullable or blank cells preserve
native presentation; the ordinary NONE row does not invent quantities.

`select_craft_weapon` accepts a currently offered `outfitting_row` handle.
`cancel_craft_weapons` invokes native Cancel, and `scroll_visible_list` uses the
currently offered direction. Selection goes through the native list handler;
it is not a remote mutation of a craft or inventory object. Fresh presentation,
current craft/slot and row authority are required, including after reopening.

Removing an installed weapon returns its launcher and native loaded-clip amount
to base stock. Selecting a launcher consumes it and creates the native unarmed
rearming weapon. A READY craft may become REARMING; this does not restore its
previous ammunition or promise immediate readiness. The available list is
rebuilt when the modal is reopened, so a just-returned launcher can then appear.
Cancel leaves native equipment unchanged.

The interface applies to both UFO and TFTD rules. Native, adapter and live
qualification are distinct; support for this route does not establish every
loadout, rearming-shortage branch, mod layout or full-project completion.
