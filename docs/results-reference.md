# Results Reference

EvaCAM always writes a YAML results file. Exploration runs may also write a CSV.

## YAML Shapes

Single-objective runs write:

```yaml
summary:
  area:
    total:
      width: ...
      height: ...
      area: ...
  timing:
    search_latency: ...
    variation:
      mode: monte_carlo
      samples: 9
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

Exploration runs write one top-level map per objective, for example:

- `ReadLatency`
- `Area`
- `SearchEnergy`

If no legal design point is found, the output is:

```yaml
status: no_valid_solutions
```

## Summary Section

`summary` contains three main groups:

- `area`: bank, mat, and subarray dimensions plus area efficiency
- `timing`: search latency, write/reset/set timing, and bandwidth
- `power`: dynamic-energy metrics and leakage

Write-related keys vary by cell type:

- `write_latency` and `write_dynamic_energy` for most technologies
- `reset_*` and `set_*` for some NVM types
- `erase_*` and `program_*` for `SLCNAND`

Variation summary keys:

- `summary.timing.variation` is emitted only when variation is enabled
- `mode` and `samples` describe the aggregation run
- metric blocks currently include:
  - `matchline_delay`
  - `search_latency`
  - `search_dynamic_energy`
  - `sense_margin`
- for `mode: single_point`, each metric block contains:
  - `nominal`
  - `sample`
- for `mode: monte_carlo`, each metric block contains:
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

## Exploration CSV

The exploration CSV is only emitted for full exploration without pruning. Its name is:

```text
<prefix>_<capacity_kib>K_<word_width>_<associativity>_<IN|EX>_<VOL|CUR>.csv
```

`<prefix>` comes from `extra.output_file_prefix`. If you need a non-default YAML path, use `-o`.
