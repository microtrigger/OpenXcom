# Multiwindow dogfight — wire60 migration

`dogfight.visible` is exactly `{windows, geoscape}`. There are1–4 windows in
current native paint order, with canonical local `w0`…`w3` handles. Each window:
`handle`, `bounds:{x,y,width,height}`, `minimized`, nullable printed `status`,
`distance`, `ammo1`, `ammo2`, `interception`, `damage:null`, existing ordered mode
`buttons:[{id,selected}]`, and boolean `can_minimize/can_restore`.

Bounds are logical pixels within640×400. A handle routes a control within this
one fresh grant; it is not a craft ID, target ID, hidden roster or persistent
order. No craft names, model values, faction, health percentages or outcomes are
added. Minimized windows have null expanded telemetry, no modes and no minimize
grant. Expanded windows have null interception number and no restore grant.
Native paint/sibling-overlap/identity binding must independently certify them.

Explicit breaking migration: `choose_dogfight_mode` now requires
`{window:"wN",mode:"…"}` even with one window. Native command suffix is `wN mode`.
New `minimize_dogfight`/`restore_dogfight` take only `{window:"wN"}`. Only the exact
window/mode or minimize/restore flag advertised by the fresh read is usable.
Callbacks may warn, no-op or show a native TFTD depth/airborne error; dispatch is
not minimize/restore success, combat outcome, target destruction or survival.

`geoscape` is null whenever any window is expanded. When all are minimized it
may expose only `{clock,speed,speeds,buttons}`; clock has the8 nullable printed
hour/minute/second/weekday/day/month/year/funds strings. Speeds retain native
local handles/labels/selection; buttons contain only actual Options. Existing
`choose_geoscape_speed` and `open_pause_menu` may be offered on that proof. Full
background Geoscape controls are not claimed by this limited subset.

Fresh actor/window roster identity, reflow, reorder, removal/replacement and
visibility invalidate old grants natively; client capabilities add zero-send
stale and per-window target checks. Exact duplicate commands query the original
receipt and never dispatch twice. Uncertain mutation is never retried as new.
Native multiwindow and live battle proof remain separate from adapter tests.
