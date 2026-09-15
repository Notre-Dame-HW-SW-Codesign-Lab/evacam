# Legacy MCAM variation-aware distance statistics

Run `python3 scripts/plot_mcam_distance_statistics.py` after building the Python
binding. The default output is a **new** `results/mcam_legacy_distance_statistics/`
directory. The script refuses to reuse an existing output directory; use
`--output-dir` to select another destination for subsequent runs. It does not
modify the old composition plots or the provisional pair-response plots.

The defaults cover 8- and 16-cell rows at 5% and 10% resistance standard deviation,
with 10,000 varied trials at every reachable squared Euclidean distance. The
query remains all zero, matching the historical composition plots. The original
eight resistances retain their legacy meaning: sorted descending and indexed by
absolute symbol difference. No 8x8 pair-response table is involved.

For each distance, dynamic programming counts the possible ordered stored
vectors. Random choices weighted by the number of remaining completions sample
these vectors uniformly, with replacement. Compositions are consequently weighted
by their coordinate permutations, not sampled uniformly as composition bins.
The old stratified Monte Carlo subset is not pooled to estimate probabilities.
These weights describe an explicit synthetic workload, not a measured workload.

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

Each run writes PNG/SVG/PDF plots, `distance_statistics.csv`, the sampled voltages
in `voltage_samples.npz`, input snapshots, and `metadata.json`. The CSV additionally
records the empirical fraction outside the three-SD band and the 0.135th/99.865th
percentiles as diagnostics. Extreme sample quantiles remain noisy at this sample
count. `exact_lower_minus_upper_3sigma_v` subtracts a distance's upper band from
the exact-match lower band; a positive value means separation of those statistical
bands, not an amplifier-margin or hardware-error guarantee.

Use `make test-mcam-distance-statistics` for conditional-sampling, variation,
statistical-summary, and native-voltage-conversion tests.
