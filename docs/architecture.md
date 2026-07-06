# Architecture Notes

This is the high-level execution flow for the current codebase.

## Main Flow

1. `src/app/main.cpp` parses CLI arguments with `CliOptionsParser`.
2. `EvaCamContextBuilder` validates the config path, applies thread settings, enables verbose logging, and loads the config.
3. `EvaCamExplorer` runs the design-space exploration.
4. `EvaCamOutput` prints the console summary and writes YAML results.

## Main Components

- `CliOptions`: CLI parsing and usage text
- `EvaCamContextBuilder`: runtime setup, thread configuration, config loading, and default output path selection
- `EvaCamConfig`: aggregate runtime configuration object that stores parsed input sections, resolved exploration domains, and loaded technology/cell state
- `EvaCamExplorer`: search over legal organizations and optimization targets
- `EvaCamOutput` and `ResultsYaml`: console and YAML serialization

## Module Layout

- `src/app`, `include/app`: exploration orchestration and top-level runtime flow
- `src/input`, `include/input`: CLI parsing, cell config loading, YAML node helpers, and unit parsing helpers
- `src/output`, `include/output`: result serialization
- `src/config`, `include/config`: tool/architecture config loading, section readers, normalization, validation, and derived exploration settings
- `src/technology`, `include/technology`: technology models, memory-cell models, and built-in technology tables
- `src/circuit`, `include/circuit`: reusable circuit blocks and shared equations
- `src/model`, `include/model`: bank, mat, and result abstractions
- `src/cam`, `include/cam`: the CAM subarray and CAM-specific blocks layered on top of the common circuit/model code
- `src/factories`, `include/factories`: object construction helpers

## EvaCamConfig

`EvaCamConfig` is the central in-memory configuration object shared across exploration and hardware blocks.

- `ReadConfigFromFile()` delegates tool and architecture YAML loading to `EvaCamYamlLoader` and then loads technology/cell objects through `TechnologyLoader`
- Parsed settings are grouped into typed sections such as `InputConfig`, `PeripheralConfig`, `ConstraintConfig`, and `RuntimeSizingConfig`
- Loaded modeling state lives under `technology`, which is a `TechnologyContext` containing:
  - `tech`: the main CMOS/peripheral technology model
  - `fefetTech`: the FeFET technology model used when a FeFET-specific table is needed
  - `cell`: the parsed `MemCell`
- Exploration helpers such as `SetDeepExploration()`, `BuildResultLimits()`, and `ApplyResultLimits()` operate on this aggregate state

Validation and pretty-printing are intentionally split out of the class into `EvaCamConfigValidator` and `EvaCamConfigPrinter`.

## Config Loading Pipeline

Top-level config loading is now an explicit four-step pipeline:

1. `EvaCamYamlLoader` loads the root YAML and coordinates the flow.
2. `ConfigSectionReaders` parse each top-level section into typed config state.
3. `ConfigNormalizer` applies derived exploration/default shaping.
4. `InputRuleValidator` enforces conditional input rules before runtime objects are loaded.

The referenced cell config follows a similar split:

- `CellYamlLoader` parses the cell config by section
- `YamlNodeHelpers` owns generic YAML node access, scalar conversion, and enum helpers
- `YamlUnitParsers` owns quantity parsing and unit tables

Variation policy is also separated from runtime object loading:

- `TechnologyLoader` loads technology and cell runtime objects
- `VariationConfigBuilder` derives the runtime variation configuration from the parsed `MemCell`

## Technology

`Technology` is a read-mostly model of process parameters used by the circuit equations.

- `Initialize()` selects a `TechnologySpec` from the built-in tables in `TechnologyTables.cpp`
- `ApplySpec()` copies scalar process values such as `vdd`, `vth`, capacitances, mobility terms, and fin geometry from the selected spec
- `ExpandTemperatureTables()` expands the compact 11-point current tables in `TechnologySpec` into the 101-entry runtime tables used by the formulas
- `InterpolateWith()` blends between adjacent supported process nodes so EvaCAM can model intermediate nodes between the tabulated anchor points

`TechnologyLoader` builds the `TechnologyContext` used by `EvaCamConfig`. It loads the primary technology model, interpolates to the requested node when needed, loads the `MemCell`, chooses a compatible FeFET technology model, and delegates runtime variation assembly to `VariationConfigBuilder` when needed.

## Input Boundary

The main user-facing interface is the YAML config, the referenced cell config, and the CLI.

## Exploration Modes

There are two broad run shapes:

- Single-objective optimization: returns the best point for the selected target
- Full exploration: returns best points across multiple objectives and may emit a CSV

The config key `optimization.deep_exploration` expands the search space used during optimization.

## Where To Extend

- Add or adjust CLI behavior in `src/input/CliOptions.cpp`
- Add new tool or architecture YAML fields at the split-config boundary and in the typed config structs owned by `EvaCamConfig`
- Add new cell-YAML fields in `CellYamlLoader`
- Add technology-table entries in `src/technology/TechnologyTables.cpp` and update `TechnologyLoader` if new loading rules are required
- Change result serialization in `src/output/ResultsYaml.cpp`
- Change exploration logic in `src/app/EvaCamExplorer.cpp`

## Documentation

When documenting future changes, keep these up to date:

- CLI usage
- YAML schema and examples
- Output file behavior
- Supported and unsupported modeling modes
