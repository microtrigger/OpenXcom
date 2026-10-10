<!-- SPDX-License-Identifier: GPL-3.0-or-later. New project documentation, 2026-10-10. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Request-bound displayed HUD prototype

The explicit `--presented-hud` launcher option selects presentation protocol `/2`
and enables `read_presented_hud({})`. Its separate public schema is
`openxcom-mcp-presented-hud/1`. The response contains exactly four displayed
numbers (`time_units`, `energy`, `health`, `morale`) or one generic `null` HUD.
The numbers are bound to the same epoch and challenge as the successfully
presented cursor pixels. They describe that displayed frame, not live simulation
state or an instantaneous world snapshot.

## Compatibility and scope

Without the option, the existing `/1` handshake and response remain unchanged.
The 96-byte request and admission ordering are unchanged in both versions.
Protocol `/2` adds only a nullable HUD field to the same presentation envelope.
`read_presented_cursor` keeps its previous public schema and omits that additional
field. Historical HUD and cursor files keep their previous unverified semantics;
no historical value is substituted into a presentation response.

No new game-state query, entity list, terrain access, gameplay input, controller,
forced rendering, simulation step, FOV update or game-RNG operation is added.
The existing maximum of four presentation requests per second remains shared by
both presentation tools. No new connection, listener or persistent registration
is introduced.

## Visual proof

Only the actual top Battlescape state's normal blit can arm the four approved
NumberText widgets. Each actual widget blit can supply its UI rectangle and
foreground palette index. It supplies no numeric value, digit count, unit pointer
or simulation data. Hidden, temporarily hidden, bordered, cropped, pending-redraw,
offscreen and unsupported-size widgets supply no candidate. Candidates are
cleared before each normal composition and display reset; interrupted requests
retain the existing fail-closed transport behavior.

After successful final `SDL_Flip`, the decoder reads the four bounded rectangles
from the supported final software display surface. Every scaled pixel must agree
with its logical pixel's foreground/nonforeground classification. Complete 3×5
NumberText glyphs, holes, interdigit spacing and all trailing pixels are decoded
using the fixed rendering alphabet. A whole blank trailing glyph cell is required.
Only canonical numbers 0–999 are supported. For example, a 15-pixel widget showing
`9999` and the same widget clipping `99990` are indistinguishable and both return
null. Internal numeric values are never read to decide availability.

Any unproven field makes the entire HUD null. Top-state overlays supply no HUD;
intersecting composed cursor/FPS rectangles invalidate it. Focus, palette,
software-renderer, integer-scale and successful-presentation restrictions are
inherited from the cursor proof. Foreground/background palette collapse, partial
glyphs, malformed digit shapes and incomplete trailing cells fail closed. As with
the cursor prototype, this does not establish physical display visibility or
freedom from desktop-window occlusion.

## Verification and limits

Synthetic tests cover all ten glyphs, ordinary one/two/three-digit numbers,
clipped-number equivalence, leading zeros, extra trailing ink, missing ink,
spacing, contrast collapse, offscreen/missing candidates and integer scales 1–6.
Source-boundary tests tie the alphabet to the real NumberText renderer, check
actual-blit provenance and final-flip ordering, and exclude numeric-widget and
simulation queries from the new path. These structural tests do not replace
runtime tests of the actual UI.

Protocol tests cover version isolation, exact four-field shape, strict numeric
bounds, null HUD, request/epoch checks, existing cursor-schema compatibility and
absence of historical fallback. The opt-in and default-OFF builds pass; the OFF
binary contains no observer symbols or activation strings. All 125 Python tests
and the historical and both-version presentation SDK checks pass.

### Selected UFO/TFTD runtime checks

[Build and runtime evidence](presented-hud-build-evidence.json) records 14 successful
`/2` reads, seven per game. Menu, overlay and cursor overlap yielded null HUD.
Battle values were UFO 58/62/33/100 and TFTD 55/61/38/100 in field order
TU/energy/health/morale. Kneeling changed TU to 54/51 respectively, with the other
three fields unchanged; dismissing the overlay restored those displayed numbers.
Reload restored TU 58/55. The producer epoch remained unchanged within each game,
and every successful request used a distinct challenge. TFTD pointer exit caused
generic unavailability; returning the pointer did not revive the closed channel
or return an old HUD.

An additional initial UFO menu request returned generic unavailability, with no
established cause. An explicit relaunch preceded the seven successful UFO reads.
This failure is retained in the evidence rather than classified as a passing case.

Validated response crops were compared with separately sampled game screenshots:
nine matched exactly; UFO kneeling differed in 882 RGB bytes and TFTD battle in
45 bytes. Those two crops contained animated map content, but the exact causes
were not established. Other captures were not compared. These results establish
neither simultaneous screenshot equivalence nor full-frame proof. Screenshots,
raw packets, assets and saves are excluded from publication.

Native sources were frozen during runtime checks. Only documentation and metadata
changed afterward; the games were not rerun for those edits. No complete
noninterference, timing, platform or gameplay-controller certification, or
end-to-end resource saving, is claimed.
