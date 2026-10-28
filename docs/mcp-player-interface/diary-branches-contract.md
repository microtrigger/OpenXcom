# Visible diary branches

UI schema 40 retains the living-soldier diary with ordinary Combat and Awards
navigation and a bounded mission-detail route. The prior overview/performance
contract remains distinct from these additional branches.

`open_diary_combat`, `open_diary_performance` and `open_diary_awards` use the
actual shown buttons. Performance tabs use normal button press/release/click
handling so the native radio selection changes with the report. The current
report includes its mode, nullable headings and description, visible rows and
totals. Awards descriptions mean whatever is currently shown; no new tooltip
hover action or medal-sprite identity serialization is implied.

The overview can offer `select_diary_mission` only for a fresh shown row with
an unobscured click target and a valid native history-row mapping. Its opaque
row handle is local to that observation, not a persistent mission ID. A clipped
caption may remain null without hiding an otherwise visible clickable row.
Selection invokes the ordinary list hover/click with the actual widget sender.

`soldier_diary_mission` describes the currently displayed mission texts and
three-cell report rows. Previous/Next/OK remain ordinary native buttons.
Private identity witnesses prevent a changed history list or record mapping
from reusing a previous painted grant; these identifiers are not exposed.
The existing injury lookup uses a non-inserting lookup with the same displayed
default, rather than modifying history merely to read a missing entry.

All actions retain visible-window, current-paint, freshness and real-overlay
checks. This is player-known historical UI, not a tactical enemy-state graph.
Independent owned-history reads do not authorize these actions. Empty live
history cannot qualify mission details or populated awards. The legacy direct
malformed-history constructor path is not silently declared repaired by the
MCP mapping checks.

Empty-history Combat/Performance live routes are locally qualified in both
games. Stock rules without commendations hide Awards; populated Awards and
mission details have separate native-fixture qualification, not populated live
acceptance. The no-record caption now ignores only its own currently painted, demonstrably
empty transparent list allocation; other overlays and stale state still block it. Native test
launchers are maintained separately on the tools/native-test-runner-20261028
branch; consult its dependency and integration status before use. Full regression
and the complete player-equivalent interface remain open.

The scoped empty-list correction is native-fixture qualified in both games,
including overlay, stale-paint and added-row negatives. It is not a new populated
live-history acceptance claim.

Archived-soldier routes use explicit `memorial_soldier_diary` and
`memorial_soldier_diary_performance` states under the separate Memorial contract.
They retain native reverse-order arrow dispatch and current dead-roster identity
guards rather than weakening the living-base authority check.
