# Development Workflow

## Requirements

- `g++` with C++17 support
- `yaml-cpp`
- C++ standard library thread support
- `make`

Optional tools:

- `valgrind` for runtime checks
- `clang++` for ThreadSanitizer checks
- `pdflatex` for the UML diagram target

## Common Commands

Build the main binary:

```bash
make
```

Run a known-good example:

```bash
make run CONFIG_FILE=config/2FeFET_TCAM/2FeFET_TCAM.config.yaml
```

Run the shipped compiled subarray-dimension matrix:

```bash
make subarray-dimension-test
```

Override the tester config or concurrent run count when needed:

```bash
make subarray-dimension-test \
  SUBARRAY_DIMENSION_TEST_CONFIG=config/2FeFET_TCAM/2FeFET_TCAM.subarray_dimension_test.yaml \
  SUBARRAY_DIMENSION_TEST_JOBS=8
```

Run focused tests:

```bash
make test-yaml
make test-exploration
```

Run the ultra-long deep-exploration threading regression separately:

```bash
make test-deep-exploration-threading
```

This test exhausts the fully unpinned deep geometry and mux domains once with
1, 2, 3, 7, and 16 workers. It forces scheduler yields, compares exact candidate
accounting and the selected organization for every optimization objective, and
prints informational scaling timings without imposing a timing assertion.

Run the focused multithreading suite under Clang ThreadSanitizer:

```bash
make test-thread-sanitizer
make TSAN_STRESS_ITERATIONS=5 test-thread-sanitizer
```

ThreadSanitizer uses isolated objects under `thread-sanitizer/`. The stress
count repeats the deterministic thread-count matrix, worker-exception and
one-shot lifecycle tests, concurrent quiet/visible Python-backend runs, and
mixed `run()`/`EvaCAMMatch` configuration loading.

Run the same suite under Helgrind:

```bash
make test-thread-helgrind
```

Pull requests run ThreadSanitizer once. Scheduled CI repeats ThreadSanitizer
five times and also runs Helgrind.

Run the default valgrind check:

```bash
make test
```

Run the broader config sweep:

```bash
make test-all-valgrind
```

Synchronize the default configuration library packaged with `evacam_py` after
changing files under `config/lib/`:

```bash
make sync-python-package-data
make test-python-package-data
```

Generate the default deterministic corner-sweep configs without running EvaCAM:

```bash
python3 scripts/run_corner_sweep.py
```

Run the default corner sweep after building `EvaCAM`:

```bash
make -j
python3 scripts/run_corner_sweep.py --run --jobs 16
```

The default helper sweep covers the supported memory-device on/off corner
inputs at `0%`, `2%`, `4%`, `6%`, and `8%`, excluding the all-zero case. Use
`--corner-values` to provide different percent levels and `--corner-fields` to
limit the sweep to `on`, `off`, or their full YAML field names.

## Editor Tooling

This repository depends on multiple include directories from the `Makefile`. Editors and linters such as ALE or `clangd` may show false include errors unless they can read a local `compile_commands.json`.

Generate it locally from the current build commands:

```bash
./scripts/generate_compile_commands.sh
```

The generated file is local editor metadata and is ignored by git.

## Test Style

The current tests are small assert-based executables under `tests/`; there is no external unit-test framework. Add focused regression tests close to the parser or exploration logic you changed.

## Editing Guidance

- Keep code in the intent-based module directories:
  - `app`: top-level workflow and exploration
  - `input`: CLI parsing and YAML input helpers
  - `output`: result serialization helpers
  - `config`: config loading, validation, and derived settings
  - `technology`: process and cell models plus technology tables
  - `circuit`: reusable circuit primitives and equations
  - `model`: bank/mat/subarray/result hierarchy
  - `cam`: CAM-specific implementations
  - `factories`: object construction helpers
- Keep new YAML examples under the grouped `config/` tree, which is the canonical layout for active configs
- Use v2 filenames for active configs: `*.config.yaml`, `*.architecture.yaml`, `*.cell.yaml`, `*.memory_device.yaml`, and `*.sensing.yaml`
- Do not add new `*_tool_config.yaml`, `*_architecture_config.yaml`, or `*_cell_config.yaml` files except migration/reference fixtures
- Treat `config/old_style_config/` as legacy reference only; do not add new examples there
- Update the matching docs when you add or remove parsed keys
- Prefer `README.md`, `docs/input-files.md`, `docs/schema.md`, and `docs/results-reference.md` for user-facing changes
- `docs/input_samples/` contains reference-only v2 input samples, not physically valid experiments
