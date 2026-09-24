# NAND TCAM validation audit: Yang et al. (2023)

Audit date: 2026-09-24. **The current NAND model is synthetic and is not
validated against this paper.** The available publication material supports
a qualitative topology comparison, but does not expose the electrical
parameters and measurement boundaries needed for numerical reproduction.
An independent linear RC calculation also finds a concrete false acceptance
of the configured sensing margin for a long string.

No electrical values were fitted to the paper's headline energy. This audit
adds a reference solver, executable probe, reproducible sweeps, and regression
tests; it leaves the production model's numerical calculations unchanged.

## Publication evidence and access

Haozhang Yang, Peng Huang, Runze Han, Xiaoyan Liu, and Jinfeng Kang,
“An ultra-high-density and energy-efficient content addressable memory design
based on 3D-NAND flash,” *Science China Information Sciences* 66, 142402 (2023),
[DOI 10.1007/s11432-021-3502-4](https://link.springer.com/article/10.1007/s11432-021-3502-4).
The publisher abstract reports **0.196 fJ/bit/search from HSPICE**, and a
**157× cell-density comparison for 16-layer NAND**. These are simulation and
cell-density claims, not measured silicon energy or full-macro area.

The [Science China Press summary](https://techxplore.com/news/2023-05-ultra-high-density-energy-efficient-content-memory-based.html)
and its [structural figure](https://scx1.b-cdn.net/csz/news/800a/2023/an-ultra-high-density.jpg)
are public. My interpretation of the figure is that complementary wordline
pairs occur along each stored-word string, consistent with EvaCAM's core
series-key organization. The figure does not establish EvaCAM's reserved
validity pair, peripheral circuit implementation, or operating conditions.
The summary's greater-than-0.6 V window concerns a separate four-level CAM;
it is not a reference for this SLC implementation's per-class sensing margin.

Public-access checks did not obtain the article or a simulation supplement:

| Access path | Result during this audit |
| --- | --- |
| Springer | Subscription preview; no public SharedIt link |
| [Official SCIS PDF](https://scis.scichina.com/en/2023/142402.pdf) | Timed out, including a curl attempt |
| SciEngine and PKU repository searches | No accessible full-text download found |
| [OpenAlex DOI record](https://api.openalex.org/works/https://doi.org/10.1007/s11432-021-3502-4) | Closed-access metadata; no repository full text or PDF URL |
| arXiv exact title/author searches | No matching preprint found |
| Internet Archive CDX / availability API | HTTP 503 / 429; archive availability remains unknown |

These access results do not prove that an author or archived copy does not
exist. The missing evidence is explicitly recorded as null in
[`nand-yang-2023.reference.yaml`](nand-yang-2023.reference.yaml): array size,
logical bit denominator, compact-device/current data, biases, parasitics,
precharge and evaluation timing, sensing requirements, included peripherals,
activity assumptions, and waveform/table reference points.

## Energy, latency, and area comparison

The ordinary EvaCAM executable was run with the unchanged canonical
[`NAND_TCAM.config.yaml`](../../config/NAND_TCAM/NAND_TCAM.config.yaml).
It searches 64 entries of 32 logical bits: 2,048 logical bits, represented by
4,352 physical flash cells, including validity and padding cells.

| Quantity | EvaCAM synthetic example | Published reference / comparability |
| --- | ---: | --- |
| Whole-query dynamic energy | 19.845905 pJ | Paper's whole-query size/boundary unavailable |
| Energy / searched logical bit | 9.690383 fJ/bit/search | Reported 0.196 fJ/bit/search; unmatched conditions |
| Whole-query latency | 80 ns | No comparable timing point in accessible material |
| Area | 304.288 µm², planar model | 3D cell-density comparison cannot validate this footprint |

Normalization is `whole_query_energy / (entries * logical_key_width)`, not
division by the number of encoded physical cells. The table is contextual;
**the difference between the two energy values is not a model error estimate**.
The paper's inclusion of charge pumps, wordline drivers, sensing, output
handling, initial charging, and reset could not be established. Its bit
denominator and benchmark geometry must also be reconciled before comparison.

The synthetic energy ledger provides an actionable diagnostic: wordline
capacitance charging contributes 8.385429 fJ/logical-bit, roughly 86.5% of the
total. Bitline/internal-node precharge contributes 0.655981 fJ/logical-bit.
With all wordlines initially raised to 5 V, these costs follow the configured
capacitances and switching schedule. Changing them to reproduce a single
headline number would not establish physical accuracy.

This baseline includes EvaCAM's local wire and bank accounting. The separate
RC experiment below deliberately omits wire parasitics in **both** circuits
to isolate the exponential approximation. Consequently its 68-wordline
voltages and margins are not the ordinary baseline's wire-loaded values.

## Independent RC experiment

[`NandValidationProbe.cpp`](../../tests/NandValidationProbe.cpp) invokes the
actual C++ NAND model with zero wire resistance/capacitance and exports its
device parameters, geometry, voltages, conductance, reference, and margins.
It also exports cases that the bank would reject, so rejection behavior
cannot hide an approximation error. A model time constant is inferred from
its reported voltage; it is not treated as independent reference evidence.

[`nand_rc_reference.py`](../../scripts/nand_rc_reference.py) independently
solves the nodal equations `C dv/dt = -G v` through a symmetric tridiagonal
eigendecomposition. The source-to-drain ladder is:

```text
ground -- Rselect -- Csource -- Rdata[0] -- Cinternal[0] -- ...
       -- Rdata[L-1] -- Cinternal[L-1] -- Rselect -- Cbitline
```

Each capacitance is a shunt to ground. All nodes initially equal 0.8 V for
the discharge comparison. Resistances are 10 kΩ read-on, 5 kΩ pass, 1 GΩ off,
and 1 kΩ select; capacitances are 1 fF source, 0.05 fF per internal node, and
20 fF bitline. Decision time is 50 ns, offset allowance 10 mV, and required
per-class margin 100 mV. These are synthetic inputs, not extracted paper data.

The sweep uses 16, 32, 64, 68, 128, 256, and 512 data wordlines, with key
widths 7, 15, 31, 32, 63, 127, and 255 respectively. Each geometry exercises
all-zero match, all-one match, wildcard match, source-side mismatch,
drain-side mismatch, and programmed invalid marker: **42 cases**. This is a
controlled geometry sweep, not a claim that 16 data wordlines reproduce the
paper's 16-layer design.

Reference tests cover single-RC and two-RC analytic responses, an independently
assembled dense matrix exponential with nonuniform initial voltages, the
static first-moment identity, precharge boundaries, and a 1 GΩ blocking
device. The first moment is calculated separately with a static nodal solve.
Across the sweep it agrees with the C++ model to a maximum relative error of
`4.61e-10`. That verifies the moment calculation for this declared circuit;
it does not establish that a single exponential reproduces its waveform.

| Data wordlines | C++ match voltage at 50 ns | Full RC match voltage at 50 ns | Maximum sampled waveform error, this pattern |
| ---: | ---: | ---: | ---: |
| 16 | 0.000000002 V | 0.000000001 V | 5.361 mV |
| 32 | 0.000039522 V | 0.000035161 V | 9.727 mV |
| 64 | 0.006619 V | 0.006022 V | 17.578 mV |
| 68 | 0.008590 V | 0.007834 V | 18.320 mV |
| 128 | 0.085423 V | 0.080705 V | 30.237 mV |
| 256 | 0.299009 V | 0.299771 V | 47.850 mV |
| 512 | 0.538209 V | 0.582868 V | 67.805 mV |

The table uses the all-zero matching pattern. Waveforms are sampled on linear
and logarithmic grids through six first-moment time constants, with the
decision instant explicitly included. Sampled maxima are not a proof of the
continuous-time maximum. A 10 mV diagnostic threshold was chosen for this
audit, not taken from the paper: 16 of 42 waveforms exceed it somewhere on
the grid, and 3 exceed it at the decision instant. The largest sampled error
across all patterns is 67.881 mV; the largest decision error is 44.659 mV.

**Concrete failed check:** at 512 wordlines, the C++ model chooses a reference
of 0.668113 V and reports 119.903 mV available margin for the all-zero match.
The independent waveform gives
`0.668113 - 0.582868 - 0.010 = 0.075245 V`, below the required 100 mV.
The all-one match also fails. Thus two sampled cases pass the model's margin
check while failing the same check with the full linear-network voltage.
One valid counterexample is sufficient; no exhaustive exact worst-pattern
ordering is claimed. At 68 wordlines, the all-zero decision error is only
0.756 mV and its time-to-half-precharge (`t50`) error is about 1.13%, but that result
does not generalize to longer strings or nonlinear flash devices.

Generated curves: [`rc-comparison.png`](../../results/nand-validation/rc-comparison.png)
and [`rc-comparison.svg`](../../results/nand-validation/rc-comparison.svg).

## Precharge initial-condition check

The discharge experiment above deliberately grants the model its assumed
uniform initial voltage. A separate experiment asks whether the configured
5 ns precharge could establish it from discharged internal nodes. It opens
the source select, sets all data devices to their pass resistance, and drives
the bitline with an ideal 0.8 V step. Finite driver impedance, wire parasitics,
and nonlinear flash effects are absent.

| Data wordlines | Source-end voltage after 5 ns | Time to 99% of 0.8 V |
| ---: | ---: | ---: |
| 68 | 0.798571 V | 3.680 ns |
| 128 | 0.696641 V | 10.635 ns |
| 256 | 0.271899 V | 37.263 ns |
| 512 | 0.012972 V | 138.774 ns |

The 99% criterion is an audit diagnostic, not a paper specification. For the
longer strings the assumed uniform precharge state is unsupported even in
this ideal-driver experiment. This is a separate limitation from the
exponential waveform error; the two experiments should not be combined as
though they used the same initial state.

## Reproduce and inspect

From the repository root, with the project's C++ build dependencies and
Python NumPy, SciPy, Matplotlib, and PyYAML installed:

```sh
make -j4 validate-nand-literature
make test-nand-rc-reference test-nand-validation
```

The default audit directory is `results/nand-validation/`. It contains:

- `audit.json`: explicit validation status, missing evidence, diagnostics,
  source/executable SHA-256 hashes, and Python-library versions.
- `baseline.yaml` and `baseline-energy-breakdown.csv`: actual whole-query
  output and a logical-bit-normalized energy ledger.
- `rc-comparison.csv`, `waveforms.csv`, and `precharge-comparison.csv`:
  numerical comparisons and time series.
- `rc-comparison.png` / `.svg`: a standalone plot generated with Matplotlib.
- `inputs/wl*/`: complete input snapshots and raw C++ probe output.

These generated artifacts are under the ignored results directory; the
scripts, reference metadata, tests, and this report are source-controlled
inputs. Regenerate the results when model code or parameters change. The
Make target rebuilds the binary and probe before running the comparison.

The audit command's normal exit code zero means the experiment completed,
not that the model was validated. For an explicit paper-validation gate:

```sh
OPENBLAS_NUM_THREADS=1 python3 scripts/validate_nand_literature.py --require-paper-validation
```

This currently returns **2**, preserving the failed/not-established status.
Populating missing metadata alone cannot make it pass: independent numerical
reference-point comparisons must be implemented first. Regression tests pass
when they correctly detect and preserve the documented counterexample.

## Required follow-up before physical accuracy claims

1. Replace the single-exponential voltage evaluation with a validated
   multi-node transient backend, or explicitly restrict its use to a
   characterized approximation domain. Recompute the reference and both
   class margins from that backend; retain the 512-wordline case as a
   regression that must cease to be falsely accepted.
2. Model the actual precharge transient and carry its final node voltages
   into evaluation. Derive or validate precharge duration for each geometry,
   including driver impedance and wire loading. Keep charge/energy accounting
   consistent with that state evolution.
3. Obtain the paper's full parameters or simulation deck and record each
   quantity with its source and uncertainty. Match encoding, validity policy,
   logical geometry, bias waveforms, peripheral scope, and energy denominator
   before constructing a separate paper configuration.
4. Compare match and mismatch waveforms, mismatch locations, sensing window,
   threshold-crossing time, and energy integrated over identical rails and
   intervals. Include layer-count and parasitic sweeps, not just one energy
   scalar. Report absolute and relative errors using declared acceptance
   criteria and the reference's numerical/digitization uncertainty.
5. Add voltage-dependent flash current/capacitance and a separate 3D area
   model if those paper outputs are targets. Validate both against device or
   circuit data; agreement with an ideal linear ladder does not validate
   threshold distributions, retention, disturb, program/erase behavior, or
   physical 3D density.
