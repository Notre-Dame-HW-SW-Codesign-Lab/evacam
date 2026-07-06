# Input Files

EvaCAM expects one tool config YAML file. It references one architecture config and
one cell config. An optional third reference supplies custom sense-amplifier data.

## File Roles

- `config/<cell-group>/*_tool_config.yaml`: optimization, exploration, modeling, and output controls
- `config/<cell-group>/*_architecture_config.yaml`: modeled memory architecture; identical architectures may be shared
- `config/<cell-group>/*_cell_config.yaml`: cell/device descriptions
- custom sense-amp YAML files referenced by `custom_sense_amplifier_file`

Example:

```yaml
architecture_file: ./2FeFET_TCAM_architecture_config.yaml
cell_file: ./2FeFET_TCAM_cell_config.yaml
```

All three references are resolved relative to the tool config file.

## Tool Config Structure

Required fields:

- `architecture_file`
- `cell_file`
- `optimization`

Optional sections and fields:

- `custom_sense_amplifier_file`
- `design_constraints`
- `exploration.use_cacti_assumption`
- `exploration.enable_pruning`
- `modeling.use_updated_lib`
- `modeling.exclude_precharge_latency`
- `modeling.include_leakage`
- `modeling.scaled_voltage`
- `output.yaml_file`
- `output.exploration_csv_prefix`

New tool configs reject architecture-owned sections instead of applying precedence
rules. Unified legacy configs remain temporarily supported with a deprecation warning.

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
- `memory.word_width`: width with units, for example `64bits`
- `organization.banks.total` and `organization.banks.active`: bank organization
- `organization.mats.total` and `organization.mats.active`: mat organization
- `organization.subarray.dimensions`: optional fixed physical subarray `[rows, columns]`; requires explicit bank and mat organization and is not supported with DSE/deep exploration
- `organization.bit_serial_width`: optional fixed bit-serial width
- `peripherals.input.encoder_type`: currently `encoding_two_bit`
- `sensing.worst_case_sense_margin`: optional matchline sensing margin
- `physical_limits.max_nmos_size` and `physical_limits.max_driver_current`

Use the grouped examples under `config/` as the source of truth for current syntax.

Variation is not configured in the tool or architecture config. Variation enablement and sigma values are defined in the cell config under `variation`.

For a fuller list of implemented sections and fields, see [schema.md](schema.md).

## Cell File Structure

The shipped cell configs use these top-level sections:

- `cell`
- `access_device`
- `resistance`
- `read`
- `write`
- `multilevel`
- `variation`
- `match`
- `ports`

Representative fields:

- `cell.type`
- `cell.cell_process_node`
- `cell.area`
- `read.mode`
- `read.voltage`
- `write.set.pulse`
- `write.reset.energy`
- `variation.with_variation`
- `variation.mode`
- `variation.lut_file`
- `variation.samples`
- `variation.seed`
- `variation.memory_device_resistance_on_stdev`
- `variation.memory_device_resistance_off_stdev`
- `variation.memory_device_resistance_on_max_var`
- `variation.memory_device_resistance_off_max_var`
- `ports.row`
- `ports.column`

If `variation.with_variation` is enabled in the cell config, EvaCAM uses its built-in resistance variation model. There is no top-level variation block. You may also provide `variation.lut_file` as a future-facing hook for an external variation lookup table path; EvaCAM currently accepts and propagates the filename but does not consume the LUT yet.

Supported cell-level variation controls:

- `variation.mode: single_point` for one sampled run
- `variation.mode: monte_carlo` with `variation.samples: <N>` for Monte Carlo analysis
- `variation.mode: corner` for deterministic independent low/high resistance corners
- `variation.monte_carlo_granularity: cell | effective` to choose whether Monte Carlo samples each modeled matchline cell independently or samples one effective resistance for all modeled cells; default is `cell`
- `variation.seed` as an optional reproducibility/testing override; if omitted, EvaCAM derives the base seed from the current time
- `variation.lut_file` as an optional path to a future external variation LUT file

For `single_point` and `monte_carlo`, use `*_stdev` fields such as `memory_device_resistance_on_stdev`; these are interpreted as bounded-Gaussian standard-deviation fractions. In Monte Carlo `cell` granularity, EvaCAM samples the effective memory resistance of each modeled matchline cell branch independently, then reduces those branches into the aggregate matchline resistance used by the timing model. In `effective` granularity, EvaCAM samples one effective on/off resistance pair per Monte Carlo sample and applies it across all modeled matchline cells. For `corner`, use `*_max_var` fields such as `memory_device_resistance_on_max_var`; these are interpreted as deterministic raw variation bounds, so `5%` produces low/high corners at `0.95x` and `1.05x` nominal resistance. Corner mode derives its own sample count from the number of active effective resistance components and ignores user-provided `variation.samples` and `variation.seed`.

## Units and Formatting

The current examples rely on unit-suffixed scalar values. Common forms include:

- `45nm`
- `350K`
- `512B`
- `64bits`
- `1V`
- `70mV`
- `10000ohm`
- `10ns`
- `6fF`
- `300F^2`

Keep units explicit and consistent with the shipped examples.

## Minimal Workflow

1. Copy a known-good config from `config/<cell-group>/`.
2. Keep its referenced cell config unchanged at first.
3. Change one parameter at a time.
4. Run EvaCAM after each change.
5. Only move to larger structural edits after the small edits run cleanly.

## Reference-Only Full Examples

The files below are not intended for real runs:

- `docs/tool_config_full_example.yaml`
- `docs/architecture_config_full_example.yaml`
- `docs/cell_config_full_example.yaml`
- `docs/custom_sense_amp_full_example.yaml`

See [FULL_INPUT_EXAMPLES_WARNING.md](FULL_INPUT_EXAMPLES_WARNING.md).

For current runtime restrictions, see [limitations.md](limitations.md).

## Practical Advice

- Prefer starting from a real config under `config/`, not the full reference examples.
- Keep referenced files near the tool config when practical; relative references are resolved from the tool config directory.
- If a run fails while parsing YAML, check indentation first.
- If a run parses but reports no valid solutions, the issue is usually an unsupported parameter combination rather than YAML syntax.
