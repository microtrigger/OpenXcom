# Monthly report: visible text and normal acknowledgement

Schema26 additively adds `monthly_report` and `acknowledge_monthly_report`:
157 actions,68 semantic states. The action takes empty arguments.

The exact visible object contains nullable current logical UI strings
`title`, `month`, `rating`, `income`, `maintenance`, `balance`, `description`,
`failure`, and `buttons`. No scores, funding, countries, hidden ending state or
continuation eligibility are recomputed or exposed from the model. Each string
retains the existing1024-byte resource bound; null is not an inferred value.

Only the actually admitted normal report button (`ok`) offers acknowledgement.
If the failure text or large failure-panel button is visible, no normal action
is offered, even if another button has the same label. The large button's
terminal lose-game continuation is explicitly NOT implemented here. Admission
does not query private `_gameOver` or funds. The failure panel remains observable
without a semantic action. Never automatically acknowledge a second screen.

The ordinary native handler is not a generic Close. Its normal branch pops the
report, awards monthly service and may queue commendations, psi training and
saving. Its first defeat acknowledgement instead reveals the failure panel.
Do not claim Geoscape return, successful funding or continued campaign from a
dispatch receipt; record the actual next state. Existing game logic remains
unchanged. First-month ordinary report qualification does not cover every
continuation, defeat or financial calculation.

Active top/modal, current full paint, widget revisions, allocation bounds,
later-widget occlusion and control ownership remain guarded. Logical text is
not gated by glyph ink, line count, selected font, character completeness or
fitting. Native title17px and metric9px allocations share a final spare row;
their fixed structural observation strips are16px and8px respectively, solely
for those named native allocations. Description/failure use full allocations.
Full widget viewport/presentation checks still apply. Cursor overlap alone
does not suppress text; actual control overlap prevents affected actions.

Fresh authority is required. Exact duplicate commands query receipts without
replay; unknown outcomes stop. Native fixtures use literal constructor,
provider, init and callback bodies with native widgets and explicit financial,
Game/domain and continuation-constructor seams. They do not qualify the real
monthly economy or full-game render loop. Synthetic adapter tests are not live
acceptance. Test failures and their qualification boundaries remain separate.

Actual destination selection is still OPEN/BLOCKED on a fog-equivalent design;
this independent report provider neither implements it nor changes its status.
