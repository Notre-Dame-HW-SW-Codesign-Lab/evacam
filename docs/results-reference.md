# Results Reference

EvaCAM always writes a YAML results file. Exploration runs may also write a CSV.

## YAML Shapes

Single-objective runs write:

```yaml
geometry:
  logical_capacity_bits: 4096
  allocated_capacity_bits: 4096
  logical_word_width_bits: 64
  entry_count: 64
  bits_per_cell: 3
  physical_columns_per_word: 22
  word_padding_bits: 2
  physical_cell_count: 1408
  comparison_columns_per_step: 22
  comparison_steps: 1
summary:
  area:
    total:
      width: ...
      height: ...
      area: ...
  timing:
    search_latency: ...
    exact_match_sense_margin: ...
    minimum_required_sense_margin: ...
    variation:
      mode: monte_carlo
      samples: 9
      sample_file: results/example_variation_samples.csv
      plot_file: results/example_variation_histograms.svg
      matchline_delay:
        nominal: ...
        mean: ...
        stddev: ...
        min: ...
        max: ...
        p95: ...
  power:
    read_dynamic_energy: ...
    leakage_power: ...
breakdown:
  subarray_area:
    total_cell_area: ...
```

For single-bit CAM, logical bits and physical columns are one-to-one. For
MCAM, `bits_per_cell` is `log2(num_resistance_state)` and the inferred physical
width is `ceil(logical_word_width_bits / bits_per_cell)`. Explicit MCAM
dimensions may be wider and the surplus appears in `word_padding_bits`; a
narrower supplied width is rejected before modeling.

Exploration runs write one top-level map per objective, for example:

- `ReadLatency`
- `Area`
- `SearchEnergy`

When `exploration.enable_pruning` is enabled, these winners are selected from
the constraint-passing Pareto frontier. The exploration CSV contains only that
frontier. The participating metrics are read, write, and search latency; read,
write, and search dynamic energy; area; and leakage. EDP is derived from its
latency and energy components and is not a separate dominance dimension.
Each winner has the same objective value as exhaustive exploration, although
its organization can differ when the exhaustive canonical tie winner is
dominated on other metrics.

If no legal design point is found, the output is:

```yaml
summary:
  timing:
    minimum_required_sense_margin: 0.070V
status: no_valid_solutions
```

The configured requirement remains available even though there is no valid
candidate from which to report a modeled exact-match margin.

## Summary Section

`summary` contains three main groups:

- `area`: bank, mat, and subarray dimensions plus area efficiency
- `timing`: search latency, sense-margin diagnostics, write/reset/set timing,
  and bandwidth
- `power`: search/read/write dynamic-energy metrics and leakage

Write-related keys vary by cell type:

- `write_latency` and `write_dynamic_energy` for most technologies
- `reset_*` and `set_*` for some NVM types
- `erase_*` and `program_*` for `SLCNAND`

Sense-margin and variation keys:

- `summary.timing.variation` is emitted only when variation is enabled
- `summary.timing.exact_match_sense_margin` is the nominal modeled voltage
  separation at the exact-match decision boundary and is always emitted
- `summary.timing.minimum_required_sense_margin` is the configured
  `read.min_sense_voltage` acceptance threshold and is always emitted
- `mode` and `samples` describe the aggregation run
- `sample_file` is emitted for Monte Carlo and corner runs and points to the per-sample CSV
- `plot_file` is emitted for Monte Carlo runs when SVG histogram generation succeeds and is not disabled
- metric blocks currently include:
  - `matchline_delay`
  - `search_latency`
  - `search_dynamic_energy`
  - `exact_match_sense_margin`
- for `mode: single_point`, each metric block contains:
  - `nominal`
  - `sample`
- for `mode: monte_carlo` and `mode: corner`, each metric block contains:
  - `nominal`
  - `mean`
  - `stddev`
  - `min`
  - `max`
  - `p95`

## Breakdown Section

`breakdown` provides lower-level subarray contributions for:

- area
- search latency
- search dynamic energy
- write dynamic energy
- leakage

These values are useful for debugging which peripheral block dominates a result.

## Variation Sample CSV

Monte Carlo and corner variation runs also write a per-sample CSV next to the YAML results file.
For example:

```text
results/2FeFET_TCAM_results.yaml
results/2FeFET_TCAM_variation_samples.csv
results/2FeFET_TCAM_variation_samples_histograms.svg
```

The CSV uses base SI units and has one row per sample or corner:

```csv
sample,corner_label,memory_device_res_on_corner,memory_device_res_off_corner,matchline_delay_s,search_latency_s,search_dynamic_energy_j,exact_match_sense_margin_v,reference_delay_s,nominal_matchline_delay_s,nominal_search_latency_s,nominal_search_dynamic_energy_j,nominal_exact_match_sense_margin_v,nominal_reference_delay_s
0,...
```

Full-exploration runs write one sample CSV per objective that has Monte Carlo data,
using the objective name in the file stem.

## Exploration CSV

The exploration CSV is emitted for full exploration. Its name is:

```text
<prefix>_<capacity_kib>K_<word_width>_<IN|EX>_<VOL|CUR>.csv
```

`<prefix>` comes from `output.exploration_csv_prefix`. For a non-default YAML path, use CLI `--output`.

Each row ends with four candidate-audit fields: a versioned canonical candidate
identity, row-driver optimization level, priority-encoder optimization level,
and comparison columns per step. The identity serializes every modeled exploration input;
it is not a floating-point metric hash. Rows with the same identity therefore
represent the same canonical design candidate.
