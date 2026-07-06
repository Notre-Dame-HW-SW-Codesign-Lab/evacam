# Development Workflow

## Requirements

- `g++` with C++17 support
- `yaml-cpp`
- OpenMP support
- `make`

Optional tools:

- `valgrind` for runtime checks
- `pdflatex` for the UML diagram target

## Common Commands

Build the main binary:

```bash
make
```

Run a known-good example:

```bash
make run CONFIG_FILE=config/2FeFET_TCAM/2FeFET_TCAM_tool_config.yaml
```

Run focused tests:

```bash
make test-yaml
make test-exploration
```

Run the default valgrind check:

```bash
make test
```

Run the broader config sweep:

```bash
make test-all-valgrind
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
- Treat `config/old_style_config/` as legacy reference only; do not add new examples there
- Update the matching docs when you add or remove parsed keys
- Prefer `README.md`, `docs/input-files.md`, `docs/schema.md`, and `docs/results-reference.md` for user-facing changes
- `docs/tool_config_full_example.yaml`, `docs/architecture_config_full_example.yaml`, and `docs/cell_config_full_example.yaml` are schema references, not physically valid experiments
