# Output Files

EvaCAM produces console output and a YAML results file for every run.

## YAML Results Path

Default behavior:

- output directory: `results/`
- filename pattern: `<config-base>_results.yaml`

If the input file ends with `_tool_config.yaml` or `-tool-config.yaml`, that suffix is removed before the results filename is built. Legacy system/config suffixes are also stripped.

Examples:

- `config/2FeFET_TCAM/2FeFET_TCAM_tool_config.yaml` -> `results/2FeFET_TCAM_results.yaml`
- `config/ReRAM-2T2R/ReRAM-2T2R_tool_config.yaml` -> `results/ReRAM-2T2R_results.yaml`

You can override the YAML output path in the tool config:

```yaml
output:
  yaml_file: path/to/output.yaml
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
<prefix>_<capacity_kib>K_<word_width>_<IN|EX>_<VOL|CUR>.csv
```

Example shape:

```text
output_2048K_512_IN_VOL.csv
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
sample,corner_label,memory_device_res_on_corner,memory_device_res_off_corner,matchline_delay_s,search_latency_s,search_dynamic_energy_j,exact_match_sense_margin_v,reference_delay_s,nominal_matchline_delay_s,nominal_search_latency_s,nominal_search_dynamic_energy_j,nominal_exact_match_sense_margin_v,nominal_reference_delay_s
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

## Corner Sweep Helper

The repository includes a helper for generating and optionally running a
deterministic 2FeFET-TCAM corner sweep:

```bash
python3 scripts/run_corner_sweep.py
```

By default, the helper sweeps the two corner inputs currently supported by
EvaCAM:

- `variation.memory_device_resistance_on_max_var`
- `variation.memory_device_resistance_off_max_var`

The default levels are `0%`, `2%`, `4%`, `6%`, and `8%`. The all-zero case is
excluded because corner mode requires at least one positive `*_max_var` field,
so the default sweep contains 24 runs and 80 evaluated corners.

Running the script without `--run` only generates per-run configs and writes a
pending manifest under `results/corner_sweep/`. To execute EvaCAM, build the
binary first and pass `--run`:

```bash
make -j
python3 scripts/run_corner_sweep.py --run --jobs 16
```

Each EvaCAM process is launched with `--threads 1`; `--jobs` controls process
parallelism. Completed runs are skipped on later invocations unless `--force`
is supplied. Per-run SVG plots and Markdown tables are disabled by default; add
`--artifacts` to create them.

Generated output uses this layout:

```text
results/corner_sweep/
├── manifest.csv
├── summary.csv
└── runs/
    └── on02_off04/
        ├── cell_config.yaml
        ├── tool_config.yaml
        ├── corner_results.yaml
        ├── corner_variation_samples.csv
        └── run.log
```

`manifest.csv` records the status of each generated case. `summary.csv`
aggregates nominal, minimum, and maximum values for matchline delay, search
latency, search dynamic energy, and exact match sense margin.

Use `--corner-values` to choose different integer percent levels:

```bash
python3 scripts/run_corner_sweep.py --corner-values 0 1 5 10
```

Values may omit `0`; in that case every generated combination is run because
there is no all-zero case to exclude.

Use `--corner-fields` to sweep a subset of the supported corner inputs. The
aliases `on` and `off` may be used instead of full YAML field names:

```bash
python3 scripts/run_corner_sweep.py --corner-fields on --corner-values 0 5 10
```

After a completed sweep, generate aggregate rankings, sensitivity tables, and
plots with:

```bash
python3 scripts/analyze_corner_sweep.py
```

The analysis output is written to `results/corner_sweep/analysis/`. Use
`--no-plots` when only CSV and Markdown outputs are needed.

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
