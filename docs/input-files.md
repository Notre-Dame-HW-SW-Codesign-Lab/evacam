# Input Files

EvaCAM expects one run config YAML file. It references one architecture
config, one cell config, and one technology file. The architecture and cell
files then reference reusable sensing and memory-device files. Access-device
parameters are defined directly in each cell file. Standalone access-device
files and access-device path references are not supported.

## File Roles

- `config/<cell-group>/*.config.yaml`: optimization, exploration, modeling, and output controls
- `config/<cell-group>/*.architecture.yaml`: modeled memory architecture and sensing reference
- `config/<cell-group>/*.cell.yaml`: CAM cell topology, layout, ports, and device references
- `config/<cell-group>/*.memory_device.yaml`: reusable memory-device electrical model data
- `config/<cell-group>/*.sensing.yaml`: sensing mode and sense-amplifier reference
- `config/lib/technology/*.yaml`: reusable technology tables
- `config/lib/sense_amp/*.sense_amp.yaml`: reusable sense-amplifier model data
- `config/<cell-group>/*.subarray_dimension_test.yaml`: compiled batch-test
  manifests that expand row and column sets over ordinary run configs

Example:

```yaml
architecture: ./2FeFET_TCAM.architecture.yaml
cell: ./2FeFET_TCAM.cell.yaml
technology: ../lib/technology/cmos.updated.yaml
```

All references are resolved relative to the file that contains them.

## Subarray Dimension Tester Config

Run a tester config with the compiled mode:

```bash
./EvaCAM --subarray-dimension-test --threads 4 \
  config/2FeFET_MCAM/2FeFET_MCAM.subarray_dimension_test.yaml
```

The supported structure is:

```yaml
schema: subarray_dimension_test
name: 2FeFET_MCAM_power_of_two_spread
base_config: 2FeFET_MCAM.config.yaml
rows: [8, 16, 32, 64, 128]
columns: [8, 16, 32, 64, 128]
threads_per_run: 1
output:
  directory: ../../results/subarray_dimension_tests/2FeFET_MCAM
  summary_csv: summary.csv
```

A per-dimension config pattern can instead select existing fixed configs:

```yaml
schema: subarray_dimension_test
name: fixed_config_spread
config_pattern: fixed_{rows}x{columns}.config.yaml
rows: [8, 16, 32, 64, 128]
columns: [8, 16, 32, 64, 128]
threads_per_run: 1
output:
  directory: ../../results/subarray_dimension_tests/fixed
  summary_csv: summary.csv
```

Specify exactly one of `config_pattern` or `base_config`. Both are resolved
relative to the tester config. A pattern must contain both placeholders and
uses existing per-size configs. A base config is loaded for every run and the
compiled tester overrides dimensions, capacity, storage width, vector
dimensions where applicable, and comparison-column width; it does not generate
run configs. EvaCAM runs the Cartesian
product in row-major order and
verifies that each result reports the requested physical subarray dimensions,
storage width, and comparison-column width. For MCAM, each column is one vector
dimension and storage width is dimensions times bits/cell.
Dimension values must be unique integers from `8` through `512`.

`--threads` controls concurrent configuration runs. `threads_per_run` is a
positive integer controlling exploration workers inside each configuration;
keeping it at `1` avoids nested oversubscription for fixed single-subarray
tests. `output.directory` is relative to the tester config unless absolute,
and `output.summary_csv` must stay beneath that directory.

Referenced configs must be single-target configs. Pattern-mode inputs must be
fixed configs whose
architecture files should use explicit single-bank, single-mat organization,
`organization.subarray.dimensions` matching the filename expansion, and
`organization.comparison_columns_per_step` matching the physical column count.
For single-bit CAM, `memory.word_width` equals the column count. For MCAM,
`memory.vector_dimensions` equals the column count exactly.
Base-config mode preserves its technology, cell, topology, peripherals,
sensing, routing, and optimization target while replacing only the matrix
sizing fields for each run.

## Run Config Structure

Required fields:

- `architecture`
- `cell`
- `technology`
- `optimization`

Optional sections and fields:

- `design_constraints`
- `exploration.use_cacti_assumption`
- `exploration.enable_pruning`: for `optimization.target: Exploration`, retain
  only the Pareto frontier after applying constraints; it is rejected for
  single-target runs and does not skip model evaluations
- `modeling.exclude_precharge_latency`
- `modeling.include_leakage`
- `modeling.scaled_voltage`
- `output.exploration_csv_prefix`

New run configs reject architecture-owned sections and legacy aliases instead of
applying precedence rules. Use `schema: config`, `architecture`,
`cell`, and `technology`. Prefer CLI `--output` for non-default YAML results
paths. `output.results` is still accepted for compatibility but emits a warning.

## Architecture Config Structure

The shipped configs use these top-level sections:

- `design`
- `memory`
- `routing`
- `peripherals`
- `sensing`
- `wires`
- `organization`
- `matchline`

Representative fields:

- `design.target`: design target such as `CAM`
- `design.search_function`: search mode
- `design.system_process_node`: system-level process node used for technology, wire, and peripheral modeling, with units, for example `45nm`
- `design.device_roadmap`: roadmap such as `HP`
- `design.temperature`: temperature with units, for example `350K`
- `memory.capacity`: capacity with units, for example `512B`; optional or exact scalar `auto` only when fixed `organization.subarray.dimensions` derives capacity
- `memory.physical_capacity`: implemented capacity for irregular word widths
- `memory.word_width`: BCAM/TCAM width with units, for example `64bits`; invalid for MCAM
- `memory.vector_dimensions`: positive, unitless MCAM vector length; invalid for non-MCAM
- `organization.banks.total` and `organization.banks.active`: bank organization
- `organization.mats.total` and `organization.mats.active`: mat organization
- `organization.subarray.dimensions`: optional fixed physical subarray `[rows, columns]`; requires explicit bank and mat organization and is not supported with DSE/deep exploration
- `organization.comparison_columns_per_step`: optional number of physical
  columns evaluated on each matchline in one serial step
- `peripherals.input.encoder_type`: currently `encoding_two_bit`
- `sensing`: reference to a `*.sensing.yaml` file
- `matchline.additional_cap`: optional additional matchline capacitance; it loads
  every TCAM match state, including all-match and mismatch paths
- `matchline.match_transistor.cmos_width`: optional match transistor width
- `physical_limits.max_nmos_size` and `physical_limits.max_driver_current`
- `sensing.sensing_mode`: `nvsim_vol`, `nvsim_cur`, `self_clock`, `dual_the`, or `discharge`; inferred from the referenced sense-amplifier name when omitted

Use the grouped examples under `config/` as the source of truth for current syntax.
Reference-only samples for every input role live under
[`docs/input_samples/`](input_samples/). Those samples use neutral placeholder
values and include inline comments with accepted unit suffixes.

Variation is not configured in the run or architecture config. Variation
enablement and sigma values are defined in the memory-device config under
`variation`.

For a fuller list of implemented sections and fields, see [schema.md](schema.md).

## Cell File Structure

The shipped cell configs use these top-level sections:

- `schema`
- `name`
- `cam_type`
- `memory_device`
- `access_device`
- `layout`
- `ports`

Representative fields:

- `cam_type`: `TCAM`, `BCAM`, `MCAM`, or `ACAM`
- `memory_device`: path to a `*.memory_device.yaml` file
- `access_device`: cell-level selector model with `type`, optional `cmos_width`,
  `voltage_drop`, and `leakage_current`
- `layout.cell_process_node`
- `layout.area`
- `layout.aspect_ratio`
- `ports.row`
- `ports.column`
- `ports.*.*.cmos_region`, `num_cmos`, `cmos_width`, and `is_nmos`: per-port
  selector connection and sizing fields. `num_cmos` must be positive when
  `cmos_region` is an electrical terminal (`gate`, `source`, `drain`, or `diode`).

Memory-device files own electrical behavior and variation. Common sections are:

- `type`
- `resistance`
- `capacitance`
- `device`
- `read`
- `write`
- `match`
- `sram`
- `flash`
- `variation`
- `mcam`

MCAM is currently limited to the shipped 2FeFET topology: a `FEFETRAM`
memory device, `access_device.type: none`, two gate-connected searchline row
ports, and two drain-connected matchline column ports. Both port maps must use
the indices `0` and `1`. The `mcam` section must define
`num_resistance_state`, one positive `resistance_state` and one non-negative
`searchline_voltage` for every state. The state count must be a power of two,
and MCAM match inputs are integer symbols from `0` through
`num_resistance_state - 1`.

CAM rows are stored words or vectors, and columns are physical bits or vector
elements. For MCAM, `memory.vector_dimensions` is the vector length and equals
the required physical column count. `mcam.num_resistance_state` determines
`bits_per_cell = log2(states)`, so encoded storage per vector is
`vector_dimensions * bits_per_cell`. There is no partial-symbol padding and a
fixed subarray must supply exactly the configured number of vector columns.
Complete-vector search sets `organization.comparison_columns_per_step` to that
column count.

EvaCAM sorts the resistance table internally from HRS to LRS. Per-coordinate
distance `0` therefore represents the all-match HRS path, while increasing
absolute symbol distance selects progressively lower resistance paths. Row
distance is squared Euclidean distance, `sum((stored[i] - query[i])^2)`, rather
than Hamming mismatch count. Optional
`state_variation` and `ml_precharge_voltage` collections must also contain one
entry per state when present. Without `ml_precharge_voltage`, MCAM reads use
the technology `Vdd` as the matchline precharge voltage.

EvaCAM also sorts the searchline-voltage table from low to high. Following the
paper's analog-inverse scheme, symbol `s` drives the paired FeFET gates with
`V[s]` and `V[N - 1 - s]`. Every reversed pair must have the same sum. EvaCAM
therefore derives the analog center as `(V[0] + V[N - 1]) / 2` and validates
the other pairs against it; the center is not a separate input.

The shipped eight-state resistance and searchline-voltage values are
provisional infrastructure inputs, not a calibrated device model. The voltage
table and complementary mapping are based on the illustrative 3-bit inputs in Kazemi et al.,
[Scientific Reports 12, 19201 (2022)](https://www.nature.com/articles/s41598-022-23116-w).

`read.min_sense_voltage` is the assumed minimum voltage difference the hardware
can detect; the shipped MCAM example uses `70mV`. MCAM treats this as a
diagnostic requirement by default. Match results expose the actual margin,
required margin, signed slack, and pass/fail status while preserving the ideal
squared-Euclidean hit decision. Add `strict_sense_margin: true` to the sensing
file when an unsatisfied boundary must reject the design or operation. Best
match uses the actual best/runner-up rows, and threshold reachability depends
on the query, so the diagnostic is deliberately data dependent.

If the memory-device config contains a `variation` section, EvaCAM uses its
built-in resistance variation model. Omit the section for nominal-only runs.
There is no top-level variation block. You may also provide
`variation.lut_file` as a future-facing hook for an external variation lookup
table path; EvaCAM currently accepts and propagates the filename but does not
consume the LUT yet.

Supported memory-device variation controls:

- `variation.mode: single_point` for one sampled run
- `variation.mode: monte_carlo` with `variation.samples: <N>` for Monte Carlo analysis
- `variation.mode: corner` for deterministic independent low/high resistance corners
- `variation.monte_carlo_granularity: cell | effective` to choose whether Monte Carlo samples each modeled matchline cell independently or samples one effective resistance for all modeled cells; default is `cell`
- `variation.seed` as an optional reproducibility/testing override; if omitted, EvaCAM derives the base seed from the current time
- `variation.lut_file` as an optional path to a future external variation LUT file

For `single_point` and `monte_carlo`, use `*_stdev` fields such as `memory_device_resistance_on_stdev`; these are interpreted as bounded-Gaussian standard-deviation fractions. In Monte Carlo `cell` granularity, EvaCAM samples the effective memory resistance of each modeled matchline cell branch independently, then reduces those branches into the aggregate matchline resistance used by the timing model. In `effective` granularity, EvaCAM samples one effective on/off resistance pair per Monte Carlo sample and applies it across all modeled matchline cells. For `corner`, use `*_max_var` fields such as `memory_device_resistance_on_max_var`; these are interpreted as deterministic raw variation bounds, so `5%` produces low/high corners at `0.95x` and `1.05x` nominal resistance. Corner mode derives its own sample count from the number of active effective resistance components and ignores user-provided `variation.samples` and `variation.seed`.

For MCAM exact-match calls, `mcam.state_variation` supplies the bounded-Gaussian
fraction for each sorted resistance state in `single_point` and `monte_carlo`
modes. Sampling is deterministic for a fixed seed and honors `cell` versus
`effective` granularity.

## Units and Formatting

The current examples rely on unit-suffixed scalar values. Keep units explicit
and consistent with the shipped examples.

Valid unit suffixes used by the input parsers:

- Length: `m`, `cm`, `mm`, `um`, `nm`
- Feature width: `F`
- Feature area: `F^2`
- Area: `m^2`, `cm^2`, `mm^2`, `um^2`, `nm^2`
- Capacitance: `F`, `mF`, `uF`, `nF`, `pF`, `fF`
- Capacitance per length: `F/m`, `pF/m`, `fF/m`
- Current: `A`, `mA`, `uA`, `nA`, `pA`
- Data size: `B`, `KB`, `MB`, `GB`
- Energy: `J`, `mJ`, `uJ`, `nJ`, `pJ`, `fJ`
- Power: `W`, `mW`, `uW`, `nW`, `pW`
- Resistance: `ohm`, `kohm`, `Mohm`, `Gohm`
- Temperature: `K`
- Time: `s`, `ms`, `us`, `ns`, `ps`, `fs`
- Voltage: `V`, `mV`
- Word width: `bit`, `bits`
- Variation fractions: bare fractions such as `0.05` or percentages such as `5%`

Numeric values are checked before circuit calculations. In particular,
temperature must be a whole value between `300K` and `400K`; dimensions,
resistances, pulse durations, and required loads must be positive; and
capacitances, leakage, and variation magnitudes must be non-negative. Signed
set/reset voltages and currents are accepted because their sign can encode
programming polarity. Invalid inputs report the full field name and expected
domain, for example `design.temperature must be between 300K and 400K`.

## Minimal Workflow

1. Copy a known-good config from `config/<cell-group>/`.
2. Keep its referenced cell config unchanged at first.
3. Change one parameter at a time.
4. Run EvaCAM after each change.
5. Only move to larger structural edits after the small edits run cleanly.

## Reference-Only Samples

The files under [`docs/input_samples/`](input_samples/) are not intended for
real runs. They show every v2 input role with generic placeholder values and
unit comments:

- [`sample.config.yaml`](input_samples/sample.config.yaml)
- [`sample.architecture.yaml`](input_samples/sample.architecture.yaml)
- [`sample.cell.yaml`](input_samples/sample.cell.yaml)
- [`sample.memory_device.yaml`](input_samples/sample.memory_device.yaml)
- [`sample.sensing.yaml`](input_samples/sample.sensing.yaml)
- [`sample.sense_amp.yaml`](input_samples/sample.sense_amp.yaml)
- [`sample.technology.yaml`](input_samples/sample.technology.yaml)

Read [`docs/input_samples/README.md`](input_samples/README.md) before using
those samples.

Legacy shipped files with names such as `*_tool_config.yaml`,
`*_architecture_config.yaml`, and `*_cell_config.yaml` are migration/reference
fixtures. Do not use them as the starting point for new active configs.

For current runtime restrictions, see [limitations.md](limitations.md).

## Practical Advice

- Prefer starting from a real config under `config/`, not the full reference examples.
- Keep referenced files near the run config when practical; relative references are resolved from the file that contains them.
- If a run fails while parsing YAML, check indentation first.
- If a run parses but reports no valid solutions, the issue is usually an unsupported parameter combination rather than YAML syntax.
