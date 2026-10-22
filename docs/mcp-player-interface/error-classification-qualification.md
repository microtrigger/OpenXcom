# Structured failure classification qualification — 2026-10-22

## Change and requirements

This bounded Python adapter change implements the decisions in the
[error-handling contract](error-handling-contract.md), not a logging service or
an automatic recovery loop. Native wire format, UI layout, gameplay and visible
state boundaries are unchanged. No glyph/readability gate is added or relaxed.

Twenty closed reason codes distinguish transport/framing/parsing/validation,
local admission, request identity, resource and concurrency failures, unspecified
native refusal and outcome uncertainty. Existing error/status keys are retained;
`failure` is additive. Consumers with an exact closed-key parser must allow this
new metadata. The native schema remains unchanged.

A failed read does not erase an earlier dispatched receipt. A native unavailable
receipt is conservatively classified unknown because the current native protocol
uses it both before and after entering a handler. No action is automatically
replayed, and unknown native close reasons are not guessed.

Research, allocation and production runners now preserve only allowlisted failure
metadata and method/status evidence, rather than discarding it into a single
`tool_error_stop_no_replay` label. They also stop for uncertainty metadata on a
valid receipt, even when `isError` is false. Other historical/supervised profiles
and their harnesses are outside this bounded migration.

## Checks

| Check | Result |
| --- | --- |
| Independent fault-injection suite, normal / optimized Python | 27 / 27 passed |
| Entire adapter Python suite, normal / optimized | 1,267 / 1,267 run; passed, 2 existing skips in each mode |
| Research/allocation/production runner tests, normal / optimized | 111 / 111 passed |
| Runner evidence privacy/vocabulary tests (included above) | 8 passed in each mode |
| Independent source review | Accepted after corrections below |

The 27 injected cases include transport deadline/EOF/empty record/framing/I/O,
parse and validation errors, stale authority and ineligible actions/targets,
ledger/resource/concurrency failures, unspecified native errors, handler mutation
followed by unavailable, unknown and pending receipts, opaque legacy exceptions,
result queries, preserved receipt identity, unsupported normal state, no replay
and no raw exception/payload leakage. They are synthetic protocol fixtures, not a
new real-game acceptance run.

Review caught and corrected two false-certainty cases: a zero-length SEQPACKET
record is not necessarily EOF, and an opaque execute exception cannot be labelled
“not sent” merely from the tool name. Result-query failures also preserve outcome
uncertainty. An initial broad test invocation had an incorrect working directory
and overlapped test-file finalization; it is not accepted as qualification. Final
runs use frozen inputs and the adapter directory required by subprocess tests.

No native files changed, so no new native build or live campaign is claimed.
Old recordings and raw evidence were not rewritten. In particular, this work
neither reproduces nor establishes the root cause of the earlier production
read error: its detailed response was not retained. Remaining gaps are explicit
in the path-family inventory, including indistinguishable native close causes.
