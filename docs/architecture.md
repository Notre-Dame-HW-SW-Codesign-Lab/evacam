# Architecture Notes

This is the high-level execution flow for the current codebase.

## Main Flow

1. `src/main.cpp` parses CLI arguments with `CliOptionsParser`.
2. `EvaCamContextBuilder` validates the config path, applies thread settings, enables verbose logging, and loads the config.
3. `EvaCamExplorer` runs the design-space exploration.
4. `EvaCamOutput` prints the console summary and writes YAML results.

## Main Components

- `CliOptions`: CLI parsing and usage text
- `EvaCamContextBuilder`: runtime setup and default output path selection
- `EvaCamConfig`: YAML loading, validation, configuration state, and helper methods for exploration
- `EvaCamExplorer`: search over legal organizations and optimization targets
- `EvaCamOutput` and `ResultsYaml`: console and YAML serialization

## Input Boundary

The main user-facing interface is the YAML config and the referenced cell YAML. Most documentation should stay centered on those files and the CLI, because that is where users interact with the tool.

## Exploration Modes

There are two broad run shapes:

- Single-objective optimization: returns the best point for the selected target
- Full exploration: returns best points across multiple objectives and may emit a CSV

The CLI flag `--deep-exploration` expands the search space used during optimization.

## Where To Extend

- Add or adjust CLI behavior in `src/CliOptions.cpp`
- Add new YAML fields in the YAML parsing helpers and `EvaCamConfig`
- Change result serialization in `src/ResultsYaml.cpp`
- Change exploration logic in `src/EvaCamExplorer.cpp`

## Documentation Priority

When documenting future changes, keep these stable surfaces up to date first:

1. CLI usage
2. YAML schema and examples
3. Output file behavior
4. Supported and unsupported modeling modes
