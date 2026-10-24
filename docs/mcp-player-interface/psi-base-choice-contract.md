# Psi Training: current native base-button choices

The `psi_training` DTO retains title, read-only bases captions and OK buttons, and
adds `rows` with up to eight `{handle, cells:[caption]}` entries. These are currently
painted actionable native button choices, not soldier records or world base IDs.
Handles r0..r7 enumerate only admitted controls within the fresh grant. Duplicate
and empty captions are allowed; no name-based identity or selection is inferred.
Read-only captions can remain when cursor ownership prevents the corresponding
button action. No hidden eligibility, capacity, statistics, total-base count or
unavailable-control reasons are exposed.

`open_psi_training_base` uses existing `visible_row` argument grammar: exactly
`{"row":"rN"}` from the current grant. Its fixed `visible_control_semantics`
label is `Open psi training base`, not invented visible text. The adapter captures
only current displayed choice handles and reuses existing grant/ledger checks.
No arbitrary index, coordinate, world identifier or automatic retry is accepted.

Native ownership snapshots the constructor's ordered button/base pointer pairing.
Current vectors must still exactly match that private snapshot even after repaint;
no SavedGame enumeration, current lab eligibility query or model name lookup is
performed. Existing frame binding includes widget identity/revisions/geometry and
admitted-row mapping, preventing a hidden earlier button from retargeting stale r0.
Top/modal/current paint/full viewport and later-widget/cursor/FPS hitbox guards
remain. Logical captions have no glyph, pixel-readability or fit gate.

Dispatch builds an ordinary mouse Action with the selected button as sender and
calls unchanged `btnBaseXClick`. That callback pushes AllocatePsiTrainingState for
the existing paired base; it does not pop the parent, toggle training or advance
time. The allocation constructor's ordinary visible capacity/Unknown projection
is separate from selecting a base. Existing allocation OK recalculates stat strings
and pops back; it is not rollback. No assignment/training completion is implied.

Qualification distinguishes new menu callback/paired-base push capture seams from
prior actual allocation-constructor fixtures. Prior unchanged constructor/Unknown
coverage may be bound by exact dependency hashes; it is not a new end-to-end
navigation run. No live, lawful Psi-lab setup or training outcome is established.
