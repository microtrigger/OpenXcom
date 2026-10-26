# UFO gameplay acceptance — 2026-10-26

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

This establishes the observed UFO mission outcome, campaign return and new save.
It does not qualify every campaign branch or TFTD. The repaired report projection
has not yet been live-qualified in another completed mission.
