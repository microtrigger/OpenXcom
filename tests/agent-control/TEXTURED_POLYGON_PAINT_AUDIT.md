# Empty textured polygons and the agent paint audit

The experimental agent connection closes when an audited raster operation fails.
SDL_gfx `texturedPolygon`, however, returns a negative result for some completely
clipped polygons. A real interception reproduced the connection closure at this
primitive. The primitive identification alone does not establish the exact live
polygon coordinates or clip rectangle.

The correction is confined to `OPENXCOM_EXPERIMENTAL_AGENT_CONTROL`. Before this
primitive, a valid polygon/texture with an axis-aligned bounding box disjoint
from the actual destination clip (or an empty clip) is recognized as a no-op.
An intersecting polygon still uses the original SDL_gfx call and paint audit;
invalid vertex counts do too. No negative raster result is generally ignored.
The non-agent build retains its original call. Existing projectile-write
bookkeeping and all other audit/transport guards remain unchanged.

This is raster return-value bookkeeping, not a runtime text-readability gate.
It exports no game state and changes no combat rules, action authority, or
visible UI text contract.

## Reproduction

`run-textured-polygon-tests.py` compiles the real `Surface.cpp`, agent paint audit,
and SDL_gfx. It uses synthetic 8-bit surfaces, not a game or stock artwork.
Sixteen cases compare all destination bytes against an ordinary SDL_gfx draw:
inside, partially clipped, fully outside, half-open clip boundaries, custom and
empty clips, degenerate geometry, and signed-coordinate extremes. An invalid
two-vertex polygon must still make the audit unhealthy.

Run with `--source`, `--sdk` (the SDK `usr` directory), and a fresh `--output`.
`--sanitize` adds ASan/UBSan except vptr (unrelated UI types are not linked). To reproduce the old failure, use the unchanged
baseline as `--source`, the corrected checkout as `--fixture-source`, and
`--expect-empty-clip-failure`; only the exact expected assertion/exit is accepted.
Each result records source hashes and verifies that its inputs stayed unchanged.

These tests do not qualify a live dogfight. Real UFO/TFTD interception must
separately establish whether this bounded correction handles the encountered
case and lets the existing public-control route continue.
