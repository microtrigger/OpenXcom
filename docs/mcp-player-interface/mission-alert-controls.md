# Mission-alert Intercept and Center controls

A real UFO campaign alert exposed Intercept and Center labels but only offered
Cancel through MCP. This increment connects the two already-visible controls:

- `intercept_mission_alert {}` invokes the unchanged native `btnInterceptClick`:
  reset to five-second time, center the globe on the existing alert site, and
  open native craft selection. It does not automatically dispatch a craft.
- `center_mission_alert {}` invokes the unchanged native `btnCenterClick`:
  reset time, center the existing site, and dismiss the alert.
- `dismiss_mission_alert {}` remains the ordinary Cancel behavior.

No site identifier, coordinates, faction, deployment, hidden targeting options,
or additional fields are exposed. Button labels and action authority share the
existing completed-paint, coverage, overlay, modal, top-state, revision and
freshness guards. Arguments are empty; targets cannot be supplied by the caller.
The wire schema and visible payload are unchanged; the action catalog adds two
entries. Use a matching producer/client build: older strict clients do not know
the added actions.

## Verification scope

`run-mission-alert-tests.py` extracts current native projection, dispatcher and
callbacks and runs them through the retained controller with explicit model and
owning-state seams, real offscreen widgets, fonts and palettes. It checks ordinary
callback effects, stale authority, failed paint, hidden buttons and duplicate
receipts. These seams do not prove a real InterceptState constructor or gameplay
transition. `--master` scopes available stock resource coverage explicitly.
Python research-completion tests cover button/action atomicity, canonical order,
empty arguments, forbidden target fields and wrong-state rejection. Live
verification is recorded separately; native/synthetic checks alone are not a
complete campaign or all-UFO/TFTD acceptance claim.

## Scoped result, 2026-10-26

- UFO native fixture: **142 checks passed**, including exact existing mission
  binding, timer reset/centering, Intercept push, Center/Cancel pop, invisible
  controls, freshness, real overlay denial, failed-paint denial and duplicate
  receipts. The model/owning seams remain explicit; TFTD resources were not
  present, so this is not TFTD native acceptance.
- Focused Python contract: **34 normal + 34 optimized passed**. Full native
  incremental build passed. Broad Python baseline and candidate each ran 1,495
  tests with the same seven failures, 19 errors and seven skips; full suite is
  **not** passing.
- Short real UFO continuation V19 offered both new actions. Executing
  `center_mission_alert {}` returned to Geoscape at five-second speed and the
  campaign was saved in a new slot. **Intercept was not executed live**; its
  callback is covered only by the scoped native fixture.

Earlier fixture failures are retained privately: outdated audio-event seams,
missing playback linkage, a symlink-resolved asset working directory and an
obsolete cursor-as-occluder expectation. The dedicated harness now uses the real
FPS-overlay seam and initializes stock-font resource lookup; ordinary cursor
visibility is not treated as an occluding window.
