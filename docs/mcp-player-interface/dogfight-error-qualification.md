# Interception-warning qualification — 2026-10-22

## Candidate identity

The measured feature candidate is commit
`59f9aafe9f5bc7be7f8162ba041356d41408b7a1`, tree
`4256ac67e5e65cf66693190e8cf7d496e7692dbe`.
The Release, experimental-agent-control ON binary SHA-256 is
`47f887d0d79ca78eb2d315e4b8a44c5c952cd1f18eabf55da4040ad9b419013d`.
The candidate adds explicit `continue_pursuit` and
`return_interceptor_to_base` choices to the active `dogfight_error` popup;
it neither chooses automatically nor exposes hidden altitude/depth/world data.

## Checked without live acceptance

- Full Release ON build passed.
- Actual constructor/provider/native-handler fixture: 187 checks passed normally
  and 187 under AddressSanitizer/UndefinedBehaviorSanitizer with vptr and leak
  checking excluded. This is scoped fixture evidence, not full-game sanitizer
  acceptance.
- Existing provider and controller regressions: 344 and 371 checks passed.
- Python adapter suite: 1,240 tests passed, with two pre-existing skips.
- Private bounded live-runner v2 regressions: 62 passed normally and 62 with
  Python `-O`. Synthetic runner checks are not real-game route acceptance.

Both popup choices have native fixture coverage only. Reading the popup never
selects either choice; dispatch is not proof of pursuit, combat or return.

## Natural TFTD attempts: not a popup PASS

The first attempt ended at 05:30:12 UTC with the preserved partial result
`visible_contact_cancel_required`: the runner could not establish its expected
visible Cancel readiness. It did not qualify either new popup action.

The v2 attempt ended at 05:42:45 UTC with **INCONCLUSIVE**
`ordinary_combat_not_depth_warning_no_error_action`. Ordinary pursuit reached
an actual dogfight showing STANDOFF, distance 626 and ammunition 6/200, rather
than the requested depth-warning popup. The new popup was never observed and
neither new action was dispatched. This is natural scenario mismatch, not a
passing depth-warning route or evidence that the popup provider failed.
Source/runner inputs remained unchanged; v2 recorded the clean candidate commit,
tree and binary identities above. No video or audio acceptance is claimed.

The first runner's hard-coded legacy `restored_base_commit` was incorrect and
must not be treated as its current source identity. Its recorded source-file
closure and binary hashes are the authoritative evidence for that attempt.
V2 replaced that legacy field with actual Git head/tree/dirty metadata.

## Still pending

Natural live observation and separate explicit execution of **both** popup
choices remain pending, including the depth-related and airborne engagement
restriction cases. Return-to-base must be confirmed by later public craft/base
UI, not popup disappearance. Continuing pursuit needs its own bounded observed
outcome. Existing ordinary-combat qualification does not substitute for these
warning-specific routes; neither complete mode coverage nor hidden enemy damage
is claimed here. Private resources, captures and raw logs are not published.
