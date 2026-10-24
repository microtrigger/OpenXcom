# Basescape current painted name

The existing nullable `basescape.base_name` now observes the actual painted
TextEdit child, including native nonmodal character preview. It never falls
back to the Base model, stored editor value or reconstructed glyphs. No public
fields, actions, editing rules or native navigation callbacks are added/changed.

Two explicitly named Basescape wrappers reuse the bounded default-off capture
also used by BaseInfo. Its full-widget paint-to-current-destination, viewport,
identity, clip/dirty/revision and actual-draw epoch checks remain unchanged.
BaseInfo provider and capture algorithm are unchanged; old tests are not new
Basescape integration evidence.

Stock Basescape allocates 127×17 at (193,0); the later location widget begins at
y=16. After validating the **full 17-pixel** editor allocation, name observation
uses a fixed 16-pixel strip for exactly17-high allocations, excluding the native
spare bottom row. Other heights use their full allocation. This structural rule
never depends on fonts, text width, glyph presence or line count. Private
geometry authority still binds the full editor allocation, not the shorter strip.

Later controls and noncursor/FPS overlays covering the retained strip make the
name null; a spare-row-only overlay does not. Cosmetic cursor/caret readability
is not a name gate. The name is not an outward action region. Existing buttons,
base slots, cursor/FPS authority bindings and modal/top-state guards remain.
A null name does not authorize a hidden identity fallback for flow verification.

Current native qualification requires literal constructor, init and setBase,
with real text widgets and explicit Game/Mod/domain/BaseView/Mini seams. Native
initialization may select a base and populate labels; projection must not repeat
those model reads/mutations. No actual arrival/transfer, facility/slot navigation,
renaming, full game or live-flow claim follows from those seams. Versioned proof
records current results and limitations separately.
