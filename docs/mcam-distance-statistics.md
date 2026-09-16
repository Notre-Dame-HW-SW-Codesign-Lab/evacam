# Default MCAM distance statistics with all points

To render the same saved statistics without any points, using thicker lines and
opaque gold/blue bands for higher contrast on a TV, run:

```sh
python3 scripts/plot_mcam_voltage.py --presentation-from results/mcam_distance_statistics_all_points
```

This writes full-range and active-region PNG/PDF/SVG figures under
`results/mcam_distance_statistics_tv/` for the same three sizes and variation
levels. It copies the statistics CSVs exactly and does not rerun simulations.
The mean is dark navy, the ±3σ band gold, the ±1σ band blue, and the exact-match
band gray with dashed boundaries. Points and their colorbar are omitted.
Use `--output-dir` for a different destination, or `--sizes`/`--levels` for a subset.

Add `--with-points` to restore every original nominal composition and varied
trial using the same high-contrast bands, a thicker navy mean curve, larger
nominal markers, and darker orange trial markers. This version defaults to
`results/mcam_distance_statistics_tv_all_points/` and reads the original raw
arrays directly: no binning, deduplication, subsampling, or new simulations.
The colorbar still reports distinct nonzero deltas for each composition.

```sh
python3 scripts/plot_mcam_voltage.py --presentation-from results/mcam_distance_statistics_all_points --with-points
```

Run `python3 scripts/plot_mcam_voltage.py` after building the Python
binding (`make -j4 test-pybind-match`). This defaults to the distance-based model
and statistical output bands. `scripts/plot_mcam_distance_statistics.py` is also
a direct entry point for the same workflow. The default output is a **new**
`results/mcam_distance_statistics_all_points/`
directory. The script refuses to reuse an existing output directory; use
`--output-dir` to select another destination for subsequent runs. It does not
modify the old composition plots or the provisional pair-response plots.

The defaults cover 8-, 16-, and 32-cell rows at 0%, 5%, and 10% resistance standard deviation,
with 10,000 varied trials at every reachable squared Euclidean distance. The
query remains all zero, matching the historical composition plots. The original
eight resistances retain their legacy meaning: sorted descending and indexed by
absolute symbol difference. No 8x8 pair-response table is involved.
The 128-cell case remains excluded. The 64-cell case requires `--band-only`,
which skips the 1,329,890,705 nominal compositions and saves only sampled
distance statistics. For a smaller run, use
`--sizes 8 --levels 5 --samples 1000 --output-dir results/mcam_statistics_preview`.
The historical composition/support-envelope workflow is available with
`python3 scripts/plot_mcam_voltage.py --mode support-bounds`; the provisional
pair workflow requires `scripts/plot_mcam_pair_response.py` explicitly.

Each figure overlays every nominal delta-count composition and every varied
trial at its original coordinates. There is no binning, deduplication, grouping,
or point subsampling, including coincident points. The nominal totals are 6,435,
245,157, and 15,380,937 for 8, 16, and 32 cells respectively. Color represents the
number of distinct nonzero deltas in that individual composition; marker size
is fixed. Full-range figures contain all points, and active-region figures
show all points within the displayed distance range. Scatter layers are
rasterized in PDF/SVG to keep exports practical; axes and text remain vector.
Native voltage conversion and nominal exports run in chunks, and nominal data
is shared across variation levels under `shared_nominal/NxN/`.

For each distance, dynamic programming counts the possible ordered stored
vectors. Random choices weighted by the number of remaining completions sample
these vectors uniformly, with replacement. Compositions are consequently weighted
by their coordinate permutations, not sampled uniformly as composition bins.
The old stratified Monte Carlo subset is not pooled to estimate probabilities.
These weights describe an explicit synthetic workload, not a measured workload.
Completion counts remain exact Python integers when the 32-cell counts exceed
64 bits; sampling then uses float64 conditional probabilities. The 8- and
16-cell cases retain the original integer-ticket sampler and random streams.

Each trial then samples independent per-cell resistances using the configured
relative standard deviations and the existing truncated Gaussian input model:
`max(R*1e-12, R-3*sigma) <= sampled_R <= R+3*sigma`. NumPy supplies the random
draws, with a recorded seed; its random sequence is not identical to the native
sampler. The statistical distribution is the same. Conductances add along a row,
and the native `sense_mcam_conductances` method computes the sensed voltages using
the unchanged legacy RC model and fixed nominal one-step-mismatch sensing time.

The plotted band is the sample mean plus/minus three **output voltage** standard
deviations at each distance (`ddof=1`). It includes both composition diversity
and device variation. It is not the nominal composition range, a simultaneous
three-sigma resistance corner, a confidence interval for the mean, or a certified
error bound. Gaussian output distributions and 99.73% coverage are not assumed.
The arithmetic band is not clipped to physical voltage limits.
Nominal points and trials can lie outside this statistical band. At 0% device
variation, different compositions can still produce nonzero output spread at
the same squared distance.

Each run writes PNG/SVG/PDF plots, `distance_statistics.csv`, the sampled voltages
in `voltage_samples.npz`, input snapshots, and `metadata.json`. The CSV additionally
records the empirical fraction outside the three-SD band and the 0.135th/99.865th
percentiles as diagnostics. Extreme sample quantiles remain noisy at this sample
count. `exact_lower_minus_upper_3sigma_v` subtracts a distance's upper band from
the exact-match lower band; a positive value means separation of those statistical
bands, not an amplifier-margin or hardware-error guarantee.

The shared `nominal_compositions.npy` is an unbinned structured array with
`delta_counts`, `squared_distance`, `voltage_v`, and `nonzero_delta_kinds` fields.
Load it with `numpy.load(path, mmap_mode="r")` to inspect all nominal points
without loading the complete file into memory. The row number identifies the
composition; the counts retain its exact identity even when voltages coincide.

Use `make test-mcam-distance-statistics` for conditional-sampling, variation,
statistical-summary, and native-voltage-conversion tests.

For the 64×64 TV plots, generate statistics without points and render them from
the saved CSVs:

```sh
python3 scripts/plot_mcam_distance_statistics.py --sizes 64 --levels 0 5 10 \
  --samples 10000 --band-only \
  --output-dir results/mcam_distance_statistics_64x64_band
python3 scripts/plot_mcam_distance_statistics.py --sizes 64 --levels 0 5 10 \
  --presentation-from results/mcam_distance_statistics_64x64_band \
  --output-dir results/mcam_distance_statistics_tv_64x64
```
