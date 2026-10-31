# Offscreen CRAFT article provider test

This source-only test extracts the exact ArticleState registration, paint and observation methods from the supplied checkout and compiles them with that checkout's real Text/Font/Surface/AgentUi modules. It uses SDL_Init(0), explicitly checks that video/audio are uninitialized, and never launches the game or opens a window. Unexpected sound execution throws.

Usage:

```
sh run.sh /path/to/checkout/src /path/to/sdk/usr /path/to/legally-owned-data /path/to/new-build-directory
```

Requires a C++11 compiler, Python3, SDL1.2/SDL_image/SDL_gfx/SDL_mixer and yaml-cpp SDK libraries/headers. The supplied SDK layout uses `include` and `lib/x86_64-linux-gnu`. Data must contain the standard OpenXcom font YAML/resources and the ordinary UFO/TFTD crafts, ufopaedia and en-US language rules. No resources are distributed by this test; no download is performed. The output directory must not already exist.

Checks cover stock Interceptor/Skyranger/Barracuda/Triton geometries and seven exact logical text lines; non-Craft registration null; no unrelated ITEM/table DTO fields; stable/changed/restored-text binding; dirty/hidden/clip/offscreen/modal/wrong-top/full-destination guards; sibling and external overlay on blank allocation edges; soft-wrap and oversized-text behavior; unchanged full-canvas 8-bit pixel indices before/after registration. It asserts the exact public-fork protocol identifier.

Scope: whole observation/paint methods execute unchanged, but State/Game/Screen lifecycle and article constructors/model setup are fixture seams. Native font/rendering and widget implementations are real. The fixture generates article Text from ordinary rules and language solely as test input; production observation reads only certified painted Text. Background artwork and full game constructors/dispatch/socket/live campaign are not covered. State::blit and Game overlay methods are extracted exactly from the supplied checkout. Do not infer full player-equivalent or all-localization/layout support.
