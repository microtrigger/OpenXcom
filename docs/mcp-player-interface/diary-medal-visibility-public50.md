# Diary medal visibility: plain public50 candidate

Base commit: `5ab560ef15211d599b9f6a49162dc64696bf6236`.
Wire identity remains `openxcom-agent-control/50`; use this branch's unchanged
matching adapter. This branch contains only the Diary visibility correction and
its source-only regression harness. It does not contain the separate CRAFT
extension or the later Names66 development.

The Diary creates medal surfaces for the Awards view. Clearing those surfaces
while leaving them visible can obscure otherwise painted text in Combat and
Missions. During `init()`, the correction sets visibility of both medal surface
arrays to whether the current display is `DIARY_COMMENDATIONS`. The ordinary
Awards path restores visibility. The change adds exactly two statements to
`SoldierDiaryPerformanceState.cpp`; it changes no observation fields, actions,
protocol version, adapter, generic visibility guard or model data projection.

## Qualification

The retained qualification for these exact C++ bytes records a fresh offscreen
native harness with 556 assertions and 16 rendered provider records, plus a full
repository CMake configure/build with 330 translation units. That build reported
24 standard-library deprecation warnings. The baseline and fixed adapter suites
each recorded 1701 passing tests and seven skipped native integration checks in
both normal and optimized Python. Twelve native-output boundary methods passed
in both Python modes using synthetic transport envelopes.

The source-only harness is in
`tests/agent-control/diary-medal-visibility/`. Its README describes the required
external SDK and legally available stock resources, explicit fixture seams and
output-directory constraints. No resources, SDK, saves, executable, private
diagnostic logs or host configuration are added by this change.

On z2-ai, the publication checkout was reconstructed from the pinned Git base
and compared with the retained source snapshot. Python regressions and harness
syntax/extraction were checked again. Native compilation was not repeated:
no installed C++ compiler or CMake was available. The earlier native/build
results are retained evidence, not new execution on z2-ai.

Live public50 gameplay, native socket/controller dispatch, full Awards gameplay,
production adoption and complete player-equivalent coverage remain unverified.
The earlier portable harness attempt with successful inner assertions but a
nonzero outer shell exit remains a failed attempt; the later successful frozen
run does not erase it. Live results from local63 do not qualify this branch.
