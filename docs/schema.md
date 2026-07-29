# Schema Reference

This document covers the YAML fields currently parsed by EvaCAM. Treat this file
and the shipped examples under `config/` as the source of truth for real
runs.

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
- `exploration.use_cacti_assumption`, `exploration.enable_pruning`
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
- `memory.word_width`
- `memory.capacity`: required unless fixed `organization.subarray.dimensions` is supplied; may be exact scalar `auto` only with fixed subarray dimensions
- `routing.type`: currently only `H-tree`

Useful optional keys:

- `organization.banks.*`, `organization.mats.*`, `organization.mux.*`: pin exploration to fixed powers-of-two values
- `organization.subarray.dimensions`: fixed physical subarray `[rows, columns]`; requires explicit `organization.banks` and `organization.mats`, derives or validates `memory.capacity`, and is rejected with `optimization.target: Exploration` or `optimization.deep_exploration: true`
- `organization.bit_serial_width`: fixed bit-serial width
- `peripherals.input.encoder_type`: currently `encoding_two_bit`
- `memory.physical_capacity`: required when `memory.word_width` is not a power of two
- `sensing`: reference to a `*.sensing.yaml` file
- `matchline.additional_cap`: optional additional matchline capacitance
- `matchline.match_transistor.cmos_width`: optional match transistor width
- `physical_limits.max_nmos_size`: transistor-width limit in feature-size multiples
- `physical_limits.max_driver_current`: retained but currently has no model effect

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
  and `leakage_current`.
- `ports.row` and `ports.column` are maps keyed by integer index.
- Each port defines `cmos_region`, `num_cmos`, `cmos_width`, and `is_nmos`
  directly. A `num_cmos` value of zero means no access device is present.

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

- Variation is memory-device-driven. A memory-device `variation` section enables variation; omit the section for nominal-only runs. Run and architecture configs do not support a `variation` section.
- Stochastic variation sampling uses a fixed bounded-Gaussian model; `variation.distribution` is not a supported input.
- Supported user-facing variation modes are `single_point`, `monte_carlo`, and `corner`.
- `variation.mode: nominal` is not a supported input; disable variation instead.
- `variation.samples` is required for `monte_carlo` and must be greater than 1.
- `variation.monte_carlo_granularity` is optional for `monte_carlo`; supported values are `cell` and `effective`, and the default is `cell`.
- `variation.seed` is an optional memory-device override intended for reproducible testing; otherwise the variation seed is derived from the current time.
- `variation.mode: corner` uses deterministic `*_max_var` fields, derives `samples`, and ignores user-provided `samples` and `seed`.
- `multilevel.enabled` appears in some shipped legacy configs but is not currently parsed.
- `flash.mlc` is not a parsed memory-device key; MLC/SLC behavior comes from `type`.
- `read.wordline_boost_ratio` and `read.read_floating` are parsed but currently have no model effect.
- `mcam.resistance_state` is used by the experimental MCAM matchline timing model. The model evaluates the nonzero MCAM states and uses the state that produces the largest one-mismatch matchline delay.
- `mcam.searchline_voltage` together with `mcam.center_voltage` is used for MCAM searchline row-driver energy. Without these fields, the model falls back to the per-port `search0`/`search1` voltages.
- `mcam.resistance_state`, `mcam.ml_precharge_voltage`, `mcam.searchline_voltage`, and `mcam.state_variation` accept either sequences or maps keyed by integer state index. Some parsed MCAM fields remain reserved for future model extensions.

## Sensing File

Required or common fields:

- `schema`
- `internal`: whether the architecture uses internal sensing
- `sensing_mode`: `nvsim_vol`, `nvsim_cur`, `self_clock`, `dual_the`, or `discharge`; inferred from `sense_amplifier` when omitted
- `sense_amplifier`: reference to a `*.sense_amp.yaml` file
- `worst_case_sense_margin`: optional matchline sensing margin

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

Architecture and run config notes:

- `design.system_process_node` is the authoritative modeled technology node. `layout.cell_process_node` records the process node associated with the cell definition.
- `routing.type: non_h_tree`, `peripherals.input.custom_encoder: true`, and unsupported sense-amplifier types parse but are rejected by current CAM validation.
- `docs/input_samples/` contains reference-only v2 sample files for every input role. They use generic placeholder values and are not physically valid experiments.
