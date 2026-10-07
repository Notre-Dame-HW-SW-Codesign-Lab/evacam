# NAND RC reference and analytical-model verification

As of 2026-10-02, production NAND3D uses analytical first-moment RC estimates
while preserving 3D geometry. The nodal solver remains a separate numerically
verified **linear RC reference**, outside normal CAM evaluation. Its agreement
with independent numerical solutions is not device calibration. The shipped
electrical values remain synthetic.

## Rerun after technology-library fallbacks

Re-executed 2026-10-02 against the analytical production model and the new
missing-parameter fallbacks. Model/configuration/integration/results tests,
Python/CLI tests, the independent analytical matrix check, the nodal reference,
and the NVSim audit tests passed. The pinned native NVSim source was rebuilt
and all 16 matched-circuit checks passed again. Those native comparisons verify
the inherited reference equations; they do not establish complete NVSim/NAND3D
operation equivalence or measured flash accuracy.

An additional run exercised both planar and 3D NAND with supplied values and
with all 17 supported RC/driver/sense fields omitted, across legacy 45 nm HP at
300 K, legacy 65 nm LSTP at 350 K, and updated 45 nm HP at 350 K:

- All 12 configurations and 72 encoded patterns agreed with an independently
  assembled conductance-matrix first moment. Maximum analytical voltage
  difference was `5.63e-13 V`, against a `1e-9 V` acceptance limit. The 3D
  precharge energy also matched the full-recharge CV² ledger.
- Re-entering the resolved library values explicitly produced identical model
  predictions, design feasibility, and numeric result metrics in all six
  fallback cases. Only the defaulted inputs emitted fallback warnings.
- The 3D legacy 45 nm HP fallback fixture returned `no_valid_solutions`;
  defaults are estimates and do not guarantee a feasible design. The supplied
  fixture and the other five fallback fixtures returned valid designs.
- The 42-case distributed-RC audit retained the known approximation limitation:
  the 512-wordline planar `match0` case reports 119.903 mV analytical margin
  versus 75.245 mV in the full RC reference, below its 100 mV requirement.
  Both all-zero and all-one matching cases disagree on pass/fail. Sixteen
  sampled waveforms exceed the audit's 10 mV diagnostic tolerance; this is
  not a claim of agreement with the complete transient waveform.

Rerun artifacts live in `output/validation/nand-analytical-rerun/`:
`fallback-verification.json`, its reproducible `check_fallbacks.py` and input/
output snapshots under `fallbacks/`, `rc-audit/audit.json`, and
`nvsim/report.json` with the fresh native source/build provenance. Software and
equation verification passes; physical device calibration remains unestablished.

The [NVSim justification record](nand-nvsim-justification.md) separates proven
circuit-level benefits from unvalidated physical-model departures. In particular,
the earlier one-pole counterexample compares two EvaCAM circuit treatments;
it is not a direct accuracy comparison with the original NVSim flash model.

## Reproduce the checks

```bash
make test-nand-rc-ladder test-nand3d-numerics
make test-nand3d-config test-nand3d-model test-nand3d-integration test-nand3d-results
make test-pybind-nand3d
./EvaCAM config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml
```

The independent Python checks require NumPy, SciPy, and PyYAML. They call
the compiled reference solver through `test-bin/NandRcLadderProbe`.
The C++ solver uses adaptive implicit integration of a tridiagonal nodal
system. The Python reference assembles conductance with an edge-incidence
matrix and applies a dense matrix exponential to the voltage and integrated
source-charge states. Longer homogeneous ladders are also checked against
the independent symmetric modal solution in `scripts/nand_rc_reference.py`.

The tests cover analytic one- and two-pole circuits, nonuniform initial
voltages, two finite driven boundaries, floating-network charge conservation,
finite-driver precharge, carryover of the actual precharged node voltages
into evaluation, source charging energy, a 1 GOhm blocking device at both
ends of a string, tolerance convergence, and rejected solver inputs/step limits.
These checks compare the equations with a different numerical method; they
do not adjust device inputs to fit the expected outputs.

The same target exercises analytical NAND3D through NandValidationProbe.
It independently reconstructs threshold-state pairs, read/pass biases, validity
and dummy devices. A dense conductance solve computes the far-node response
integral (the first moment), independently of the C++ cumulative-resistance
expression. Six patterns and mux factors one and two check exponential voltage,
inferred time constant, signed margin, DC conductance and full-charge CV² energy.

C++ regressions also exhaust a short ternary truth table against the analytical
bounds, check equality with the planar backend on identical zero-wire circuits,
exercise the lumped analytical limit and confirm that short supplied phase times
do not claim simulated state. Results expose time constants and full-precharge
assumptions without transient diagnostics. Parser, geometry, bank scheduling,
and CLI/Python/YAML parity tests cover the restored application path.

The restored analytical example reports 655 ns query latency, 44.3451 pJ
dynamic energy and 384.167 mV analytical sense margin. Its layout remains
196.280 µm². The NAND3D and planar NAND model/configuration/result/integration
tests, CLI/Python parity, YAML/input validation, NVSim reference comparison and
test-inventory checks pass. Both the default `make test` and a full-leak-check
run of the analytical 3D example report zero Valgrind errors and no leaks.

## Historical transient results and retained reference evidence

The following full-example outputs and full-suite run record were captured
before the analytical restoration. They document the previous transient backend;
current application behavior is specified in [3D NAND TCAM](../nand-3d-tcam.md).
The independent ladder tests and long-string counterexample remain active.

The complete synthetic example also runs through the CLI and Python APIs.
Its one-block, 64-entry, 32-bit-key configuration reports 655 ns full-query
latency, approximately 44.345 pJ dynamic energy, 196.280 um² occupied area,
and 385 mV sampled sense margin. It uses four sequential select groups,
68 storage layers, and two dummy layers. These values are reproducible
regression outputs, not literature targets.

The complete `test-unit`, `test-regression`, and `check-unit-test-inventory`
targets pass. This includes existing planar NAND and other CAM regressions,
both bank-routing modes, shared-resource scheduling, concurrent pattern
evaluation, and CLI/Python/YAML result consistency.
The default `make test` Valgrind check and a separate full-leak-check run of
the canonical NAND3D example both report zero errors. The NAND3D run exits
with zero bytes in use and all allocated blocks freed.

The following controlled circuit starts uniformly at 0.8 V and is evaluated
for 50 ns. Each storage device alternates between 10 kOhm and 5 kOhm;
the source and drain select resistances are 1 kOhm. Source, internal-node,
and bitline capacitances are 1 fF, 0.05 fF, and 20 fF respectively. No wire
parasitics are added. The maximum step is 5 ns. These are solver fixtures,
not the complete example's finite-precharge waveform.

| Storage devices | Independent bitline voltage (V) | C++ voltage at 1 nV local tolerance (V) | Absolute endpoint error (V) |
| ---: | ---: | ---: | ---: |
| 68 | 0.00816339965462 | 0.00816340036919 | 7.15e-10 |
| 128 | 0.0807049311675 | 0.0807049318000 | 6.32e-10 |
| 256 | 0.299770558455 | 0.299770558815 | 3.60e-10 |
| 512 | 0.582867925883 | 0.582867925665 | 2.18e-10 |

For the 512-device case, reducing local tolerance from 10 uV to 1 uV to
1 nV reduces endpoint error from 1.26 uV to 0.176 uV to 0.000218 uV.
Accepted steps increase from 164 to 365 to 3,764. Local tolerance is an
integration control, not a guaranteed bound on accumulated global error.
The regression therefore checks the endpoint independently.

The earlier planar-model counterexample is preserved without changing its
initial condition, decision time, reference, or capacitance. At reference
0.6681125434317466 V and offset 0.01 V, the 512-device match margin is
approximately **75.245 mV**, below the required 100 mV. The transient solver
correctly retains that failure. The one-pole model predicted approximately
119.903 mV and incorrectly passed this circuit. See the
[earlier validation audit](nand-yang-2023.md) for the original evidence.

## What remains for paper validation

The separate 3D geometry and reference nodal solver help isolate assumptions,
but do not supply missing characterization. The
memory-device file must be populated with a paper's actual string layout,
electrical parameters, biases, timing, sensing, and peripheral costs before
claiming numerical agreement. Compare the same number of searched entries,
key encoding, active groups, and energy boundary.

This first backend supports SLC complementary-pair exact/ternary search.
It treats cell conduction as piecewise constant resistance and node
capacitances as grounded lumped values. Nonlinear I-V behavior, explicit
mutual capacitance, threshold distributions, retention, read disturb,
MLC/TLC/QLC sensing, and physical write/erase waveforms are not modeled.
Current search feasibility uses analytical extrema over supported patterns;
the bounds apply only within the single-exponential approximation, not to the
full distributed circuit or hardware. Each public
pattern evaluation reports its own voltage and margin.

The published comparisons and remaining characterization work are tracked
in the [3D model plan](../nand-3d-model-plan.md). Solver agreement must stay
separate from device calibration in result metadata and research claims.

The [Kondo/Tanzawa SLM benchmark](nand-kondo-2022.md) extends this solver
verification to interior grounded cell loads and piecewise driven bitline
waveforms. It records publication residuals and source ambiguities separately
from the numerical checks; it does not calibrate the NAND3D device.
An eight-section circuit hypothesis subsequently brings the extracted
voltage-delay curves within 3.25% without parameter fitting. This identifies
circuit construction as material to the comparison; the source netlist and
data-0 current measurement rule still need independent confirmation.
