# Visible craft destinations: schema 49

`choose_visible_craft_point` is available only in the native craft destination
selection screen. It accepts an integer screen point advertised in the current
visible front-globe sample set, with a fresh observation/capability. It invokes
the normal globe click and destination confirmation; it does not create a
waypoint directly or accept hidden world coordinates. Cancel keeps the craft's
previous assignment; OK performs the normal launch/redirect.

Both UFO and TFTD were exercised on isolated copies of existing test campaigns:
select a ready interceptor, choose a visible point, cancel the provisional
confirmation, choose again and confirm, inspect the resulting visible waypoint,
read its actual name and pursuing craft, rename it through the native editor,
open/cancel interception, and close to Geoscape. UFO used INTERCEPTOR-1; TFTD
used BARRACUDA-1. All gameplay inputs used MCP. One read-only UFO screenshot
investigated a transient unpainted interception screen; a subsequent fresh MCP
read settled without visual input. This diagnostic remains in the evidence.

Native tests exercise fresh grants, visible projection/domain bounds, native
callbacks, cancellation and cross-screen transitions, with explicit fixture
seams. These tests are not live campaigns. A locally rejected attempt to finish
an already-committed name editor sent no game action; subsequent reads confirmed
the committed name. No unknown game action was replayed.

Passive recordings retain real SDL_mixer postmix audio. The earlier global
integer-rate clock fit rejected the longer recording at its 150 ms bound.
Callback-anchored piecewise resampling retains that bound, checks continuity,
and does not fill gaps or insert silence. Audio evidence is a digital alignment
check, not a physical speaker measurement.

This stage does not establish full player-equivalent coverage. Graph modes,
remaining ordinary event combinations and the explicitly deferred rare/final
campaign scenarios remain separate work. No GitHub CI run is implied by local
build, regression or live results.
