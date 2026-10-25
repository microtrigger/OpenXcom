# Narrow read-timeout recovery

A completed read request (`R`) can time out before its reply is received without implying that any gameplay action occurred. The adapter can retain its existing owned channel **only** when an internal progress record proves a complete R send, completed handshake, and a select wait returning no readable descriptor before any receive. It is not a reconnect mechanism, and a generic timeout string is not sufficient evidence.

The adapter invalidates all UI grants and target caches and reports the fixed `read_recovery_required` classification. During this state only a new explicit read or close is allowed; action execution, duplicate-action receipt queries and event reads cannot send. Recovery additionally requires an empty command ledger or entries whose retained status is `dispatched`. Other terminal native statuses, pending or unknown outcomes conservatively disable recovery.

One subsequent explicit read may retire exactly one response matching the expired request's epoch and challenge, then obtain a fresh state under a distinct challenge. Retired UI is never returned as a current observation and its grant is never usable. Revision/event counters cannot regress, and the fresh capability must differ from the previous and retired non-null capabilities. Retirement and the fresh exchange share one original-duration read budget, not two added waits.

An instantaneous empty preflight poll does not prove that no delayed duplicate can arrive. Each subsequent reply must match its own new challenge. Unexpected, duplicate, malformed or uncorrelated packets, EOF, recovery timeout and unknown action outcomes fail closed. There is no drain-until-match, automatic retry loop, action replay or increased timeout.

## Native expiry response

The existing pending-read expiry boundary now returns an exact `read_error` packet using the ordinary agent-control envelope and these additional fields:

- `read_error_schema`: `openxcom-agent-read-error/1`
- `epoch` and `challenge`: the session and expired request correlation
- `reason`: `read_expired`

The response carries no capability, command outcome or UI state. Other generic native errors remain uncorrelated. An old client rejects the new response rather than granting authority. A new client can safely correlate a delayed state from an old peer, but an old generic unavailable response cannot complete recovery. A read_error received during an ordinary non-recovery read remains terminal in this first increment.

## Player workflow and qualification limits

The private player driver recognizes only the exact read-only classification and offers an explicit recovery-only `observe`/`finish` decision without stale gameplay controls. Other tool errors and every unknown mutation remain terminal. A post-dispatch read failure does not erase its already received dispatch receipt or replay the preceding action. Normal play resumes only from a fresh observation; saving still requires an actually offered save control and ordinary fresh authority.

Focused tests use deterministic packet/clock seams and actual driver/evidence wrappers. A synthetic offered-save dispatch is not proof of persisted game data. Native expiry fixtures and the current build are separately qualified. This feature does not guarantee recovery from a continuously stalled game, successful saving, or a passing full regression suite.

### Correlated expiry received during a normal read

An exact `openxcom-agent-read-error/1` response with reason `read_expired`, matching the current epoch and read challenge, can also produce `read_recovery_required` after a normal read exchange. The response has already been consumed and validated; the private pending context marks that fact. An explicit subsequent observation does **not** drain or wait for that response again: it sends one fresh read with a different challenge under the same existing recovery budget. The complete-handshake and empty-or-all-dispatched ledger restrictions still apply. The expired read grants no semantic authority.

Malformed, uncorrelated, generic, extra or duplicate packets remain fail-closed. Another expiry during the fresh recovery read is terminal, not recursive recovery. No action is replayed and no transport timeout is increased. This addresses a source-proven contract gap; it does not identify the unrecorded response responsible for the earlier restored-session read60 failure.
