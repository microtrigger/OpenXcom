# Natural facility construction completion

Bounded live UFO and TFTD runs now qualify starting General Stores at a second
base, advancing ordinary Geoscape time through ten daily construction ticks,
receiving the construction-complete notification and opening the correct base.
This uses existing controls; it adds no action or schema change.

`choose_geoscape_speed` selects a freshly offered visible speed control. Native
simulation steps and event pauses remain in force. Incidental public notices
are handled only through their offered controls; this is not permission to
launch combat or bypass an unsupported event. The UFO run encountered and
dismissed a public contact notice; the TFTD run did not require that branch.

The existing `production_complete` screen shows the construction message.
`open_production_destination` invokes its ordinary callback: reset speed, close
the notification and open the affected Basescape. The construction branch is
not the manufacturing destination, even though both use the same provider.

In both runs, owned-base snapshots confirm that the same General Stores object
changes from ten remaining construction days to zero. The second base's storage
capacity increases from zero to 50, and its monthly facility maintenance rises
from 4,000 to 9,000. Funds do not change during this waiting interval; the
construction cost was paid when construction started. Ten daily ticks are not
a claim that ten full elapsed days passed after the initial placement.

All exported records associated with the first base retain their identities
and values across the interval: layout, personnel, stock and craft inventory,
craft status/equipment and bounded soldier-history records. This is a check of
owned-state preservation in these runs, not byte-identical game state or a
promise that normal time advancement can never change a base.

Each run creates a new named save through the ordinary UI and verifies its
presence in the save list. Save contents were not inspected. Loading these
particular saves remains a separate acceptance step.

Gameplay used MCP without visual fallback. This acceptance does not cover
queued or multi-cell construction, every facility type, every intervening
public event, transfers into the new storage, or every campaign scenario.
The full player-equivalent goal and the known regression-suite backlog remain
open; see [ordinary coverage](ordinary-coverage-status.md).
