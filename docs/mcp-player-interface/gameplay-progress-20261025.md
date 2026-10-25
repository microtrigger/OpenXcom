# Gameplay and playback-observation progress — 2026-10-25

## Observed gameplay

The continued UFO tactical mission reached the native **UFO is recovered**
debrief with an **OK** rating. MCP acknowledged the subsequent Lost in Service
screen, returned to the Geoscape and created a new campaign save. This was a
continuation across several runs, not proof of one uninterrupted new-campaign
route. The final run used no gameplay screenshots or GUI actions; technical
cleanup was separate. Individual casualty identities were not inferred from
missing selectable soldiers or unexposed memorial rows.

The input-context fix exposes current player targeting/action/cursor state;
`tactical.mode = map` alone does not mean a primary click will move. The ordinary
Geoscape interception entry and targetless craft-selection branch are available,
with native no-op, modal, stale and constructor-mode distinctions retained.

## Accepted playback observation

A short subsequent campaign/UI run verified delivery of opaque accepted-playback
records and the same content ID before and after a same-process load. A tentative
operator-declared UI-sound association persisted; this is not a native sound-class
label or proof of direct model listening. Observed camera values were null and
position effects were zero: that run did not qualify tactical camera context,
directional localization or mute behavior. An options proposal was refused before
dispatch; it was not counted as a settings test.

Playback mode reports Sound playback requests accepted with positive gains, not
guaranteed physical audibility. Existing native visibility gates remain unchanged.
It exports no source unit, race, world coordinates, filename or kill/stun result.
Content IDs survive load only within the same process; restart creates unrelated
IDs. Audio reset conservatively makes observation unavailable until restart.
Queue/capacity gaps and current availability are explicit. Music/direct mixer/FLC
paths are outside this observer. See [sound-playback.md](sound-playback.md).

The alternative bounded postmix PCM transport remains available outside playback
mode. Earlier waveform-classification experiments and their incomplete action-window
coverage are not evidence of comprehensive acoustic recognition; they are not the
primary playback consumer and no licensed templates are included here.

## Read-only presentation contract correction

A later short run stopped after an event-read validation failure, before the
proposed target action was dispatched. Source analysis and a saved public native
projection reproduced two incorrect action-presence requirements in read-only
presentation validation (craft destination targets and psi-training rows). The
correction removes only those event-context requirements; fresh live UI validation
retains both authority checks. No action can be authorized from event history.

The failed event packet was not retained. The deterministic reproduction therefore
does not establish its exact contents or prove the historical failure cause.
Two focused checks passed. A subsequent short live run passed the corrected target
presentation and target-selection path, entered a new landing-site battle and
reached turn 3. Three soldiers moved outside; two shots consumed ammunition
20 → 19 → 18. A contact subsequently disappearing is not proof of a hit or kill.
Retained targeting and explicit cancellation back to normal input context were
observed. The mission was still in progress, not won. A new tactical save was completed
through MCP at turn 3.

AI-turn accepted-playback records and mixer pan parameters were delivered; full
tactical camera-context coverage remains unqualified. A private display formatter
size limit was found; gameplay continued using the public observations embedded
in the decision view, without falsely acknowledging an undisplayed formatted view.
That private formatter correction is separate from this public contract fix.
See [presentation-authority.md](presentation-authority.md).

## Qualification boundary

The playback increment passed 32 focused native checks in normal and scoped
ASan/UBSan configurations (without LeakSanitizer), two focused Python checks, a
full native build and CLI readback. Native fixtures use synthetic mixer/entropy
seams; the UI run provides only the separate live evidence described above.
**The full aggregate suite remains NOT PASS / not requalified by this increment.**
No blanket campaign, tactical, audio-recognition or all-mission readiness claim
follows from these results. Further short live sessions and fixes are tracked
separately; an ongoing session is not preemptively reported as successful.
