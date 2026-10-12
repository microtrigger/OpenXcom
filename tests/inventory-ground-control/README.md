# Finite native Inventory Ground gate

This default-OFF gate verifies the bounded ordinary Inventory Ground pilot. It is
not a model benchmark or a full-fairness verdict. All input is a local operator
fixture; it never grants remote confirmation authority. The production command
remains `K<challenge>` (33 bytes) then `G<epoch><capability><id>` (97 bytes), with
fresh local consent and a mandatory capless `I` from the surviving inventory.

## Native positive coverage

`core` reconstructs 40 independent worlds: ten fixture pairs, with
`maximizeInfoScreens` off and on. Each pair compares one ordinary mouse click on
the actual Ground button with one locally confirmed G dispatch. Fixtures cover
empty ground, one item, a manually dropped noncanonical layout, multiple pages,
last-page wrapping, the native exactly-full-page-to-empty-page boundary,
stackable and nonstackable items, zero TU, natural primed-item animation, and
loaded ground weapons with distinct ammunition quantities.

Each action must enter `btnGroundClick` once and add exactly one
`Inventory::arrangeGround` call. No init, close, construction, or destruction may
occur. The semantic oracle compares complete save state and relevant runtime,
simulation RNG, ownership, ammunition, TU, ground page and stack cache.
Libc/seedless RNG state is not captured or compared. Only the ordinary
layout coordinates and presentation page/stack cache are permitted to change.

K and post-G I images are independently checked against the full physical SDL
surface, including cursor and tooltip. Paired final images use naturally
occurring `Inventory::_animFrame` phases 1 through 8. The gate never freezes or
resets a timer, forces a redraw for comparison, crops the comparison, or masks
pixels. Retained frames are private fixture evidence, never protocol fields.

## Negative and lifecycle coverage

The 42 fixed profiles additionally cover cancel on a nonzero page and a
noncanonical layout; consumed X1/X2, wheel, hotkey, outside click and modifiers;
held items; stale input/actor/reinit; focus/display/palette/overlay changes;
unseen or held consent; actual 120-second capability and consent expiration;
disconnect; duplicate-pending and final receipt/conflict/full-ledger handling;
runtime disabled and missing CLOSE prerequisite; unsupported non-TU/debug
inventory; capless I and J/K command-kind separation; hidden/moved Ground button
and post-context visibility/held-item changes; failed I; lost inventory,
reopened inventory, replaced parent and replaced save.

The observation barrier rejects O/V/J/K until the exact surviving inventory's I
is delivered. Losing that destination does not clear it. Duplicate commands
return receipts only and never rearm or re-execute.

## Running

Enable `EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE` together with its CMake
prerequisites, then run `run-inventory-ground-control.py` with absolute binary
and data paths and `xcom1` or `xcom2`. Each profile gets an isolated options
directory and process. Run both masters in an exclusive native graphical
session with the local dependency libraries available. The aggregate output is
bounded and states `full_fairness:false`. Child diagnostics must stay private.

Pure checks need no game or assets:

```
python3 -m unittest discover -s tests/inventory-ground-control -p 'test*.py'
python3 -O -m unittest discover -s tests/inventory-ground-control -p 'test*.py'
```

## Sensitivity

`make-inventory-ground-mutant.py` contains 25 predeclared actual production-source
faults. It accepts only a separate disposable checkout, rejects source aliases
and repeat/combined mutations, and never changes gate or oracle code. Each
native binary must be built from the frozen source with exactly the declared
production fault. Before execution, map each fault to its expected relevant
semantic assertion and selected profile. A source edit or successful compile
is not a detection. Crashes, watchdogs, timeouts, launch/build failures and
unrelated assertions do not count. Report surviving or inconclusive faults
honestly; a redundant single guard can survive. The unseen-consent mutation is
explicitly a composite three-guard fault, not an individual-guard claim.
