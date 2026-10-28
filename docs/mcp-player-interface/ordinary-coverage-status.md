# Ordinary player-interface coverage

The goal remains the full player-equivalent interface for both UFO and TFTD.
An implemented action, a passing native fixture and a completed live scenario
are separate claims. The older Geoscape inventory is a dated source audit, not
an authoritative statement that later providers are still missing.

| Area | Established bounded coverage | Important remaining scope |
|---|---|---|
| Tactical play | Historical full-mission campaign-return/save evidence remains valid; recent reserve None/Snap/Aimed/Auto and kneel-reserve slices have separate short live qualification. | Current-contract and unusual scenario qualification must be recorded independently; historical success is neither erased nor promoted to every branch. |
| Owned-base information | Read-only structured snapshots independent of the selected window, including personnel, stock, craft, projects, transfers and bounded own-history records. | Do not call this an exhaustive serialization of every engine field. Direct reads never grant actions against hidden windows. |
| Infrastructure | Visible facility list/preview/Cancel, one ordinary General Stores construction and bounded non-lift removal in both games; second-base cost/name/lift and direct live two-base reads also qualified. Natural General Stores completion and return to the correct base now pass in both games. | Queued/multi-cell construction, eight-base/custom-layout variants and the full dismantling matrix remain distinct. |
| Base management | Bounded Sell/Sack, project, personnel and craft routes exist. One-item inter-base transfer, confirmation Cancel, natural arrival and destination navigation now pass in both games. | Containment management and additional own-history/expense UI routes need source-specific audit and qualification; a snapshot is not their UI implementation. |
| Campaign events | Research and manufacturing completion have prior bounded live acceptance; natural facility completion now has separate UFO/TFTD acceptance. One-item transfer arrival also has bounded acceptance. Several later modal contracts exist. | Resource-shortage and additional arrival variants and complete event chains need separate acceptance. Provider presence alone does not prove the chain. |
| Late/rare branches | Existing contracts and historical evidence are retained. | Base defense/destruction, campaign-final and rare matrices are not the next routine test merely to increase a count. |

The ordinary functional gap addressed after placement is the native base
facility left-click and `DismantleFacilityState` confirmation route. Its bounded
UFO/TFTD acceptance cancels confirmation without mutation, then explicitly
confirms removal of one non-lift test structure and checks native economic and
layout results; see [dismantling](facility-dismantle-contract.md). Access-lift confirmation can delete an entire base and must not
be silently included in that qualification.

Natural construction acceptance uses ten ordinary daily ticks, checks the same
facility and the new storage capacity, preserves the observed first-base records
and verifies a new UI-created save; see
[construction completion](construction-completion-contract.md). The subsequent
[transfer and arrival](transfer-arrival-contract.md) stage loads those saves,
transfers one item into the completed storage and verifies natural delivery in
both games. Loading the newly created arrival-complete saves remains pending;
identity continuity across SavedGame replacement is not claimed.

The full regression suite is not yet passing. Exact baseline diagnostic
comparisons in individual contracts do not waive unresolved failures.
