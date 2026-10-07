# Named CAM repairs

Implemented 2026-10-07 following the [old-code audit](old-evacam-comparison.md).
These changes repair configuration migration and circuit bookkeeping. They
are not a calibration to the papers' headline results.

## Changes

- Restored explicit current sensing for ReRAM VLSI14, ISSCC15, and ISSCC16.
  The generic NVSim converter contributes its characterized delay and energy.
- Preserved requested physical feature size in both CMOS and FeFET technology
  contexts while retaining the existing electrical table/interpolation rules.
  YAML results report physical size, electrical table bounds, and alpha.
- Unified explorer capacity and word geometry with the match API: stored
  entries × physical columns. Removed the legacy byte-valued subarray row
  override. Implicit and explicit 64 × 256 inputs now agree in area, delay,
  energy, and matchline cell count.
- Migrated PCM, SRAM ESSCIRC15, 8T BCAM, and 10T BCAM active
  partitions to split entries rather than divide a short word into invalid
  fragments. Each still allocates its configured capacity.
- Corrected ASPDAC12 physical capacity to 1152 B (128 × 72 bits). Its explicit
  eight-column schedule uses nine steps with the existing output accumulator.
  This preserves the examined HV schedule, not a claimed measured chip clock.
  Comparison widths must divide the full word and the active data partitions;
  the local circuit uses only its share of parallel comparison paths.
- Connected `memory_device.match.is_nvm_discharge` and column matchline port
  flags to the resistance equations. Either true enables participation; false
  does not override another true setting. Non-matchline port flags are rejected.
  Previously ignored memory-device width/capacitance keys are rejected with
  supported alternatives documented in [the schema](../schema.md).
- Separated line wire capacitance from terminal capacitance. A diode-connected
  device contributes one gate plus one drain load per device, so repeated
  cells scale linearly. Topology-specific terminal loads replace generic line
  loads; delay and explicit matchline-energy formulas no longer add the same
  terminal inventory twice. TCAM and MCAM share the RC first moment, including
  wire capacitance behind the discharge resistance. Inactive comparison
  columns still contribute their attached capacitance.

## Independent circuit check

The regression solves the nodal equation obtained by integrating
`C dv/dt = -Gv` from uniform precharge to zero: `G integral(v dt) = C 1`.
For 8-, 32-, and 128-segment ladders, two discharge resistances, and two wire
resistances, the far-end first moment agrees with the analytical expression
within relative error `1e-9`. This tests the distributed-load and endpoint-load
accounting independently of the production expression. It does not establish
accuracy of the subsequent single-exponential/Horowitz waveform approximation
or any particular fabricated CAM circuit.

## Canonical example results

The before column is the saved paper-audit executable. The after column uses
the repaired source and canonical inputs. Config changes are included, so
these are end-to-end comparisons, not isolated model sensitivities.

| Config | Before delay | After delay | Before search energy | After search energy |
| --- | ---: | ---: | ---: | ---: |
| FeFET-2Fe1T-DATE-2021 | 357.621ps | 768.466ps | 3.503pJ | 4.934pJ |
| MRAM-2T2R-ASPDAC12 | 1.244ns | 20.079ns | 59.948pJ | 419.676pJ |
| MRAM-4T2R-VLSIC12 | no valid result | 2.945ns | — | 2.548nJ |
| MRAM-6T2R-VLSIC11 | 13.992ns | 2.685ns | 7.222nJ | 1.970nJ |
| PCM-2T2R-JSSC11 | 1.059ns | 1.276ns | 16.191nJ | 21.922nJ |
| ReRAM-2.5T1R-ISSCC16 | 612.547ps | 2.156ns | 542.955pJ | 883.063pJ |
| ReRAM-3T1R-ISSCC15 | 542.886ps | 1.178ns | 42.285pJ | 112.268pJ |
| ReRAM-4T2R-VLSIC14 | 188.338ps | 1.380ns | 0.733pJ | 46.909pJ |
| SRAM-16T-ESSCIRC15 | 279.113ps | 599.574ps | 240.557pJ | 273.723pJ |
| ReRAM-2T2R-VLSI21 | no valid result | input error | — | — |

All nine examples with a complete topology now produce a valid result. The
MRAM improvements reflect corrected diode loading, not reduced sensing
requirements. The VLSI21 placeholder still has no matchline and is rejected;
constructing that circuit requires a supported, explicit topology. Its likely
source paper describes a different distance-computing design.

The inherited paper/config differences recorded in the
[paper comparison](named-cam-papers.md) remain relevant. For example, the VLSI14
exploration input still describes a 40 nm, 64 × 16 array, whereas the paper's
macro is 180 nm, 128 × 32. Specialized sensing, layout, activity, and encoding
are not supplied by a filename. These repairs do not turn the exploration
inputs into paper reproductions or justify a device accuracy percentage.

## Evidence and verification

- [Run manifest and results](../../output/validation/named-cam-fixes/runs.json).
- [Named-config regressions](../../tests/NamedCamRegressionTest.cpp): sensing,
  geometry equivalence, allocation, serial steps, partitions, and resistance
  sensitivity through both input paths.
- [RC ladder check](../../tests/CamSubArrayMatchTest.cpp) and
  [linear terminal loading](../../tests/CamLineTest.cpp).

The final combined validation command passed (exit 0):

```sh
make -j4 -k test-unit test-generated-v2-configs test-montecarlo test-corner test-mat-decoder test-htree-routing test test-pybind-match test-pybind-run
```

This includes the full unit aggregate, config parsing, Monte Carlo/corner,
routing/decoder, Python run/match API, and Valgrind checks. Valgrind reports
zero errors and no leaks. The new test target is registered in CI.
[Validation log](../../output/validation/named-cam-fixes/validation.log) and
[source hashes](../../output/validation/named-cam-fixes/provenance.json) accompany
the runs.
Original legacy files and previous audit outputs remain unchanged.
