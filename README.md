# EvaCAM

EvaCAM is a C++ simulator and design-space exploration tool for content-addressable memory (CAM) arrays and related memory-cell technologies.

It reads a system config YAML, loads a cell config YAML, explores valid organizations, and writes result summaries as YAML. Full-exploration runs can also emit a CSV of explored points.

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
- `old_style_config/`: legacy configuration files kept for reference only
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
./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM_system_config.yaml
```

Or use the `make` wrapper:

```bash
make run CONFIG_FILE=config/2FeFET_TCAM/2FeFET_TCAM_system_config.yaml
```

By default, EvaCAM writes YAML results to `results/<config-name>_results.yaml`.
The `make run` wrapper also saves the console output to `results/<config-name>_run.log`.

For example:

- `config/2FeFET_TCAM/2FeFET_TCAM_system_config.yaml`
- `results/2FeFET_TCAM_results.yaml`
- `results/2FeFET_TCAM_run.log`

## CLI

Usage:

```text
./EvaCAM [OPTIONS] <cfg_file>
```

Options:

- `-t, --threads N`: number of OpenMP threads; default is all available cores
- `-v, --verbose`: enable verbose logging
- `--no-variation-plots`: skip Monte Carlo variation histogram SVG generation
- `-d, --deep-exploration`: test a wider set of design options during optimization
- `-o, --output FILE`: write YAML results to a custom path
- `-h, --help`: print usage and exit

Examples:

```bash
./EvaCAM -v config/8T-BCAM_65nm/8T-BCAM_65nm_system_config.yaml
./EvaCAM -t 8 -o results/custom.yaml config/ReRAM-2T2R/ReRAM-2T2R_system_config.yaml
./EvaCAM -d config/2FeFET_TCAM_DSE/2FeFET_TCAM_DSE_system_config.yaml
```

## Input Files

EvaCAM consumes a system config file and a separate cell config file.

- The system config selects design targets, array organization, peripheral options, optimization mode, and the path to the cell config.
- The cell config describes the device, ports, voltages, currents, related physical parameters, and any variation settings.

Start with the shipped examples under `config/`, which is the canonical layout for active configs. Each subdirectory contains a cell config plus the system configs that use it. `old_style_config/` remains in the repository only as legacy reference material.

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
2. Load and validate the system config and referenced cell config.
3. Build the exploration context.
4. Explore valid organizations and score results.
5. Print a console summary and write YAML output.

More detail:

- [Architecture Notes](docs/architecture.md)

## Troubleshooting

Common issues and their likely causes are documented here:

- [Troubleshooting](docs/troubleshooting.md)
- [Development Workflow](docs/development.md)
