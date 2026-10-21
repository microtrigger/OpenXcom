# Bounded production qualification — 2026-10-21

This is a measured slice, not exhaustive manufacturing acceptance. The game
binary was built from the readable-UI milestone; the opt-in runner uses only
fresh MCP UI observations and one-shot actions. Private game resources, captures
and raw run logs are deliberately not published here.

## Passed in real stock campaigns

Both UFO Defense and Terror from the Deep were started independently with legal
stock resources. Research unlocked the respective pistol through the normal UI.
The production phase exercised:

- Engineers: 0, 1, 7, a request larger than the available 10, an additional step
  at that limit, max, clear, and a decrease at zero. Visible allocated/free staff
  and workshop space were checked after each operation.
- Units: 1, 7, 1,000, 100,000, a finite decrease, clear, max/infinite, repeated
  max, and return to finite. These are quantity intents, not raw keyboard entry.
- Stop of a newly editable draft restored its prior visible production summary.
  This is **Stop Production**, not Cancel or a test of stopping a running order.
- A real order for 3 units with 3 engineers, sell disabled, then normal time
  advancement until the matching production-completed message.
- Base Information → Stores before and after: exact visible item count changed
  from 0 to 3, for Laser Pistol and Gauss Pistol respectively. Absence counted as
  zero only after a full bounded list scan without unknown labels. The list was
  opened at its constructor-established top position; a stable viewport after a
  down action established the bottom where stock arrows remain visible.

The accepted production phases used 60 actions in UFO and 58 in TFTD, separately
from their bounded research phases. Source, runner and binary hashes remained
unchanged across each run. Real desktop recordings cover both runs.

## Runner and regression checks

`tests/agent-control/research-live/run_production_smoke.py` is opt-in, requires an
explicit source/binary/data prefix and a new isolated output directory, and waits
for operator-observed game focus. It does not read saves, inspect hidden game
state, synthesize GUI input or replay unknown mutations. Legacy research-only
production guards remain unchanged.

103 runner regression tests passed normally and with Python `-O`. A queued
research completion can replace an incidental screen during pre-dispatch fresh
readiness. That exact transition now triggers a fresh read and exact-topic check,
not a retry of the discarded action. Other unexpected screens and unknown
dispatch outcomes still stop the run. An earlier TFTD attempt stopped safely on
this transition and is not counted as a passing run.

Example (run from an environment with a real visible display):

```sh
python3 -B tests/agent-control/research-live/run_production_smoke.py \
  --source /path/to/source --binary /path/to/openxcom \
  --data /path/to/legal-stock-data --master xcom1 \
  --completion-allocation --article-navigation \
  --output /new/private/run-directory \
  --focus-ready-file /new/private/run-directory/focus-ready
```

For TFTD use `--master xcom2`; the stock route may require
`--allow-unknown-selection`, which permits only one uniquely displayed selection
and then requires the expected detail title before allocating staff.

## Still pending

The fresh bases were personnel-limited, not independently workshop-limited.
Neither pistol requires manufacturing materials, so material shortage, hangars,
craft construction, running-order cancellation/refunds and inventory overflow
are not qualified. Schema-overlimit requests have unit coverage, not live
coverage here. Mouse hold, wheel and keyboard entry are not exercised by these
semantic quantity intents. Transfers, mission-loot overflow, interception and
the remaining Geoscape event inventory stay separate milestones in the plan.
