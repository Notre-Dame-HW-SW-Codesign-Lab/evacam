# Plan: Kondo and Tanzawa bitline RC validation

Status: planned; this document does not report a completed reproduction.

## Objective and evidence boundary

Reproduce the single-line model (SLM) in Kondo and Tanzawa,
[Pre-Emphasis Pulse Design for Reducing Bit-Line Access Time in NAND Flash
Memory](https://doi.org/10.3390/electronics11131926), Electronics 11, 1926
(2022). Compare EvaCAM's production RC solver with published transient and
settling-time curves, with numerical error and extraction uncertainty reported
separately.

This establishes correlation for a distributed bitline circuit. It does not
validate NAND3D transistor physics, CAM search energy, complete query latency,
or commercial-chip performance. The canonical NAND3D device remains synthetic.
The paper's coupled three-line model and PASS-transistor variant are outside
this first implementation.

## Starting point and published inputs

`include/model/NandRcLadder.h` and `src/model/NandRcLadder.cpp` implement the
production linear solver. It accepts grounded capacitances, series resistors,
finite-resistance end boundaries, and initial node voltages. It currently
returns endpoint states and integrated source charge, with no interior
resistive shunts or waveform interface. The compiled probe and independent
references already exist in `tests/NandRcLadderProbe.cpp`,
`tests/test_nand3d_numerics.py`, and `scripts/nand_rc_reference.py`.

Table 1 and Figures 2-7 of the paper supply the following starting information:

| Quantity | Published value or reference |
| --- | --- |
| Bitline resistance / capacitance | 1 MOhm / 3 pF |
| Cell resistance, data 1 / 0 | 5 MOhm / 50 MOhm |
| Nominal cell current, data 1 / 0 | 100 nA / 10 nA |
| Pre-emphasis / final input voltage | 600 mV / 500 mV |
| Cell positions along the line | 25%, 33%, 50%, 66%, 75%, 100% |
| Circuit and current definitions | Figures 2(a) and 5 |
| Delay and transient references | Figures 3, 4, 6, and 7 |

The paper uses a 90-110% settling window. Recover its precise voltage and
current targets before freezing fixtures: distinguish nominal values from
loaded steady-state values, and distinguish sense-terminal current from cell
current. Do not substitute EvaCAM's CAM decision-time definition. Source:
[open article](https://www.mdpi.com/2079-9292/11/13/1926).

## Work sequence

### 1. Freeze the source and measurement contract

- Record the article version, DOI, source URL, download date, SHA-256, figure
  panel, units, axis calibration, and extraction method in a reference manifest.
  Keep downloaded originals in a local cache; automated tests must work offline.
- Inspect the diagrams and captions to establish terminal orientation, initial
  conditions, cell placement, stimulus transitions, and settling windows.
- Confirm how the tabulated R and C apply to the distributed line. Record the
  chosen discretization and its conservation of total R and C. If the original
  SPICE section count is unavailable, document that gap and show mesh convergence.
- Resolve whether 33% and 66% denote rounded thirds or literal percentages.
  Preserve the printed labels and quantify the difference if it cannot be resolved.
- Digitize reference curves with uncertainty from pixel scale and line width.
  Keep publication data separate from simulator outputs and identify obscured
  or overlapping curves. Missing data must remain missing.

### 2. Reproduce the far-end case with the existing solver

Build the 100%-position SLM fixture first. Ground the far end through the
selected cell resistance and apply the piecewise input at the near end.
Represent the first physical line segment with the existing driven boundary;
do not add an arbitrary small driver resistor to approximate an ideal source.
Account explicitly for any capacitance attached directly to the imposed-voltage
node. Its switching impulse must not become a finite settling-current sample.

Run the zero-pre-emphasis baseline and the published pulse-width cases. Split
integration exactly at each voltage transition, retaining the previous phase's
node state. Use observation times on both sides of transitions and document
which one-sided sample is reported.

Extend the probe, or add a dedicated benchmark probe, to return node voltages
and instantaneous terminal currents at requested times. Calculate current from
the circuit equations; integrated charge divided by a sample interval is not an
instantaneous current. Preserve the existing probe invocation and result fields.

### 3. Support interior cell positions

Add optional nonnegative per-node conductance to ground to `NandRcLadder`.
Retain the current API as a zero-shunt wrapper or otherwise preserve existing
callers. Stamp shunts into the diagonal and include their effect in charge
balance, steady-state checks, and the solver's permitted voltage envelope.
Reject nonfinite, negative, or incorrectly sized shunt inputs.

At an interior cell position, retain the open-ended line beyond the cell tap;
truncating the ladder changes the loading. Put the tap at its exact coordinate
using suitable segmentation instead of silently rounding it to a nearby node.
This extension remains a tridiagonal linear network and does not require
mutual capacitances or a nonlinear transistor model.

### 4. Verify the circuit independently

- Extend the independent matrix-exponential reference to stamp grounded shunts
  from circuit connectivity. Do not copy the production time-stepping algorithm.
- Check the far-end DC result against `I = Vfinal / (Rline + Rcell)` and
  the interior result against `I = Vfinal / (x * Rline + Rcell)` under the
  uniform-line interpretation. Beyond the tap, the DC line current is zero.
- Check zero-shunt equivalence, one-node analytical solutions, KCL, nonuniform
  initial conditions, pulse transitions, and charge accounting.
- Refine spatial mesh, integration tolerance, maximum step, observation spacing,
  and settling-event interpolation separately. Report each convergence study.

### 5. Implement the paper's measurements and comparisons

For each observable, settling time is the earliest time after which it stays
inside the declared window, including any later pre-emphasis transition. A
first crossing alone is insufficient. Allow settling before the pulse ends
when the subsequent waveform also stays inside the window.

Extend the observation horizon until the final tail is demonstrably settled.
Return an explicit unresolved result when the configured horizon or solver
budget is exhausted. Refine crossings near pulse transitions and retain the
bracketing interval as timing uncertainty.

Generate voltage and sense-current overlays, delay versus pulse width, and
worst delay across the declared positions and data states. Reproduce individual
curves before comparing aggregate improvements. Keep signed residuals as well
as absolute errors so systematic bias remains visible.

## Deliverables and repository changes

These are proposed new outputs, not files already implemented by this plan:

| Path | Purpose |
| --- | --- |
| `docs/validation/nand-kondo-2022.reference.yaml` | Source manifest, circuit interpretation, case list, and measurement rules |
| `docs/validation/data/kondo-2022/` | Extracted curve points, axis metadata, uncertainty, and provenance |
| `scripts/validate_nand_kondo.py` | Reproducible runner, comparisons, and report generation |
| `tests/test_nand_kondo_validation.py` | Measurement, fixture, and numerical regression tests |
| `docs/validation/nand-kondo-2022.md` | Final findings, error tables, exclusions, and reproduction commands |
| `output/validation/nand-kondo-2022/` | Generated JSON results and standalone SVG/PNG plots |

Extend `tests/NandRcLadderTest.cpp` for every new production solver method.
Keep circuit-only fixtures in the validation/test tree; they are not CAM memory
devices. Any future runnable CAM example must use the canonical split YAML tree
and an ordinary `.memory_device.yaml`, never a `.spec` file.

Add proposed `make validate-nand-kondo` and `make test-nand-kondo-validation`
targets. Register the test target in `test-unit` or its Python test aggregation
and `.github/workflows/cpp-tests.yml`; refresh the unit-test inventory if needed.
Document plotting dependencies. Keep normal CI small and deterministic, and
make full mesh/pulse sweeps an explicit validation command. Handle generated
test artifacts through the Makefile's cleanup conventions.

## Error reporting and completion gates

Report source uncertainty, mesh error, integration error, and observable
extraction error independently. For each curve, include point count, bias/case
identity, voltage RMSE and maximum error, current RMSE and maximum error, and
absolute/relative settling-time error. Define relative-error denominators near
zero explicitly. Include solver settings, source hashes, and model version.

Proposed engineering criteria, to freeze before examining EvaCAM-to-paper
residuals:

- Numerical comparison: at most 10 uV voltage error, 0.01 nA current error,
  and 5 ns settling-time difference against the independent circuit reference,
  excluding undefined instantaneous switching impulses.
- Mesh and observation convergence: delay changes below 0.5%; numerical error
  must also remain small relative to the extracted reference uncertainty.
- Literature comparison: use an uncertainty budget from digitization and the
  numerical study, plus a separately stated 5% timing / full-scale-waveform
  engineering allowance. Report both raw errors and the decision rule; this
  allowance is not a tolerance asserted by the paper.

If a definition or input remains ambiguous, publish the competing interpretations
and mark affected comparisons incomplete. Do not tune R, C, current targets,
or acceptance limits to force a match. A completed investigation may honestly
report disagreement; a reproduction claim requires every declared comparison
to satisfy its frozen rule.

Run focused tests first, then `make test-nand-rc-ladder test-nand3d-numerics`,
the NAND3D model/integration regressions, and `make test` after runtime changes.
Finish with a full offline reproduction and inspection of every plot.

## Relationship to the other plans

This work can proceed independently of
[Park reference extraction](nand-park-2025-extraction-plan.md). It supplies
additional solver evidence for the
[nonlinear NAND3D extension](../nand-3d-nonlinear-model-plan.md), but does not
provide its transistor calibration.
