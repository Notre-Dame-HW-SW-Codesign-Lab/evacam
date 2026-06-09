# Input Files

EvaCAM expects one system config YAML file. That config file must reference a cell config file through `memory.cell_file`.

## File Roles

- `config/<cell-group>/*_system_config.yaml`: system-level configurations grouped by the cell they use
- `config/<cell-group>/*_cell_config.yaml`: the corresponding cell/device description
- custom sense-amp YAML files referenced by `advanced.custom_sa_input_file` when `sensing.custom_sense_amp` is true

Example:

```yaml
memory:
  cell_file: ./config/2FeFET_TCAM/2FeFET_TCAM_cell_config.yaml
```

## System Config Structure

The shipped configs use these top-level sections:

- `design`
- `memory`
- `routing`
- `peripherals`
- `sensing`
- `optimization`
- `wires`
- `organization`
- `matchline`

Representative fields:

- `design.target`: design target such as `CAM`
- `design.search_function`: search mode
- `design.system_process_node`: system-level process node used for technology, wire, and peripheral modeling, with units, for example `45nm`
- `design.device_roadmap`: roadmap such as `HP`
- `design.temperature`: temperature with units, for example `350K`
- `memory.cell_file`: path to the cell config
- `memory.capacity`: capacity with units, for example `512B`; optional or exact scalar `auto` only when fixed `organization.subarray.dimensions` derives capacity
- `memory.word_width`: width with units, for example `64bits`
- `optimization.target`: objective such as `LeakagePower` or `Exploration`
- `optimization.deep_exploration`: expands the default exploration search space when organization geometry is not pinned
- `organization.banks.total` and `organization.banks.active`: bank organization
- `organization.mats.total` and `organization.mats.active`: mat organization
- `organization.subarray.dimensions`: optional fixed physical subarray `[rows, columns]`; requires explicit bank and mat organization and is not supported with DSE/deep exploration
- `extra.output_yaml_file`: optional override for the results YAML path

Use the grouped examples under `config/` as the source of truth for current syntax.

Variation is not configured in the system config. Variation enablement and sigma values are defined in the cell config under `variation`.

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
- `ports.row`
- `ports.column`

If `variation.with_variation` is enabled in the cell config, EvaCAM uses its built-in bounded-Gaussian variation model today. There is no top-level variation block. You may also provide `variation.lut_file` as a future-facing hook for an external variation lookup table path; EvaCAM currently accepts and propagates the filename but does not consume the LUT yet.

Supported cell-level variation controls:

- `variation.mode: single_point` for one sampled run
- `variation.mode: monte_carlo` with `variation.samples: <N>` for Monte Carlo analysis
- `variation.seed` as an optional reproducibility/testing override; if omitted, EvaCAM derives the base seed from the current time
- `variation.lut_file` as an optional path to a future external variation LUT file

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

- `docs/system_config_full_example.yaml`
- `docs/cell_config_full_example.yaml`
- `docs/custom_sense_amp_full_example.yaml`

See [FULL_INPUT_EXAMPLES_WARNING.md](FULL_INPUT_EXAMPLES_WARNING.md).

For current runtime restrictions, see [limitations.md](limitations.md).

## Practical Advice

- Prefer starting from a real config under `config/`, not the full reference examples.
- Keep `memory.cell_file` consistent with the working directory you use to launch EvaCAM, or use a stable repo-root-relative path such as `./config/<cell-group>/...`.
- If a run fails while parsing YAML, check indentation first.
- If a run parses but reports no valid solutions, the issue is usually an unsupported parameter combination rather than YAML syntax.
