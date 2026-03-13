# Output Files

EvaCAM produces console output and a YAML results file for every run.

## YAML Results Path

Default behavior:

- output directory: `results/`
- filename pattern: `<config-base>_results.yaml`

If the input file ends with `_config.yaml` or `-config.yaml`, that suffix is removed before the results filename is built.

Examples:

- `yaml/config/2FeFET_TCAM_config.yaml` -> `results/2FeFET_TCAM_results.yaml`
- `yaml/config/ReRAM-2T2R_config.yaml` -> `results/ReRAM-2T2R_results.yaml`

You can override the YAML output path with:

```bash
./EvaCAM -o path/to/output.yaml yaml/config/2FeFET_TCAM_config.yaml
```

## Console Summary

For non-exploration runs:

- EvaCAM prints the best result for the selected optimization target.
- If no valid solution is found, it prints `No valid solutions.`

For full-exploration runs:

- EvaCAM prints a compact table of the best results across multiple objectives.
- If pruning is enabled, the console output states that the results are pruned.

## YAML Result Content

The YAML results file contains a summary of the chosen design point and a breakdown of area, latency, energy, and leakage components.

Representative sections include:

- `summary`
- `breakdown`

Representative metrics include:

- total area
- read and write latency
- search latency
- dynamic energy
- leakage

## Exploration CSV

When the optimization target is `Exploration`, EvaCAM may also emit a CSV containing explored design points.

Current behavior:

- CSV is written only for full exploration without pruning
- filename comes from the config-derived output prefix and key run parameters

The generated name follows this pattern:

```text
<prefix>_<capacity_kib>K_<word_width>_<associativity>_<IN|EX>_<VOL|CUR>.csv
```

Example shape:

```text
output_2048K_512_1_IN_VOL.csv
```

## Output Directory Behavior

- EvaCAM creates parent directories for the YAML output path when needed.
- The `make run` target also creates the `results/` directory before execution.
