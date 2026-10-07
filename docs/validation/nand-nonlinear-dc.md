# Experimental nonlinear NAND DC evaluator

Status (2026-09-28): the first numerical prototype is implemented. The reduced
cell law and standalone DC string solver have synthetic circuit tests and an
independent numerical reference. **There is no Park fit, held-out physical
validation, nonlinear transient, or nonlinear CAM configuration yet.** The DC
release gate in the [implementation plan](../nand-3d-nonlinear-model-plan.md)
remains open.

The reduced law is an experimental departure from NVSim-derived modeling.
Its numerical checks do not establish better device predictions. The
[NVSim justification record](nand-nvsim-justification.md) requires a controlled
comparison with the inherited model, including equal calibration opportunity
and held-out errors, before accepting it as a physical-model improvement.

## Model contract

`NandCellCurrentModel` takes an immutable copy of
`NandCellCurrentParameters`. Geometry and temperature are explicit per model;
gate, source, drain, and threshold voltages are supplied per evaluation. Each
string device may use its own model, gate voltage, and threshold. Select and
dummy devices must be included explicitly when constructing a circuit. No CAM
validity pairs or encoding are inserted by this electrical solver.

All electrical values use SI units. Positive current flows from drain to source;
source terminal current is its negative and gate current is zero. The returned
Jacobian differentiates drain current with respect to the three **absolute**
terminal voltages while holding the other terminals and threshold fixed.

For smoothing voltage `q = 2*n*(kB/e)*T`, define:

```text
P(x) = log(1 + exp(x))
xs = (Vg - Vs - Vth)/q
xd = (Vg - Vd - Vth)/q
Id = beta*q²/2 * (P(xs)² - P(xd)²) + Gleak*(Vd - Vs)

A(x) = beta*q*P(x)*sigmoid(x)
dId/dVg = A(xs) - A(xd)
dId/dVs = -A(xs) - Gleak
dId/dVd = A(xd) + Gleak
```

This is a chosen reduced smooth potential-difference approximation, **not an
implementation of BSIM-CMG**. It has a smooth subthreshold region, the square-law
triode/saturation limit in strong inversion, source/drain antisymmetry, zero
current at zero Vds, and nonnegative dissipated power. Stable softplus and
`log1p`/`expm1` differences avoid overflow and small-Vds cancellation. Very deep
off-state currents can still underflow in double precision.

`beta` is supplied for the stated geometry; diameter and gate length are
provenance, not inputs to an inferred geometry law. Temperature changes the
smoothing voltage only. There is no temperature-dependent mobility, body effect,
DIBL, calibrated channel-length modulation, taper model, or layer-count multiplier.
`Gleak` is an explicitly supplied parallel conductance, may be zero, and is part
of the physical approximation. The solver adds no numerical conductance floor.
Threshold/charge state is explicit; tests' thresholds do not establish physical
CAM state separation. Terminal charge and capacitance are outside this DC model.

Every model declares admissible absolute terminal-voltage and threshold
intervals. Extrapolation is rejected. Constructor guardrails are beta in `(0,1]`
A/V², slope factor in `[1,10]`, leakage conductance in `[0,1]` S, temperature in
`[100,1000]` K, positive finite dimensions, and declared voltage/threshold limits
inside `[-100,100]` V. Terminal-voltage intervals must have positive width;
a single allowed threshold is permitted. These are experimental numerical
bounds, not characterized physical operating ranges.

The [Park reference audit](nand-park-2025.md) records missing source voltage,
temperature, threshold state, and other conditions. The
[publisher article](https://www.jsts.org/jsts/XmlViewer/f438850) describes a
BSIM-CMG fit to TCAD; the [official BSIM-CMG description](https://bsim.berkeley.edu/models/bsimcmg/)
describes a substantially richer surface-potential model. Those sources were
reviewed before choosing this explicitly reduced prototype. No complete Park
model card has been obtained, and none of its digitized points was used to choose
the synthetic test parameters or tolerances. The frozen reference is unchanged.

## DC algorithm and diagnostics

`NandNonlinearString::Solve` accepts 1–512 devices in source-to-drain order and
fixed endpoint voltages. It initializes nodes by linear voltage interpolation,
assembles internal-node KCL and its analytical Jacobian, and uses Newton steps
with partial-pivoted dense elimination and a residual-decreasing line search.
The Jacobian is generally nonsymmetric. The existing linear RC solver is not
used or changed by this addition.

Trial nodes must stay inside the passive interval between endpoint voltages;
all cell domains must include that full interval and their gate bias. Steps are
damped, not clipped. Success requires both the maximum KCL residual and the
**full undamped Newton correction** to meet their tolerances. The current test is
`max|KCL| <= absoluteCurrentTolerance + relativeCurrentTolerance*max|cell current|`.
Absolute current tolerance is per internal node, not a guaranteed relative error
bound on a very small terminal current. Reported current is the drain-end current.

Results include every node voltage, terminal current, maximum KCL residual,
maximum Newton correction, iteration count, and backtrack count. Iterations count
Jacobian/residual evaluations, including the final convergence check. Invalid
inputs, singular Jacobians, exhausted iterations, and failed line searches throw
explicit errors. A fully underflowed network without leakage can have a singular
Jacobian and is rejected. There is no resistance fallback or hidden regularizer.

The dense implementation favors an easily audited prototype over sweep speed.
Bias continuation and a sparse/banded implementation remain later optimizations;
convergence for every valid parameter combination is not promised. Work budgets
bound failure. Models are immutable and all solve workspace is local to a call.

## Verification and reproduction

```sh
make -j2 test-nand-cell-current test-nand-nonlinear-string test-nand-nonlinear-numerics
make -j2 test-unit test-regression
make unit-test-inventory check-unit-test-inventory
make test
valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all test-bin/NandNonlinearStringTest
```

The focused suites cover:

- Cell current and derivatives: strong-inversion and subthreshold analytic
  limits, resistor limit, zero/tiny/reverse Vds, common-mode invariance,
  finite-difference Jacobians, invalid domains, and large finite biases.
- DC circuits: one-device and zero-bias cases, heterogeneous resistor limits,
  identical-device potential telescoping through 512 devices, KCL,
  selected-device positions, mirrored reverse-bias strings, near-off current,
  tolerance refinement, concurrent calls, actual backtracking, and explicit
  line-search/iteration/singular-Jacobian failures.
- Independent Python reference: conductance quadrature rather than the C++
  potential-difference formula, and SciPy's finite-difference Powell hybrid
  root solver rather than the C++ analytical-Jacobian Newton implementation.
  It compares 18 single-cell bias cases and 24 heterogeneous circuits with
  2, 3, and 8 devices, plus their mirrored reverse-bias circuits.

The heterogeneous comparisons use 1e-17 A absolute KCL tolerance, 1e-10 relative
KCL tolerance, and 1e-12 V correction tolerance. Acceptance is 3 nV for nodes and
`2e-15 A + 2e-7*|Ireference|` for current. These are synthetic numerical criteria,
not paper-correlation allowances. NumPy, SciPy, and PyYAML are required.
All three test targets are registered in CI and the unit-test aggregation.

Verification on 2026-09-28: the focused suites, full `test-unit` and
`test-regression` aggregations, and inventory check pass. The existing linear
RC, Kondo, Park-reference, and NAND CLI/Python tests are included in those
aggregations. Both the default `make test` Valgrind run and a direct Valgrind
run of `NandNonlinearStringTest` report zero errors, zero bytes in use at exit,
and all allocated blocks freed. All three new production callables are mapped
to focused tests in the inventory.

## Electrical probe

Build `make nand-nonlinear-string-probe`. The test-only executable accepts a
YAML/JSON circuit file, or `-` for standard input, and emits full-precision YAML
with `model: experimental_smooth_potential_dc_v1` and
`calibration_status: uncalibrated`. Errors produce nonzero exit and no success
record. For example:

```yaml
source_v: 0
drain_v: 0.7
devices:
  - gate_v: 2
    threshold_v: 1
    parameters: &cell
      beta_a_per_v2: 2.0e-5
      slope_factor: 1.5
      leakage_conductance_s: 1.0e-12
      temperature_k: 300
      channel_diameter_m: 4.7e-8
      gate_length_m: 5.0e-8
      minimum_voltage_v: -2
      maximum_voltage_v: 8
      minimum_threshold_v: -1
      maximum_threshold_v: 5
  - gate_v: 6
    threshold_v: 1
    parameters: *cell
```

This protocol is for electrical experiments, not an EvaCAM input schema.
Optional `solver` fields must all be supplied when that block is present:
`absolute_current_tolerance_a`, `relative_current_tolerance`,
`voltage_tolerance_v`, `max_iterations`, and `max_backtracks`. If absent, the C++
defaults are 1e-14 A, 1e-8, 1e-10 V, 100 iterations, and 40 backtracks per step.
The shipped `transient_rc` CAM examples retain their existing meaning.

## Next gates

Resolve Park's missing conditions from independent sources or declare bounded
assumption scenarios before fitting. Fit only the calibration partition, assess
parameter identifiability/sensitivity, freeze parameters, then report eligible
held-out errors. A failed or assumption-dependent fit must remain visible.
Only after that DC investigation should a nonlinear transient and CAM
configuration/results/API integration proceed. Static current correlation will
still supply no evidence for capacitances or CAM search energy.
