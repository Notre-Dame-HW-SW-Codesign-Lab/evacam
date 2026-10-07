# Schema Reference

This document covers the YAML fields currently parsed by EvaCAM. Treat this file
and the shipped examples under `config/` as the source of truth for real
runs.

## NAND String TCAM

The complete supported fixture is `config/NAND_TCAM/NAND_TCAM.config.yaml`.
The [NAND model guide](nand-tcam.md) defines its operation and geometry units.

| File | Required NAND configuration |
| --- | --- |
| Cell | `cam_type: TCAM`, `topology: nand_string`, `memory_device`, and `layout`; generic `ports` and `access_device` are omitted |
| Architecture | `design.search_function: EX`, fixed `organization.subarray.dimensions: [strings, key_bits]`, explicit total/active organization, `flash.page_size`, `flash.block_size` |
| Sensing | `internal: true`, `custom_sense_amp: false`, `sensing_mode: discharge`; no generic sense-amplifier reference |
| Memory device | `type: SLCNAND` with the `nand` fields below |

With a selected technology library, omitted `resistance`, `capacitance`,
`wordline_driver`, and `sense` values use CMOS estimates and emit a warning.
Whole groups or individual fields may be omitted. Explicit values always win;
nulls, invalid values, and unknown keys are rejected. All other groups below
remain required (apart from the documented optional sensing/efficiency fields).
See [technology fallbacks](nand-tcam.md#technology-library-fallbacks) for the
derivations, limits, and recorded provenance. The same rules apply to `nand3d`.

| `nand` key | Meaning and units |
| --- | --- |
| `model` | `analytical_rc` |
| `calibration_status`, `source` | Status is `synthetic`, `uncalibrated`, or user-declared `calibrated`; nonblank source is required. The example is `synthetic`; labels do not establish calibration evidence |
| `resistance.read_on`, `.pass`, `.off`, `.select` | Ohms; must satisfy `off > read_on >= pass > 0` and positive select resistance |
| `capacitance.gate`, `.internal`, `.bitline`, `.source`, `.select` | Farads; gate and bitline are positive; remaining terms may be zero |
| `threshold.low`, `.high` | Volts; state thresholds |
| `bias.read`, `.pass`, `.precharge` | Volts; `low < read < high < pass`, with positive precharge |
| `supply_efficiency` | Supply efficiency in `(0, 1]` for modeled capacitive charging |
| `sensing.decision_time` | Common electrical evaluation time in seconds |
| `sensing.min_margin` | Required per-class reference margin in volts |
| `sensing.reference_voltage` | Volts; zero selects automatic midpoint, otherwise below precharge |
| `sensing.offset` | Nonnegative comparator offset allowance in volts |
| `wordline_driver`, `sense`, `page_buffer` | `area` (m²), `latency` (s), `energy` (J), and `leakage` (W); `page_buffer` requires all four, while driver/sense omissions use the technology fallback |
| `query`, `setup`, `precharge`, `recovery` | Each requires `latency` (s) and `energy` (J) |
| `program_page`, `erase_block` | Positive complete local operation `latency` (s) and `energy` (J), per physical page/block |

`layout.area` is one planar flash-transistor footprint in F². Physical flash
cells include encoding, validity, and padding; CMOS select devices are counted
separately. Legacy generic `flash`, `read`, `write`, and `resistance` device
fields do not replace the `nand` model. See [limitations](limitations.md#nand-string-tcam)
for rejected objectives, geometry, variation, and peripheral options.

## 3D NAND TCAM

The canonical example is
`config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml`. It uses the normal
`memory_device` YAML schema with `type: NAND3D` and a `nand3d` mapping;
there is no additional `.spec` input format. The cell uses
`topology: nand_string` and `cam_type: TCAM`. Its `layout` supplies
`cell_process_node`; planar `area` and `aspect_ratio` are not accepted.

The `nand3d` electrical resistance/capacitance/bias, sensing, peripheral, and
operation groups use the same units as the `nand` groups above. Additional
required fields are:

| `nand3d` key | Meaning |
| --- | --- |
| `model` | `analytical_rc` |
| `storage_mode` | `SLC` |
| `stack.storage_layers`, `stack.dummy_layers` | Storage and dummy layer counts; validity/padding are included in storage layers |
| `layout.string_rows`, `layout.string_columns` | Sequential select groups and strings per group |
| `layout.hole_pitch_x`, `layout.hole_pitch_y`, `layout.layer_pitch` | Lateral hole pitches and vertical layer pitch, in meters |
| `layout.staircase_step_width`, `layout.staircase_contact_length`, `layout.isolation_width` | Physical staircase/isolation lengths, in meters |
| `layout.peripheral_placement` | `beside` adds peripheral area; `under_array` overlaps it with the array footprint |

`flash.page_size` is one selected group's SLC page (`string_columns` bits).
`flash.block_size` is `string_rows * string_columns * storage_layers` bits.
Fixed subarray dimensions are `[string_rows * string_columns, logical_key_bits]`.
Storage layers must accommodate two devices per key bit plus a validity pair;
total storage and dummy layers may not exceed 4096. String rows must be positive,
string columns must be byte aligned and at least eight, and their product may
not exceed 1,048,576. The analytical model assumes full precharge and reset each
round. The obsolete `solver` and `precharge_driver_resistance` inputs are rejected.
See [3D NAND TCAM](nand-3d-tcam.md) for scheduling, physical capacity, and
numerical-verification versus device-calibration scope.

## Subarray Dimension Tester Config

The compiled `--subarray-dimension-test` mode accepts a separate tester schema:

- `schema: subarray_dimension_test`
- `name`: non-empty test name
- exactly one input source:
  - `config_pattern`: ordinary run-config path template containing `{rows}`
    and `{columns}`
  - `base_config`: one ordinary run config whose sizing fields are overridden
    in memory for each pair
  Both paths are resolved relative to the tester config.
- `rows`, `columns`: non-empty sequences of unique integers in `8..512`; their
  Cartesian product defines the runs
- `threads_per_run`: optional positive exploration thread count, default `1`
- `output.directory`: required results directory
- `output.summary_csv`: optional summary filename beneath the output directory,
  default `summary.csv`

Input run configs must use `schema: config` and a fixed optimization target.
`base_config` mode does not generate per-dimension config files.
The tester selects that target from each in-process result and rejects a run if
its reported subarray dimensions differ from the requested pair.

## Run Config

Required fields and sections:

- `schema: config`
- `architecture`
- `cell`
- `technology`
- `optimization`

`architecture`, `cell`, and `technology` paths are resolved relative to the run config.
Unknown keys in run, architecture, and sensing configurations are rejected.

Optional fields and sections:

- `design_constraints`
- `exploration`
- `modeling`
- `output`

Optimization keys:

- `optimization.target`: `ReadLatency`, `WriteLatency`, `ReadDynamicEnergy`, `WriteDynamicEnergy`, `ReadEDP`, `WriteEDP`, `LeakagePower`, `Area`, `SearchLatency`, `SearchEnergy`, `SearchEDP`, or `Exploration`
- `optimization.deep_exploration`: expands default unpinned exploration domains
- `optimization.buffer_design`, `optimization.row_driver`, and `optimization.priority_encoder`: `latency`, `balance`, or `area`

Other mappings:

- `design_constraints`: legacy constrained-DSE controls, renamed and moved without a behavior change
- `exploration.use_cacti_assumption`
- `exploration.enable_pruning`: valid only with `optimization.target:
  Exploration`; when `true`, the exploration CSV contains the constrained
  Pareto frontier instead of every valid modeled candidate
- `modeling.exclude_precharge_latency`
- `modeling.include_leakage`, `modeling.scaled_voltage`: retained but currently have no model effect
- `output.results`: deprecated compatibility option for overriding the default results YAML path; prefer CLI `--output`
- `output.exploration_csv_prefix`: controls exploration CSV naming

Legacy fields such as `custom_sense_amplifier_file`, `modeling.use_updated_lib`,
and `output.yaml_file` are rejected. New configs must reference technology and
sensing files instead.

## Architecture Config

Required sections:

- `design`
- `memory`
- `routing`
- `peripherals`
- `sensing`
- `wires`

Optional sections:

- `organization`
- `matchline`
- `flash`
- `physical_limits`

Common required keys:

- `design.target`: currently only `CAM`
- `design.search_function`: `EX`, `BE`, or `TH`
- `design.system_process_node`: system-level process node used for technology, wire, and peripheral modeling, for example `45nm`
- `design.device_roadmap`: `HP`, `LSTP`, `LOP`, `FEFET`, or `LP`
- `memory.word_width`: required for non-MCAM and rejected for MCAM
- `memory.vector_dimensions`: required for MCAM and rejected for non-MCAM
- `memory.capacity`: required unless fixed `organization.subarray.dimensions` is supplied; may be exact scalar `auto` only with fixed subarray dimensions
- `routing.type`: `H-tree` or `non_h_tree`; both currently require internal sensing

Useful optional keys:

- `organization.banks.*`, `organization.mats.*`, `organization.mux.*`: pin exploration to fixed powers-of-two values
- `organization.subarray.dimensions`: fixed physical subarray `[rows, columns]`; requires explicit `organization.banks` and `organization.mats`, derives or validates `memory.capacity`, and is rejected with `optimization.target: Exploration` or `optimization.deep_exploration: true`
- `organization.comparison_columns_per_step`: fixed number of physical columns
  evaluated per matchline step; it defaults to the selected physical word
  width and an explicit value must divide that width exactly
- `peripherals.input.encoder_type`: currently `encoding_two_bit`
- `memory.physical_capacity`: required when a non-MCAM `memory.word_width` is not a power of two
- `sensing`: reference to a `*.sensing.yaml` file
- `matchline.additional_cap`: optional additional matchline capacitance applied
  to every TCAM match state, including all-match and mismatch paths
- `matchline.match_transistor.cmos_width`: optional match transistor width
- `physical_limits.max_nmos_size`: transistor-width limit in feature-size multiples
- `physical_limits.max_driver_current`: retained but currently has no model effect

Array rows are stored entries and columns are physical cells in each entry.
Active bank/mat partitions divide a word across subarrays; the ratio of total
to active partitions divides the entries. Search energy still covers all
searched subarrays. Comparison width is divided evenly among active data partitions and cannot
exceed the columns present on a local matchline. A candidate whose comparison
width cannot be divided evenly among those partitions is rejected. All attached cell terminals load the matchline,
including during a serial comparison step.

`memory.physical_capacity` is a byte capacity, like `memory.capacity`. For
128 words of 72 bits, both are `1152B`; `9kb` means 9 KiB in this schema.
An explicit comparison width must divide the full physical word width; partial
final steps are rejected. Serial latency and energy include all steps when
`peripherals.output.accumulator` is enabled.

## Cell File

Required fields and sections:

- `name`
- `cam_type`
- `memory_device`
- `layout` with `cell_process_node`, `area`, and `aspect_ratio`
- `ports`

Common implemented optional sections:

- `schema`
- `access_device`

Important notes:

- `cam_type` should be set explicitly in real inputs.
- Accepted values are `TCAM`, `BCAM`, `MCAM`, and `ACAM`.
- `BCAM` is currently parsed as an alias for the existing `TCAM` modeling path.
- `memory_device` references a `*.memory_device.yaml` file.
- `access_device` defines the cell-level selector model. Its `type` is
  `none`, `cmos`, or `diode`; it can also define `cmos_width`, `voltage_drop`,
  and `leakage_current`. It must be an inline mapping; standalone
  access-device files and path references are not supported.
- `ports.row` and `ports.column` are maps keyed by integer index.
- Each port defines `cmos_region`, `num_cmos`, `cmos_width`, and `is_nmos`
  directly. A `num_cmos` value of zero means no access device is present.
- `is_nvm_discharge: true` on a column `matchline` or `matchline_bitline`
  port includes the memory resistance in that port's discharge path. Setting
  it on another port is rejected.

## Memory Device File

Required fields:

- `name`
- `type`

Common implemented optional sections:

- `schema`
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

Important notes:

- `match.is_nvm_discharge: true` enables NVM discharge participation for all
  matchline ports. A port-level `true` can also enable participation; a port
  `false` does not disable the cell-wide setting. Both default to false.
  Direct FeFET match paths include the memory resistance; `topology: fefet_gate`
  instead places it in the separate control node. Enable this only
  when the NVM carries matchline current in the modeled schematic.
- `match.cmos_width` and `match.additional_cap_on_ml` are rejected; use the
  architecture's `matchline.match_transistor.cmos_width` and
  `matchline.additional_cap` instead.
- Variation is memory-device-driven. A memory-device `variation` section enables variation; omit the section for nominal-only runs. Run and architecture configs do not support a `variation` section.
- Stochastic variation sampling uses a fixed bounded-Gaussian model; `variation.distribution` is not a supported input.
- Supported user-facing variation modes are `single_point`, `monte_carlo`, and `corner`.
- `variation.mode: nominal` is not a supported input; disable variation instead.
- `variation.samples` is required for `monte_carlo` and must be greater than 1.
- `variation.monte_carlo_granularity` is optional for `monte_carlo`; supported values are `cell` and `effective`, and the default is `cell`.
- `variation.seed` is an optional memory-device override intended for reproducible testing; otherwise the variation seed is derived from the current time.
- `variation.mode: corner` uses deterministic `*_max_var` fields, derives `samples`, and ignores user-provided `samples` and `seed`.
- Multi-bit CAM behavior is selected exclusively with `cam_type: MCAM` in the
  cell file and configured through the memory-device `mcam` section.
- `flash.mlc` is not a parsed memory-device key; MLC/SLC behavior comes from `type`.
- `read.wordline_boost_ratio` and `read.read_floating` are parsed but currently have no model effect.
- `mcam.num_resistance_state` must be a power of two in `2..64`. `mcam.resistance_state` and `mcam.searchline_voltage` are required and must each contain exactly that many entries.
- The resistance entries may be supplied in any order. EvaCAM sorts them from HRS to LRS so distance `0` is the all-match state and larger absolute symbol distances select lower resistance states.
- MCAM vectors contain exactly `memory.vector_dimensions` integer elements in
  `0..num_resistance_state-1`. Squared Euclidean distance is
  `sum((stored[i] - query[i])^2)`; Hamming mismatch count is not used.
- For MCAM, `bits_per_cell = log2(num_resistance_state)`, physical columns equal
  `vector_dimensions`, and encoded storage width equals
  `vector_dimensions * bits_per_cell`. Fixed subarray dimensions must provide
  exactly that many vector columns, with no partial-symbol padding.
- Searchline-voltage entries may be supplied in any order and must be distinct. EvaCAM sorts them from low to high and drives the paired FeFET gates for symbol `s` with `V[s]` and `V[N-1-s]`, following the paper's analog-inverse mapping. Every reversed pair must have the same sum; EvaCAM derives the common center from that sum rather than accepting a separate center input. MCAM does not fall back to binary per-port search voltages.
- `mcam.ml_precharge_voltage` is optional; otherwise the MCAM matchline precharges to technology `Vdd`. `mcam.state_variation` is optional and is sampled by exact-match `single_point` and `monte_carlo` evaluation.
- `mcam.resistance_state`, `mcam.ml_precharge_voltage`, `mcam.searchline_voltage`, and `mcam.state_variation` accept either sequences or maps keyed by integer state index. Every supplied collection must define all configured states.
- MCAM inputs are restricted to the shipped 2FeFET topology: a `FEFETRAM` memory device, `access_device.type: none`, two gate-connected searchline row ports, and two drain-connected matchline column ports, all indexed `0` and `1`.
- The shipped eight-state resistance and searchline-voltage tables are provisional infrastructure examples, not calibrated correlation data. The voltage examples and reversed-pair mapping come from Kazemi et al., [Scientific Reports 12, 19201 (2022)](https://www.nature.com/articles/s41598-022-23116-w).

## 2FeFET-1T Gate-Controlled TCAM

The DATE21 example and its paper-reference fixture select these cell fields:

```yaml
topology: fefet_gate
gate_node:
  switching_voltage: 0.5V
  additional_capacitance: 0fF # optional wiring/parasitic load
```

This topology requires `cam_type: TCAM`, a `FEFETRAM` memory device, CMOS
access, two single-device source/drain searchline ports, and one NMOS-drain
matchline port. Searchlines must be complementary `0/Vdd` signals. Both
device and matchline NVM-discharge flags must be false. Memory-device
`read.power` and `read.energy` must be zero or omitted because the model derives
control-node charging and divider dissipation. Only nominal exact search is
supported; variation and approximate search are rejected.

The control-node load includes the pull-down gate and two FeFET drain
capacitances, plus `additional_capacitance`. Its switching voltage must lie
strictly between the derived match and mismatch levels. The two FeFET
resistances affect this node, while the matchline discharges through the
separate CMOS transistor. See [model equations and validation](validation/named-cam-sensing-topology.md).
This is an uncalibrated first-order model; the example's `0.5V` is an explicit
digital switching assumption, not a measured transistor threshold.

## Sensing File

Required or common fields:

- `schema`
- `internal`: whether the architecture uses internal sensing
- `sensing_mode`: `nvsim_vol`, `nvsim_cur`, `self_clock`, `dual_the`, or `discharge`; inferred from a generic `sense_amplifier` name when omitted; defaults to `nvsim_vol` for a scalar amplifier
- `sense_amplifier`: reference to a `*.sense_amp.yaml` file
- `worst_case_sense_margin`: optional matchline sensing margin
- `strict_sense_margin`: optional boolean, default `false`; for MCAM, require
  every evaluated decision boundary to meet `read.min_sense_voltage` instead
  of reporting a diagnostic failure and continuing

## Analytical TCAM Timing

For nominal exact non-NAND TCAM, the sensing file may select an analytical
decision event independently of the amplifier model:

```yaml
decision: {model: voltage_threshold, threshold: 0.5V}
# Alternatively: decision: {model: differential}
```

`legacy_horowitz` is the default. `voltage_threshold` evaluates the one-miss
line at the specified voltage and requires sufficient separation from the
all-match line. `threshold` is required only for that model and must be between
zero and precharge. `differential` finds the first time the separation reaches
`read.min_sense_voltage`; its ideal reference is between the two lines. Neither
option characterizes an amplifier or replica controller. Infeasible margins
reject the candidate. These options currently reject variation, approximate
search and `exclude_precharge_latency: true`.

The architecture file may independently select explicit search phases:

```yaml
search_timing:
  control: broadcast          # or decoded
  precharge: overlap_input    # or serial
  driver_load: physical_line  # or legacy_scaled
  recovery: 0ps               # optional, nonnegative
```

The first three keys are required when the section is present. Broadcast
control omits search address decoding; enabled search muxes still contribute
select control. `physical_line` uses the calculated row terminal/wire load;
`legacy_scaled` retains the historical 1.6 multiplier. This shared driver sizing
also affects read/write estimates. Overlap starts evaluation when both query
preparation and precharge finish; serial adds them. Recovery separates
successive evaluations, including multiplexed or bit-serial operations.
Reported cycle time assumes serialized searches, without pipelining. Absent
`search_timing`, the existing schedule remains unchanged. The same nominal
exact TCAM restrictions apply. See the [equations and validation report](validation/analytical-cam-timing.md).

## Sense-Amp File

Sense-amp files live under `config/lib/sense_amp/` by default.

Common implemented fields:

- `schema`
- `name`
- `model`
- `supported_modes`
- `layout`
- `transistors`
- `iv_converter`

`model: nvsim_cmos` uses the built-in NVSim-style equations with YAML-backed parameters.

`model: scalar` selects a separately characterized amplifier through the same
`sensing.sense_amplifier` reference. Do not set `custom_sense_amp` manually.
For example, this **synthetic illustration** supplies per-amplifier quantities:

```yaml
schema: sense_amp
name: example-characterized-amplifier
model: scalar
geometry: {area: 2um^2}
timing: {latency: 40ps}
power: {read_dynamic_energy: 7fJ, leakage: 0W}
load: {capacitance: 1fF}
```

Area, latency, dynamic energy and input capacitance must be positive; leakage
may be zero. Geometry can alternatively supply positive `height` and `width`
in feature units (`F`). Area, energy and leakage scale by amplifier count;
latency and input capacitance apply to one sense path. Zero leakage stays zero.
Characterize these values at the intended process, load, bias, input swing and
temperature. They do not automatically scale with operating conditions or
reconstruct a nonlinear sensing circuit. Never use a whole-array paper delay
as the amplifier-only latency.

Architecture and run config notes:

- `design.system_process_node` is the authoritative modeled technology node. `layout.cell_process_node` records the process node associated with the cell definition.
- `peripherals.input.custom_encoder: true` and unsupported sense-amplifier types parse but are rejected by current CAM validation. Both routing types reject external sensing.
- `docs/input_samples/` contains reference-only v2 sample files for every input role. They use generic placeholder values and are not physically valid experiments.

## Numeric and Physical Domains

EvaCAM rejects non-finite values and validates physical domains before using
inputs in circuit equations. Error messages identify the full field path and
the required domain.

- `design.temperature` must be a whole number from `300K` through `400K`, the
  range covered by the technology current tables.
- System and cell process nodes, capacities, word widths, layout area, aspect
  ratio, resistances, pulse durations, and required sense-amplifier timing,
  energy, and load values must be positive.
- Capacitances, leakage, access-device voltage drop, variation magnitudes, and
  optional power/energy values must be non-negative.
- Technology current tables must be strictly positive because they are used as
  divisors or in logarithmic interpolation. Technology capacitances, mobility,
  and planar/FinFET geometry fields may use documented zero sentinels.
- Programming voltage and current may be signed to represent polarity, but a
  non-SRAM set/reset operation must provide a non-zero voltage, current, or
  explicit energy and a positive pulse duration.
- The shipped ReRAM 2.5T1R model uses a zero port `cmos_width` for an omitted
  transistor contribution; port widths are therefore non-negative, while
  modeled cell/access transistor widths are positive when present.
- Integral quantities such as bit counts, byte capacities, process nodes, and
  temperatures must resolve to whole numbers within the destination integer
  range.
