# OpenXcom [![Workflow Status][workflow-badge]][actions-url]

## Fair-Play MCP Interface Project

This fork is planning a resource-efficient interface for joint human–assistant play that preserves fog of war and normal game rules. Start with the **[English work plan](docs/mcp-player-interface/README.md)**; the **[Russian working version](docs/mcp-player-interface/PLAN.ru.md)** is maintained separately. Default-off HUD, final-pixel cursor and fixed-window mixed-audio prototypes are implemented with a local read-only stdio MCP adapter. Optional request-bound pixels, displayed HUD and 500 ms post-admission application audio are available within their documented limits; separate default-off, locally confirmed kneel/stand, next/previous unit-selection and ordinary map right/left-click pilots are now available under the practical-play/1 scope. Left-click movement and inventory opening/read-only viewing/ordinary closing/Ground-button rearrangement are bounded pilots. Individual item transfer, reload and prime operations remain manual; firing and end-turn control remain unimplemented.

The [supervised unit-selection extension](docs/mcp-player-interface/SUPERVISED-SELECTION-PILOT.md)
adds ordinary next/previous controls with the same fresh displayed-panel capability,
local confirmation and shared one-use command ledger as stance. Its finite native
UFO/TFTD gate checks ten manual/controlled pairs and nineteen adversarial worlds
per game; existing stance checks also pass in both. See [verification and limits](docs/mcp-player-interface/SUPERVISED-SELECTION-EVIDENCE.md).
It does not enumerate hidden units, replace normal gameplay validation or establish
full fairness or token savings.

The [ordinary map right-click pilot](docs/mcp-player-interface/SUPERVISED-TURN-PILOT.md)
now adds a fresh full tactical view and locally confirmed screen location. Its
normal turn/door path passes 15 manual/controlled pairs and 29 adversarial worlds
in each game, with 30 paired outcome views; nine actual-source fault controls are
detected in both. Camera/pose generations, type-bound capabilities and mandatory
post-action viewing protect this limited control. See [evidence and earlier failures](docs/mcp-player-interface/SUPERVISED-TURN-EVIDENCE.md).
Sliding doors, full controller acceptance and token measurements remain open.

The [ordinary map left-click pilot](docs/mcp-player-interface/SUPERVISED-MAP-LEFT-CLICK-PILOT.md)
proposes one locally confirmed normal click, which may walk, select a visible
friendly or do nothing. Both games pass 19 manual/controlled pairs, 36 adversarial
worlds and 38 paired outcome views. Ordinary TU/energy/reserves, first contact,
a regular door, reaction-queue formation and movement continuing after observation
failure are checked. All 18 actual-source faults are detected in both games,
and prior stance/selection/right-click regressions pass; see [evidence and limits](docs/mcp-player-interface/SUPERVISED-MAP-LEFT-CLICK-EVIDENCE.md).

The opt-in [inventory-opening slice](docs/mcp-player-interface/SUPERVISED-INVENTORY-OPEN-PILOT.md)
adds locally confirmed ordinary opening and a capless final inventory image. All
14 native profiles pass in both games, including 12 opening pairs and 24 additional
worlds; all 15 actual-source faults are detected in each. Real official-SDK →
production-game opening/view/manual-close checks also pass in UFO and TFTD.
Individual item transfer, reload and prime remain manual; closing and Ground require their separate opt-ins below. See [opening evidence and limits](docs/mcp-player-interface/SUPERVISED-INVENTORY-OPEN-EVIDENCE.md).

The separately enabled [ordinary inventory CLOSE pilot](docs/mcp-player-interface/SUPERVISED-INVENTORY-CLOSE-PILOT.md)
adds a fresh J image/capability and locally confirmed C proposal. Its inventory-owned
overlay does not reinitialize or rearrange ground on cancellation; normal OK,
deferred destruction, gravity, lighting and FOV remain ordinary game behavior.
All 34 native profiles pass in each game, including 14 manual/C pairs; real
SDK → production-game opening/I/J/C/fresh-V chains also pass once in each.
All ten prior-control regression runs pass and default-OFF bytes remain unchanged.
Seventeen of 18 source faults are detected in both games; the surviving single-guard
mutation is explicitly documented.
See [evidence, earlier fixture failures and limits](docs/mcp-player-interface/SUPERVISED-INVENTORY-CLOSE-EVIDENCE.md).

The separate [ordinary Ground button slice](docs/mcp-player-interface/SUPERVISED-INVENTORY-GROUND-PILOT.md)
adds normal ground-slot/stack rearrangement and page cycling with a K-only grant,
fresh local consent and mandatory capless I. Both games pass 42 native profiles,
including 20 manual/G pairs each; actual SDK/production-game chains pass once
per game. Source-fault evidence separates 50 initial runs from two targeted
repeats and retains the surviving UI-generation guard. See
[evidence, corrections and limits](docs/mcp-player-interface/SUPERVISED-INVENTORY-GROUND-EVIDENCE.md).
Individual item transfer, reloading and priming remain manual.

The [combined official-SDK → production-game smoke](docs/mcp-player-interface/COMBINED-SDK-LIVE-EVIDENCE.md)
now verifies selection, turn, actual movement, inventory opening/view and manual
close followed by a fresh tactical view in UFO attempt 01 and TFTD attempt 04.
Three earlier TFTD nonmovement outcomes are retained, not counted as movement
passes. The frozen production executable is unchanged. Local confirmation and
post-stop lifecycle observations are operator-attested; an independent archive
review checks all 26 returned images and five runs' receipts. This adds bounded
integration evidence, with no model/token benchmark or full-fairness claim.

**Current plan and actual progress:** see the [English status checklist](docs/mcp-player-interface/README.md#plan-versus-actual-progress) or [Russian working checklist](docs/mcp-player-interface/PLAN.ru.md#план-и-фактическое-выполнение). Selected request-bound pixel/HUD checks cover both games, including the [bounded natural-FOV/pause gate](docs/mcp-player-interface/NATURAL-FOV-GATE.md). The earlier [mixed-audio slice](docs/mcp-player-interface/MIXED-AUDIO-NATIVE.md) adds fixed 500 ms final SDL_mixer PCM for a qualified 22050 Hz signed-16-bit stereo software-mixed profile; 190 Python tests and a native real-SDL dummy-mixer oracle pass. Selected [UFO/TFTD menu checks](docs/mcp-player-interface/mixed-audio-game-evidence.json) also pass: eight fresh clips include exact silence at verified zero volume, and four post-exit requests fail closed. These checks do not establish full gameplay audio parity, physical audibility, continuous sound history or frame/HUD synchronization. The full fairness gate, audio parity, gameplay controller and actual end-to-end token comparison remain open.

The [bounded battlefield-audio gate](docs/mcp-player-interface/BATTLEFIELD-AUDIO-GATE.md) passes one land-only run each in UFO/TFTD: visible/outside-player-team-FOV grenade events, exact independent same-buffer PCM, unchanged mixer output and paired state/engine-RNG equality. Six deliberate controls are rejected; see [aggregate evidence](docs/mcp-player-interface/battlefield-audio-evidence.json). Normal End Turn input and timers reach the next-turn input wait before hostile AI. This dummy-mixer result does not establish physical audibility, isolated grenade PCM, full audio parity, timing/libc-RNG equality or audiovisual synchronization. Stage 5 remains open; no observer fields or controller are added.

The [diagnostic-isolation boundary](tools/mcp-observer/DIAGNOSTIC-ISOLATION.md) now discards both child stdout and stderr with `DEVNULL` and requires native qualification of the effective diagnostic options and logger level. Rejection is terminal for that process at the next observer boundary; settings are never rewritten. Raw engine logs are not observation inputs. This remains a cooperative trusted-local-process boundary. [Bounded checks](docs/mcp-player-interface/diagnostic-isolation-evidence.json) now cover normal and rejected diagnostic profiles in UFO/TFTD menus, with empty launcher stderr in all four final runs. Earlier failed visual attempts remain recorded without an established cause. This adds no battle or full-fairness claim; Stage 5 and controller acceptance remain open.

The separate, default-off [smoke-density render gate](docs/mcp-player-interface/SMOKE-DENSITY-GATE.md) passes its first normal run in each UFO/TFTD master: smoke 7/8 matches in all eight complete frames, 24 native observer responses succeed, and smoke 6 changes both the frame and cursor crop in all eight phases. Actual production smoke blits are witnessed in a seeded, discovered but terrain-empty depth-0 fixture. Simulation/timers are skipped and the audio subsystem is disabled. Hidden-density-to-HUD and wrong-smoke-frame source mutations are rejected in both games; all six normal/mutant runs completed on their first attempt without crashes, timeouts or exclusions. See [aggregate evidence](docs/mcp-player-interface/smoke-density-evidence.json). This is bounded render/transport evidence; Stage 5 and controller acceptance remain open.

The separate, default-off [smoke viewport-clipping gate](docs/mcp-player-interface/SMOKE-CLIPPING-GATE.md) passes its first normal run in each UFO/TFTD master on the same frozen binary. Four fresh worlds form centered smoke 6/7 positive controls and wholly clipped smoke 6/7 comparison pairs: all eight clipped complete frames and normalized native transcripts match, with 32 valid native responses and eight centered phases changing both frame and actual cursor crop per game. An actual post-blit witness verifies full right-edge exclusion at x=336 in the 320-pixel-wide map surface. The discovered, terrain-empty depth-0 fixture pins both RNG seeds, skips simulation/timer-driven thinking and disables audio. The actual-density-to-HUD leak and collapsed-centered-group controls are rejected in both games at their intended assertions; the latter tests positive-control sensitivity, not a clipping bypass. All six normal/mutant runs completed on their first attempt without crashes, timeouts or exclusions; see [aggregate evidence](docs/mcp-player-interface/smoke-clipping-evidence.json). This is bounded render/transport evidence; hidden-movement/queue cases, full Stage 5, timing parity, controller acceptance and real token measurements remain open.

The default-off [bounded native timing calibration](docs/mcp-player-interface/TIMING-CALIBRATION.md) completed its four planned UFO/TFTD profiles with equal complete frames and normalized native transcripts in every pair. The deliberate 5 ms control meets the frozen sensitivity rule in TFTD but remains inconclusive in UFO. This same-thread native return-path measurement excludes simulation and audio; it does not measure MCP stdio latency or establish timing equivalence. The [aggregate evidence](docs/mcp-player-interface/timing-calibration-evidence.json) is retained without retries or trimming. Timing and the original full Stage 5 remain open. The later practical-play/1 decision retains residual timing channels as an experimental limitation; it does not reinterpret this calibration as a pass.

The [supervised stance pilot](docs/mcp-player-interface/SUPERVISED-STANCE-PILOT.md) now supports one explicit kneel/stand proposal at a time, approved through a displayed local game dialog. Fresh rendered context, one-use actor/session binding and duplicate protection preserve the normal action path. Both UFO/TFTD pass 12 UI/controller parity pairs, 19 safety cases and five deliberate fault controls each. Separate actual MCP/game demonstrations verify standing 40 TU → kneeling 36 TU → standing 28 TU in both games, with seven tool calls and two local confirmations per game. [Evidence and earlier failures](docs/mcp-player-interface/SUPERVISED-STANCE-EVIDENCE.md) retain the corrected TFTD button-surface issue and stopped demonstration attempts. This is a limited practical-play/1 pilot; the original full Stage 5, broader control and actual token comparison remain open.

**Fork modification notice (2026-10-12):** this work adds optional read-only HUD/cursor/mixed-audio observers, isolated render gates including smoke density and viewport clipping, bounded native timing calibration, tests, build evidence and an MCP adapter with optional locally confirmed stance, unit-selection, ordinary right/left-click and inventory opening/closing pilots, plus bounded combined SDK/live-game evidence. New project code/documentation uses GPL-3.0-or-later; upstream and third-party notices remain intact. See [license and redistribution notices](LICENSE-NOTICES.md). No original X-COM game resources or new binaries are included in these additions.

[workflow-badge]: https://github.com/OpenXcom/OpenXcom/workflows/ci/badge.svg
[actions-url]: https://github.com/OpenXcom/OpenXcom/actions

OpenXcom is an open-source clone of the popular "UFO: Enemy Unknown" ("X-COM:
UFO Defense" in the USA release) and "X-COM: Terror From the Deep" videogames
by Microprose, licensed under the GPL and written in C++ / SDL.

See more info at the [website](https://openxcom.org)
and the [wiki](https://www.ufopaedia.org/index.php/OpenXcom).

Uses modified code from SDL\_gfx (LGPL) with permission from author.

## Installation

OpenXcom requires a vanilla copy of the X-COM resources -- from either or both
of the original games.  If you own the games on Steam, the Windows installer
will automatically detect it and copy the resources over for you.

If you want to copy things over manually, you can find the Steam game folders
at:

    UFO: "Steam\SteamApps\common\XCom UFO Defense\XCOM"
    TFTD: "Steam\SteamApps\common\X-COM Terror from the Deep\TFD"

Do not use modded versions (e.g. with XcomUtil) as they may cause bugs and
crashes.  Copy the UFO subfolders to the UFO subdirectory in OpenXcom's data
or user folder and/or the TFTD subfolders to the TFTD subdirectory in OpenXcom's
data or user folder (see below for folder locations).

## Mods

Mods are an important and exciting part of the game.  OpenXcom comes with a set
of standard mods based on traditional XcomUtil and UFOExtender functionality.
There is also a [mod portal website](https://openxcom.mod.io/) with a thriving
mod community with hundreds of innovative mods to choose from.

To install a mod, go to the mods subdirectory in your user directory (see below
for folder locations).  Extract the mod into a new subdirectory.  WinZip has an
"Extract to" option that creates a directory whose name is based on the archive
name.  It doesn't really matter what the directory name is as long as it is
unique.  Some mods are packed with extra directories at the top, so you may
need to move files around inside the new mod directory to get things straighted
out.  For example, if you extract a mod to mods/LulzMod and you see something
like:

    mods/LulzMod/data/TERRAIN/
    mods/LulzMod/data/Rulesets/

and so on, just move everything up a level so it looks like:

    mods/LulzMod/TERRAIN/
    mods/LulzMod/Rulesets/

and you're good to go!  Enable your new mod on the Options -> Mods page in-game.

## Directory Locations

OpenXcom has three directory locations that it searches for user and game files:

<table>
  <tr>
    <th>Folder Type</th>
    <th>Folder Contents</th>
  </tr>
  <tr>
    <td>user</td>
    <td>mods, savegames, screenshots</td>
  </tr>
  <tr>
    <td>config</td>
    <td>game configuration</td>
  </tr>
  <tr>
    <td>data</td>
    <td>UFO and TFTD data files, standard mods, common resources</td>
  </tr>
</table>

Each of these default to different paths on different operating systems (shown
below).  For the user and config directories, OpenXcom will search a list of
directories and use the first one that already exists.  If none exist, it will
create a directory and use that.  When searching for files in the data
directory, OpenXcom will search through all of the named directories, so some
files can be installed in one directory and others in another.  This gives
you some flexibility in case you can't copy UFO or TFTD resource files to some
system locations.  You can also specify your own path for each of these by
passing a commandline argument when running OpenXcom.  For example:

    openxcom -data "$HOME/bin/OpenXcom/usr/share/openxcom"

or, if you have a fully self-contained installation:

    openxcom -data "$HOME/games/openxcom/data" -user "$HOME/games/openxcom/user" -config "$HOME/games/openxcom/config"

### Windows

User and Config folder:
- C:\Documents and Settings\\\<user\>\My Documents\OpenXcom (Windows 2000/XP)
- C:\Users\\\<user\>\Documents\OpenXcom (Windows Vista/7)
- \<game directory\>\user
- .\user

Data folders:
- C:\Documents and Settings\\\<user\>\My Documents\OpenXcom\data (Windows 2000/XP)
- DATADIR build flag
- C:\Users\\\<user\>\Documents\OpenXcom\data (Windows Vista/7/8)
- \<game directory\>
- . (the current directory)

### Mac OS X

User and Config folder:
- $XDG\_DATA\_HOME/openxcom (if $XDG\_DATA\_HOME is defined)
- $HOME/Library/Application Support/OpenXcom
- $HOME/.openxcom
- ./user

Data folders:
- $XDG\_DATA\_HOME/openxcom (if $XDG\_DATA\_HOME is defined)
- $HOME/Library/Application Support/OpenXcom (if $XDG\_DATA\_HOME is not defined)
- DATADIR build flag
- $XDG\_DATA\_DIRS/openxcom (for each directory in $XDG\_DATA\_DIRS if $XDG\_DATA\_DIRS is defined)
- /Users/Shared/OpenXcom (if $XDG\_DATA\_DIRS is not defined or is empty)
- . (the current directory)

### Linux

User folder:
- $XDG\_DATA\_HOME/openxcom (if $XDG\_DATA\_HOME is defined)
- $HOME/.local/share/openxcom (if $XDG\_DATA\_HOME is not defined)
- $HOME/.openxcom
- ./user

Config folder:
- $XDG\_CONFIG\_HOME/openxcom (if $XDG\_CONFIG\_HOME is defined)
- $HOME/.config/openxcom (if $XDG\_CONFIG\_HOME is not defined)

Data folders:
- $XDG\_DATA\_HOME/openxcom (if $XDG\_DATA\_HOME is defined)
- $HOME/.local/share/openxcom (if $XDG\_DATA\_HOME is not defined)
- DATADIR build flag
- $XDG\_DATA\_DIRS/openxcom (for each directory in $XDG\_DATA\_DIRS if $XDG\_DATA\_DIRS is defined)
- /usr/local/share/openxcom (if $XDG\_DATA\_DIRS is not defined or is empty)
- /usr/share/openxcom (if $XDG\_DATA\_DIRS is not defined or is empty)
- the directory data files were installed to
- . (the current directory)

## Configuration

OpenXcom has a variety of game settings and extras that can be customized, both
in-game and out-game. These options are global and affect any old or new
savegame.

For more details please check the [wiki](https://ufopaedia.org/index.php/Options_(OpenXcom)).

## Development

OpenXcom requires the following developer libraries:

- [SDL](https://www.libsdl.org) (libsdl1.2)
- [SDL\_mixer](https://www.libsdl.org/projects/SDL_mixer/) (libsdl-mixer1.2)
- [SDL\_gfx](https://www.ferzkopp.net/wordpress/2016/01/02/sdl_gfx-sdl2_gfx/) (libsdl-gfx1.2), version 2.0.22 or later
- [SDL\_image](https://www.libsdl.org/projects/SDL_image/) (libsdl-image1.2)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp), version 0.5.3 or later

The source code includes files for the following build tools:

- Microsoft Visual C++ 2010 or newer
- Xcode
- Make (see Makefile.simple)
- CMake

It's also been tested on a variety of other tools on Windows/Mac/Linux. More
detailed compiling instructions are available at the
[wiki](https://ufopaedia.org/index.php/Compiling_(OpenXcom)), along with
pre-compiled dependency packages.
