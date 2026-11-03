# Craft ammunition captions — wire schema 58

Stage60 changes native presentation evidence for existing CraftInfo ammunition
strings. Public fields and actions do not change: exactly two weapon entries,
each `{name, ammo}`, independently nullable printed strings. Existing UTF-8 and
packet limits remain unchanged; multiline strings already fit this contract.

A complete visible AMMO/MAX/rearm-time caption is preserved as printed text,
including line boundaries and locale formatting. Null means unknown, not zero,
empty weapon, ready status or absence of ammunition. No model count, rule ID,
calculated rearm time or numeric interpretation is added.

Native final paint, full text extent, clipping, overlay and freshness proof is
required. Adapter tests establish string shape/transport, not visibility or
campaign success. Do not infer the original null's cause from text metrics until
the native renderer fixture demonstrates it. Broader coverage remains open.


## Bounded live result, 2026-10-29

Both games used dedicated copies of earned test campaigns and ordinary same-weapon remounts, no injected ammo. UFO left Stingray printed0/MAX6/(6hours), then1/MAX6/(5hours), then6/MAX6 without ETA. TFTD right Craft Gas Cannon printed0/MAX200/(4hours), then50/MAX200/(3hours), then200/MAX200 without ETA. Current own-base snapshots agree with rearming flags and returned/consumed clips; both reached READY/full fuel and unique MCP saves. Gameplay was MCP-only with no visual fallback. These checks do not accept arbitrary mod fonts/layouts, actual service shortages, positive repair or full current-build game cycles.
