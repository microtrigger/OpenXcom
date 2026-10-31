# Diary decorative visibility native regression

This source-only harness qualifies the narrow Diary medal visibility patch against public base commit `5ab560ef15211d599b9f6a49162dc64696bf6236` (plain wire 50). It compiles actual engine/UI/provider code and compares the unmodified baseline's visibility prefix with the candidate's. No complete baseline source, game binary, SDK, fonts, retail data or campaign saves are distributed here.

## Reproduction

Use a checkout containing the candidate patch, a compatible SDL 1.2/yaml-cpp SDK prefix and legally available stock data. Extract the unchanged baseline file from the pinned commit in a repository that already contains that commit; no network access is required by this harness:

```sh
BASELINE=$(mktemp)
git -C "$REPOSITORY" show 5ab560ef15211d599b9f6a49162dc64696bf6236:src/Basescape/SoldierDiaryPerformanceState.cpp > "$BASELINE"
sh "$CANDIDATE/tests/agent-control/diary-medal-visibility/run.sh" "$CANDIDATE/src" "$SDK_USR" "$STOCK_DATA" "$BASELINE" /path/to/NEW_BUILD
```

The output directory must not exist. Generated source extracts, copied harness, objects and executable are written only there, not into the source checkout or this bundle. Both baseline and candidate paths are explicit CLI inputs. The baseline SHA256 is pinned and recorded in EXTRACTION.json; mismatching bytes are rejected. Output paths inside the source checkout or harness bundle are rejected after resolving symlinks. Expected terminal output: `PASS checks=556 no SDL video/audio`, preceded by 16 actual provider JSON records (two games, living/memorial, Combat/Missions, baseline/fixed). A baseline-only candidate is expected to fail caption-restoration assertions. The harness never launches the full game or initializes SDL video/audio.

## Scope and seams

25 native translation units compile afresh, including actual Font/Text/TextList/Surface/Window/AgentUi/AgentProtocol. Font glyphs and stock English language data are loaded from the caller's external data directory. Exact native provider, paint and dead-roster methods are extracted; only the init prefix before model population is exercised. Constructor geometry and rows, Game/Screen/State/Mod/Base/Soldier/SavedGame objects, synthetic Awards pixels and direct mode assignments are explicit fixtures. Full constructors, campaign state, native dispatch/controller/socket and button callbacks are not executed. Fixture popup/audio/timer/RNG stubs throw if invoked and must remain uncalled.

Assertions preserve exact ordered captions, values and separators; baseline omissions; 320x200 8-bit raster-index equality (not rendered RGB); Awards visible medals/provider/raster; mode round-trip; fresh binding and paint; overlay/modal/clipping, dirty/ABA and roster-negative guards. Headings outside Awards remain intentionally null while ordinary section headings are rows. No generic visibility or freshness guard is weakened and no text is reconstructed from hidden campaign data.

This is an offscreen native regression, not live public-branch acceptance, production adoption or complete player-equivalent coverage. Full build, adapter integration and gameplay qualification remain separate gates. Prior failed qualifications must be retained separately; a new passing harness does not erase them.
