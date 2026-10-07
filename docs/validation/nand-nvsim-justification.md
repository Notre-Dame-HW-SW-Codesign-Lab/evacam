# Justification of NAND modeling changes from NVSim

Audit date: 2026-10-02. EvaCAM is derived from NVSim. NVSim's applicable
equations, assumptions, and validation are the baseline for assessing changes.
Additional detail, a different implementation, or passing software tests does
not by itself justify a replacement as a modeling improvement.

The current work demonstrates numerical benefits for specified synthetic
circuits and implements additional CAM operations. It has **not demonstrated
superior physical prediction accuracy over NVSim**. Several departures remain
experimental or replace calculated quantities with supplied inputs. The
[executed matched validation](nand-nvsim.md) now supplies a native baseline,
16 shared-circuit comparisons, and an exact subarray energy reconciliation.
Physical prediction accuracy and complete peripheral equivalence remain open.

Production disposition, 2026-10-02: retain 3D geometry and use analytical RC for
both planar and vertical NAND. Nodal and nonlinear experiments remain verification
tools. The departure table records the earlier transient design, not the current
application default. Full precharge/reset assumptions and the known one-pole
limitations now apply to both analytical paths. See
[the current 3D contract](../nand-3d-tcam.md).

## Baseline and scope

The source reference is NVSim commit
`6334d00ca2e0320a9c930eae45de145ca65c210c`, inspected in the local NVSim
checkout and available as a
[pinned upstream source](https://github.com/SEAL-UCSB/NVSim/blob/6334d00ca2e0320a9c930eae45de145ca65c210c/SubArray.cpp).
`SubArray.cpp` SHA-256 is
`3775e14849cd93dbf7bfb9dc1a9e71e6824dd1a8c3d49111a3870f6dc531ceb1`.
The local Makefile has modifications. The subsequent validation exported the
pinned tracked sources into an isolated directory, compiled them, and executed
the native CLI and component adapter without modifying the original checkout.
The [validation record](nand-nvsim.md) retains build options, hashes, and inputs.

NVSim's published NAND validation concerns conventional planar SLC operations.
Its read and energy comparisons are evidence in that domain. They cannot be
discarded because an analytical formula is simple or includes an empirical
correction. Equally, the publication's area reference was inferred using 90%
array efficiency, and supplied program/erase durations limit what timing
agreement establishes. See Dong et al., *NVSim: A Circuit-Level Performance,
Energy, and Area Model for Emerging Nonvolatile Memory*, Table III and
Section VII-A, [DOI 10.1109/TCAD.2012.2185930](https://doi.org/10.1109/TCAD.2012.2185930).

EvaCAM's `SLCNAND` analytical TCAM backend is already an adaptation. It is not
an unchanged NVSim flash baseline. A comparison between that backend and
`NAND3D` does not establish a comparison with NVSim. Conventional page reads
and whole CAM queries also have different operations and accounting boundaries.

In particular, the `20*tau` delay floor in NVSim must be assessed as part of
the inherited model, including its empirical validation. A source comment
requesting review does not prove the correction is wrong. Solving a different
ideal circuit more precisely does not justify removing a correction from a
chip-level estimate. Its replacement needs a comparison of the same observable
against the same independent evidence.

## Change audit

| Departure | Reason to investigate or extend | Evidence and current disposition |
| --- | --- | --- |
| Page-read operation to complementary-pair TCAM search | A query biases several wordlines according to the key and must distinguish match, mismatch, and wildcard patterns. | Encoding and integration tests establish the implemented semantics. This is an added capability. Physical search accuracy still requires a characterized CAM circuit. The reserved validity pair is our architectural assumption. |
| Aggregate string delay to a nodal RC transient | Internal-node voltages and sampled sense margins may require more than an aggregate delay. | On 16 matched extracted circuits, the compiled nodal solution passes the independent reference checks; NVSim's uncorrected estimate is already within 0.48%. The approximately 20× corrected delay reflects its retained empirical floor, not demonstrated hardware error. No physical superiority is established. |
| Assumed precharge state to finite-driver precharge and carried state | A short precharge interval can leave internal nodes below the target voltage. | Analytic and matrix-exponential tests verify charging and state propagation. The benefit is demonstrated for the supplied RC network. Actual driver impedance, capacitances, and hardware reset protocol still need characterization. |
| Generic CMOS resistance and derived cell loading to supplied state-dependent resistances and capacitances | CAM needs distinct read, pass, and blocking behavior; a vertical string may need different device characterization. | The new inputs allow characterization but do not supply it. Synthetic values have no demonstrated accuracy advantage over NVSim's estimates. This departure remains unvalidated. |
| Fixed sensing assumptions to configured bias, decision time, reference, and margin checks | CAM classification needs an explicit decision rule across stored/query patterns. | Sampled-pattern and truth-table checks verify the rule. Comparator assumptions, bias values, and pattern coverage require circuit evidence. Configurability alone is not increased accuracy. |
| Planar cell-area accounting to vertical grid, stack, staircase, and placement geometry | A vertical array needs separate footprint and layer accounting. | Geometry tests check units and accounting identities. The rectangular footprint and supplied pitches have no layout-derived accuracy evidence yet. Treat this as an experimental extension, not a validated area improvement. |
| Aggregate switching energy to integrated precharge charge plus explicit switching and overhead terms | Incomplete charging can change the supply energy, and operation boundaries must remain explicit. | Matched precharge experiments verify source charge and recover native CV² at full charge. The native subarray ledger reconciles to floating-point precision, including inherited erase accumulation. Matched whole-operation physical energy validation remains missing. |
| NVSim peripheral calculations to supplied NAND driver, sensing, and buffer costs | Special voltage and placement requirements may need characterized peripherals. | Supplied costs can represent measurements, but synthetic costs reduce predictive coverage. Retain applicable NVSim calculations as a comparison baseline; establish a discrepancy before replacing a term. No accuracy improvement is currently demonstrated. |
| Calculated program/erase energy to supplied complete operation costs | Separating page and block operations permits consistent measurement boundaries. | Operation units are explicit, but the underlying prediction has been outsourced to inputs. This is not an improved program/erase physics model. Reconcile tunneling, well charging, high-voltage supply, and peripheral terms before claiming equivalent coverage. |
| Resistive devices to the experimental smooth nonlinear DC law | Local terminal biases can affect current and source-voltage shifts along a string. | Jacobian, KCL, analytic-limit, and independent numerical checks verify the chosen law. No measured or TCAD comparison establishes that law's physical superiority. Keep it experimental until a controlled comparison against the baseline passes. |

## What the existing evidence establishes

The [earlier RC audit](nand-yang-2023.md) gives a concrete numerical limitation:
for its 512-wordline fixture, EvaCAM's one-pole approximation reports about
119.903 mV sense margin while the independent RC solution gives 75.245 mV,
below the required 100 mV. The [nodal verification](nand3d-rc.md) preserves
that failure with the same reference and decision conditions. This justifies
using the nodal solution for that specified distributed circuit. It does not
show that NVSim falsely accepts the same case: NVSim was not the comparator
and does not expose that CAM decision rule.

The finite-precharge experiment also provides a specific reason to examine
initial conditions: its ideal-driver 512-wordline network reaches only about
13 mV at the source end after 5 ns, rather than the assumed 0.8 V. The
production finite-driver solver has separate reference checks. Neither
experiment establishes the charging time of an actual flash string without
characterized circuit inputs.

The [Kondo comparison](nand-kondo-2022.md) constrains a particular RC circuit
interpretation. Its improved publication correlation does not compare our
model with NVSim or calibrate flash transistors. Similarly, the
[nonlinear DC tests](nand-nonlinear-dc.md) establish that the selected equations
are solved consistently. They do not establish that those equations improve
predictions on device data. These evidence categories must remain separate.

## Required evidence for accepting a replacement

For every changed physical assumption or equation, record:

1. **Original model:** pinned NVSim source, formula, parameter provenance,
   intended operation, and domain in which its evidence applies.
2. **Demonstrated limitation:** an observable discrepancy, a violated
   assumption under the new use case, or an operation the baseline cannot
   represent. An age, style, complexity, or TODO argument is insufficient.
3. **Proposed change:** its derivation, new inputs, and additional assumptions.
   Separate capability extensions from claims of lower prediction error.
4. **Controlled comparison:** identical physical inputs and measurement
   boundaries, changing one assumption at a time; use an independent reference.
   Include a limiting case where both methods should agree.
5. **Quantitative benefit and cost:** predeclared error metrics and acceptance
   limits, failures, runtime, memory, and sensitivity. Added input requirements
   and reduced predictive coverage count as costs.
6. **Disposition:** retain the baseline, accept the change only in the tested
   domain, or keep the proposal experimental. Record unresolved cases explicitly.

Preserve applicable inherited modeling while evaluating alternatives. For a
changed physical model, compare against both the unchanged baseline and a
baseline calibrated on the same allowed data; otherwise extra fitting freedom
can masquerade as a better model. Freeze parameter sets before held-out tests.
Unresolved operating conditions must remain explicit in both comparisons.

## Comparison status and remaining requirements

The [executed validation](nand-nvsim.md) establishes:

1. **Native baseline complete:** pinned SLC CLI execution with raw area,
   latency, energy, and component results. This is a documented nominal fixture,
   not a reproduction of the publication's characterized chip.
2. **Shared RC comparison complete:** 16 identical extracted circuits, common
   initial conditions, independent reference, analytical limiting check, and
   separate finite-precharge/state experiments. The inherited correction is
   preserved; its physical accuracy is not tested by the ideal RC comparison.
3. **Subarray energy reconciliation complete; physical equivalence pending:**
   read/program/erase/write totals reconcile, supplied versus calculated costs
   are mapped, and page/block normalization is explicit. The inherited erase
   accumulation and printed subarray field discrepancy are recorded. Peripheral
   characterization, high-voltage conversion coverage, and matched complete
   operation measurements remain required before accepting replacements.
4. Fit the proposed DC law only on the frozen calibration partition. Compare
   held-out current errors with the applicable NVSim-derived baseline and
   the current linear model under the same declared assumptions. Passing
   numerical tests alone cannot satisfy this gate.

Passing the shared-circuit checks does not mark the outstanding physical-model
replacements as validated improvements. Retain the applicable baseline until
those replacements satisfy their own observable and evidence requirements.
