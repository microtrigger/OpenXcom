# Scanner presentation contract — schema 53

`scanner` offers only `close_scanner` with empty arguments and the fixed semantic
label `Close scanner`. It invokes the ordinary exit handler; the receipt does
not establish a successor screen, refund TU, or repeat scanner use.

Visible fields are exactly:

- `viewport`: null or `{x,y,width,height}` within the 640×400 logical canvas;
  width/height are 1–152. Run coordinates are local to this viewport.
- `palette`: at most 63 unique RGB triplets (integer channels 0–255), containing
  only colors used in the presented pixels, numbered by first appearance.
- `pixels`: fixed row-major string of exactly width×height characters (at most
  23104). Alphabet `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/`:
  `A` is transparent/omitted; characters 1–63 select palette entries 0–62.
  This is a direct character raster, not base64 bytes or run-length encoding.
- `content_status`: `bounded` or `unavailable`.

The raster is sparse final-surviving opaque ScannerView ink, including the native
direction arrow when visible. Static background/grid is deliberately omitted.
It provides no unit identities, factions, Z/world coordinates, movement points,
classification or hidden multiplicity. Do not interpret an empty bounded raster
as absence of units or lack of activity. Unavailable content is unknown and has
empty palette and empty pixel string; an independently known viewport and close control may remain.
Overflow is unavailable, not silent truncation. There is no `truncated` field.

The native provider owns final-frame, foreign-overlay and animation freshness
proof. Adapter fixtures validate structure, closed fields, canonical palette ordering,
bounds, packet/result/event compatibility and capability freshness; they do not
prove native visibility or a real campaign action. Scalar booleans are not valid
coordinates, dimensions, palette indices or RGB channels.

The broader project remains open. Purchase price nulls and unrelated caption
providers are not fixed or qualified by this Scanner contract.

The initial 1024-run candidate failed an ordinary dense native case and was
replaced before acceptance. The fixed raster admits full 152×152 coverage without
a run-count ceiling. A palette overflow is still unavailable, never truncation.
