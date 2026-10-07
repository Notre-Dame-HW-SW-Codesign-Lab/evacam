# Kondo and Tanzawa (2022): SLM bitline investigation

This benchmark implements the single-line model investigation in the
[validation plan](nand-kondo-2022-plan.md). It compares EvaCAM's production
linear RC solver with an independent numerical reference and extracted
publication curves. It does not calibrate flash transistor physics, CAM
search energy, or complete query latency.

## Follow-up: circuit construction explains much of the mismatch (2026-09-25)

**An eight-section series-R/shunt-C circuit agrees with all 50 extracted
Figure 3b voltage-delay points within 3.25%, without changing total R, total
C, cell resistances, source voltages, or the voltage measurement rule.** The
far-cell data-1 current-delay curve agrees within 4.26%. The data-0 current
definition remains unresolved, so complete paper reproduction is still pending.

The original experiment below approximates a continuously distributed line.
On reinspection, Figure 2(a) places capacitors downstream of series resistors,
and Figures 6-8 plot eight spatial samples from 1/8 to 8/8. These observations
motivate a discrete eight-section circuit with 125 kOhm series resistors and
375 fF capacitors at the eight downstream nodes. The original netlist is
unavailable; eight plotted samples alone do not prove its section count.

The follow-up compares three constructions with identical total R and C:

| Circuit construction | Far data-0 baseline voltage delay | Extracted Figure 3b |
| --- | ---: | ---: |
| Original 96 finite volumes | 3.049 us | 3.447 us |
| Eight finite volumes, same endpoint convention | 3.054 us | 3.447 us |
| Eight series-R/shunt-C sections | 3.439 us | 3.447 us |

The control shows that reducing the numerical mesh alone does not fix the
comparison. Capacitance placement in a coarse physical circuit changes its
response. For example, the unloaded eight-section ladder's first RC moment
is `R*C*(8+1)/(2*8)`, 12.5% above the uniform distributed line's `R*C/2`.
Increasing the section count converges to the distributed circuit; it does
not necessarily reproduce a paper that simulated a fixed discrete circuit.
The Figure 3b baseline is extracted separately from Figure 4b's 3.443 us
aggregate baseline in the original table below.

| Publication comparison | Points | Maximum raw relative timing error | RMSE |
| --- | ---: | ---: | ---: |
| Figure 3b, far data 0 | 12 | 1.15% | 0.018 us |
| Figure 3b, far data 1 | 12 | 2.30% | 0.044 us |
| Figure 3b, near data 0 | 13 | 1.36% | 0.013 us |
| Figure 3b, near data 1 | 13 | 3.25% | 0.042 us |
| Figure 4a, far data 1 | 13 | 4.26% | 0.099 us |

All 91 extracted current-waveform points in the twelve Figure 6/7 series
pass the original pixel-plus-5%-of-full-scale comparison allowance with the
eight-section circuit. Sensed-current RMSE ranges from 1.21 to 5.12 nA,
compared with 4.62 to 22.71 nA for the distributed reference evaluated at the
same points. This is not a claim of 5% or 10% relative waveform accuracy.
The largest sensed-current point residual remains 11.34 nA.

### Data-0 measurement is still an open question

Changing the circuit does not reproduce the data-0 current-delay curve under
the original loaded 90-110% rule. At the seven published far-cell delay
points with pulse widths from 0 to 2.2 us, the eight-section circuit's source
current is 18.63-19.25 nA. Its loaded DC current is 9.804 nA. Those points
are inconsistent with either a 10% window around that DC current or a
one-sided 10 nA upper limit, under this circuit interpretation.

A diagnostic 20 nA upper limit, with no lower bound, reproduces all thirteen
far-cell data-0 delay points within 3.37% (0.111 us RMSE). This is nominal
data-0 current plus 10% of nominal data-1 current. It is an **exploratory
measurement hypothesis suggested by the residuals**, not an independently
established paper definition. No threshold optimization was performed, and
this alternative does not replace the original benchmark's rule or status.
Agreement on the same curves used to suggest a rule cannot validate that rule.

### Follow-up scope and reproduction

The [assumption contract](nand-kondo-2022.assumptions.yaml) records the source
evidence, controls, and exploratory status. No observations or original
acceptance limits were changed. Comparisons use only the native 25% and 100%
taps: 33% and 66% are not nodes of an eight-section line, and are not silently
rounded or given extra circuit sections. Figure 4b's maximum over all six
positions and Figure 3a's ambiguous data label remain excluded from this
follow-up. The circuit choice was investigated after seeing the original
residuals; these are improved correlations, not a held-out validation study.

```sh
make test-nand-kondo-validation       # 20 focused tests, including this follow-up
make investigate-nand-kondo
```

The follow-up writes `output/validation/nand-kondo-2022/assumptions/report.json`,
62 compiled C++ case snapshots, and three PNG/SVG pairs:
`voltage-circuit-assumptions`, `current-circuit-assumptions`, and
`current-measurement-assumptions`. The figures compare publication observations
with the independent circuit solutions. The production solver independently
passes the original numerical limits for all 62 eight-section publication
cases; this is supporting evidence, separate from the paper residuals above.
The current-rule helper also has an analytic one-node crossing test. All three
final plots were visually inspected. Tests remain in the existing CI target.

Normal exit zero indicates that the investigation and numerical checks
completed. `--require-reproduction` still returns 2. To establish reproduction,
obtain independent confirmation of the original RC sections, endpoint
capacitances, and data-0 current measurement rule; then resolve the interior
taps and excluded figures. No production NAND3D device parameters or solver
equations were altered for this follow-up.

## Original distributed-line findings (2026-09-25)

**The linear solver passes numerical verification; the paper is not fully
reproduced.** All 324 primary cases settle and meet the frozen numerical
limits. Four of the six sensed-current waveform series exceed the literature
allowance. All six cell-current series are within that allowance, which uses
5% of the displayed current full scale plus extraction and numerical
uncertainty; this is not a 5% relative-current accuracy claim.

The worst-case Figure 4b sensed-current curve has about +2.179 us mean bias,
2.225 us RMSE, and 2.692 us maximum absolute error under the primary loaded
90-110% convention. None of its 22 extracted points passes the frozen rule.
The Figure 4b voltage-delay curve has -0.158 us mean bias, 0.322 us RMSE,
and 0.514 us maximum error. These discrepancies are much larger than the
observed numerical and mesh errors.

| Primary worst-case quantity | EvaCAM SLM | Extracted Figure 4b |
| --- | ---: | ---: |
| Voltage delay, no pre-emphasis | 3.049 us | 3.443 us |
| Minimum voltage delay on sampled pulse grid | 1.952 us | 2.193 us |
| Sense-current delay, no pre-emphasis | 8.376 us | 6.338 us |
| Minimum sense-current delay on sampled pulse grid | 4.863 us | 3.649 us |

The corresponding modeled reductions are 35.96% for voltage and 41.93% for
current. Those ratios are close to the paper's Table 2 values of 36% and 43%,
while the absolute curves disagree. No resistance, capacitance, or threshold
was tuned to obtain those ratios. Aggregate percentages alone would have
hidden the mismatch.

The data-0 alternative (sense current below nominal 10 nA, with no lower
bound) gives a worst baseline of 12.009 us and a sampled minimum of 4.051 us.
Every reported convention has its own crossing refinement; the alternative
is not interpolated across a sparse late-time tail. Its changed result
illustrates the importance of obtaining the authors' actual measurement rules.
It does not resolve the other waveform discrepancies.

The paper's unknown netlist discretization, source transition details, and
ambiguous measurement definitions prevent attributing these residuals to a
specific missing effect. The public review-page index refers to author-response
attachments, but direct retrieval returned HTTP 429/403 during this audit;
those attachments were not obtained. This does not establish that a deck or
additional characterization is unavailable from the authors.

### Numerical and convergence evidence

| Check | Largest observed error | Frozen limit |
| --- | ---: | ---: |
| Cell voltage, 324 cases | 1.01e-10 V | 10 uV |
| Sensed current, 324 cases | 5.47e-06 nA | 0.01 nA |
| Primary settling time, same observation grid | 0.000112 ns | 5 ns |

The settling comparison above isolates solver error on the same grid. It is
not the uncertainty of a continuous-time event or of a paper figure.
Crossing brackets are at most 1 ns (up to floating-point roundoff).

The production solver uses a 20 ns maximum step and 1e-10 V local tolerance.
The reference stamps a dense conductance matrix from edge connectivity and
grounded shunts, then evaluates its symmetric matrix exponential. It is also
checked against a separately assembled nonsymmetric augmented exponential.
DC division, KCL, integrated shunt charge, analytic one-node responses,
nonuniform initial states, two driven ends, and pre/post-transition continuity
have independent checks.

- Mesh: 24, 48, 96, 192, and 384 nominal segments. At the far data-1 cell
  with a 2 us pulse, sensed-current settling approaches 2.913542 us;
  the 96-segment value is 2.913252 us. Doubling the mesh at every compared
  timing point changes delay by at most 0.0354%, below the 0.5% criterion.
- Integration tolerance: 1e-7, 1e-9, and 1e-11 V. In the near data-1
  2.4 us case, maximum current error decreases from about 0.00133 nA to
  0.0000141 nA to 0.000000142 nA.
- Maximum step: 100, 20, and 5 ns at fixed tolerance, with 200 ns base
  observations and separately refined crossings. Delays agree to the shown
  precision; adaptive error control dominates this comparison.
- Observation spacing: 40, 10, and 2 ns, each with event refinement.
  Delay changes are below 0.001 ns in the declared near-cell case, well
  below 0.5%. This is a convergence study, not a proof that every possible
  excursion is found for arbitrary inputs.
- Literal percentages versus exact thirds: at a 2.4 us pulse, the largest
  voltage-delay change is 10.945 ns and the largest current-delay change is
  5.241 ns across the tested states. The interpretations remain distinct.

### Per-series literature residuals

Bias is model minus publication. Delay units are us, voltage units mV, and
current units nA. `Within rule` counts individual points satisfying the stated
uncertainty budget and engineering allowance. It does not resolve an ambiguous
source definition. Figures 3a, 3b, 4a, and 4b remain incomplete as reproduction
claims even where points satisfy the rule. Exact per-point signed residuals,
absolute/relative timing errors, and separated uncertainty components are in
`report.json`. `near` is 25%, `far` is 100%, and `third` / `two_thirds` use
printed 33% / 66%; data-state suffixes preserve the source labels.

| Series | Points | Unit | Bias | RMSE | Maximum absolute error | Within rule |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| `fig3a_common_rise` | 5 | mV | +14.250 | 14.343 | 17.038 | 5/5 |
| `fig3a_p1.0` | 6 | mV | +22.784 | 22.975 | 26.163 | 6/6 |
| `fig3a_p1.2` | 6 | mV | +22.355 | 22.481 | 25.281 | 6/6 |
| `fig3a_p1.4` | 5 | mV | +25.548 | 25.559 | 26.931 | 5/5 |
| `fig3a_p1.6` | 5 | mV | +24.816 | 25.010 | 26.689 | 5/5 |
| `fig3a_p1.8` | 5 | mV | +26.690 | 26.693 | 27.243 | 5/5 |
| `fig3a_p2.0` | 5 | mV | +27.059 | 27.060 | 27.425 | 5/5 |
| `fig3a_p2.2` | 5 | mV | +26.006 | 26.060 | 27.515 | 5/5 |
| `fig3a_p2.4` | 4 | mV | +27.113 | 27.119 | 27.866 | 4/4 |
| `fig3b_far0` | 12 | us | +0.072 | 0.723 | 1.790 | 2/12 |
| `fig3b_far1` | 12 | us | -0.033 | 0.517 | 1.605 | 4/12 |
| `fig3b_near0` | 13 | us | +0.073 | 0.396 | 1.383 | 9/13 |
| `fig3b_near1` | 13 | us | +0.085 | 0.415 | 1.431 | 9/13 |
| `fig4a_far1` | 13 | us | -0.093 | 0.589 | 1.087 | 4/13 |
| `fig4a_far0` | 13 | us | +3.292 | 3.760 | 6.129 | 0/13 |
| `fig4a_third1` | 13 | us | -0.507 | 1.163 | 2.586 | 4/13 |
| `fig4a_third0` | 13 | us | +2.849 | 3.505 | 6.103 | 1/13 |
| `fig4a_two_thirds0` | 13 | us | +2.953 | 3.577 | 6.139 | 1/13 |
| `fig4b_sense_delay` | 22 | us | +2.179 | 2.225 | 2.692 | 0/22 |
| `fig4b_voltage_delay` | 22 | us | -0.158 | 0.322 | 0.514 | 9/22 |
| `fig6_0_cell_current` | 9 | nA | +1.568 | 1.761 | 2.861 | 9/9 |
| `fig6_0_sense_current` | 8 | nA | -12.531 | 16.339 | 29.502 | 5/8 |
| `fig6_1_cell_current` | 8 | nA | +2.219 | 2.309 | 3.245 | 8/8 |
| `fig6_1_sense_current` | 6 | nA | -18.052 | 22.750 | 40.060 | 4/6 |
| `fig6_2_cell_current` | 7 | nA | +3.158 | 3.275 | 5.042 | 7/7 |
| `fig6_2_sense_current` | 7 | nA | -8.009 | 14.119 | 30.881 | 5/7 |
| `fig7_0_cell_current` | 8 | nA | +6.235 | 6.657 | 10.190 | 8/8 |
| `fig7_0_sense_current` | 7 | nA | -7.121 | 10.528 | 18.853 | 6/7 |
| `fig7_1_cell_current` | 8 | nA | +6.559 | 6.912 | 9.639 | 8/8 |
| `fig7_1_sense_current` | 7 | nA | -4.532 | 9.473 | 17.630 | 7/7 |
| `fig7_2_cell_current` | 9 | nA | +5.153 | 6.696 | 12.855 | 9/9 |
| `fig7_2_sense_current` | 7 | nA | -2.709 | 4.623 | 8.697 | 7/7 |

### Implementation and checks

The production addition is an overload accepting one nonnegative grounded
conductance per node. Existing callers retain the zero-shunt interface.
Ground current is included in integrated charge accounting and the passive
voltage envelope. The probe adds piecewise phases, selected-node observations,
instantaneous boundary currents, and carried node state while preserving its
original endpoint invocation and result fields.

The following completed successfully:

```sh
make test-nand-kondo-validation                 # now 20 focused Python tests
make validate-nand-kondo                        # complete offline experiment
make -j4 test-nand-rc-ladder test-nand3d-numerics \
  test-nand3d-config test-nand3d-model test-nand3d-integration test-nand3d-results
make -j4 test-pybind-nand3d test-nand-model test-nand-integration
make test                                      # Valgrind: 0 errors, 0 bytes left
make test-inventory-generator check-unit-test-inventory
```

All four final PNG plots were visually inspected; their SVG counterparts
come from the same Matplotlib figures. Source/executable hashes in the final
report were checked against the files used. The strict reproduction gate is
covered by a focused regression that returns 2 even when numerical checks pass.
The new test is included in `test-unit` and the GitHub Actions matrix.

The next evidence needed for a reproduction claim is the original SLM deck,
or an explicit section/endpoint capacitance specification, source waveforms,
and voltage/current event definitions. Preserve these residuals while those
inputs are resolved. The current evidence supports the implemented linear
circuit equations; it does not validate a physical NAND device or CAM macro.

## Reference and measurement contract

Junnosuke Kondo and Toru Tanzawa, “Pre-Emphasis Pulse Design for Reducing
Bit-Line Access Time in NAND Flash Memory,” *Electronics* 11, 1926 (2022),
[DOI 10.3390/electronics11131926](https://doi.org/10.3390/electronics11131926).
The publisher's updated PDF of 22 June 2022 was retrieved on 25 September
2026. Its hash and original URL are in
[`nand-kondo-2022.reference.yaml`](nand-kondo-2022.reference.yaml).
The full PDF and extracted JPEGs remain in the ignored local source cache.

The frozen fixture uses a 1 MOhm uniform distributed line with 3 pF total
capacitance, a 5 MOhm or 50 MOhm selected cell, and an ideal source that
steps from zero to 0.6 V, then to 0.5 V at the selected pulse width. The
zero-pre-emphasis case steps directly to 0.5 V. Initial line voltages are
zero. The paper supplies the resistance, capacitance, and bias values;
uniform distribution, endpoint treatment, and instantaneous source transitions
are declared interpretations. The paper does not disclose its section count,
netlist, numerical dataset, or SPICE version.

The first physical line segment is the solver's source-boundary resistance.
An interior cell is a grounded shunt at its exact coordinate; the open-ended
line beyond it remains present. Finite-volume node capacitances conserve
3 pF including the half-segment capacitance attached directly to the ideal
source. This algebraic capacitance has explicit switching charge and stored
energy changes. Its impulses are excluded from finite sensed-current samples;
its supply energy is left unspecified without a switching-path model.

The current positive direction is from the source into the line. Cell current
is local cell voltage divided by its resistance. Loaded DC current is
`0.5 / (position * 1e6 + Rcell)`. The primary settling windows are 90-110%
of loaded DC cell voltage and sensed current. Settling is the last window
entry, including both sides of later pulse transitions. A passive-network
maximum-principle bound certifies the final tail. Crossings retain brackets
and are refined to at most 1 ns for the primary C++ measurement. Observation
spacing is refined separately to check detection of excursions between samples.
A bounded horizon produces an unresolved result when the tail cannot be
certified; exhausted solver budgets fail explicitly, without a success report.

## Publication ambiguity and extraction

The committed dataset contains 296 observations in 32 series. See the
[data contract](data/kondo-2022/README.md) for pixel coordinates, calibration,
uncertainty, independent extraction checks, and exclusions. Source observations
are never inferred from simulator output.

Several distinctions prevent an unconditional reproduction claim:

- Nominal 100 nA / 10 nA values differ from the currents after line loading.
  A literal nominal 90-110% window is unreachable for some far-end data-1
  cases. The runner retains those unresolved alternatives.
- The abstract describes a two-sided current window; the discussion of
  Figure 8 allows data-0 current below nominal 10 nA. Both conventions are
  evaluated, retaining the two-sided loaded convention as the primary one.
- Figure 3a labels data 0, but its window is centered around 0.476 V and
  its waveform suggests a different state. Both stated data 0 and alternative
  data 1 are compared without relabeling the observations.
- The printed 33% and 66% positions may mean literal percentages or rounded
  thirds. The main run uses literal percentages and reports the difference
  when exact thirds are used.
- The upper Figures 6/7 spatial profiles have no numerical time legend.
  They are not used as time-specific voltage evidence.

## Reproduce offline

Install the ordinary C++ build dependencies and Python NumPy, SciPy,
Matplotlib, and PyYAML. No PDF reader or internet connection is required for
tests or report generation.

```sh
make test-nand-kondo-validation
make validate-nand-kondo
make investigate-nand-kondo
make test-nand-rc-ladder test-nand3d-numerics
```

The full runner evaluates 324 cases: six cell positions, both data states,
and 27 pulse widths (0-5 us in 0.2 us increments plus 2.3 us). It also runs
mesh, integration-tolerance, maximum-step, observation, and position-definition
studies. `--jobs` controls independent-case concurrency. The default output is
`output/validation/nand-kondo-2022/`:

- `report.json`: complete per-case measurements, signed residuals, uncertainty
  budgets, numerical comparisons, convergence, and source/executable hashes.
- `inputs/`: complete circuit/phase snapshots and raw production-probe outputs.
- `current-waveforms.png` / `.svg`: six publication current comparisons.
- `voltage-waveforms.png` / `.svg`: Figure 3a voltage comparison and residuals.
- `delay-comparisons.png` / `.svg`: individual and worst-case settling curves.
- `convergence-and-definitions.png` / `.svg`: mesh and convention sensitivity.

The numerical gate is 10 uV voltage, 0.01 nA current, and 5 ns settling-time
error. Mesh and observation convergence target 0.5%. Literature allowances
combine source extraction uncertainty, abscissa sensitivity, numerical and
mesh uncertainty, and a separately declared 5% engineering allowance (timing
ordinate or waveform full scale). Relative timing errors use a 1 ns denominator
floor. Raw residuals remain visible even when uncertainty is large.

Normal exit zero means the numerical benchmark completed successfully.
`--require-reproduction` returns 2 while publication ambiguities prevent a
reproduction claim. Passing solver tests does not change that status.
