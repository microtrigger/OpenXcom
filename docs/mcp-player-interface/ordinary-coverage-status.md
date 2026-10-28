# Ordinary player-interface coverage

The goal remains the full player-equivalent interface for both UFO and TFTD.
An implemented action, a native test and a live scenario are distinct claims.
Earlier mission evidence remains valid without implying universal coverage.

| Area | Implemented contract | Remaining scope |
|---|---|---|
| Tactical play | Player-visible tactical actions and reserve controls. | Current-contract and unusual scenario qualification remain separate. |
| Owned-base information | Read-only snapshots of all owned bases, independent of the selected window. | Do not claim exhaustive serialization; reads never grant hidden-window actions. |
| Infrastructure | Facility selection, preview, placement, bounded dismantling, second-base creation and construction-completion routes. | Other layouts, facility types and destructive branches remain distinct. |
| Base management | Sell/Sack, projects, personnel, craft, transfer/arrival, Monthly Costs and containment routes. | Additional own-history UI routes require implementation and qualification. |
| Campaign events | Research, manufacture, construction and arrival routes, with bounded modal coverage. | Complete event chains and shortage variants remain separate. |
| Late/rare branches | Existing contracts are retained. | Base defense/destruction, campaign-final and rare matrices remain deferred. |

See [dismantling](facility-dismantle-contract.md),
[construction completion](construction-completion-contract.md),
[transfer and arrival](transfer-arrival-contract.md),
[Monthly Costs](monthly-costs-contract.md), and
[containment management](containment-contract.md) for their bounded contracts.
Own-base reads are never a substitute for visible action authority.

[Freshness fixture repair](freshness-fixture-repair.md) does not waive the
remaining regression backlog. The current full adapter regression suite passes with seven explicit integration
skips; skipped artifact-dependent checks are not passes, and local test success
does not imply a CI run.

Containment has bounded synthetic live acceptance, not evidence of a natural
battle capture. The [Diary Overview/Performance route](diary-contract.md) has bounded local
empty-history live qualification; populated native reports remain distinct
from populated live history. Combat, Awards, mission details and Memorial have separate implemented contracts
and bounded qualifications. Stock-hidden Awards and broader sprite/tooltip
coverage remain distinct. Ongoing-campaign Memorial Statistics now exposes
painted viewport rows, native scrolling and ordinary OK; both games have live
qualification across all 28 report rows. Ended-campaign teardown is not covered
by that ordinary-campaign qualification. Debrief STATS/SCORE and real re-equip
shortage paths are separately qualified; debrief overflow remains native-only.
The full project is not complete.

Visible globe marker inspection now routes through the native target click and
chooser; bounded own-base/craft/overlap/contact live cases are recorded for the
two games. TargetInfo editing/followers remains native-only pending an ordinary
live opportunity. [Target and audio qualification](target-and-audio-qualification.md)
distinguishes these cases from complete globe coverage. Future passive recordings
use real continuously retained postmix sound and explicit callback-clock/video
alignment gates. Earlier mute recordings have not been retroactively repaired.
