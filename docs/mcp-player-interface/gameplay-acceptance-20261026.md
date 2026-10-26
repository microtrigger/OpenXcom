# UFO and TFTD gameplay acceptance — 2026-10-26

## Observed route and outcome

A disposable Beginner UFO campaign followed ordinary interception and landing-site
dispatch into a real ground assault. Scouting, visible contact, deliberate firing,
opponent turns and the final tactical transition used engine-backed player-visible
observations and native MCP actions. The mission continued across saved sessions
without rolling back losses. Contact disappearance alone was not treated as a
confirmed kill; ammunition changes during opponent turns were labelled inferred
reaction fire rather than explicit shot events.

The native interface reached **UFO is recovered**, with an **OK** rating.
The displayed debrief reported **five aliens killed**, **two X-COM operatives
killed** and **141 total points**. The campaign continuation was acknowledged
through native controls and a new campaign save completed.

## Mixed reporting, not an entirely pure-MCP pass

Two bounded, read-only visual recoveries were needed after tactical completion:
the debrief table was unavailable through MCP, and the subsequent casualty screen
did not expose its displayed rows. Each inspection was followed by the corresponding
native acknowledgement. Neither inspection supplied tactical input.

The battle used MCP actions, but the entire run is therefore classified as
**mixed MCP/visual reporting**, not pure MCP. Passive video/audio covered the
reported final session. Saving and normal application closure did not reverse
the outcome.

## Subsequent repair and qualification

The report-row correction is documented in
[debrief-visible-tables.md](debrief-visible-tables.md). It reads current painted
logical widget captions rather than reconstructing statistics or querying hidden
Soldier/Diary state. Freshness, viewport and real-overlay guards remain required.

Focused checks passed: 88 native drawn-cell checks, 39 debrief-provider checks,
61 casualty constructor/provider/handler checks, 52 normal Python tests and
30 optimized Python tests. The executable build passed. These are isolated native
widget/provider and adapter-contract checks with explicit domain seams, not a
live replay of the completed mission or a clean full-suite claim.

At the end of that UFO run, the repaired report projection had not yet been
live-qualified in another completed mission. The later TFTD qualification below
does not retroactively make the earlier UFO reporting route pure MCP.


## Later TFTD mission and live report qualification

A separate disposable Beginner TFTD campaign reached a real alien-submarine
assault. The mission continued from the latest newly saved state between bounded
sessions, without replaying the battle or reversing losses. Player-visible
structured map observations drove scouting, contact, deliberate firing and
inventory reloading through native MCP actions; opponent turns were allowed to
complete.

On 2026-10-26, the native debrief reported **ALIEN SUB is recovered**, **OK**,
**four aliens killed**, **one X-COM operative killed** and **101 total points**.
Its stats, recovery and total tables were readable through the repaired MCP
projection. The following promotions and lost-in-service rows were also readable
and acknowledged natively. This is live qualification of the corrected debrief
and casualty-table path in a newly completed TFTD mission, not a replay or an
inference from save contents. Campaign Geoscape returned and a **new campaign
save** was created and verified in the native load list.

Passive video/audio recording was verified before gameplay in each successful
session. A bounded read-only visual recovery was used at initial campaign startup
when the first projection was incomplete; normal native observations then resumed.
Therefore the complete campaign route remains **mixed MCP/visual observation**.
The two subsequent battle-continuation sessions, including the final report-table
reads and campaign save, required no visual gameplay fallback. Native desktop
launch and normal application closure are distinct from gameplay controls.

Transient empty transition projections were resolved by fresh read-only
observations, not repeated inputs. Inventory targets were re-resolved after
mutation; they are snapshot-local handles, not permanent item identities.
Hidden-state filters, freshness and real-overlay guards remained in place.

This adds one end-to-end TFTD mission to the observed UFO result. It is not a
full-suite pass, exhaustive campaign qualification, or coverage of every mission
type. No game resources, saves or private diagnostics accompany this document.
