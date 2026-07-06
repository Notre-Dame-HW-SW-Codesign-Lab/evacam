# EvaCAM

EvaCAM is a C++ simulator and design-space exploration tool for content-addressable memory (CAM) arrays and related memory-cell technologies.

It reads a tool config that references architecture and cell YAML files, explores valid organizations, and writes result summaries as YAML. Full-exploration runs can also emit a CSV of explored points.

## Repository Layout

- `src/app/`, `include/app/`: application flow and exploration orchestration
- `src/input/`, `include/input/`: CLI parsing and YAML input helpers
- `src/output/`, `include/output/`: result serialization helpers
- `src/config/`, `include/config/`: configuration parsing, validation, and derived settings
- `src/technology/`, `include/technology/`: technology models, cell models, and built-in tables
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
- OpenMP support
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
./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM_tool_config.yaml
```

Or use the `make` wrapper:

```bash
make run CONFIG_FILE=config/2FeFET_TCAM/2FeFET_TCAM_tool_config.yaml
```

By default, EvaCAM writes YAML results to `results/<config-name>_results.yaml`.
The `make run` wrapper also saves the console output to `results/<config-name>_run.log`.

For example:

- `config/2FeFET_TCAM/2FeFET_TCAM_tool_config.yaml`
- `results/2FeFET_TCAM_results.yaml`
- `results/2FeFET_TCAM_run.log`

## CLI

Usage:

```text
./EvaCAM [OPTIONS] <tool_config.yaml>
```

Options:

- `-t, --threads N`: number of OpenMP threads; default is all available cores
- `-v, --verbose`: enable verbose logging
- `-q, --quiet`: suppress normal stdout output
- `--no-variation-plots`: skip Monte Carlo variation histogram SVG generation
- `-o, --output FILE`: write YAML results to a custom path
- `-h, --help`: print usage and exit

Deep exploration is configured in the tool YAML with
`optimization.deep_exploration: true`; it is not a CLI option.

Examples:

```bash
./EvaCAM -v config/8T-BCAM_65nm/8T-BCAM_65nm_tool_config.yaml
./EvaCAM -t 8 -o results/custom.yaml config/ReRAM-2T2R/ReRAM-2T2R_tool_config.yaml
./EvaCAM -q config/2FeFET_TCAM_DSE/2FeFET_TCAM_DSE_tool_config.yaml
```

## Python API

Python bindings are available for full simulator runs and match evaluation through the `evacam_py` module.

More detail:

- [Python API](docs/python-api.md)

## Input Files

EvaCAM consumes a tool config that references separate architecture and cell files.

- The tool config selects optimization, exploration, modeling, and output controls and references the other inputs.
- The architecture config describes capacity, organization, routing, peripherals, sensing, and wires.
- The cell config describes the device, ports, voltages, currents, related physical parameters, and any variation settings.

Start with the shipped examples under `config/`, which is the canonical layout for active configs. Architecture files are shared by tool configs when the modeled hardware is identical. `config/old_style_config/` remains in the repository only as legacy reference material.

More detail:

- [Input Files](docs/input-files.md)
- [Schema Reference](docs/schema.md)
- [Supported Modes And Limits](docs/limitations.md)
- [Full Example Warning](docs/FULL_INPUT_EXAMPLES_WARNING.md)

## Outputs

EvaCAM always prints a console summary and writes a YAML results file.

When the optimization target is `Exploration`, EvaCAM may also write a CSV of explored points. The CSV is emitted only for unpruned full-exploration runs.

More detail:

- [Output Files](docs/output-files.md)
- [Results Reference](docs/results-reference.md)

## Validation

Available make targets:

- `make test-yaml`: build and run the YAML helper test
- `make test`: run a sample config under valgrind
- `make test-all-valgrind`: run a larger set of configs under valgrind
- `make uml`: build the repository UML PDF from `docs/repo_uml.tex`

## Architecture

High-level flow:

1. Parse CLI options.
2. Load and validate the tool config and its referenced architecture and cell configs.
3. Build the exploration context.
4. Explore valid organizations and score results.
5. Print a console summary and write YAML output.

More detail:

- [Architecture Notes](docs/architecture.md)

## Troubleshooting

Common issues and their likely causes are documented here:

- [Troubleshooting](docs/troubleshooting.md)
- [Development Workflow](docs/development.md)
