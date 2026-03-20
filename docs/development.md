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
make run CONFIG_FILE=yaml/config/2FeFET_TCAM_config.yaml
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

## Test Style

The current tests are small assert-based executables under `tests/`; there is no external unit-test framework. Add focused regression tests close to the parser or exploration logic you changed.

## Editing Guidance

- Keep code in the intent-based module directories:
  - `app`: top-level workflow and exploration
  - `io`: CLI parsing plus YAML/result serialization helpers
  - `config`: config loading, validation, and derived settings
  - `technology`: process and cell models plus technology tables
  - `circuit`: reusable circuit primitives and equations
  - `model`: bank/mat/subarray/result hierarchy
  - `cam`: CAM-specific implementations
  - `factories`: object construction helpers
- Keep new YAML examples under `yaml/config/` or `yaml/cell/`
- Update the matching docs when you add or remove parsed keys
- Prefer `README.md`, `docs/input-files.md`, `docs/schema.md`, and `docs/results-reference.md` for user-facing changes
- `docs/config_full_example.yaml` and `docs/cell_full_example.yaml` are schema references, not physically valid experiments
