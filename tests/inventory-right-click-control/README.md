# Finite ordinary inventory RIGHT gate

This separate default-OFF local fixture exercises U/X and the ordinary RIGHT
callback. It is not a model benchmark or a full-fairness verdict. Build
`EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE` and its declared prerequisites;
activate only with `OPENXCOM_INVENTORY_RIGHT_CLICK_CONTROL_GATE=1`.

The wrapper currently declares 46 isolated profiles per game. Never run any
native profile alongside another GUI operator. Pure checks do not launch a game:

```
python3 -m unittest discover -s tests/inventory-right-click-control -p 'test*.py'
python3 -O -m unittest discover -s tests/inventory-right-click-control -p 'test*.py'
```

## Scope and independent oracles

`core` reconstructs 48 worlds: 12 fixtures × manual RIGHT / confirmed X × both
maximize-info settings. Held fixtures include hand singleton and tall items,
ground singleton/tall/stack/page-offset/primed items, a refused zero-TU placement
that remains held, and a loaded weapon. Unheld fixtures close an empty inventory,
a loaded-weapon inventory, and a paged ground inventory. Preparation uses real
SDL input; ordinary RIGHT cancellation and ordinary close are compared directly.

Before initial I, the gate independently records complete battle save and
runtime, simulation RNG, TU/energy, item owner/ammunition/fuse/slot/tile, held
identity, ground page/stack cache, UI text, and relevant pressed-button state.
Libc RNG is excluded. Counts require one ordinary RIGHT callback and its existing
invClick tail, no controller-induced init/rearrange/OK shortcut, and no dereference
of a destroyed inventory in the gate. After normal parent restoration, closed
worlds compare the same full save/runtime as their manual counterparts.

Every returned U/I/V image is checked against the independent physical SDL
surface, including its canonical RGB palette encoding. The returned post-view image and the actual rendered collection-entry frame
are retained separately. Settled-natural-phase parity requires an observed
natural phase advance from that collection entry in both paths, then compares
eight whole inventory-primer or map-animation phases. Initialized
inventory phase zero is not a parity phase. This is not a claim of arbitrary
transient equivalence: ordinary item-cache redraw can clear a primer while its
old phase label remains until the next natural timer tick. There are no masks,
timer resets, forced redraws, or elapsed-time-based image substitutions.
Already-rendered overlay text, buttons, and pointer ring are separately checked
against the physical screen. Text must present both possible outcomes and normal
gravity/lighting/FOV without clipping; the pointer center remains transparent.

The controller must have a null-destination, unsatisfied I obligation before
ordinary mouseOver and mouseClick. After the real transition, the surviving
inventory requires exact I, or the original restored parent requires exact V.
Wrong view requests do not clear either obligation. Read transport failures,
lost/reopened inventories, changed saves/parents, and exact receipts are separate
profiles. `nested-transition` makes a real push+pop inside the ordinary hover
hook: restored pointer/topology must still leave the unresolved barrier poisoned.
This negative permits the extra normal restoration init caused by its own push.
`nested-save-notice` invokes the actual before-save-change notification during
hover without freeing live callback pointers; it tests that notification hook,
not a complete synchronous save replacement. Separate save-I/save-V profiles
perform real save replacement after dispatch.

Input tests retain held items after denial, real expiration, or disconnect;
CLOSE/Ground held refusal stays intact. Invalid overlay children must consume
the original owned DOWN/UP pair. Test-only parent-handle observation checks that
Y UP and a pending mouse UP do not leak after close. Missing old releases must
not swallow later new ordinary mouse/Y pairs, including disconnected cases.
Other profiles cover strict pointer domains, kind separation, shared ledger64,
capless I revocation, stale input/actor/reinit/held selection, prebattle/base/debug
refusal, runtime/CLOSE activation, and independence from CLICK/Ground flags.

## Predeclared source sensitivity

`make-inventory-right-click-mutant.py` declares 25 actual production-source
recipes. `fault-map.json` fixes each profile and semantic assertion endpoint
before native execution. A disposable source copy is required; aliases,
hardlinks to the normal checkout, duplicate anchors, repeat/combined mutations,
and test/oracle mutation are rejected. Composite recipes are labeled explicitly.

Application and compilation are setup only. Detection requires ordinary failure
exit at the exact mapped semantic endpoint. An unrelated assertion, crash,
timeout, watchdog, launch/build failure, or changed frozen input is inconclusive.
A exact profile PASS is a survivor. Do not expand endpoints or repeat runs to
improve a result. Freeze source, binary, dependencies and mapping before runtime,
retain all failed attempts, and distinguish native, pure, and live SDK evidence.

The frozen r3 fixture passes 16 focused cells and a separate 92-cell full
baseline across UFO/TFTD. The 50 source-fault cells finish with 46 mapped
detections, two survivors and two inconclusive deliberate duplicate-callback
mutant crashes. Fault-phase acceptance remains false. The separate 298 earlier
control regressions pass; none of the historical mutation corpora was rerun.
See [full evidence and retained r1/r2 failures](../../docs/mcp-player-interface/SUPERVISED-INVENTORY-RIGHT-CLICK-EVIDENCE.md).
This README is updated after those frozen-source runs; fixture code and mapping
are unchanged. Reopen tests do not force allocator address reuse, and therefore
do not prove that exact-address scenario at runtime.

## Separate existing CLICK regression

The existing CLICK gate adds isolated `orphan-left-release` without changing
its previous 54 profile semantics or physical line-number assertion endpoints.
The new helper is appended after the historical source. It owns LEFT DOWN in an
E overlay, retires with Escape while deliberately omitting UP, then sends a new
ordinary LEFT DOWN/UP to the held item's original hand. Fixed endpoint 9005 checks
exactly one new ordinary click and callback; 9006 checks the restored item/TU.
The identical fixture is intended for exact old 7b8954 production and a narrowly
fixed CLICK-on/RIGHT-off production build. Historical runtime results remain
historical; this additional regression does not rewrite them.


## Retained early RIGHT fixture failures

The initial focused r1 runs retained a context-shape failure at world 15,
assertion 278, and the restoration-motion hook failure. Activated-warning refusal
is the source-supported explanation for the context failure; the exact unavailable
payload was not retained. Later fixture revisions do not rewrite those outcomes.
In r2, real whole-frame differences at endpoint 105 were retained in UFO worlds
14/15 and independently confirmed in TFTD worlds 38/39. Each differed by exactly
nine pixels at x=1..3, y=153..155. The manual frame had its primer indicator and
the controlled frame did not.
Ordinary `drawItems()` clears/rebuilds the item cache without advancing the primer
phase label. The r3 measurement retains the returned view and actual collection-entry frame,
then requires a subsequent observed natural phase advance from collection entry
before settled parity. Neither retained frame is called the literal first
post-dispatch frame.
No pixels are masked and no timer, draw call, elapsed-time match, or production
behavior is changed. Any r3 result must be reported separately from r2's actual
transient difference.
