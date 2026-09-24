# Plan: nonlinear electrical support for NAND3D

Status: proposed implementation following the current linear NAND3D backend.
This document does not claim nonlinear support or paper correlation is complete.

## Objective and staged scope

Extend `type: NAND3D` so that string current follows local transistor biases
instead of fixed read/pass/off resistances. First establish DC current
correlation using the cases in the
[Park reference extraction plan](validation/nand-park-2025-extraction-plan.md),
then integrate a numerically verified nonlinear transient into CAM evaluation.

The physical device remains an ordinary `.memory_device.yaml`. Keep
`nand3d.model: transient_rc` and the synthetic shipped example compatible.
Introduce a separate, explicitly selected nonlinear electrical mode within
NAND3D; do not overload planar `FLASH` or introduce a `.spec` input.

Use three distinct release gates:

1. A device/string DC evaluator with frozen calibration and held-out results.
2. A nonlinear transient solver verified against independent circuit solutions.
3. Integration with NAND3D encoding, phase scheduling, sensing, and results.

Static I-V agreement alone does not validate capacitances, transient delay,
program/erase operation, threshold-state separation, peripheral energy, or CAM
search energy. Those require separate evidence and metadata.

## Current code and required separation

`Nand3dCamModel::Encode` currently reduces stored/query states to resistances;
`Simulate` passes those values into `NandRcLadder`. This discards the local gate
and channel voltages that a transistor law needs. The linear solver's positive
definite tridiagonal assumptions also cannot simply be reused for an arbitrary
nonlinear Jacobian.

Separate the following responsibilities while preserving the existing backend:

| Component | Responsibility |
| --- | --- |
| String description and encoder | Device ordering, threshold state, physical geometry, and per-phase gate bias |
| Cell current law | Current and derivatives from local terminal voltages and device parameters |
| String DC solver | Internal-node equilibrium and terminal current |
| Nonlinear transient solver | Stateful voltage evolution and integrated terminal charge |
| Existing CAM model / bank adapter | Groups, mux rounds, sensing, routing, footprint, and result aggregation |

A conventional read reference string must be constructible without CAM validity
pairs or complementary encoding. Use a circuit validation probe for that case;
do not force the paper's physical string through CAM capacity rules. Reuse the
same cell law and electrical solver later in the CAM backend.

## 1. Freeze the characterization contract

Complete the Park source audit and partition before calibration. The paper uses
BSIM-CMG and TCAD reference curves; consult the
[publisher article](https://www.jsts.org/jsts/XmlViewer/f438850) and
[official BSIM-CMG documentation](https://bsim.berkeley.edu/models/bsimcmg/)
when specifying terminal conventions and model-version compatibility.

Inventory the bias domain, selected-device positions, material/geometry
assumptions, select-device behavior, and unavailable inputs. Determine whether
the accessible data identify a local cell law or only whole-string behavior.
Several string transfer curves at one operating point do not uniquely determine
a general transistor I-V surface.

Use an explicit decision gate:

- If a complete usable compact-model card/deck is available, run it in a
  compatible simulator to generate independent cell/string reference cases.
  Record model version, simulator, license, and extraction settings.
- Otherwise, develop a reduced smooth cell law with documented assumptions,
  fitted only to the calibration partition. Call it an approximation and show
  parameter nonuniqueness and sensitivity. Do not claim BSIM equivalence.
- If held-out predictions fail because essential inputs are missing, retain the
  DC tool as experimental and publish the gap. Do not promote fitted string-level
  lookup curves into a general local transistor model.

## 2. Define a bounded, testable cell-current interface

Add proposed `NandCellCurrentModel` and parameter types under `include/model/`
and `include/technology/`, with implementations under the matching `src/` paths.
The evaluator accepts local gate/source/drain voltages, threshold state,
temperature and geometry within its supported domain. It returns terminal
current and a consistent Jacobian with documented units and orientation.

For the reduced law, choose a continuous transition from subthreshold to strong
inversion, finite output conductance where justified, and explicit source/drain
orientation. Avoid discontinuous `on/off` switches and arbitrary layer-count
scale factors. The law must conserve terminal current and behave sensibly as
drain-source voltage approaches zero. Audit reverse-bias behavior needed during
precharge/recovery; reject unsupported bias excursions instead of clipping them.

Make threshold/charge state explicit. A threshold shift between the two CAM
states is an additional assumption unless independently characterized. Model
select and dummy devices explicitly or label their retained linear resistances
as approximations. Record the admissible geometry/bias range and warn or reject
extrapolation through a deliberate policy.

Use immutable parameters and per-call working storage. Evaluation must remain
safe for concurrent pattern and design runs.

## 3. Solve and validate nonlinear DC strings

Assemble KCL at internal nodes using local terminal biases, including the
source-voltage shift of each selected/pass device. Implement damped Newton
iteration with a line search, scaled residual tests, a bounded iteration budget,
and a robust pivoted tridiagonal or sparse solve as appropriate. The Jacobian
need not satisfy the linear RC solver's symmetric positive-definite assumptions.

Use a reproducible equilibrium initialization and continuation in applied bias
where needed. Temporary numerical conductance aids must be recorded, reduced,
and shown not to alter the result. Require both current residual and voltage
update convergence; report failure rather than falling back to fixed resistance.

Fit the declared parameters on the frozen calibration set, with meaningful
bounds, multiple initializations, and separate linear-current and log-current
losses. Respect extraction uncertainty and censoring. Record parameter sensitivity
and alternative fits with comparable loss. Freeze one parameter set before
evaluating all eligible held-out cases.

Report current RMSE, maximum error, on-current relative error, log-current error
in decades, and selected-WL/bias/layer-count trends. Use predeclared current
floors for relative/log metrics and show residuals by case. Set numerical and
paper-correlation criteria from the extraction uncertainty before inspecting
held-out residuals; the authors' fit accuracy is not an automatic EvaCAM tolerance.

## 4. Implement the nonlinear transient

Start with the explicitly supplied grounded capacitances already supported by
NAND3D and a nonlinear conductive network. Solve

```text
C * (v_next - v_previous) / dt + f(v_next, gate_biases) = source_injection
```

using an implicit method with damped nonlinear iterations. Adapt the time step
using an independent local-error estimate, with voltage and current scaling.
Separate time-step rejection, Newton convergence, and global work limits in
diagnostics. Do not treat Newton convergence as proof of time accuracy.

Land exactly on gate/driver transitions and retain actual internal voltages
across precharge, evaluation, recovery, and mux rounds. Compute source charge
and energy from the same converged states and quadrature as the time evolution.
Define the handling of reverse source current and energy returned to a supply.
Verify KCL, dissipated energy, and stored capacitor-energy changes separately.

Keep mutual and bias-dependent capacitance as a later explicit extension.
Static current fitting cannot identify terminal charge or capacitance. Until
transient characterization exists, results must identify their capacitance
parameters as supplied/synthetic, even if DC current correlation passes.

## 5. Integrate the memory-device schema and CAM path

Proposed configuration additions, subject to the characterization decision:

```yaml
schema: memory_device
type: NAND3D
nand3d:
  model: nonlinear_transient
  # Existing stack, layout, phase, sensing, and peripheral sections remain.
  current_model:
    kind: reduced_gatetransistor
    # Typed parameters, validity domain, state assumptions, and provenance.
  # Dedicated nonlinear solver controls supplement time-integration controls.
```

This is a schema sketch, not a runnable configuration or a promise that those
names are implemented. Resolve exact names and required fields before coding.
Embed fitted scalar parameters in the memory-device file. If a sampled model
requires companion data, use a declared relative data-file reference with strict
validation and package-data support; the device definition still lives in the
memory-device YAML.

Update `Nand3dMemoryDevice`, `MemoryDeviceYamlLoader`, and physical validators
with units, finite bounds, supported model selection, and mutually exclusive
parameter sets. The existing shared `NandDeviceSpec` parsing assumes resistances;
refactor only the shared fields needed to allow model-specific current inputs.
Reject misspelled fields and incomplete profiles without silent defaults.

Change encoding to preserve low/high state and gate bias through simulation.
Keep complementary pairs, validity semantics, physical storage accounting,
groups, and sense-mux scheduling unchanged. Select electrical behavior explicitly
through the NAND factory/model path. Test both old and new backend selections.

## 6. Preserve result and energy meaning

Add a distinct model identifier and report current-law kind, calibration dataset
hash, parameter-set version, supported operating range, and DC/transient
validation status separately. Record Newton residuals, iterations, accepted and
rejected steps, and any extrapolation. Keep console, YAML, and Python consistent.

Retain the existing linear conductance field's meaning for `transient_rc`.
For nonlinear results, report an operating-point current and, when useful,
explicitly labeled secant `I/V` or differential `dI/dV` values with their biases.
Do not present one of these as a bias-independent resistor conductance.

Keep sense margins and sampled-pattern coverage explicit. DC calibration of one
threshold state does not establish match/mismatch margins. Account for modeled
terminal charging and supplied peripheral overheads without double counting.
Leave area and page-program/block-erase estimates separately sourced; a nonlinear
read-current law supplies no new evidence for those quantities.

## 7. Verification and deliverables

Add focused C++ tests for every new production method, using proposed
`NandCellCurrentModelTest.cpp` and `NandNonlinearStringTest.cpp` plus existing
config/model/results/integration suites. Cover analytical resistor limits,
single-device cases, Jacobian finite-difference checks, DC KCL, source/drain
orientation, near-off leakage, invalid inputs, and explicit convergence failures.

Use an independent SPICE/reference implementation for small nonlinear circuits.
Check transient step/tolerance refinement, DC limits, charge balance, and
energy balance. Re-run the existing linear matrix-exponential comparisons and
[Kondo benchmark](validation/nand-kondo-2022-plan.md) after shared solver changes.
Compare full CAM phase schedules, encoding, match/mismatch margins, and repeated
mux rounds, not only isolated cells. Exercise concurrent evaluations.

Proposed additional deliverables:

- `scripts/validate_nand_park.py` and a compiled electrical probe for DC sweeps.
- `docs/validation/nand-park-2025-model.md` with calibration and held-out results.
- `config/NAND_3D_TCAM_NONLINEAR/` with the ordinary top-level, architecture,
  cell, and `.memory_device.yaml` files, clearly labeling unsupported or
  synthetic parameters. Ship a paper-correlated label only after the DC gate.
- Standalone curve/residual plots and machine-readable reports under
  `output/validation/nand-park-2025-model/`.
- Updates to `docs/nand-3d-tcam.md`, input/schema documentation, limitations,
  results reference, and Python API documentation.

Register new tests in the Makefile aggregations and
`.github/workflows/cpp-tests.yml`, and refresh the unit-test inventory. Keep
full fitting and large sweeps outside routine CI; CI uses frozen parameters
and a bounded representative reference set. Follow repository cleanup rules.

Run narrow tests first, then NAND3D/planar NAND regressions, CLI/YAML/Python
consistency tests, `make test-unit test-regression check-unit-test-inventory`,
and `make test` for runtime/memory changes. Include a full nonlinear example
under Valgrind. Record commands and reproducible result paths in the report.

## Completion criteria and dependencies

The DC milestone requires traceable inputs, an immutable calibration split,
one frozen parameter set, and reported held-out errors. Passing is assessed
per declared observable/domain; unresolved cases remain visible.

The transient milestone additionally requires independent numerical agreement,
conservation checks, and explicit convergence diagnostics. It remains physically
uncalibrated in time if only static references exist.

The CAM milestone requires compatible linear behavior, complete new-mode
configuration/results/API support, stateful phase and sensing checks, and no
unlabeled expansion of the calibration claim. MLC/TLC/QLC, program/erase physics,
retention, disturb, and statistical process variation remain separate work.

Start schema/interface and numerical prototypes after reviewing the extraction
contract; defer paper calibration until the Park dataset is frozen. Kondo
validation can proceed independently and does not substitute for cell data.
