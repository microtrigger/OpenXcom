# Visible second-base creation and multi-base reads

UI schema 35 adds `open_new_base` from the actual Basescape Build New Base
button, `cancel_new_base` from the non-first-base site-selection window,
`confirm_new_base` and `cancel_new_base_confirmation` from the shown cost/area
confirmation, and `place_access_lift_cell` from the actual lift-placement grid.
The first four take empty arguments; lift placement uses exact integer `x,y`
cell coordinates 0–5 and a fresh offered native click center. The existing
`place_base_site` and base-name editing/confirmation remain separate actions.

`build_base` now includes shown `buttons`: empty for the initial base, with a
Cancel button only when the ordinary additional-base window exposes it.
`confirm_new_base` contains the shown `cost`, `area` and buttons, without an
invented title or hidden numeric cost. `place_lift` contains shown `title`,
current grid geometry/cells and an empty buttons array: the native lift window
has no Cancel button. This bounded lift route requires a non-initial base and
a stock-compatible one-cell access lift. Custom initial-base layouts and
arbitrary modded footprints are not covered by it.

Every action runs through its corresponding actually open visible native UI
handler with freshness and real-overlay guards. Site selection only accepts a
fresh advertised globe sample; native terrain/rule validation remains native.
The lift action uses ordinary BaseView hover/click with a real sender, not a
direct facility mutation. Its authority is the visible click center, not pixel
purity of decorative grid borders touched by the ordinary title.

## Important purchase boundary

Before cost confirmation, the temporary base is not part of the saved campaign.
Outer Cancel discards it; cost-confirmation Cancel returns to site selection.
**Cost OK immediately charges funds and inserts the base into SavedGame**, then
opens the name editor. It is not a harmless preview. Naming and lift placement
must be completed afterward; no refund or post-purchase Cancel is promised.
Insufficient funds follows the ordinary error handler without insertion.

Read-only `read_player_bases` may observe the newly purchased empty base before
its name/lift are complete. Its identity must persist through those steps.
These reads never grant mutation authority or require opening every base.
Navigation-only comparisons distinguish changing campaign clock fields from
owned-base state; they do not assert that Geoscape time has stopped.

Native checks pass: 84 actual-model cost/name/lift/collector checks and 69
site-provider checks, with frozen inputs. The former uses real SavedGame,
region costs, name editing and BaseView/lift logic with explicit owning/resource
seams. The latter uses actual widgets and literal provider bodies but explicit
Globe/sample/site-selection seams; it does not prove real globe eligibility.
Protocol, strict native-to-adapter checks and the production build pass.

Short live UFO and TFTD campaigns each qualify outer Cancel and cost Cancel
without owned-state/funds changes, then explicitly purchase a second base at
the shown cost and finish its name and one access lift. The temporary base is
absent from snapshots before purchase; the purchased empty base is present
before naming and keeps the same ID through name/lift completion. The first
base remains unchanged. Both bases are read directly from each Basescape and
Geoscape with stable owned-state contents/IDs; the advancing campaign clock is
not conflated with a base mutation. Each run uses MCP gameplay and passive video
without visual fallback. This is live two-base evidence, not only a fixture.

Selected 242 adapter tests pass normally and with Python optimization. Full
1,571-test discovery retains the same 14 failures, 16 errors and seven skips
and the same failure/error IDs as baseline `2e73510`. Other globe navigation,
eight-base limits, custom initial layouts and the full modded/rare matrix remain
separate acceptance; the full interface is not declared complete.
