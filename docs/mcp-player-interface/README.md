# Experimental semantic MCP control

This fork lets an MCP-capable agent observe and operate OpenXcom through a bounded
interface to the game's **displayed UI**, rather than unrestricted memory access
or unrestricted desktop input. An explicit in-game cursor-motion action
is available separately from semantic gameplay actions. It supports UFO Defense and Terror from the
Deep with separately supplied original game data.

**Status: experimental, with unified qualification completed on 2026-10-21.**
The integrated tree passed a full build, native normal/sanitizer tests, adapter
normal/optimized replay, and OFF/ON raster parity. Actual UFO/TFTD runs passed the
stock-fixture Basescape return route and fresh-campaign research completion,
article navigation, staff release, manufacture detail/Cancel and retained-history
paging. These are bounded routes, not exhaustive gameplay coverage or a CI claim.
Native popup/report-stack tests retain explicit ownership/resource/availability
seams; retained-history checks do not prove whole-route retention. The qualification above predates the [active article UI-text contract](article-ui-text-contract.md).
Article body delivery now follows the open viewport layout rather than per-pixel
readability proof; calendar fields retain their existing contract.
Build the native game and Python adapter from the **same branch**.

## What the agent gets

| Tool | Purpose |
| --- | --- |
| `read_ui_state` | Fresh bounded UI presentation: visible text/numbers, screen state, offered action IDs, revision and one-use action authority. |
| `execute_ui_action` | Dispatch one currently offered semantic action with fresh authority and a unique command ID. This changes the game. |
| `read_ui_action_result` | Read the receipt for that command without replaying it. |
| `read_public_events` | Page retained public presentation/message/effect/audio events using a separate cursor; gaps are explicit. |

Implemented surfaces include campaign setup and menus, Geoscape time controls,
base selection and management, research/manufacturing and purchase/equipment
controls, tactical camera/selection/actions, and inventory. Availability depends
on the screen and snapshot: **the current offered-action list is authoritative**,
not this summary. Visible rows and grid handles are local to the current view.

The interface does not reveal hidden units, hidden world state, damage or the
cause of a sound/effect. A camera grid is not a legal-movement map; missing events
are not proof that nothing happened. Public event order is not synchronized video.
Normal game rules, costs, time units and reaction fire still apply.

## Architecture and prerequisites

```text
Agent / MCP client
    <-> JSON-RPC over stdin/stdout
Python launch_presentation.py + adapter.py
    <-> inherited anonymous local sockets
Matching experimental OpenXcom executable + original game data
```

The launcher starts the game itself and connects it to the adapter. It does not
attach to an arbitrary already-running game. There is **no HTTP/SSE endpoint,
network port or OAuth service** in this adapter.

- Use a POSIX environment supporting `AF_UNIX`, `SOCK_SEQPACKET` and inherited
  file descriptors. Linux is the reference path below. This control profile is
  not a native Windows build. On Windows, a Linux/WSL environment would need the
  agent, Python and Linux game together plus working GUI/audio; that setup is not
  established by simply pointing at a Windows `openxcom.exe`.
- Python 3 (standard library only for the adapter), CMake, a C++11 compiler,
  SDL **1.2**, SDL_image, SDL_mixer, SDL_gfx and yaml-cpp development libraries.
- A working graphical session and legal UFO/TFTD assets. The repository does not
  supply the original game's resources. Keep saves/configuration in a separate
  test user directory when experimenting with agent control.
- The agent's executor must be able to launch Python and the game and access the
  display/audio session and chosen data/user directories. Installing this config
  in a local client does not connect a hosted web chat to that local process.

## Build and check the game

From the checkout root, use a fresh build directory so old experimental flags do
not leak into this profile:

```sh
cmake -S . -B build-agent -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_AGENT_CONTROL=ON \
  -DOPENXCOM_BUILD_LOADING_PRESENTATION_HELPERS=ON
cmake --build build-agent -j2
python3 tools/mcp-observer/launch_presentation.py --help
```

On Linux the executable is `build-agent/bin/openxcom`. First launch that binary
normally to verify your data and display setup. Game arguments such as
`-data /absolute/path/to/data` and `-user /absolute/path/to/test-user` can be passed
through the launcher after the executable. Use your actual data layout and an
existing writable test user directory; do not copy commercial assets into Git.

Do not enable acceptance-test fixtures for normal play. The semantic profile
cannot be combined with the supervised-stance or independent mixed-audio build
profiles. Public audio history in `read_public_events` is a separate mechanism.

## Connect to your agent (Codex example)

The MCP client must start **the launcher**, not `adapter.py` alone, to get live
semantic tools. Replace every `/absolute/...` placeholder with your Linux paths.
The following registers a local stdio server; it does not alter game code:

```sh
codex mcp add openxcom -- python3 -B \
  /absolute/path/to/OpenXcom/tools/mcp-observer/launch_presentation.py \
  --agent-control -- /absolute/path/to/OpenXcom/build-agent/bin/openxcom \
  -data /absolute/path/to/data -user /absolute/path/to/test-user
codex mcp list
```

Alternatively, add this entry to your Codex `config.toml` (do not register it twice):

```toml
[mcp_servers.openxcom]
command = "python3"
args = [
  "-B", "/absolute/path/to/OpenXcom/tools/mcp-observer/launch_presentation.py",
  "--agent-control", "--", "/absolute/path/to/OpenXcom/build-agent/bin/openxcom",
  "-data", "/absolute/path/to/data", "-user", "/absolute/path/to/test-user"
]
cwd = "/absolute/path/to/OpenXcom"
startup_timeout_sec = 30
tool_timeout_sec = 60
```

Use an absolute Python executable if the client has a different `PATH`. Preserve
any existing client settings. Follow your client's normal permission prompts;
this guide does not require disabling approval or sandbox checks. See the
[official Codex MCP configuration guide](https://developers.openai.com/codex/mcp/)
for config location and client-version details.

Restart/reload the client connection. In Codex CLI, `/mcp` shows active servers.
Ask the agent to list the OpenXcom tools and call `read_ui_state` **before taking
any action**. A successful MCP handshake/tool list alone does not prove that the
native game channel is ready. The game must reach a supported displayed screen.

Other stdio MCP clients can use the same executable and argument array in their
own server configuration. This adapter negotiates MCP protocol `2025-11-25`;
check client compatibility. No API key is needed by this local adapter (your
agent/model service may have its own authentication).

### Suggested first instruction to the agent

> Use OpenXcom's MCP tools. First read the displayed UI and describe the current
> screen and offered actions. Do not start a campaign or change the game until I
> ask. For each approved action, obtain fresh state, dispatch once, inspect the
> receipt and read the resulting UI. Stop if the outcome is unknown.

### Correct action loop

1. Call `read_ui_state`. Use its actual `epoch`, `revision`, `capability`, offered
   action ID and visible local handles; do not invent or reuse stale values.
2. Call `execute_ui_action` with `command_id` (a new 32-character lowercase hex
   ID), those authority fields, `action` and its schema-correct `args`. Consult
   the live tool schema; different actions have different arguments.
3. Inspect the receipt/observation. If needed, query `read_ui_action_result` with
   the **same** command ID; this reads status and does not replay the action.
4. Read fresh UI before the next command. `dispatched` means the handler ran, not
   that the desired gameplay result occurred. Verify the visible postcondition.

After `outcome_unknown`, transport failure or an untrustworthy receipt, stop and
reconcile with the human; do not retry with a new ID. Exact duplicates are receipt
queries, not a general retry strategy. For events, use the epoch from UI state and
`after`/`limit` (`1..16`); follow `next_cursor` and handle `gap` explicitly.

## In-game cursor motion (native schema 26)

Build and launch the matching native game and adapter as above; no extra server,
permission switch or desktop automation is needed. `read_ui_state` includes
`cursor: {"x": 0, "y": 0, "width": 320, "height": 200, "visible": true}` when
cursor motion is safely available, otherwise `cursor: null`. Use the actual
returned dimensions: these are **logical game-canvas coordinates**, origin at
top-left, not desktop/window pixels. Only when `move_cursor` is offered, call
`execute_ui_action` using fresh authority and `action: "move_cursor"`,
`args: {"x": 318, "y": 198}` (an example for a 320×200 canvas).
Both coordinates must be integers within `[0,width)` / `[0,height)`; native wire
limits are 0…4095. Old schema-25 adapters must be upgraded with the game.

The action dispatches ordinary in-game hover, not a click or desktop mouse warp.
It can change tooltips, highlights, selection previews or tactical edge scrolling;
**no click does not mean no game effect**. Held physical buttons, retained native
pressed/drag states, local modal handlers and pending UI transitions refuse motion.
Mapping accounts for cursor letterbox bands and actual display scaling (scales
below 1 are unsupported); unrepresentable coordinates fail closed. Existing
semantic actions, including `inventory_hover`, keep their existing event behavior.

A successful dispatch receipt is not proof of its desired effect. Verify the
post-presentation `observation.cursor` and subsequent visible UI. Freshness binds
the current screen, display and cursor revision. Virtual position persists through
ordinary screen initialization when safe; real mouse motion/button input takes
ownership back. Unsafe initialization or a changed canvas can reset ownership.
The action does not warp the physical desktop cursor; unrelated native screen
initialization can still retain the game's existing pointer-refresh behavior.
A semantically unsupported screen may offer
**only** cursor motion: that does not unlock hidden values or gameplay actions.

## Observation versus control

- `--agent-control` explicitly enables direct semantic actions. It does **not**
  require a per-action confirmation inside the game's local UI. Client approvals
  and the user's task boundaries still apply.
- For a read-only client policy, allow only `read_ui_state`,
  `read_ui_action_result` and `read_public_events`; exclude `execute_ui_action`.
  The underlying game is still built/launched with agent control, so this is a
  client-side restriction, not a different read-only native build.
- `adapter.py` alone exposes historical `read_last_hud`/`read_last_cursor`, not
  current action authority. Historical observations cannot substitute for fresh UI.
- The separate `--supervised-stance` family is an older local-human-confirmation
  pilot, with its own build flags. It cannot be mixed with `--agent-control`.

## Failure classification

Semantic tool errors may include additive `failure` metadata: fixed `code`,
`category`, `severity`, `phase`, `action_outcome`, and `recovery`. Existing error
and receipt statuses are retained. These fields explain the safe next decision;
they do not authorize automatic retries. See the [error contract and path-family
inventory](error-handling-contract.md).

A valid `unsupported` observation is not a tool failure. A `dispatched` receipt
followed by unavailable observation does not turn the known dispatch into an
unknown command. Conversely, native receipt `unavailable` does **not** prove no
mutation: the native code can return it after entering a handler, so its added
classification requires reconciliation. `action_outcome` applies to the described
operation; it never clears uncertainty from an earlier command.

## Troubleshooting and shutdown

- **Only historical tools appear:** the client launched `adapter.py`, or omitted
  `--agent-control`; fix the command and reconnect.
- **`unavailable` / launch failure:** check matching native/adapter versions,
  build flags, game data, display session and supported screen. The launcher
  deliberately suppresses game stdout/stderr; diagnose startup by running the
  game normally outside MCP, not by putting logs on the protocol stream.
- **An expected action is absent:** do not guess it. This can be an unsupported
  screen/coverage gap, not necessarily a failed click.
- **Stale authority / invalid arguments:** read UI again only when no action has
  an unknown outcome; use exact live schema and offered handles.
- **Restarting the client opens another game:** each launcher starts a new game.
  Closing MCP stdin/ending the connection does not terminate the game. Close the
  old game normally before explicitly starting another session.

The [readability/allocation checkpoint](readability-qualification.md) records the
new narrowly scoped UI and allocation checks and their limits.

The next approved gameplay qualification slices and evidence boundaries are in
[the gameplay plan](gameplay-plan.md). Planned coverage is not completed coverage.

## Development checks and remaining limits

```sh
(cd tools/mcp-observer && python3 -B -m unittest discover -v)
(cd tools/mcp-observer && python3 -B -O -m unittest discover -v)
sh tests/agent-control/run-portable-smoke.sh
```

Adapter and asset-free smoke tests do not replace full native/live-game acceptance.
See [native button regression commands](../../tests/agent-control/README.md) for
asset-dependent visibility, stale-action and raster-parity checks.
Historical fixtures may need matching protocol versions or generated includes;
a portable run-all native suite is unfinished. The MCP-versus-screen/input
benchmark has not run. This series reconstructs saved development snapshots;
it does not reproduce their original commit history.

### Live Basescape return smoke

Use a matching build with `EXPERIMENTAL_AGENT_REPORT_FIXTURE=ON` in a real desktop
session and separately supplied stock data:

```sh
python3 tools/mcp-observer/run_basescape_return_smoke.py \
  --binary /path/to/openxcom --data /path/to/data --master xcom1 \
  --output /tmp/ufo-return --focus-ready-file /tmp/ufo-return/focus-ready
```

Wait for `ready.json`, visually verify and focus the newly launched game, then
create the marker from a separate shell without taking game focus. The runner
waits at most 120 seconds; it never simulates focus or replays uncertain actions.
It creates an isolated unsaved stock fixture, checks offered actions and receipt
plus fresh displayed postconditions, and writes a private `RESULT.json`.
Repeat with `xcom2` and a new output directory. `--sdk /path/to/sdk/usr` supports
non-system libraries; `--before-return-gate /path/to/new-marker` optionally pauses
for a Basescape screenshot and obtains fresh authority after that marker appears.
Close the test game normally afterward: MCP EOF deliberately leaves it running.

### Cursor regression scope

The focused `tests/agent-control/run-cursor-controller-tests.py` exercises the
actual socket/controller/authority/receipt loop with explicit Game/input seams.
`run-cursor-core.py` exercises real native `Action`/`InteractiveSurface` hover and
source-extracted cursor mapping/dispatch; window/top-state/cursor rendering remain
seams. `run-cursor-inventory-hover.py` checks the existing inventory hover path and
held-item invariance with lookup/render/domain seams. These are **not live-game
acceptance** or full event-loop tests. Specialized drag-readiness overrides are
source-audited, not all instantiated by the focused fixture. Ordinary real input
reclaiming the virtual position and cross-screen behavior still need live checks.

Controller/core runners take `--source`, `--sdk`, `--output` (a new external
evidence directory) and optional `--sanitize`; core tests additionally require
`--data`. The inventory runner takes only `--output` and optional `--sanitize`.
See each runner's `--help`. Scoped sanitizers and test seams are recorded in their results;
do not describe these as unrestricted whole-game sanitizer coverage.
