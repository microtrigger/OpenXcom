# Lost in Service continuation

The `lost_in_service` screen exposes the current painted title and ordinary OK
button. `acknowledge_lost_in_service` takes no arguments and invokes the existing
`CommendationLateState::btnOkClick` handler, which pops that screen. A dispatch
receipt is not proof of the next screen; observe the resulting current state.

This increment deliberately does not expose the five-column memorial/medals
list. It does not reconstruct soldiers, diary records, kill totals, hidden IDs,
or eligibility from the game model. Existing constructor-side medal `makeOld`
behavior remains unchanged and is not repeated by observation or acknowledgment.

Admission retains active-state, modal, actual paint destination, clipping,
revision, widget geometry, and real-overlay checks. Ordinary cursor overlap and
glyph readability are not admission criteria. Hidden or stale controls do not
become actionable.

The missing continuation was observed after an ordinary UFO mission debrief and
promotions. That historical session used one visible OK repair, then resumed MCP
and saved the campaign in a new slot. The new provider is a subsequent code fix;
that session is not retrospective live qualification of this provider.

## Qualification boundary

The focused constructor/provider/parser/handler fixture passed 103 checks in
normal mode and 103 under the scoped address/undefined sanitizers. Game, Mod,
Soldier/Diary and rendering-loop seams remain explicit; the language fixture
loads each master independently and can use a missing-key fallback instead of
the fully merged stock caption. Leak, vptr and enum sanitizer coverage is not
claimed. Python catalog/schema and the private player-driver continuation passed
6 normal and 6 optimized-mode cases. These focused results are not a full-suite
PASS, a new live acknowledgement run, or proof of general UI coverage.

The completed campaign continuation reached a public `UFO is recovered` debrief,
promotions, and the strategic screen, followed by a new-slot MCP save. It used
two documented visual exceptions: inspection of an inventory whose existing
native actions were omitted by the private player driver, and the unsupported
Lost in Service acknowledgement. The private driver whitelist was subsequently
fixed and tested separately; that private harness is not part of this public
source patch. The completed session did not encounter a read timeout, so the
read-recovery change remains synthetically qualified, not naturally exercised
by that session. No generic historical timeout root cause is claimed.
