# Matched NVSim flash validation

Executed 2026-10-02. **The numerical checks pass; better physical accuracy than
NVSim is not established.** NVSim's uncorrected aggregate RC estimate is already
within 0.48% of the independent solution on these 16 matched fixtures. The nodal
solver improves numerical precision and represents finite precharge and carried
state, but these results do not justify removing NVSim's empirical delay floor
or replacing its device and peripheral estimates with uncharacterized inputs.

This executes the baseline, shared-circuit, and subarray energy portions of the
[model-change audit](nand-nvsim-justification.md). No production model was
retuned for this comparison. Physical calibration, area accuracy, and equivalent
whole-operation energy prediction remain separate acceptance gates.

Subsequent scope change: production 3D NAND now uses analytical RC, retaining
its vertical geometry. The transient measurements and energy mapping below
describe the implementation audited before that restoration. The RC reference
and pinned NVSim comparison remain verification tools; see
[the current 3D contract](../nand-3d-tcam.md) for application behavior.

## Reproduction and provenance

```sh
make validate-nand-nvsim NVSIM_SOURCE=/path/to/NVSim
make test-nand-nvsim-validation
# Offline replay of the frozen native extraction, without an NVSim checkout:
make validate-nand-nvsim
```

The full run exports tracked files at commit
`6334d00ca2e0320a9c930eae45de145ca65c210c`, verifies source hashes, and builds
both the original CLI and a small [native adapter](../../tests/NvsimFlashProbe.cpp)
against the unmodified source. It does not use the checkout's working-tree
source edits or existing executable. The original checkout is never modified.
The adapter exposes native component values; it does not link to EvaCAM.

The recorded build used GCC 13.3.0, `-std=c++11 -O2`. Source hashes, compiler,
commands, and binary/adapter hashes are in [build.json](data/nvsim/build.json).
The [manifest](nand-nvsim.reference.yaml) pins the revision, source and extracted
data hashes, fixture grid, and acceptance limits. The limits were fixed before
the full comparison, and were not loosened after seeing the results.

Retained baseline inputs and outputs:

- [Native CLI configuration](data/nvsim/baseline.cfg) and
  [raw console output](data/nvsim/baseline.stdout.txt).
- [Full-precision extracted circuits and energy components](data/nvsim/baseline.json).
- [Evaluator](../../scripts/validate_nand_nvsim.py), which compares fresh native
  results with the frozen extraction before evaluating circuits.
- Generated `output/validation/nand-nvsim/report.json`, `comparison.png`,
  `comparison.svg`, and individual probe request/result YAML files. A replay
  identifies itself as `frozen_native_results`; a rebuild as `native_execution`.

The native CLI fixture is 32 MiB planar SLC, 45 nm HP, 350 K, a 1024-byte page,
64 KiB block, internal sensing, sense mux 2, and unrepeated aggressive wires.
It uses 16×1 mats in the bank, with one active mat and one subarray per mat.
The subarray has 1024 rows and 16384 columns. An initial 4×4 bank arrangement
was rejected by NVSim's aspect-ratio constraint; 16×1 keeps the same capacity
and subarray geometry and yields a valid result. This is a declared nominal
fixture, not a reconstruction of the paper's 50 nm, 2 Gb validation chip.

| Native CLI observable | Result |
| --- | ---: |
| Total area | 2.623 mm² |
| Page-read latency | 317.260 ns |
| Page-read energy | 15.129 nJ |
| Program-page latency / energy | 200.011 µs / 31.049 nJ |
| Erase-block latency / energy | 1.250 ms / 272.579 nJ |
| Leakage power | 270.092 mW |

The standalone electrical adapter uses an ideal input ramp. Its primary
fixture has the same subarray geometry and bitline delay as the CLI, but its
317.033 ns subarray latency differs slightly from the CLI's 317.062 ns because
the CLI propagates the predecoder ramp. Bank totals also include routing and
predecoder costs. Neither difference is attributed to a new flash model.

## Matched timing comparison

The 16 fixtures are the Cartesian product of 16/64/256/512 pages per block,
1024/4096 rows, and 1024/16384 physical columns. Other adapter conditions remain
fixed. These are probes of inherited equations, not 16 characterized devices.

For each case, extract `Rstring`, `RBL`, `Ccell`, `CBL`, and the mux delay load
directly from the initialized native subarray. Interpret its first-moment
expression as this two-node pi circuit:

```text
ground -- Rstring -- node 0 -- RBL -- node 1 (observed)
                     |                |
                 Ccell+CBL/2     CBL/2+Cmux_delay
                     |                |
                   ground           ground
```

Both nodes start at NVSim's precharge voltage. The far node is observed at
`Vpre - senseVoltage`. This circuit has exactly the first moment used in
NVSim's delay expression. It is an explicit circuit interpretation of that
expression, not a recovered transistor netlist or a unique physical topology.

An independently assembled dense conductance matrix and SciPy matrix exponential
provide the reference. A root solve determines threshold time. EvaCAM's compiled
RC solver is checked at that time and at ±0.01% around it; the voltages must
bracket the threshold. Removing wire resistance and merging capacitances gives
a separate analytical single-pole limiting check.

The native result is also replayed from its actual formula, including Horowitz
ramp handling and `max(Horowitz(...), 20*tau_log)`. Here
`tau_log = first_moment * ln(Vpre / threshold)`. The floor wins in all 16 cases.

| Check | Declared limit | Observed |
| --- | ---: | ---: |
| Compiled voltage error at three crossing samples | 3 µV absolute | ≤ 2.44×10⁻¹¹ V |
| Compiled threshold-time bracket | ±0.01% | All 16 bracket the reference |
| Single-pole limiting voltage error | 3 µV absolute | ≤ 2.43×10⁻¹¹ V |
| Uncorrected NVSim first-moment error | Report, not a physical acceptance test | −0.4751% to −0.00363% |
| Native corrected delay / ideal RC crossing | Report, not a physical error metric | 19.905 to 19.999 |

For `p64-r1024-c16384`, the common initial voltage is 0.6 V and threshold is
0.4 V. The uncorrected estimate is **15.3255 ns**, independent crossing is
**15.3300 ns**, and native corrected bitline delay is **306.5100 ns**.
The approximately 20× difference measures the explicit correction relative to
an ideal RC circuit. It is **not a measured 20× error against silicon**.
Removing it needs evidence against the chip observable it was intended to model.

At the deliberately tight 10⁻¹⁰ V solver tolerance, crossing solves use 329–549
accepted steps. Recorded subprocess times are roughly 3.4–5.2 ms, including
startup and YAML serialization. NVSim uses a scalar expression, and its build
and the existing EvaCAM debug probe have different optimization settings.
These timings provide no fair whole-simulator speed ratio; memory was not
benchmarked. Numerical precision has a computational cost, and the observed
small aggregate-delay error does not by itself warrant that cost everywhere.

### Follow-up runtime measurement

A separate in-process benchmark on `p64-r1024-c16384` compiled both implementations
with GCC 13.3.0 and `-O2`. Native files retain C++11 compatibility; EvaCAM uses
C++17. Seven batches exclude initialization, process startup and serialization.
The native path calls the actual `SubArray::CalculateLatency` (100,000 calls per
batch); EvaCAM calls the two-node discharge solver (1,000 calls per batch) to
the uncorrected first-moment crossing time. Results are consumed to prevent
elimination of the timed work.

| Timed work | Median per call | Range across batches |
| --- | ---: | ---: |
| NVSim complete subarray timing calculation | 3.294 µs | 3.270–3.353 µs |
| EvaCAM two-node discharge, 1 µV tolerance | 9.548 µs | 9.398–9.777 µs |
| EvaCAM two-node discharge, 10⁻¹⁰ V validation tolerance | 265.204 µs | 261.825–269.365 µs |

The solver takes 13 and 389 accepted steps respectively. Its discharge call is
about 2.9× or 80.5× the native subarray timing call in this fixture. This compares
the cost of the implemented calculations, not identical output contracts: the
native method includes peripheral timing, whereas the RC call evolves one
electrical phase. It is **not a whole-application slowdown factor**. Full CAM
work additionally depends on node count, pattern coverage and phase count.
The 1 µV value matches the shipped example's tolerance, but this benchmark's
two-node geometry and maximum step are those of the matched experiment.

Generated source, build commands and raw repetitions are retained under
`output/validation/nand-nvsim/` as `RuntimeProbe.cpp`, `runtime-build.json`,
`runtime.csv`, and `runtime-summary.json`.

## Precharge and energy conservation

This is a separate experiment. Use NVSim's **power** mux capacitance, not its
delay load, and its calculated precharger transistor resistance. Open the source
end and drive the far end from zero with a finite resistor. No new device values
are fitted. Duration is 0.1, 1, 5, or 30 times the charging scale
`Rdriver * (C0+C1) + RBL * C0`; that scale is not a claim of a single time constant.

The compiled source charge is compared with independent capacitor charge:
`Esupply = Vpre * Qsource = Vpre * sum(Ci * Vi)` for the zero initial state.
The maximum relative energy discrepancy is **1.14×10⁻¹⁰**, below the declared
10⁻⁵ limit. Voltages and subsequent evaluation from the charged state also pass
the 3 µV limit. Across the fixtures:

| Precharge duration / scale | Energy / fully charged CV² |
| --- | ---: |
| 0.1 | 0.1038–0.1403 |
| 1 | 0.6547–0.7099 |
| 5 | 0.9950–0.9972 |
| 30 | 1.0000 within numerical precision |

At long duration, multiplying by native physical column count recovers NVSim's
bitline charging term. There is no extra factor of one-half: this is energy
delivered by a constant-voltage source, not energy stored in the capacitors.
Lower finite-precharge energy also leaves lower voltages; it is not evidence
of an equally functional, cheaper read or search.

The power load and delay load differ in native NVSim. Consequently, a fully
charged *power-load* circuit in the primary case reaches 0.38536 V at the
*delay-load* circuit's 15.3300 ns crossing. The report records this separate
fully charged reference so this difference is not mislabeled a precharge error.
After a precharge of one charging scale the corresponding value is 0.25258 V.
State handling is a demonstrated capability on this stipulated network;
the hardware precharge protocol and circuit values still require validation.

## Native subarray energy reconciliation

All 16 native read, program, erase, and generic-write totals reconcile to a
maximum relative residual of **2.57×10⁻¹⁶** (limit 10⁻¹⁰). The ledger retains
the executed code's accounting, including its unusual erase accumulation.
The primary fixture gives:

| Operation and component | Energy (nJ) |
| --- | ---: |
| Read: bitline/cell/mux charging | 0.160878 |
| Read: all native peripherals | 13.078113 |
| **Read total** | **13.238990** |
| Program: bitline/cell/mux charging | 16.087773 |
| Program: tunneling | 0.001327 |
| Program: row driver | 13.047056 |
| Program: other write peripherals | 0.023677 |
| **Program-page total** | **29.159834** |
| Erase: bitline and source-line charging | 101.899917 |
| Erase: well charging | 0.748237 |
| Erase: embedded program total | 29.159834 |
| Erase: row driver | 138.858024 |
| Erase: other write peripherals | 0.023677 |
| **Erase-block total** | **270.689689** |
| **Generic write statistic** | **16.478522** |

The read row driver alone contributes 13.015445 nJ, about 98.31% of subarray
read energy. Improving only the 0.160878 nJ charging term cannot establish
better accuracy for the entire energy estimate.

The generic write statistic uses the native average of raw program energy and
raw erase energy divided by page count, then adds its generic peripheral terms.
It is not interchangeable with either complete operation total.

Two source-level findings need to remain visible:

1. `SubArray.cpp` adds the already accumulated `setDynamicEnergy` to
   `resetDynamicEnergy`. That embeds the complete 29.159834 nJ program total in
   erase, or about 10.77% of erase energy here. The experiment establishes what
   the code does. Whether this represents an intended program-before-erase
   protocol or an accounting defect requires a separate physical/protocol check;
   the baseline was not silently corrected.
2. `Result.cpp` prints `subarray.writeDynamicEnergy` in both the NAND erase and
   program subarray rows, while the parent totals use reset/set values. Hence
   both printed subarray values are 16.479 nJ even though the native fields are
   270.690 and 29.160 nJ. Their row-decoder detail likewise prints the generic
   write field. This is a concrete reporting discrepancy, not a reason to
   replace the flash physics.

The exact reconciliation covers the subarray. It records native computed
peripheral contributions but does not independently validate their transistor
models. Bank routing and predecoder overhead are retained in the CLI result
and lie outside this ledger. The audited code has no separately identifiable
NAND charge-pump energy term in this path; high-voltage conversion coverage
must be established before declaring whole-supply energy equivalence.

## Mapping to EvaCAM's NAND3D energy model

| Inherited term or boundary | Current NAND3D treatment | What this validation establishes |
| --- | --- | --- |
| Read bitline/cell/mux CV² | Integrated `bitline_and_internal_precharge`, divided by supplied efficiency | Same fully charged energy limit on matched capacitances; finite-state charge is numerically verified. Actual NAND3D capacitances remain uncharacterized. |
| Wordline/row-decoder switching | `wordline_capacitance` plus supplied `wordline_drivers` | Common switching-energy category, but different CAM bias sequence and driver inputs. No matched peripheral accuracy gain. |
| Select lines and decoders | Explicit select capacitance/drivers plus query/setup costs | CAM operation and accounting extension; no independently validated one-to-one cost equivalence. |
| Precharger, sense amplifiers, muxes | Supplied precharge overhead, sensing, and page buffers | Device-side charge is separated from overhead. Cost provenance and non-overlap still need characterization. |
| Recovery | Explicit grounded recovery and supplied overhead | Carried-state behavior is represented; the hardware recovery sequence is unvalidated. |
| Program tunneling, high-voltage line and well charging | Supplied complete `program_page` / `erase_block` energy | These costs replace a prediction with inputs. This is not an improved program/erase physics model. |
| Bank routing | Added outside device metrics | Keep route and device boundaries distinct; this experiment does not validate CAM routing accuracy. |

The shipped `config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml` was also executed,
with output retained as `output/validation/nand-nvsim/evacam-cam.yaml`.
Its synthetic 64-entry × 32-bit CAM reports 44.3451 pJ per search, 2 nJ per
program page and 20 nJ per erase block; the latter two are supplied operation
costs. This is neither the capacity nor the operation of the native SLC page-read
fixture. Dividing these totals to claim an accuracy or efficiency improvement
would be invalid.

## Disposition and remaining evidence

- Retain NVSim as the inherited planar-SLC baseline, including its empirical
  correction, while evaluating proposed replacements.
- Accept the nodal method's numerical solution and finite-state capability for
  the tested RC networks. The aggregate-delay gain here is small; use a demonstrated
  need for internal states or sampled CAM margins to justify the added cost.
- Keep nonlinear cell laws, 3D geometry, supplied electrical parameters and
  peripheral costs experimental until matched device/circuit evidence supports
  them. The nonlinear DC law was not calibrated in this experiment.
- Resolve the erase protocol and reporting discrepancy explicitly before changing
  accounting. Neither warrants retuning an unrelated model term.
- Reproduce the published chip conditions or obtain independent device/circuit
  data before claiming lower physical error, including held-out comparisons for
  any fitted replacement. This work does not reproduce NVSim Table III or
  validate total CAM-versus-page-read energy.

Validation run: the full native build/comparison, eight offline regression tests,
the existing NAND3D and nonlinear numerical suites, and the synthetic CAM example.
Regression tests reject changed baseline fields, missing fixtures, wrong source
hashes, removed delay correction, and invalid CLI results; they also verify
analytic limits, source-charge conservation, all native energy totals, and
the explicit unestablished hardware-improvement status.
