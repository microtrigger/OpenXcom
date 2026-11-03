# Architecture-review checkpoint: 3 November 2026

This is a source checkpoint before architecture simplification, **not a release**.
It preserves the current experimental native implementation and matching Python
adapter in one repository tree. Both UFO and TFTD remain the target; complete
player-equivalent coverage and full-cycle acceptance are still open.

## What is included

- Native and adapter control schema 66, including the intervening medikit,
  scanner, inventory selection, public article/craft fields, contact rows,
  native navigation, dogfight and editable-name extensions.
- Public globe zoom/rotation and a bounded rendered globe projection. These
  controls call ordinary engine handlers; they do not expose hidden world data.
- The globe-input audit-capacity correction: a full diagnostic buffer no longer
  disables gameplay commands. Input ownership/freshness and presentation guards
  remain active; diagnostic overflow is reported instead of becoming authority.
- Existing Python adapter regression sources and public contract notes. Historical
  native tests remain in the tree; their presence is not a current pass claim.

No commercial resources, campaign saves, recordings, SDK binaries, host secrets,
private runtime diagnostics or scenario-specific execution fixtures are added.
Repository-relative includes replace the development overlay's absolute include
paths. Test source lookup is relocated to this repository layout.

## Evidence and limits

The development checkpoints exercise real production providers, handlers and
model methods through explicit test fixtures. Recent strategic waiting,
contact/lost-contact, return-order and save/reload results were obtained with
substituted Game construction/run/render-loop infrastructure. They **do not**
prove an unmodified standalone game lifecycle, actual window/focus integration,
or complete UFO/TFTD gameplay. A dispatched command receipt is not itself proof
of the requested game outcome.

The ten-rotation UFO fixture regression followed by Options and Save/Load passed
in the retained development results. The old capacity failure was identified by
code analysis and synthetic checks, not observed as a standalone live-game hang.
Longer-than-32-event live overflow and TFTD live capacity coverage remain unrun.

The broad Python suite is synthetic protocol/validation coverage, not gameplay.
The publication run currently has known failures from the six added globe
controls and new mandatory globe fields not yet reflected by the older shared
fixtures/count assertions. These failures are retained, not silently skipped or
renamed as successful game tests. See the publication results below.

## Next work

Review and simplify the architecture before expanding the scenario harness:
consolidate source/version ownership, separate diagnostic recording from command
authority, unify repetitive contracts and share a small set of test infrastructure.
Preserve fog-of-war, stale-state rejection, ordinary handler execution and
non-replay of commands with unknown outcomes. Reconcile the shared adapter suite
and run the actual standalone product before claiming release readiness.

## Publication results

- PASS: repository-relative includes and test-source paths; `git diff --check`.
- FAILED: `cd tools/mcp-observer && python3 -m unittest discover -p "test_*.py"`: 1,916 methods, 1,862 passing, 39 failures, 8 errors and 7 explicit skips. The failures comprise 38 old catalog-count assertions (286 versus 292 actions) and one old Geoscape action-set assertion. The eight errors come from synthetic Geoscape observations missing the new globe contract. No failure was suppressed.
- NOT RUN: optimized Python suite after this known normal-suite failure; full standalone gameplay, actual display/focus acceptance, both-game complete cycles and GitHub CI.
- PASS: fresh full native CMake build with experimental agent control and loading-presentation helpers enabled, using GCC 14 and external SDL 1.2/yaml-cpp SDK dependencies. The initial packaging attempt failed on a missing raster-projection header; the exact existing dependency was included and the final complete build passed. The executable was not launched. OpenGL was unavailable, so this build uses the supported non-OpenGL path.

No release readiness or complete gameplay coverage is claimed.
