# EvaCAM

EvaCAM is a C++ simulator and design-space exploration tool for content-addressable memory (CAM) arrays and related memory-cell technologies.

It reads a run config that references architecture, cell, and technology YAML files, explores valid organizations, and writes result summaries as YAML. Full-exploration runs can also emit a CSV of explored points.

Bank-level routing can use a balanced H-tree or direct non-H-tree routes. Both modes
currently place sensing inside the mats.

## Repository Layout

- `src/app/`, `include/app/`: application flow and exploration orchestration
- `src/input/`, `include/input/`: CLI parsing and YAML input helpers
- `src/output/`, `include/output/`: result serialization helpers
- `src/config/`, `include/config/`: configuration parsing, validation, and derived settings
- `src/technology/`, `include/technology/`: technology models and YAML-backed technology loading
- `src/circuit/`, `include/circuit/`: reusable circuit primitives and equations
- `src/model/`, `include/model/`: array/bank/result hierarchy
- `src/cam/`, `include/cam/`: CAM-specific blocks built on the circuit/model layers
- `src/factories/`, `include/factories/`: factory helpers for assembling model objects
- `src/app/main.cpp`: program entry point
- `config/`: canonical shipped example tree, with one subdirectory per cell and the configs that use it
- `docs/`: usage notes and reference docs
- `config/old_style_config/`: legacy configuration files kept for reference only
- `tests/`: small focused tests

## Requirements

- `g++` with C++17 support
- `yaml-cpp`
- C++ standard library thread support
- `make`

Optional:

- `valgrind` for the `make test` targets
- `pdflatex` for the UML target

## Build

```bash
make
```

This builds the `EvaCAM` binary in the repository root.

## Quick Start

Run one of the shipped example configurations:

```bash
./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM.config.yaml
```

Or use the `make` wrapper:

```bash
make run CONFIG_FILE=config/2FeFET_TCAM/2FeFET_TCAM.config.yaml
```

By default, EvaCAM writes YAML results to `results/<config-name>_results.yaml`.
The `make run` wrapper also saves the console output to `results/<config-name>_run.log`.

For example:

- `config/2FeFET_TCAM/2FeFET_TCAM.config.yaml`
- `results/2FeFET_TCAM_results.yaml`
- `results/2FeFET_TCAM_run.log`

## CLI

Usage:

```text
./EvaCAM [OPTIONS] <config.yaml>
```

Options:

- `-t, --threads N`: number of exploration worker threads; default is all available cores
- `-v, --verbose`: enable verbose logging
- `-q, --quiet`: suppress normal stdout output
- `--no-variation-plots`: skip Monte Carlo variation histogram SVG generation
- `--subarray-dimension-test`: treat the input as a subarray dimension tester
  config and run its Cartesian matrix of ordinary EvaCAM configs
- `-o, --output FILE`: write YAML results to a custom path
- `-h, --help`: print usage and exit

Deep exploration is configured in the tool YAML with
`optimization.deep_exploration: true`; it is not a CLI option.

Examples:

```bash
./EvaCAM -v config/8T-BCAM_65nm/8T-BCAM_65nm.config.yaml
./EvaCAM -t 8 -o results/custom.yaml config/ReRAM-2T2R/ReRAM-2T2R.config.yaml
./EvaCAM -q config/2FeFET_TCAM_DSE/2FeFET_TCAM_DSE.config.yaml
```

## Subarray Dimension Tester

The compiled binary can run a configured spread of fixed subarray sizes in one
invocation. The shipped 2FeFET MCAM and TCAM testers cover every row/column combination
from `8` through `128` by powers of two:

```bash
./EvaCAM --subarray-dimension-test --threads 4 \
  config/2FeFET_MCAM/2FeFET_MCAM.subarray_dimension_test.yaml
./EvaCAM --subarray-dimension-test --threads 4 \
  config/2FeFET_TCAM/2FeFET_TCAM.subarray_dimension_test.yaml
```

Or use the Make target:

```bash
make subarray-dimension-test
```

The tester prints the complete matrix before starting. In this mode,
`--threads` controls how many independent configurations run concurrently;
`threads_per_run` in the tester YAML controls the exploration workers inside
each run. Every dimension still runs independently and receives its own
results YAML. The tester also writes `summary.csv` containing dimensions,
verified word and bit-serial widths, status, paths, latency, sense margins, and
area in raw SI units. For the shipped MCAM spread, both widths equal the column
count.

The tester config uses `schema: subarray_dimension_test`, exactly one of a
placeholder-based `config_pattern` or a single `base_config`, non-empty `rows`
and `columns` sequences, and an output directory. The TCAM example uses one
base config and creates no per-dimension config files.

## Python API

Python bindings are available for full simulator runs and match evaluation through the `evacam_py` module.

More detail:

- [Python API](docs/python-api.md)

## Input Files

EvaCAM consumes a v2 run config that references separate architecture, cell, and technology files.

- The run config selects optimization, exploration, modeling, and output controls and references the other inputs.
- The architecture config describes capacity, organization, routing, peripherals, sensing, matchline options, and wires.
- The cell config describes CAM topology, layout, ports, references a reusable memory-device definition, and defines access-device parameters inline.
- Memory-device, sensing, sense-amp, and technology YAML files hold reusable electrical/device model data.

Start with the shipped v2 examples under `config/`, which are the canonical active configs. Canonical filenames use suffixes such as `*.config.yaml`, `*.architecture.yaml`, `*.cell.yaml`, `*.memory_device.yaml`, and `*.sensing.yaml`; shared defaults live under `config/lib/`. Legacy filenames such as `*_tool_config.yaml`, `*_architecture_config.yaml`, and `*_cell_config.yaml` remain only for migration/reference checks. `config/old_style_config/` remains in the repository only as legacy reference material.

More detail:

- [Input Files](docs/input-files.md)
- [Schema Reference](docs/schema.md)
- [Supported Modes And Limits](docs/limitations.md)
- [Reference Input Samples](docs/input_samples/README.md)

## Outputs

EvaCAM always prints a console summary and writes a YAML results file.

When the optimization target is `Exploration`, EvaCAM also writes a CSV of
explored points. Set `exploration.enable_pruning: true` to write only the
deterministic Pareto frontier after constraints are applied. This filtering
still models the complete design space; leave it `false` to write every valid
candidate.

More detail:

- [Output Files](docs/output-files.md)
- [Results Reference](docs/results-reference.md)
- [Pruning and SPICE Validation Plan](docs/pruning-and-spice-validation.md)

## Validation

Available make targets:

- `make test-yaml`: build and run the YAML helper test
- `make test-generated-v2-configs`: load every generated v2 config expected to run
- `make test-v2-output-parity`: compare selected legacy/v2 output pairs with numeric tolerance
- `make test`: run a sample config under valgrind
- `make test-all-valgrind`: run a larger set of configs under valgrind
- `make uml`: build the repository UML PDF from `docs/repo_uml.tex`

## Architecture

High-level flow:

1. Parse CLI options.
2. Load and validate the run config and its referenced architecture, cell, and technology configs.
3. Build the exploration context.
4. Explore valid organizations and score results.
5. Print a console summary and write YAML output.

More detail:

- [Architecture Notes](docs/architecture.md)

## Troubleshooting

Common issues and their likely causes are documented here:

- [Troubleshooting](docs/troubleshooting.md)
- [Development Workflow](docs/development.md)
- [Development Roadmap](docs/todos.md)
