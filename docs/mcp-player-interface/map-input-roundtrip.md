# Admitted map input: coordinates and bounded diagnostics

MCP grid points are canvas-absolute, while the native map selector expects
map-relative coordinates. The MCP activation path now subtracts the freshly
admitted map origin. The ordinary physical mouse handler is unchanged. A focused
literal-source fixture demonstrated the old nonzero-origin mismatch and the
corrected roundtrip. This latent defect does **not** explain the investigated live
long-route differences: both observed viewports had origin `(0, 0)`.

`OPENXCOM_AGENT_MAP_INPUT_DIAGNOSTICS=1` enables a bounded owner-local log of MCP
map activation. It copies only the already-public chosen tile, canvas point and
map origin, then records the local selector position immediately before the
ordinary primary/secondary call. Fixed cancel/guard/no-dispatch outcomes are
separate from dispatch; no record promises arrival. No actor identity, authority
token, hidden route, path cost or world lookup is added. At most 32 log attempts
occur per process, including failed sink attempts. Logging exceptions are
suppressed and errno is preserved; this does not promise filesystem latency or
successful persistence. Runtime logs are not part of the source distribution.

A focused fixture executes literal conversion, selector and activation code with
synthetic camera parameters, tile-presence and primary-action sinks. A second
focused fixture tests the actual trace helper with a synthetic logger, including
opt-out, scope restoration, throwing sink and the bounded attempt count. These
are not full-game pathfinding or historical camera reconstructions. The combined
native build and CLI readback passed; the full aggregate suite was not requalified.

One short live adjacent-step check subsequently recorded equality between the
public admitted tile and the exact primary-call argument. The fresh public unit
position then reached that tile, spending four time units. This qualifies that
single invocation only. Earlier longer-route endpoints remain unexplained by the
retained evidence: normal input means an action is no longer busy, not that the
requested destination was reached. Native pathfinding and interruption semantics
were not changed. Future ordinary movement should check the fresh endpoint and
replan; it must not automatically replay an old target or suppress native stops.
