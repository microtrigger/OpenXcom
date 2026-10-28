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
remaining regression backlog. The full regression suite is not passing.

Containment has bounded synthetic live acceptance, not evidence of a natural
battle capture. The [Diary Overview/Performance route](diary-contract.md) has bounded local
empty-history live qualification; populated native reports remain distinct
from populated live history. Combat, Awards, mission details and Memorial
remain separate UI branches.
The full project is not complete.
