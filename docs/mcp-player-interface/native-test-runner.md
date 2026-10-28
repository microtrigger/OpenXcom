# Generic native test runner

`tests/agent-control/native_test_runner.py` is a Python-standard-library runner
for **trusted** compile, link and test commands. It is not a sandbox and does not
make an untrusted executable safe. Existing host permissions and network policy
still apply; no elevation, tunneling, shell evaluation or security changes occur.
Review the recipe and every executable before running it. An argv command can
itself invoke a shell or arbitrary program: `shell=False` does not prevent that.

## API and CLI

```python
receipt = native_test_runner.run_recipe(recipe_dict)
```

```text
python tests/agent-control/native_test_runner.py recipe.json
```

CLI exit codes: `0` all steps passed with unchanged inputs; `1` a command failed,
failed to launch, or inputs changed; `2` invalid recipe / runner I/O refusal.

Version 1 requires exactly these fields:

```json
{
  "version": 1,
  "source": "source-tree",
  "output": "new-output-directory",
  "inputs": ["source-tree/example.cpp"],
  "steps": [
    {
      "name": "compile",
      "argv": ["c++", "-c", "source-tree/example.cpp", "-o", "new-output-directory/example.o"],
      "cwd": ".",
      "env": {}
    }
  ]
}
```

Paths in `source`, `output`, `inputs` and `cwd` resolve relative to the runner's
initial working directory, **not the recipe file**. Command argument paths obey
the explicit command `cwd`. No variable substitution or token expansion is
performed. Spaces and shell metacharacters remain literal argument contents.
Use absolute paths in generated recipes when command working directories differ.

`source` must exist. `output` must not exist and must be outside the resolved
source tree. Every step has an explicit existing `cwd` (or the newly created
output directory), a nonempty argv array, and an explicit environment overlay.
The child inherits the host environment plus that overlay; environment values
are never serialized by the runner. Commands can still print secrets: keep logs
private and review them before publishing. Do not put credentials in recipes.

`inputs` is the explicit nonempty, duplicate-free file manifest. Include test
sources, headers, runner/recipe, generated inputs, compiler/linker tools, linked
objects and relevant data whenever they affect the result. The runner hashes
only listed files, not transitive dependencies. Compile/link artifacts belong in
`output`; inputs cannot be inside it. No assets or results are bundled with this
runner. A native-specific harness may prepare a private recipe and inputs, then
call this API; game-specific fixtures and model seams remain outside this tool.

## Receipts and boundaries

Each step produces `<name>.log` (combined stdout/stderr) and an exit code in
`RESULT.json`; launch failures have a null exit code and an exception class.
Execution stops at the first failed step. Receipt writes use atomic replacement
inside the unique output directory. Specified inputs receive SHA-256 hashes
before and after execution. `all_passed` requires every step exit zero and all
input hashes unchanged; missing/unreadable post-run files fail this check.

This is evidence of those commands and listed input bytes only, not live gameplay,
full acceptance, absence of subprocess side effects, or a dependency-complete
reproducible build. A file changed and restored between the two hashes is not
detected. Process crashes, forced termination, disk failures or a malicious child
can leave incomplete/untrustworthy results. There is no timeout or process-tree
containment; use trusted bounded test commands. The runner does not delete,
restore, upload or publish anything. Output collisions are refused, never reused.

Run unit tests in normal and optimized Python:

```text
python -m unittest discover -s tests/agent-control -p test_native_test_runner.py
python -O -m unittest discover -s tests/agent-control -p test_native_test_runner.py
```

## Containment harness integration

The tool branch contains two harnesses that call `run_recipe` for **each real
compile/link/test step**. They do not wrap the old runner in a single opaque step
and do not replace its native assertions. Gameplay fixture sources and assets
remain external dependencies supplied explicitly by the caller.

```text
python tests/agent-control/run-containment-tests.py --source gameplay-source --build gameplay-build --sdk native-sdk --data runtime-data --output new-native-output
python tests/agent-control/run-containment-fixture-tests.py --source gameplay-source --sdk native-sdk --output new-prepare-output
```

All argument paths are relative to the invocation directory. `--sdk` must name
the prefix containing `include` and `lib/x86_64-linux-gnu`. `--data` must contain
`common` and `standard/xcom1` / `standard/xcom2`. The native harness also expects
the original test fixture's sibling resource layout, through `data.parent`.
Only use an explicitly reviewed compatible source/build/resource set. Output
must be new, outside source; the native harness requires its parent to exist.

The native harness preserves the original source extraction, State cursor seam,
actual fixture assertions, native objects and cost/resource subset limitations.
It prepares generated source in a separate temporary input directory, records
those files and the generator/core/link recipe in the hash manifest alongside
the original source/resource/object closure, then sends five compile steps,
link and native test execution to the generic core. Temporary generated inputs
are removed after the run; their hashes remain in the private receipts and they
can be regenerated from the recorded inputs. No source or gameplay modifications
are made by the harness.

The prepare-only harness uses the core for fixture compilation, linking, `nm`,
an explicit symbol-verification subprocess, six argument-alias checks and the
three selected architecture tests in normal and optimized Python. It does not
execute campaign setup. Its manifest includes the production source subtree
read by the architecture tests, not just their Python entrypoints. Resource-
dependent mission architecture and live acceptance remain separate suites.

`RESULT.json` is written by the generic core. `QUALIFICATION.json` separately
records fixture limitations and expressly denies live/Game acceptance. The
explicit manifests are not complete compiler, SDK-header, dynamic-library or
toolchain transitive dependency closures; unchanged hashes prove only listed
input bytes. Environment overlays and generated absolute private paths appear
only in memory or local receipts, not this distributed source. Review outputs
before any sharing.

**Minimum published gameplay API dependency:**
[`faa62d0224c0b7d6a247eab558da0dde190ec420`](https://github.com/microtrigger/OpenXcom/commit/faa62d0224c0b7d6a247eab558da0dde190ec420).
The supplied native build must match the supplied source. This minimum API pin
is not a claim that every later local qualification used that exact revision.
In particular, local schema39 source/build qualification is distinct from the
published schema37 dependency. Runtime receipts deliberately say
`source_revision: caller-supplied; inspect recorded input hashes`, rather than
invent a reviewed revision for an arbitrary caller checkout. Use the recorded
source/object hashes to establish the precise tested closure; do not attribute
a later local result to this minimum commit without checking the full chain.

Neither harness downloads dependencies, changes permissions, or bypasses host
security/network restrictions. Executed helper source and binaries are trusted
code, not sandboxed by the generic runner.
