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

## Containment harness integration: pending

The core is intended to run the original native containment compile/link/test
sequence, not replace its assertions with a demonstration. Integration is **not
yet completed or qualified**: the gameplay API dependencies and publication
boundary must first be reviewed. No containment recipe or harness is shipped in
this initial three-file core.

The proposed separate tool-branch harness prepares its generated fixture inputs
in a private temporary input directory, records their source dependencies,
collects the exact linked object closure from the reviewed build, and passes
compile/link/test argv steps to `run_recipe`. All generated source and object
inputs must appear in `inputs`; their contents are not embedded in a public
recipe. Qualification details (native fixture limitations, assertions and live
versus offscreen scope) remain a separate harness receipt, not an inflated claim
inside the generic runner's `all_passed`. Refactor and acceptance against that
original containment scenario follow dependency review; gameplay source and
resources must not be bundled into this tool branch.
