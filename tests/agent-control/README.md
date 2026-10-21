# Native smoke tests

From the repository root: `sh tests/agent-control/run-portable-smoke.sh`.

Requires a C++11 compiler and Linux/POSIX. Covers geometry, socket contracts, render witnesses and event-ring logic without game assets. It does not test the GUI or full game. Other historical fixtures may need matching protocol versions or generated includes.

Typed-line WIP regression (not a full game or article-route acceptance):

```sh
python3 tests/agent-control/run-article-text-tests.py --data /path/to/runtime/data --output /tmp/article-test
python3 tests/agent-control/run-article-text-tests.py --sanitize --data /path/to/runtime/data --output /tmp/article-asan
python3 tests/agent-control/run-article-text-pixels.py --data /path/to/runtime/data --output /tmp/article-pixels
```

Requires SDL 1.2 development libraries and existing UFO/TFTD stock assets; optional
`--sdk /prefix` supplies headers/libraries. Output directories must be new and outside
the checkout. Tests cover opaque final-screen canvases, strict source/format guards,
clipping, stale mutations and exact current-source OFF/ON raster parity. This does
not establish full v22 acceptance or promote WIP to main.

Replay independently generated v22 native packet schedules through the real Python
client and MCP server (no game execution):

```sh
python3 tests/agent-control/replay_client_server_v22.py --source . --packets /path/to/native-packets --output /tmp/replay
python3 tests/agent-control/check-research-replay.py --source . --packets /path/to/native-packets
```

Repeat with `python3 -O` for optimized-mode checks. Packet input must contain native
`*-schedule.json` and their response files; focused checks use the stock
`xcom1-article-type-5` schedule. Unadvertised-action negatives must be rejected by
MCP before any native exchange, without changing client state or its command ledger.
Offered actions (including native stale-grant rejection) and exact duplicate receipt
queries still traverse the socket and match their recorded responses. Generated
packets/results are external test evidence, not game assets or live acceptance.

## Real-game research smoke (WIP)

```sh
python3 -B tests/agent-control/research-live/test_route.py
python3 -B -O tests/agent-control/research-live/test_route.py
python3 -B tests/agent-control/research-live/run_research_smoke.py \
  --source . --binary /path/to/matching/openxcom --data /path/to/runtime/data \
  --master xcom1 --output /tmp/new-research-run \
  --focus-ready-file /tmp/new-research-focus
```

Requires a matching agent-control build, existing legal stock assets and a working
SDL display; optional `--sdk /prefix` supplies runtime libraries. Both paths must
be new. Observe and focus the newly launched game, keep the cursor clear of labels,
and only then create the focus-ready file (within 120 seconds). The runner uses an
isolated profile, ordinary new campaign and fresh offered MCP actions, never a
fixture/save shortcut or mutation replay. It leaves its game open for normal close.

Use `--master xcom2` for TFTD. Strict visible labels are the default. Optional
`--allow-unknown-selection` permits exactly one uniquely visible null-label row
selection per project, then requires the expected detail title before allocation;
ambiguity or mismatch stops (one offered Cancel is allowed). Selection can create
an ordinary project/consume RNG; it does not establish the row's identity itself.

On 2026-10-21, stock UFO Laser Weapons/Laser Pistol and TFTD Gauss Technology/Gauss
Pistol completed live, opened their article and returned to the new-research notice.
TFTD required the explicit unknown-row option and subsequent detail confirmation.
These bounded smokes do not establish full v22 acceptance, complete article body
projection, or every research/navigation route. Source, binary and runner hashes,
steps and partial failures are recorded outside the checkout.

Optional `--article-navigation` additionally checks offered scroll down/up, then
Next → Previous and the original visible title before Close. Missing scroll actions
are explicitly recorded as skipped (not tested); dispatch alone does not prove text
movement or complete reading. Any unexpected return title stops the route.
Live on 2026-10-21: UFO Laser Pistol → Grenade → Laser Pistol; TFTD Gauss Pistol →
Magna-Blast Grenade → Gauss Pistol. TFTD dispatched both offered scroll directions;
UFO offered neither, so both were skipped. Article body projection remained partial.

Optional `--completion-allocation` records the initial visible scientist/laboratory
summary and checks its return after both research completions. It validates the
new-research and new-manufacture notices, opens and closes Manufacture without
starting production, revisits the same named base and ends paused. Unknown numbers,
changed totals or unexpected notice labels stop the route. Production editing and
project-detail actions are excluded unless the separate option below is enabled.
Live UFO/TFTD checks on 2026-10-21 restored the visible `[10, 0, 50]` baseline after
each project, opened the production allocation screen and finished in Pause without
issuing production-start or editing commands.

Stock pistol fixtures also cover actual UFO/TFTD article geometry, palettes and
backgrounds. The conservative cross-line pixel-ownership guard intentionally leaves
intersecting text lines unavailable: stock small-font ink can exceed the line
advance. A complete final line does not imply the preceding paragraph is readable.
Tests load existing language/assets at runtime; no game resources are bundled.

Optional `--manufacture-detail` (requires `--completion-allocation`) opens the
uniquely labelled pistol production detail, verifies its title and cancels. The
known public production summary/rows must match before and after. It never starts
production, changes engineers/quantity/sell settings or stops a project. Unknown
labels/baselines, unexpected titles and uncertain outcomes fail closed.
Live UFO/TFTD detail-and-cancel routes passed on 2026-10-21 with unchanged known
public production summaries/rows and final Pause; this is not a hidden-state proof.

Optional `--verify-visible-clock` checks complete, valid public calendar/time samples
already read during each research wait. It requires a non-regressing revision/time
and a strictly later observation; it adds no reads or mutations. This proves only
an observed interval, not research duration or causality. Unknown dates fail closed.
On 2026-10-21 both stock live routes stopped PARTIAL at their first completion:
public day/month were null, so clock advancement was not established. Do not infer
a date from stock defaults or a time-of-day alone; the projection gap remains open.
