# Output Files

EvaCAM produces console output and a YAML results file for every run.

## YAML Results Path

Default behavior:

- output directory: `results/`
- filename pattern: `<config-base>_results.yaml`

If the input file ends with `_system_config.yaml` or `-system-config.yaml`, that suffix is removed before the results filename is built. Legacy `_config.yaml` and `-config.yaml` suffixes are also stripped.

Examples:

- `config/2FeFET_TCAM/2FeFET_TCAM_system_config.yaml` -> `results/2FeFET_TCAM_results.yaml`
- `config/ReRAM-2T2R/ReRAM-2T2R_system_config.yaml` -> `results/ReRAM-2T2R_results.yaml`

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

## Variation CSV

When cell-level variation uses `mode: monte_carlo`, EvaCAM also emits a
per-sample CSV and SVG histogram plot next to the YAML results file. When
variation uses `mode: corner`, EvaCAM emits a deterministic corner CSV but
does not emit a histogram plot. The YAML `summary.timing.variation.sample_file`
and, for Monte Carlo only, `plot_file` fields point to these files.

Default example:

```text
results/2FeFET_TCAM_results.yaml
results/2FeFET_TCAM_variation_samples.csv
results/2FeFET_TCAM_variation_histograms.svg
```

The CSV columns are:

```csv
sample,corner_label,matchline_wire_res_corner,access_res_on_corner,access_res_off_corner,match_res_on_corner,match_res_off_corner,matchline_delay_s,search_latency_s,search_dynamic_energy_j,sense_margin_v,reference_delay_s,nominal_matchline_delay_s,nominal_search_latency_s,nominal_search_dynamic_energy_j,nominal_sense_margin_v,nominal_reference_delay_s
```

For Monte Carlo mode, EvaCAM writes the plot automatically with the matplotlib-based Python plotter.
If `python3` or matplotlib is unavailable, EvaCAM prints a warning and still
writes the YAML results and variation sample CSV; the YAML omits `plot_file`
when no plot was generated.
Use `--no-variation-plots` to skip SVG generation intentionally.
To regenerate the plot manually, run:

```bash
python3 scripts/plot_variation_histograms.py results/<name>_variation_samples.csv
```

For corner mode, generate a deterministic corner plot with:

```bash
python3 scripts/plot_corner_variation.py results/<name>_variation_samples.csv
```

By default, each subplot sorts corners by its metric value. To plot the original
sample order instead, run:

```bash
python3 scripts/plot_corner_variation.py results/<name>_variation_samples.csv --order sample
```

Each subplot shows the nominal result as a purple dashed line, the best corner
in green, and the worst corner in red.

Generate a separate human-readable corner table with:

```bash
python3 scripts/generate_corner_variation_table.py results/<name>_variation_samples.csv
```

The table includes one sample-level section with metric-specific plot indices
that correspond to the plot x-axis. It also includes one sorted section per
metric showing the sample, value, corner settings, and corner-setting changes
from the previous plot index.

The default plot and table exclude internal diagnostic columns such as
`reference_delay_s`. Use `--include-internal` to include them.

SVG plots can be converted to PNG with:

```bash
scripts/convert_svgs_to_png.sh <svg_path>
```

To convert every SVG in a directory:

```bash
scripts/convert_svgs_to_png.sh --dir <dir>
```

## Output Directory Behavior

- EvaCAM creates parent directories for the YAML output path when needed.
- The `make run` target also creates the `results/` directory before execution.
