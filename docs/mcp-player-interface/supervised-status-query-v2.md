# Supervised status query correlation, internal protocol version 2

Internal hello is `experimental-stance/2`; internal status is
`experimental-stance-status/2`. Old/new peers fail closed at hello; no fallback,
retry or silent downgrade is permitted. Context schemas and action wire formats,
human confirmation, command ledger, timeouts and gameplay handlers are unchanged.

An explicit status request is exactly `S` + 32-lowerhex command ID + a nonzero
32-lowerhex query nonce (65 bytes). Native replies echo that nonce for an existing
command. Old 33-byte queries, zero/malformed nonce or malformed tokens fail closed.
Unknown command retains the ordinary unavailable response; it grants no authority.
Initial/action-replay/conflicting-ID status replies carry `query_nonce:null`.

The Python client allocates increasing128-bit per-connection query nonces, never
wraps, and closes without sending on exhaustion. This also applies to exact
repeated action requests, which remain queries rather than action resends. It
checks echoed command ID and nonce before updating its ledger; null/old/different
nonce is refused without draining or retrying. Initial receipts require null and
still cannot claim `dispatched`. Existing terminal-regression guards remain.

Private client receipts retain exactly `{schema,command_id,status}` with internal
schema/2. Query nonce is removed after validation. MCP public status remains
`openxcom-mcp-stance-status/1` with the same schema, command_id, status and semantics
fields; no new transport metadata or capability is exported.

This closes the identified same-ID status-query correlation gap. It does not make
an instantaneous zero-time readability check a guarantee that no extra packet can
arrive later. Existing pre/post checks remain; stale correlated replies fail closed.
The earlier aggregate malformed-receipt failure had no recorded response case, and
its cause remains unknown. Two instrumented exact tests did not reproduce it; that
is not a fix claim. Deterministic schedule fixtures test specified barriers, not
historical scheduling. Legacy same-ID acceptance is source inference unless a
separate old-code characterization is explicitly qualified.

Tests distinguish actual extracted native parser/reply bodies with ledger/transport
seams, Python fake-clock/socket/select phase barriers, socketpair fixtures, syntax
qualification of migrated native gates, and any later full aggregate or live run.
No GUI or live game is part of this increment. No automatic uncertain-action replay.
