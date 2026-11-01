# Tactical unit information (control schema 51)

The ordinary selected-player-unit statistics button opens the native UnitInfo
screen. The MCP reports its displayed name, eighteen labelled rows, displayed
values and painted bar runs. This includes current TU, energy, health, fatal
wounds, ordinary combat attributes and five armor facings. Psi values remain
null when the native UI does not display them. The contract does not query
hidden enemy statistics or introduce a mind-probe shortcut.

Native Previous, Next and Close callbacks are available only from the fresh,
visible screen. An exact-union partition covers the overlapping HUD name and
statistics click region without weakening disjointness or overlay admission.

## Version and qualification scope

This isolated branch is based on public commit
`5ab560ef15211d599b9f6a49162dc64696bf6236` and uses the exact
`openxcom-agent-control/51` identity on both native and adapter sides. It adds
four actions and one state to public50: 265 actions, 107 states and seven MCP
tools. It does not include the separate `50-craft-stats1` CRAFT extension, the
plain50 Diary fix or later Names66 changes. Those branches are not interchangeable.

The retained checkpoint242 qualification records both UFO and TFTD passing a
short live sequence on isolated copies of existing
test battles: load, open statistics, read, next unit, previous unit, close back
to the same selected unit. No turn, damage or combat outcome was manufactured.
Both final sequences used MCP throughout and retained real continuous game
sound in passive video. Earlier failed admission attempts remain separate
failed evidence; one read-only visual diagnostic identified a normal HUD.
A private geometry diagnostic then confirmed the partially overlapping regions.

The retained Python qualification reports 1,717 methods in normal and optimized
modes: 1,710 passing and seven explicit skips in each. It also records 58 native
UnitInfo fixture checks in normal and sanitizer modes. Sanitizer scope excludes
vptr, enum and leak checking. These are historical results for the frozen source,
not new gameplay or full-engine-build evidence on the publication host.

Publication checks on z2-ai rerun this branch's own normal and optimized Python
suites and the compiled production-block geometry regression. A standalone
native protocol build checks the action names and argument kinds against the
matching Python adapter. Full game build, native SDL fixture execution and live
gameplay are not rerun there: SDL 1.2/yaml-cpp development dependencies and external
stock fixture data are not part of this source publication. No GitHub CI success
is implied.

The native fixture can be run separately with explicitly supplied dependencies:

```sh
python3 tests/agent-control/run-unit-info-tests.py \
  --source "$CHECKOUT" --sdk "$SDK_USR" --data "$STOCK_DATA" \
  --output "$NEW_EXTERNAL_BUILD"
```

The output directory must be new. `--sdk` contains `include/SDL` and
`lib/x86_64-linux-gnu`; `--data` contains external `common` and `standard/xcom1`
and `standard/xcom2` language/font files. The runner uses the included
`run-dogfight-providers.py` support module. Constructor layout, Game/Mod/player
unit objects and grayscale palette are fixtures; actual init/provider/callback
bodies and SDL widgets are used. It does not run a complete game. No resources,
saves, recordings, private runtime logs or host credentials are distributed here.

Video or audio availability and synchronization do not gate gameplay testing;
record media quality separately. Missing media is not video/audio evidence.

This is not complete tactical coverage. Medikit and motion-scanner screens,
other ordinary UI gaps, and clean end-to-end cycles on the latest build remain
open. Rare campaign branches remain deferred rather than passed.
