# Input Files

EvaCAM expects one top-level YAML config file. That config file must reference a cell YAML file through `memory.cell_file`.

## File Roles

- `yaml/config/*.yaml`: system-level configuration
- `yaml/cell/*.yaml`: cell/device-level description

Example:

```yaml
memory:
  cell_file: ./yaml/cell/2FeFET_TCAM_cell.yaml
```

## Top-Level Config Structure

The shipped configs use these top-level sections:

- `design`
- `memory`
- `routing`
- `peripherals`
- `sensing`
- `optimization`
- `wires`
- `array`
- `matchline`

Representative fields:

- `design.target`: design target such as `CAM`
- `design.search_function`: search mode
- `design.process_node`: process node with units, for example `45nm`
- `design.device_roadmap`: roadmap such as `HP`
- `design.temperature`: temperature with units, for example `350K`
- `memory.cell_file`: path to the cell YAML
- `memory.capacity`: capacity with units, for example `512B`
- `memory.word_width`: width with units, for example `64bits`
- `optimization.target`: objective such as `LeakagePower` or `Exploration`
- `array.banks.total` and `array.banks.active`: bank organization
- `array.mats.total` and `array.mats.active`: mat organization

Use the examples in `yaml/config/` as the source of truth for current syntax.

For a fuller list of implemented sections and fields, see [schema.md](/home/jbech002/Research/evacam/docs/schema.md).

## Cell File Structure

The shipped cell files use these top-level sections:

- `cell`
- `access_device`
- `resistance`
- `read`
- `write`
- `multilevel`
- `match`
- `ports`

Representative fields:

- `cell.type`
- `cell.process_node`
- `cell.area`
- `read.mode`
- `read.voltage`
- `write.set.pulse`
- `write.reset.energy`
- `ports.row`
- `ports.column`

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

1. Copy a known-good config from `yaml/config/`.
2. Keep its referenced cell file unchanged at first.
3. Change one parameter at a time.
4. Run EvaCAM after each change.
5. Only move to larger structural edits after the small edits run cleanly.

## Reference-Only Full Examples

The files below are not intended for real runs:

- `docs/config_full_example.yaml`
- `docs/cell_full_example.yaml`

See [FULL_INPUT_EXAMPLES_WARNING.md](/home/jbech002/Research/evacam/docs/FULL_INPUT_EXAMPLES_WARNING.md).

For current runtime restrictions, see [limitations.md](/home/jbech002/Research/evacam/docs/limitations.md).

## Practical Advice

- Prefer starting from a real config in `yaml/config/`, not the full reference examples.
- Keep `memory.cell_file` consistent with the working directory you use to launch EvaCAM, or use a stable repo-root-relative path such as `./yaml/cell/...`.
- If a run fails while parsing YAML, check indentation first.
- If a run parses but reports no valid solutions, the issue is usually an unsupported parameter combination rather than YAML syntax.
