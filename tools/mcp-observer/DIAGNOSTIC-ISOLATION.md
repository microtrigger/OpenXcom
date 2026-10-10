<!-- SPDX-License-Identifier: GPL-3.0-or-later. New contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Diagnostic isolation for the read-only observer

The supported launch boundary discards the game's stdout and stderr at process
creation. Both descriptors point to the operating system null sink. They are not
MCP output, adapter diagnostics, files consumed by the adapter, or captured pipes.
This includes startup output, shutdown output, invalid text, large output, and the
engine logger's stderr fallback when its normal log file cannot be opened. There
is no diagnostic reader thread, decoding step, or growing diagnostic buffer.
Normal engine log-file behavior is unchanged; those files are never an observer
input. Protocol EOF still ends observation without stopping the game.

The matching native build additionally requires a qualified effective diagnostic
configuration. After the actual option/configuration parser and logger setup,
before constructing the game or sending an observer hello, all of these must hold:

- `debug`, `debugUi`, `traceAI`, and `verboseLogging` are false.
- The effective logger severity is between `LOG_FATAL` and `LOG_INFO`, inclusive.
- Diagnostic qualification has been initialized by the normal application startup.

A debug build whose logger runs at `LOG_DEBUG` is therefore unqualified even when
all four stored options are false. No launcher flag filtering, presumed defaults,
or silent rewriting of a user's profile can substitute for these native checks.
The launcher does not turn options off to obtain a successful observation.

The native predicate is rechecked at channel admission and publication, including
before sending a completed audio window. Once a recheck rejects the profile, that
process cannot regain qualification by restoring the options. Pending output is
not published; a new process with a qualified configuration is required. The
main-thread checks do not inspect settings from the audio callback. Historical
writers recheck before publishing or reporting themselves enabled and release
their locks on rejection. Previously lawful historical snapshot bytes remain on
disk, with their original historical/unverified meaning. The existing separate
saved-battle debug-mode HUD guard is retained.

Launch argument, socket setup, process-spawn and adapter transport failures use
fixed bounded diagnostics without command arguments, paths, exception messages,
raw child bytes or tracebacks. Invalid protocol payloads still return the existing
generic observation-unavailable error. A broken model output pipe and intentional
live-game exit from the wrapper do not generate Python shutdown diagnostics.
`--help` remains an explicit operator help mode, outside an MCP session.

This boundary assumes the trusted operator selects this matching native build and
launcher. Existing hello strings and public schemas are unchanged; the protocol
is not executable authentication and does not attest an older or modified binary.
Directly launching the game with model-visible diagnostic streams bypasses the
supported isolation boundary. Cooperative same-user trust and all existing pixel,
HUD, audio, timing and renderer limitations remain unchanged. There is no new
controller, network listener, credential, registration, or Stage 5 acceptance.

## Bounded checks

Run the ordinary Python suite and optional official SDK smokes from this folder.
Run `tests/diagnostic-isolation/run.sh` from the repository for the native profile
and logger tests. The native runner links the actual production qualification
predicate with synthetic option storage; it does not replace that predicate with
an always-true test stub. The other standalone native fixtures explicitly select
`LOG_INFO` and normal option values before qualifying.

Coverage includes every rejected option and both diagnostic logger levels before
startup, before request admission, and before pending publication; initialization
ordering; terminal rejection; unchanged prior historical bytes and released locks;
child startup/runtime/shutdown diagnostics; invalid UTF-8 and multi-megabyte output;
spawn/setup failures; malformed and oversized native packets; and the real native
logger's failed-file fallback. Unisolated positive controls confirm the sentinels
and fallback exist before testing their absence through the launcher. Real game
configuration parsing and menu integration require separate bounded game checks;
these synthetic tests make no game-runtime or hidden-state timing-parity claim.

## Verified results and limits

[Aggregate evidence](../../docs/mcp-player-interface/diagnostic-isolation-evidence.json),
2026-10-10, records these bounded results:

- The Python suite completes 208 test methods in both normal and optimized modes.
  Two native-only tests are skipped by ordinary unittest; the focused native runner
  separately passes all 18 sentinel tests and 24 native stage/flag cases. These
  overlapping counts must not be summed into a new total.
- Each final normal UFO/TFTD menu run returns a cursor PNG, a successful null
  displayed-HUD result and a 44,144-byte audio WAV.
- UFO with effective `traceAI=true` and TFTD with effective `debug=true` each return
  the same generic unavailable result for all three tools. Both game menus remain
  available; observer rejection does not stop the game.
- Launcher stderr contains zero bytes in all four final profiles.

Three earlier normal UFO attempts produced unavailable visual results. Audio
succeeded in the first attempt, was not requested in the second, and a late audio
request was unavailable in the third. The exact rejection cause was not
instrumented. The final normal run succeeded with the unchanged binary after
explicit input focus before the first request and supported `asyncBlit=false` /
filters-off configuration. These changes and the successful retry do not establish
a root cause for the earlier failures; all three attempts remain in the evidence.

This is bounded menu integration, not a new battlefield, full-fairness, physical
output, audiovisual synchronization, hidden-state timing-parity or broad lifecycle
claim. Earlier slice results remain separate. Qualification rejection is sticky
for the process, but each observer retires only at its next admission/publication
or enabled-state check, not through an instantaneous global tick. Stage 5, full
audio parity and controller acceptance remain open.
