# MCAM resistance-variation sensitivity inputs

These copied configurations derive from
`config/2FeFET_MCAM/2FeFET_MCAM_32x32.config.yaml` and its architecture,
cell, memory-device, and sensing files. The original files remain unchanged.
Nominal resistance states and searchline voltages remain provisional; the
uniform 0%, 5%, and 10% standard deviations are illustrative sensitivity
inputs, not measured or calibrated process statistics.

Each folder contains five square arrays, with both vector dimensions and
comparison columns set to the array width. `stdev00` is the zero-variation
baseline; `stdev05` and `stdev10` apply the stated fractional standard
deviation to every one of the eight `mcam.state_variation` entries.
All runs specify `variation.mode: monte_carlo`, `samples: 1000`, `seed: 9876`,
and `monte_carlo_granularity: cell` in the memory-device file. No general
on/off-resistance variation is configured.

`cell` granularity uses separate draws for each modeled cell and resistance
state, reproducible for a fixed sample index and seed. `effective` would reuse
one draw per state across cells within a sample, introducing spatial
correlation; these files use `cell`. Resistance draws use a bounded Gaussian
with sigma equal to the nominal resistance times the configured fraction,
restricted to nominal ±3 sigma with a positive lower floor of nominal × 1e-12.
The sampler uses rejection sampling with a bounded fallback. Zero sigma
returns nominal resistance exactly. The nominal sense decision stays fixed
across samples; it is not retuned to each resistance realization.

Each folder has its own `2FeFET_MCAM.cell.yaml`,
`2FeFET_MCAM.memory_device.yaml`, and `2FeFET_MCAM.sensing.yaml`.
Technology and sense-amplifier references point to `../../lib/`.
Each top-level file below has a same-named `.architecture.yaml` companion
(replace `.config.yaml` with `.architecture.yaml`).

- `stdev00/2FeFET_MCAM_128x128.config.yaml`
- `stdev00/2FeFET_MCAM_16x16.config.yaml`
- `stdev00/2FeFET_MCAM_32x32.config.yaml`
- `stdev00/2FeFET_MCAM_64x64.config.yaml`
- `stdev00/2FeFET_MCAM_8x8.config.yaml`
- `stdev05/2FeFET_MCAM_128x128.config.yaml`
- `stdev05/2FeFET_MCAM_16x16.config.yaml`
- `stdev05/2FeFET_MCAM_32x32.config.yaml`
- `stdev05/2FeFET_MCAM_64x64.config.yaml`
- `stdev05/2FeFET_MCAM_8x8.config.yaml`
- `stdev10/2FeFET_MCAM_128x128.config.yaml`
- `stdev10/2FeFET_MCAM_16x16.config.yaml`
- `stdev10/2FeFET_MCAM_32x32.config.yaml`
- `stdev10/2FeFET_MCAM_64x64.config.yaml`
- `stdev10/2FeFET_MCAM_8x8.config.yaml`

Example from the repository root:

```sh
./EvaCAM config/2FeFET_MCAM_variation/stdev05/2FeFET_MCAM_32x32.config.yaml
```

These files specify variation controls; use an MCAM exact-match evaluation
path to collect per-sample decision/margin statistics. Ordinary nominal
summary metrics alone do not demonstrate the sampled decision distribution.

Generate the voltage plots and raw samples from these filesets with:

```sh
make -j4 test-pybind-match
python3 scripts/plot_mcam_voltage.py
```

See [the plotting guide](../../docs/mcam-state-variation.md) for single-size
runs, effective-granularity comparisons, output files, and the meaning of the
smooth outer bounds.
