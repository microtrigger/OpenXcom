# Visible globe targets and recorded audio qualification — 2026-10-28

This is a bounded implementation/qualification record, not completion of the
player-equivalent interface or a replacement for the earlier full mission runs.
UI schema48 adds painted globe marker inspection through the ordinary native
click path, MultipleTargets selection/cancellation and TargetInfo controls.
Only painted marker pixels/local handles are exposed. Existing visibility,
current state, real overlay, weak lifetime and single-use grant guards remain.

A native pre-dispatch rejection now has an explicit retained
`rejected_before_dispatch` result. It is used only where dispatch demonstrably
has not occurred. A failure after entering a native handler remains uncertain.
An earlier moving-marker action with an unavailable result is still unknown;
this change does not retroactively classify it or authorize replay.

## Recorded live scope

Dedicated copies of genuine UFO/TFTD test campaigns were used. No non-test save
was overwritten, no hidden tactical state was inspected and no battle was
manufactured. Earlier schema48 passes covered own-base interception entry and
Cancel in both games, craft navigation in UFO, and a real overlapping target
chooser in TFTD. A later fresh TFTD moving marker opened ALIEN SUB-1 contact,
then native Cancel returned to Geoscape. The historical unknown action was not
replayed. Some immediate post-dispatch observations were transitional/unsupported;
fresh read-only observations subsequently resolved the actual native window.

TargetInfo rename/followers/interception and its editor/overflow cases have
native fixture coverage, not a live acceptance claim. Graphs, complete article
layouts and the remaining ordinary/rare scenario matrix remain open.

## Real game sound

The prior private recorder used video-only `-an`, while SDL initialization had
also failed on a machine without a physical audio device. Those earlier videos
remain mute; missing historical sound has not been reconstructed.

The qualified replacement uses SDL's process-local dummy output to run the
actual game mixer and continuously consumes its existing sole postmix PCM
observer through MCP. No sound is inferred from game objects or action outcomes.
A first menu-only probe retained288 consecutive nonzero100ms windows but lacked
a capture clock, so its mux was explicitly not accepted as synchronized.

The public-audio/2 transport now carries actual callback REALTIME/MONOTONIC
clock readings and frame offsets. These describe digital mixing, not physical
speaker time. The recorder retains original X11 packet timestamps, rejects
missing/duplicate PCM windows and clock discontinuities, and gates gameplay on
both video capture and actual audio capture. Audio-only startup before video is
trimmed; no gameplay precedes the recording gate. Final output must contain a
real nonzero audio track, not merely an empty/silent stream.

Short final native sessions in both games confirmed continuous actual audio
while using MCP controls. Dummy-device sample-clock drift is measured rather
than hidden. Any clock-rate correction resamples recorded samples under explicit
residual/clock-jump gates; it does not generate silence to replace missing audio.
Raw source PCM and timing receipts remain private. This is not a claim of
sample-exact speaker/video synchronization or a long-session soak test.

## Checks

- Production build passed.
- Native globe-target/editor/lifetime and controller pre-dispatch tests passed.
- Native PCM callback/ring, actual SDL mixer and literal controller transport
  passed, also with AddressSanitizer/UndefinedBehaviorSanitizer.
- Native-event to Python to canonical WAV integration:6 passed, no skips.
- Audio architecture:4 passed.
- Complete Python suites:1692 tests in normal and optimized mode,7 explicit
  skips each; no failures. A later description-only wording change passed the
  focused81-test suite in both modes, with1 native-only skip.
- Initial stale fixture/prose failures were retained and corrected, not skipped.
- Live observations and recordings are separate evidence from native fixtures.
- Remote CI is not claimed by these local checks.
