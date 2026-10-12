# License and redistribution notices

## Dated fork modification notice: 2026-10-12

New contributions maintained in this fork add optional, default-off read-only
HUD, final-pixel cursor and fixed-window final-mix audio observers, a local
read-only stdio MCP adapter with optional transient presentation acknowledgements
and native MCP audio content, a separately opted-in final-pixel displayed-HUD decoder, isolated render-pair and
observer-request functional-effects gates, a diagnostic-isolation boundary,
controlled smoke-density and full right-edge smoke-clipping render-equivalence
gates with isolated negative-test patches, a default-off locally confirmed
stance-control pilot, an independently opted-in locally confirmed next/previous
unit-selection extension, an opt-in ordinary map right-click pilot, and associated tests, build helpers,
documentation and evidence metadata.
Existing upstream copyright, license and warranty notices are retained; new contribution
notices do not replace upstream attribution.

OpenXcom core source headers grant GNU GPL **version 3 or any later version**
(`GPL-3.0-or-later`). The unmodified [LICENSE.txt](LICENSE.txt) contains GPLv3.
The new project additions listed below are likewise offered under GPL version 3
or, at your option, any later version, without warranty:

- `src/Observer/`
- `tests/observer/`, `tests/cursor-observer/`, `tests/paired-observer/`,
  `tests/battle-observer/`, `tests/mixed-audio-native/` and
  `tests/diagnostic-isolation/`, `tests/stance-control/` and `tests/turn-control/`
- `scripts/build-observer-prototype.sh` and `scripts/observer-build-provenance.py`
- `tools/mcp-observer/` (source, tests, documentation and generated metadata)
- `tools/benchmarks/` (original source, tests, protocol and templates)
- `docs/mcp-player-interface/` (new project documentation and generated metadata)

The natural-FOV fixture's independent glyph row masks are adapted from
`src/Interface/NumberText.cpp`, Copyright 2010–2016 OpenXcom Developers,
GPL-3.0-or-later. Its test source retains this attribution.

Existing engine files retain their original notices; dated comments identify
this fork's observer-hook changes. Choosing GPL-3.0-or-later for the new adapter
is a conservative compatible project choice, not a claim that an IPC/process
boundary conclusively requires or exempts a component from GPL obligations.

The fair-play, historical-observation and safety requirements describe this
project's design and supported-use contract. They do not alter recipients' GPL
rights, restrict fields of use or impose additional copyright-license terms.

## Third-party scope is preserved

This is **not** a blanket relicensing of the repository. Preserve the specific
notices accompanying existing components and artwork, including:

- Scale2x/Scale3x: GPL-2.0-or-later.
- hqx and fmopl: LGPL-2.1-or-later.
- xbrz and scaler `config.h`: their GPLv3 notices and MAME exception; no new
  “or later” grant is asserted for these files.
- Weapons artwork: CC-BY-SA-4.0.
- Other existing bundled dependencies retain their own licenses and notices.

The optional official Python MCP SDK 1.29.0 is MIT licensed, copyright 2024
Anthropic, PBC: [pinned SDK license](https://github.com/modelcontextprotocol/python-sdk/blob/v1.29.0/LICENSE).
It is used only by optional interoperability tests and is neither vendored nor
bundled here; its transitive dependencies are not bundled either. The adapter's
runtime uses the Python standard library. This project does not relicense the
SDK. Distributing it later requires retaining its MIT notice and checking the
other bundled dependencies then.

## Publication boundary and later binary distribution

The project additions covered here are source, documentation and metadata only:
no new binaries, dependency archives, original UFO/TFTD resources, saves or rendered
game screenshots or raw/rendered game audio and PCM. These code licenses do not
grant rights to redistribute original soundtracks or sound-effect recordings.
Existing repository history already includes some Windows
DLL/LIB files under `deps/lib`; their presence is not a certification of the
redistributability of the entire repository or a future binary package.

A later executable/package release requires a fresh component inventory, the
applicable notices/licenses and exact corresponding source/build instructions.
Check LGPL replacement/relinking obligations where applicable. Do not infer
binary-distribution compliance merely from a successful build or this source
publication. Users must separately obtain lawful original game resources;
OpenXcom's code license does not grant rights to redistribute those resources.

## References and limits of this review

See the [official GPLv3 text](https://www.gnu.org/licenses/gpl-3.0.html) and the
[GNU FAQ on aggregation](https://www.gnu.org/licenses/gpl-faq.en.html#MereAggregation).
This is practical source-publication compliance guidance, not a legal opinion
or a complete audit of every historical repository component.
