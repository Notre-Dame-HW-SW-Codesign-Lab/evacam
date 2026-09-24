# NAND3D transient RC verification

The `NAND3D` backend is numerically verified for its stated **linear RC
circuit**. Its example memory-device parameters are synthetic. This is not
a reproduction of measured 3D NAND, a calibrated transistor model, or a
successful numerical reproduction of a published CAM macro.

## Reproduce the checks

```bash
make test-nand-rc-ladder test-nand3d-numerics
make test-nand3d-config test-nand3d-model test-nand3d-integration test-nand3d-results
make test-pybind-nand3d
./EvaCAM config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml
```

The independent Python checks require NumPy, SciPy, and PyYAML. They call
the compiled production solver through `test-bin/NandRcLadderProbe`.
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

The same target also exercises the actual `NAND3D` backend through
`test-bin/NandValidationProbe`. The reference independently constructs
threshold-state pairs and read/pass biases, includes the validity and dummy
devices, and propagates precharge, evaluation, and recovery with matrix
exponentials. Six patterns are compared with sense-mux factors one and two.
Decision voltages and signed margins agree within 3 uV; DC conductance and
the precharge supply-energy term are checked separately. This catches
encoding, circuit assembly, and phase-scheduling mistakes beyond isolated
solver verification.

## Long-string numerical evidence

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

The separate 3D geometry and nodal solver make comparisons structurally
more appropriate, but they do not supply missing characterization. The
memory-device file must be populated with a paper's actual string layout,
electrical parameters, biases, timing, sensing, and peripheral costs before
claiming numerical agreement. Compare the same number of searched entries,
key encoding, active groups, and energy boundary.

This first backend supports SLC complementary-pair exact/ternary search.
It treats cell conduction as piecewise constant resistance and node
capacitances as grounded lumped values. Nonlinear I-V behavior, explicit
mutual capacitance, threshold distributions, retention, read disturb,
MLC/TLC/QLC sensing, and physical write/erase waveforms are not modeled.
Search feasibility uses a documented sample of match/mismatch patterns;
it is not a proof over every possible stored key and query. Each public
pattern evaluation reports its own voltage and margin.

The published comparisons and remaining characterization work are tracked
in the [3D model plan](../nand-3d-model-plan.md). Solver agreement must stay
separate from device calibration in result metadata and research claims.
