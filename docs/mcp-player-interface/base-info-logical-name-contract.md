# BaseInfo rendered-child name observation

This screen-local read-only change keeps the existing `base_info` schema and
all existing actions. `base_name` is a nullable, at most 1024-byte UTF-8 string.
A known empty caption is `""`. It comes exclusively from the current painted
TextEdit child, including native keyboard-off character preview when present.
It is not reconstructed from the Base model, TextEdit stored value, or glyphs.

The separate default-off TextEdit helper is enabled only by BaseInfo. It keeps
actual paint-to-current-screen, full viewport, child identity, dirty state,
text/presentation/input revisions, structural allocation and private actual-draw
epoch checks. An actual redraw changes the epoch; cached blits do not. Overflow
fails closed. Hidden, clipped, stale, oversized, unpainted or wrongly targeted
content never falls back to the model. Failure yields a null name while other
independently valid existing controls can remain available.

Name observation uses structural later-widget and noncursor/FPS overlay checks.
A cosmetic cursor or caret is not a glyph-readability test. The name allocation
is not an actionable region. Existing button and base-slot cursor/hitbox guards,
top-state/modal checks, editing width rules and native callbacks are unchanged.
No new rename or navigation action is introduced. Other BaseInfo fields retain
their existing policies. Basescape name observation is a separate scope.

Qualification must use the current literal constructor **and init**, real
TextEdit drawing, and explicitly documented Game/Mod/domain/Mini seams. Historical
BaseInfo tests are comparison only: the recovered old implementation read the
stored editor value and did not cover native rendered preview. Native constructor
seams do not establish a full game, MiniBaseView navigation, rename or live test.
Results and limitations are recorded in the separate versioned qualification.
