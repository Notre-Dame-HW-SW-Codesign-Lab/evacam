# Output Files

EvaCAM produces console output and a YAML results file for every run.

## YAML Results Path

Default behavior:

- output directory: `results/`
- filename pattern: `<config-base>_results.yaml`

If the input file ends with `_config.yaml` or `-config.yaml`, that suffix is removed before the results filename is built.

Examples:

- `config/2FeFET_TCAM/2FeFET_TCAM_config.yaml` -> `results/2FeFET_TCAM_results.yaml`
- `config/ReRAM-2T2R/ReRAM-2T2R_config.yaml` -> `results/ReRAM-2T2R_results.yaml`

You can override the YAML output path with `extra.output_yaml_file` in the config:

```yaml
extra:
  output_yaml_file: path/to/output.yaml
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

See [results-reference.md](results-reference.md) for the current output shapes and no-solution case.

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

## Monte Carlo Variation CSV

When cell-level variation uses `mode: monte_carlo`, EvaCAM also emits a
per-sample CSV and SVG histogram plot next to the YAML results file. The YAML
`summary.timing.variation.sample_file` and `plot_file` fields point to these files.

Default example:

```text
results/2FeFET_TCAM_results.yaml
results/2FeFET_TCAM_variation_samples.csv
results/2FeFET_TCAM_variation_samples_histograms.svg
```

The CSV columns are:

```csv
sample,matchline_delay_s,search_latency_s,search_dynamic_energy_j,sense_margin_v,reference_delay_s
```

EvaCAM writes the plot automatically. To regenerate it manually, run:

```bash
python3 scripts/plot_variation_histograms.py results/2FeFET_TCAM_variation_samples.csv
```

The default plot excludes internal diagnostic columns such as `reference_delay_s`.
Use `--include-internal` to include them.

## Output Directory Behavior

- EvaCAM creates parent directories for the YAML output path when needed.
- The `make run` target also creates the `results/` directory before execution.
