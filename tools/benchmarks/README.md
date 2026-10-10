<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Created 2026-10-10. Full terms: OpenXcom LICENSE.txt. -->
# OpenXcom observation and control benchmark protocol

## Status and acceptance gate

Current `read_last_hud` / `read_last_cursor` tools expose historical observations. They do not control the game or attest current frame freshness. `require_current` fails closed. Consequently, this package's offline codec measurements are NOT an MCP-versus-mouse benchmark, an end-to-end speedup, or evidence of model-token savings.

**A final MCP-versus-GUI comparison requires measured model input/output/cached token usage across every model call and retry. Missing telemetry blocks that comparison; it is never zero.** Preserve reasoning and cache-write counts when reported. No API calls, model billing, or credential handling are implemented here.

The harness generates copyright-independent synthetic RGB inputs and can consume separately supplied, lawfully available local cursor observations. It writes only measurement metadata, not pixels. Do not distribute proprietary game resources, saves, screenshots, or crops. Source code uses GPL-3.0-or-later; consult the project's full LICENSE.txt. Game assets retain their separate rights.

## Reproduce the offline baseline

From this directory, with a compatible read-only adapter source directory:

    python benchmark.py --adapter-dir ../mcp-observer --repeats 50 --out ./local-results
    python -m unittest discover -s . -v

Optional local input: repeat `--cursor /path/to/cursor-observation.json`. Never commit raw input assets or raw local measurement metadata automatically. Outputs omit host inventory and credentials; still produce an explicitly reviewed, anonymized report before publication.

Outputs: `samples.csv` has each operation, repetition/order, wall nanoseconds, process CPU nanoseconds, and success. `results.json` contains input SHA-256, exact byte counts, independent PNG decode equality, per-operation medians/p95, source hashes, and limitations. Exact byte counts distinguish RGB, hex text, full snapshot JSON, PNG, base64, and JSON image-block framing. This is application payload, not TLS/TCP traffic. PNG base64 payload does not predict vision tokens. Microbenchmarks time validation, lossless encoding, base64, cache hit validation, and image-block serialization separately. They do not measure stdio IPC, filesystem reading, capture, or a model.

The first recorded invocation is not a cold-machine sample. Startup validation and input reads have already happened. Warm results and cache-hit results are separate. Process peak RSS is cumulative for the whole harness, not per-operation memory. Timing under background load is diagnostic only. Deterministic synthetic noise stresses incompressible data and prevents presenting only unusually compressible game images.

## Three experiments, never conflated

1. **Offline microbenchmark (implemented):** same RGB bytes before/after lossless conversion, exact decode equality, byte/CPU/latency measurements. Historical JSON parse and image conversion only.
2. **Observation task (future instrumented runner):** paired screen and structured observations from the same public visible state, e.g. report selected unit TU/energy/health/morale. Screenshot must contain exactly the relevant UI facts, and structured route must not expose hidden game/save state. Frozen matched observations can test interpretation and token cost, but not fresh acquisition latency. A full-screen baseline and a tightly cropped baseline are different declared conditions; report both when appropriate.
3. **End-to-end action task (blocked until controller + freshness):** restore the same initial save/seed, issue the same goal, allow equal public knowledge and legal actions, stop at the same success criterion or budget. Examples: select a visible unit and report its stats; move to an already-visible reachable tile then verify; open/close an in-game panel then verify. No full-game control comparison until both routes can perform these actions and obtain trustworthy post-action observations.

## Fair paired design

- Pin game source revision, ruleset/mods, save/seed identity, game speed, resolution, UI scale, viewport/crop and image detail/codec. Restore initial state before each arm. Record reproducibility hashes privately when source cannot be redistributed.
- Pin exact model version, reasoning effort, context policy, tool definitions, prompts, output format, limits, service tier, image-detail setting and supported sampling parameters. Same goal and scoring rubric; no precomputed answer or extra private state in either arm. Different transport wrappers are part of the measured condition.
- Freeze a task manifest and budget before results. Use 30 paired repetitions per task as an initial plan, balanced AB/BA order with saved random seed; expand only by a predeclared precision rule. Independent model conversations per run. Distinguish API prompt-cache state, adapter image cache, process startup and OS page cache; do not call one of these the other.
- Capture tool start/end and model request/response intervals using monotonic clocks; record causal order. Wall time runs from task dispatch to verified result and includes setup if deployment scenario requires it. Sum non-overlapping intervals; concurrent intervals cannot simply be added to wall time. API elapsed time includes queue/network/server time, not measured pure model compute.
- Include every screenshot, mouse/key action, structured call, verification, timeout, retry, tool error, and failed run. Identify planning and verification model calls. Publish success rate alongside cost; report attempts and resources per successful task without hiding failure costs. Wrong/fabricated/stale answers fail. Score HUD exact values and missing/unsupported cases separately.
- Independently verify final game outcome and observation parity. Use a public-output oracle or blinded grader; never give the evaluation oracle to the agent. Score unit/state identity and frame freshness, not just matching numbers.
- Report paired differences (MCP minus GUI), median/p95 latency, token totals, success rate and error categories, with paired bootstrap confidence intervals over trials/task strata. Speedup GUI/MCP and percent reduction 100*(GUI-MCP)/GUI need nonzero denominator, matching task quality and declared aggregation. Do not average arbitrary percent ratios or turn a transport compression ratio into model savings.

## Metric dictionary (English / Русский)

- `wall_ms`: task dispatch through final verified outcome / время от старта задачи до проверенного результата.
- `tool_elapsed_ms`: individual request/response interval including acquisition and transport / длительность вызова инструмента, включая получение данных и передачу.
- `model_elapsed_ms`: client-observed model call interval / наблюдаемое клиентом время запроса модели; не чистое время вычислений.
- `tool_calls`: all capture/action/observe/verify/retry calls, grouped by kind / все вызовы, в том числе снимки, действия, проверки и повторы.
- `wire_payload_bytes`: exact serialized request/response bytes at named boundary / точные байты на указанной границе протокола; изображения и JSON отдельно.
- `input_tokens`, `output_tokens`, `cached_tokens`, `reasoning_tokens`: provider-reported usage only / реальные счётчики поставщика, не расчёт по размеру файла. Cached is inside input; reasoning is inside output. Do not double-count.
- `cpu_ms`, `peak_rss_bytes`: instrument game, capture/encoder, adapter, and runner independently / CPU и память игры, захвата/кодека, адаптера и теста по отдельности. Mark missing instruments unknown.
- `success`, `accuracy`, `freshness_verified`, `parity_pass`: completion, correct values, post-action currency, equal knowledge / успешность, точность, свежесть после действия и равенство доступной информации.
- `estimated_cost` versus `bill_cost`: pricing calculation versus reconciled charge / расчёт по тарифу отдельно от фактического списания. Store currency, date, tier and rates. Compute and money cannot be inferred from tokens alone.

## Actual model usage instrumentation

The [official reasoning guide](https://developers.openai.com/api/docs/guides/reasoning) documents per-response input/output/total usage and nested cached/reasoning counters. The [official observability guide](https://developers.openai.com/api/docs/guides/agents-api/observability) explains subsets, repeated calls and incomplete accounting. Consult current endpoint documentation at implementation time.

`usage_ledger.py` is a network-free collector for response dictionaries or SDK response objects supplied by a caller. Before each API attempt call `begin()`, and call `finish(ordinal, response, status)` afterward, even on incomplete/error paths. It deliberately strips text, provider response IDs and account data. Tests use visibly synthetic counts only. Disable opaque automatic retries or hook every attempt. A transport failure with unknown usage leaves the final gate incomplete until supported provider accounting resolves it; never assume an unobserved attempt was free.

A future runner must execute the entire tool loop for both arms, save every request's numeric usage immediately, track all tool invocations and retries, preserve the same model context policy, and reconcile final counters without counting both cumulative turns and their component calls. If an SDK exposes only final aggregate usage, document its scope and do not mix that aggregate with per-call rows. Record unavailable reasoning as null; for a model expected to report it, missing reasoning requires investigation. Actual model compute is generally not supplied by token counters.

Required next access: an explicitly authorized model endpoint/client capable of returning usage (or a supported controlled-run usage export), exact model and settings, permission to send the selected fixture content, and a total spend cap before paid trials. Keep credentials in the provider's supported secure setup; never put them in chat, fixtures or source. A simulated unit test or local tokenizer is not acceptance evidence. Public reports require explicit review of model/version and numeric usage metadata; exclude machine inventory, host paths, account/request IDs and private operational details.

## Russian working summary

Финальное сравнение обязано измерять реальные входные, выходные и кэшированные токены всей цепочки, включая повторы. Reasoning сохраняется, когда его возвращает API. Пока телеметрии нет, эксперимент не завершён. Нынешний тест измеряет только байты, кодек и локальную обработку исторического наблюдения. Он не доказывает экономию токенов, ускорение управления игрой или актуальность состояния. Сравнение управления откладывается до появления контроллера и проверки свежести. Снимок экрана и MCP должны давать одну и ту же информацию о той же задаче; скрытые данные сохранения запрещены.
