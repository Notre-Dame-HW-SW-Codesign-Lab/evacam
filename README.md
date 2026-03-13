# EvaCAM

EvaCAM is a C++ simulator and design-space exploration tool for content-addressable memory (CAM) arrays and related memory-cell technologies.

It reads a top-level YAML configuration, loads a YAML cell description, explores valid organizations, and writes result summaries as YAML. Full-exploration runs can also emit a CSV of explored points.

## Repository Layout

- `src/`, `include/`: simulator implementation
- `yaml/config/`: example top-level configurations
- `yaml/cell/`: example cell descriptions
- `docs/`: usage notes and reference docs
- `old_style_config/`: legacy configuration files kept for reference
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
./EvaCAM yaml/config/2FeFET_TCAM_config.yaml
```

Or use the `make` wrapper:

```bash
make run CONFIG_FILE=yaml/config/2FeFET_TCAM_config.yaml
```

By default, EvaCAM writes YAML results to `results/<config-name>_results.yaml`.

For example:

- `yaml/config/2FeFET_TCAM_config.yaml`
- `results/2FeFET_TCAM_results.yaml`

## CLI

Usage:

```text
./EvaCAM [OPTIONS] <cfg_file>
```

Options:

- `-t, --threads N`: number of OpenMP threads; default is all available cores
- `-v, --verbose`: enable verbose logging
- `-d, --deep-exploration`: test a wider set of design options during optimization
- `-o, --output FILE`: write YAML results to a custom path
- `-h, --help`: print usage and exit

Examples:

```bash
./EvaCAM -v yaml/config/8T-BCAM_65nm_config.yaml
./EvaCAM -t 8 -o results/custom.yaml yaml/config/ReRAM-2T2R_config.yaml
./EvaCAM -d yaml/config/2FeFET_TCAM_DSE_config.yaml
```

## Input Files

EvaCAM consumes a top-level config file and a separate cell file.

- The top-level config selects design targets, array organization, peripheral options, optimization mode, and the path to the cell file.
- The cell file describes the device, ports, voltages, currents, and related physical parameters.

Start with the shipped examples in `yaml/config/` and `yaml/cell/`.

More detail:

- [Input Files](/home/jbech002/Research/evacam/docs/input-files.md)
- [Full Example Warning](/home/jbech002/Research/evacam/docs/FULL_INPUT_EXAMPLES_WARNING.md)

## Outputs

EvaCAM always prints a console summary and writes a YAML results file.

When the optimization target is `Exploration`, EvaCAM may also write a CSV of explored points. The CSV is emitted only for unpruned full-exploration runs.

More detail:

- [Output Files](/home/jbech002/Research/evacam/docs/output-files.md)

## Validation

Available make targets:

- `make test-yaml`: build and run the YAML helper test
- `make test`: run a sample config under valgrind
- `make test-all-valgrind`: run a larger set of configs under valgrind
- `make uml`: build the repository UML PDF from `docs/repo_uml.tex`

## Architecture

High-level flow:

1. Parse CLI options.
2. Load and validate the YAML config and referenced cell file.
3. Build the exploration context.
4. Explore valid organizations and score results.
5. Print a console summary and write YAML output.

More detail:

- [Architecture Notes](/home/jbech002/Research/evacam/docs/architecture.md)

## Troubleshooting

Common issues and their likely causes are documented here:

- [Troubleshooting](/home/jbech002/Research/evacam/docs/troubleshooting.md)
