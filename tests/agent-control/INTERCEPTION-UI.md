# Interception public-UI milestone

Supported route:

`ufo_contact/open_contact_interception` → `intercept_craft/choose_visible_row`
→ `interception_confirm/confirm_interception` or cancellation.

Only the currently rendered contact button and freshly projected craft rows grant
authority. The craft list is offered only with an existing target; no free-globe
target selection is introduced. Confirmation projects its actual displayed target,
not model coordinates, identifiers, enemy statistics or hidden eligibility flags.
Confirmation also remains usable for an already visibly selected target reached
through ordinary UI; it never accepts a target ID or creates an arbitrary target.

Craft selection invokes the ordinary eligibility callback. A receipt may therefore
mean refusal/no transition. Confirmation assigns the destination and launch status
through the ordinary callback; it does not prove arrival, combat or victory.
Cancel follows ordinary window/waypoint ownership rules. Center-on-contact remains
unsupported. List cells require complete painted text; clipped rows are omitted,
not reconstructed from craft metadata. Four-column selection is an explicit new
interception-only helper; existing generic list authority is unchanged.

All new providers retain destination-clip and text/button/list revisions across
blit, reject incomplete/currently clipped destinations, and enforce top-state,
modal, overlay and disjoint-region checks. Native tests use explicitly documented
seams; real UFO/TFTD qualification is separate.

## Remaining dogfight work

Dogfights are composite windows painted by Geoscape, not normal top states. Mode
controls are ImageButtons whose radio-group assignment occurs before their ordinary
callbacks. Calling those callbacks alone would break parity (including disengage).
A following milestone needs real image/selected-pixel witnesses, ordinary radio-path
semantic dispatch, and ownership/occlusion checks across dogfight windows.

Suggested bounded next scope is one expanded dogfight with visible status/ammo/
distance and standoff/disengage controls. Concurrent or minimized windows, UFO
preview, attack modes and weapon toggles need separately qualified control coverage.
Window disappearance alone cannot prove an outcome; never project internal crash,
destruction, pursuit or break-off flags as observations.

## Live setup and proof boundary

Use an isolated ordinary campaign and a naturally detected UFO/USO. Record the
rendered contact, craft list, target confirmation, cancellation and confirmed launch
with fresh MCP observations plus screenshots. Do not inject a UFO, edit saves or
read concealed runtime state to manufacture the route. Existing mission/other popup
interruptions remain ordinary outcomes, not successful interception proof.

## Reproducible checks

Use fresh output directories outside the checkout. For example:

```sh
python tests/agent-control/run-interception-providers.py --output /tmp/interception-providers-new
python tests/agent-control/run-interception-providers.py --sanitize --output /tmp/interception-providers-asan-new
python tests/agent-control/run-interception-list-pixels.py --sdk /path/to/sdk/usr --data /path/to/data --output /tmp/interception-list-new
python tests/agent-control/run-completion-interception-regression.py --sdk /path/to/sdk/usr --data /path/to/data --output /tmp/interception-contact-new
(cd tools/mcp-observer && python -m unittest test_agent_interception)
```

Both offscreen widget runners accept `--sanitize`. The targeted completion runner
covers contact normal/redisplay/hyperwave variants in UFO and TFTD, not the entire
historical completion fixture. The latter has unrelated historical arrivals-layout
assumptions. Existing architecture snapshot tests also contain obsolete protocol
versions/history references; compare their failure set against the baseline rather
than interpreting those unchanged failures as new acceptance.
