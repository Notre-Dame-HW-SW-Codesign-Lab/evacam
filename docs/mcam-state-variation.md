# MCAM voltage and latency plots with state variation

Generate matchline voltage and search latency versus squared Euclidean distance
for every distinct nominal composition in the 8- and 16-dimensional cases. The
plots also show a smooth outer envelope through the exact output endpoints of
the configured ±3σ resistance support:

```sh
make -j4 test-pybind-match
python3 scripts/plot_mcam_voltage.py
```

The copied inputs are in `config/2FeFET_MCAM_variation/`. The plotting command
intentionally accepts only 8×8 and 16×16. Each size has 0%, 5%, and 10%
per-state standard-deviation experiments. Every fileset uses 1,000 samples,
seed 9876, and cell granularity.
These percentages and the device resistance curve are provisional sensitivity
inputs; they do not represent calibrated process statistics.

The default output directory is `results/mcam_state_variation_compositions/`.
Use `--sizes 8 --samples 10 --sample-compositions 100` for a quick preview
before a full run. The composition target applies only to the Monte Carlo
subset; mandatory distance and bound-witness coverage can exceed it, while
nominal enumeration remains exhaustive.

The dedicated 32×32 runner streams all 15,380,937 nominal compositions through
bounded aggregation instead of retaining them as Python objects:

```sh
python3 scripts/run_mcam_32.py
```

Its default output is `results/mcam_state_variation_32x32/`, with 0%, 5%, and
10% resistance σ/R runs, 1,000 samples, seed 9876, cell granularity, and a
2,000-composition Monte Carlo target. It does not accept 64×64 or larger sizes.
`run_status.json` is updated atomically throughout preparation, nominal
streaming, sampling, rendering, and completion. During the nominal pass it
reports progress every 100,000 compositions, elapsed time, and maximum resident
memory. The 32×32 run can be resumed only by starting it again; the status file
is progress reporting rather than a checkpoint.

The exhaustive nominal table and plot groups are shared by all variation
levels under `shared_nominal/`. `nominal_compositions.csv.gz` contains exact
Python integer multiplicities, including
`2,390,461,829,733,887,910,000,000` coordinate permutations for the balanced
composition with four coordinates at each of eight deltas. Each
`stdevXX/32x32/` directory contains that level's Monte Carlo subset, bounds,
corner witnesses, metadata, and plots. Boundary witnesses and at least one
mixed composition at every distance where one exists are retained in the
sampled subset; these mandatory rows can exceed the 2,000-composition target.

To run one size or compare correlated effective-state variation:

```sh
python3 scripts/plot_mcam_voltage.py --sizes 8 --levels 5
python3 scripts/plot_mcam_voltage.py --sizes 16 --levels 5 --granularity effective --output-dir results/mcam_state_variation_effective
```

`--samples` and `--seed` override the copied inputs for a run. The script saves
resolved run inputs alongside the outputs and never modifies source configs.
Use a separate output directory when preserving multiple seed/sample settings.
Both full-range and active-region figures use the same axes across the requested
variation levels for a given size. The active crop is based on the nominal
voltage swing, so changes in variation do not move the comparison window.
Latency uses that same distance crop for comparison with the voltage figures.
By default both metrics are rendered; `--metric latency` or `--metric voltage`
selects just one set of figures. Raw nominal, sample, bound, and witness exports
always include both metrics; plot-group exports are written for rendered metrics.

## Meaning of the points and bands

- Colored nominal markers cover all 6,435 distinct delta-count compositions at
  8 dimensions and all 245,157 at 16 dimensions, including mixed deltas. Query
  symbols are zero. Points that share a distance and fall in the same bin of
  width 1/2000 of the common plotted y range are grouped for rendering; marker area shows how many
  compositions share that visible point, and color shows the maximum number of
  distinct nonzero deltas in the group. The same bin width is used across
  variation levels and full/active views. The complete rows remain in the CSV
  export, together with their coordinate-permutation multiplicity.
- Faint orange points show a deterministic, stratified Monte Carlo subset. It
  spans reachable distances, favors mixed compositions, includes every
  available bound-witness composition, and evaluates up to two coordinate
  placements when a composition contains different deltas. The plot and export therefore retain visible sample
  support without multiplying every nominal composition by 1,000 samples.
- The blue band encloses the nominal minimum and maximum across all reachable
  coordinate compositions.
- The light orange band encloses all compositions and the complete configured
  bounded resistance support. It is not a percentile or confidence interval.

`mcam.state_variation` supplies a standard-deviation fraction for each raw
resistance state. The nominal sorted resistance ordering fixes the mapping
between symbol delta and raw state; varying resistance does not reorder it.
The existing sampler restricts each state resistance to
`[max(R * 1e-12, R - 3 * R * stdev), R + 3 * R * stdev]`.
Bounds include state-zero variation in unchanged coordinates. Dynamic
programming finds the minimum and maximum conductance at each reachable
integer squared distance, then the model converts conductance to voltage at
the common nominal sensing instant. Both sampling granularities share these
outer support bounds; their correlations and sampled distributions differ.

For display, a smoothstep interpolation passes through every exact lower and
upper endpoint without moving it. Short endpoint markers show the evaluated
±3σ corner results at every reachable distance. The smooth band bridges gaps
between reachable integer distances; intermediate positions are a visual guide
and need not represent realizable vectors. Exact bounds, witness compositions,
and independently evaluated corner results are exported separately. Every
nominal and sampled point must remain enclosed; failed containment raises an
error.
The guarantee applies to this bounded model, not to unmodeled physical variation.

## Outputs

Each `results/mcam_state_variation_compositions/stdevXX/NxN/` directory contains:

- `mcam_voltage.{png,pdf,svg}` and `mcam_voltage_active_region.{png,pdf,svg}`.
- `mcam_latency.{png,pdf,svg}` and `mcam_latency_active_region.{png,pdf,svg}`,
  showing search latency in nanoseconds with matching nominal and variation bands.
- `samples.csv.gz`: the selected composition and placement indices, every
  generated sample index, delta counts, squared distance,
  conductance in siemens, voltage in volts, search latency and matchline delay
  in seconds. Gzip keeps repeated sample
  metadata compact; no sample points are discarded when rendering.
- `nominal_compositions.csv`: every exact nominal delta-count composition,
  its coordinate-permutation multiplicity, and its physical result.
- `nominal_voltage_plot_groups.csv` and `nominal_latency_plot_groups.csv` when
  that metric is rendered: metric-specific display groups used to render
  shared-point multiplicity; each records its display bin width and mean result
  within the bin.
- `voltage_bounds.csv`: unsmoothed nominal and variation voltage bounds and
  variation conductance bounds for every reachable distance.
- `latency_bounds.csv`: unsmoothed nominal and variation search-latency bounds
  in seconds for every reachable distance.
- `bound_witnesses.csv`: the delta-count compositions associated with each
  conductance endpoint. Minimum voltage corresponds to maximum conductance and
  maximum voltage corresponds to minimum conductance.
- `corner_results.csv`: explicit evaluations of those witness compositions at
  the appropriate -3σ or +3σ resistance corner.
- `metadata.json`: state data, variation controls, seed semantics, units, and
  sensing convention.
- `inputs/` and `nominal_inputs/`: snapshots of local component configs used
  for the sampled and nominal evaluations. Shared technology and sense-amplifier
  library references are resolved to absolute paths in this checkout.

Search latency includes the model's nominal peripheral latency plus the
composition-dependent matchline delay. At distance zero, exact matches use the
one-unit-mismatch boundary delay, matching `evaluate_distance_samples()`.
Latency bounds convert the extremal conductances through the same timing model,
using boundary conductance limits at distance zero. Each latency sample is
checked against both the exact bounds and the smoothed outer envelope.

The C++ and Python APIs are described in [python-api.md](python-api.md).
`evaluate_distance_samples()` returns individual results; `evaluate_distance()`
retains its existing averaged result. `distance_voltage_bounds()` exposes exact
voltage and search-latency composition/support bounds, with `include_variation=False` selecting nominal
bounds. The new analysis APIs reject unsupported corner variation mode.

## Validation

```sh
make -j4 test-cam-subarray-variation test-pybind-match
make test-mcam-voltage-plot
make test
```

Coverage includes exhaustive small-vector/query bounds, seeded samples and
averages, zero variation, both granularities, state-order preservation, copied
config inputs, exported data, and conservative smoothing containment.
