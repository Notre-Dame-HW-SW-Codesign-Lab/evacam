# CAM sensing and topology repairs

Updated 2026-10-07. This follows the [initial paper-reference fixtures](named-cam-reference-configs.md)
and preserves their `partial` status. It fixes the custom-amplifier input path,
the DATE21 FeFET discharge topology and a process-interpolation error. No
resistance, capacitance or delay was fitted to a paper headline.

The subsequent [analytical timing report](analytical-cam-timing.md) contains
the current results. Values below preserve this stage as a historical baseline.

## Changes

- A v2 `sensing.sense_amplifier` reference with `model: scalar` now reaches
  the existing custom-amplifier model. Relative paths resolve from the sensing
  file, arbitrary amplifier names work, and area, input load, latency, dynamic
  energy and leakage propagate into results. Explicit zero leakage stays zero.
  Unknown models and incomplete scalar data are rejected. The
  [schema example](../schema.md#sense-amp-file) is synthetic, not paper data.
- Both DATE21 configs now select `topology: fefet_gate`. The two FeFETs drive
  a separate CMOS gate; only the NMOS pull-down carries matchline current.
  This follows [DATE21 Section III-A and Fig. 4](https://past.date-conference.com/proceedings-archive/2021/pdf/1478.pdf).
  The separate gate delay, input slew and energy are included in search results.
  Direct-discharge two-FeFET TCAM and MCAM retain their original topology.
- Interpolation between the 120 and 200 nm tables now divides by their 80 nm
  separation. At 180 nm the weight is 0.75; the old denominator of 60 selected
  the 200 nm electrical values. Physical dimensions remain at the requested node.
- YAML labels the cell topology, amplifier model, full-search versus legacy
  single-sense timing, and the existing 50% Horowitz matchline convention.
  It reports gate delay separately from matchline delay and exposes full
  subarray latency. The paper runner rejects a switch to the legacy timing
  scope or an unintended topology change.

## Control-node model

The FeFET pair is represented by complementary `Ron` and `Roff` branches
between `Vdd`, the pull-down gate and ground. Define `C` as one CMOS gate,
two FeFET drain terminal capacitances, and optional extra node capacitance.
The model uses:

```text
Vmatch    = Vdd * Ron / (Ron + Roff)
Vmismatch = Vdd * Roff / (Ron + Roff)
tau       = (Ron || Roff) * C
tgate     = -tau * ln(1 - Vswitch / Vmismatch)
slew      = (Vmismatch - Vswitch) / (tau * Vdd)
Pdivider  = Vdd^2 / (Ron + Roff)
Echarge   = C * Vmismatch^2
```

`Echarge` is the supply work for full settling from zero, above steady divider
dissipation. The per-cell energy budget is `Echarge + Pdivider *
(tgate + tmatchline + tsense)`, multiplied by compared cells and active sense
paths. Charging every compared control node to mismatch is a conservative
activity assumption, not the paper's workload energy. The former fixed `1uW`
cell-read input is removed so this modeled energy is not counted twice.

The fixture's `Vswitch = 0.5V` is an explicit digital switching assumption.
It must fall strictly between the two settled levels. The model adds `tgate`
before matchline evaluation and uses the slower of control-node and row-driver
slews in the existing Horowitz calculation. This staged approximation does
not solve simultaneous nonlinear transistor and distributed-line transients.
It is not the paper's calibrated Preisach/BSIM FeFET model. FeFET resistances
remain fixed inputs rather than voltage- or temperature-dependent device laws.

Nominal exact TCAM search is supported. Device variation, approximate search,
noncomplementary supply rails, NVM participation in the matchline and external
cell-read energy overrides are rejected for this topology.

## Results

All times below are ns. Before and after use the same seven reference fixtures,
with the documented topology correction and process interpolation change.
The before column is the [initial reference run](named-cam-reference-configs.md#initial-results),
not the public old EvaCAM release. Historical tool comparisons remain in
[old-evacam-comparison.md](old-evacam-comparison.md).

| Design | Paper | Before | After | Current raw gap |
| --- | ---: | ---: | ---: | ---: |
| FeFET DATE21 | 0.2528 | 0.644592 | 0.490475 | +94.0% |
| MRAM4 VLSI12 | 2.50 | 0.885443 | 0.885443 | -64.6% |
| MRAM6 VLSI11 | 0.29 | 1.038 | 1.038 | +257.9% |
| PCM JSSC14 | 1.90 | 1.278 | 1.278 | -32.7% |
| ReRAM ISSCC16 | 1.00 | 2.156 | 2.156 | +115.6% |
| ReRAM ISSCC15 | 0.96 | 1.348 | 1.348 | +40.4% |
| ReRAM VLSI14 | 1.20 | 2.430 | 2.211 | +84.2% |

The after column is the full configured bank search. FeFET's breakdown is
1.265 ps control-node delay, 318.062 ps matchline and 2.735 ps generic amplifier;
the remainder comes from the other configured peripherals. A matchline-only
number is not a substitute for a paper's search latency. Modeled FeFET energy
changes from 4.062 to 2.860 pJ; ReRAM14 changes from 224.912 to 209.483 pJ.
The other five energies and all seven areas are unchanged. These energy numbers
still lack matching paper activity and measurement boundaries.

`make validate-named-cam` regenerates the table and preserves the binary, input
and runner hashes alongside all seven result files under
`output/validation/paper-configs/`. The [reference manifest](named-cam.reference.yaml)
contains paper sources and unresolved assumptions for every row.

## Verification

`FefetGateModelTest` independently integrates the two-branch Kirchhoff current
equation using RK4, detects the threshold crossing and integrates supply
current. It checks voltage, delay, slew, supply work and divider loss over 54
combinations: `Ron = 1/10/100 kOhm`, resistance ratios 64 and 160000,
`C = 0.1/1/10 fF`, and `Vdd = 0.8/1/1.2 V`. This verifies the implemented
linear circuit, not experimental device accuracy; no external SPICE solver
was used.

End-to-end regressions cover the scalar-amplifier path on ReRAM15, zero leakage,
custom-load effects, FeFET resistance isolation from matchline resistance,
gate loading and energy, both bank routes, invalid topology combinations,
and 16/32/64-bit FeFET words at 300/350/400 K. Both new and existing tests are
included in the unit target and CI. Verification passed:

```sh
make -j4 test-unit test-generated-v2-configs test-montecarlo test-corner \
  test-mat-decoder test-htree-routing test test-pybind-match test-pybind-run
make -j4 test-named-cam-regression test-physical-domain-validators \
  test-results-serialization test-nand3d-results test-nand-results \
  test-named-cam-validation check-unit-test-inventory
make validate-named-cam
```

The Valgrind run reported zero errors. All seven paper fixtures produce a
positive finite solution and pass their configured sensing margin.

## Remaining characterization

Scalar sensing is now usable, but the shipped paper fixtures still use generic
NVSim amplifiers. The original EvaCAM configs also select `CustomSenseAmp: No`;
they do not supply missing paper-characterized scalars. [NVSim-CAM Section 4.3](https://miglopst.github.io/files/li_iccad2016.pdf)
describes obtaining custom sensing costs from separate circuit characterization.

The next inputs needed are independently characterized sensing/load values,
actual device I-V curves, and paper timing/activity boundaries. DATE21's inverter,
the MRAM diode-connected keeper, ReRAM region-splitter or voltage-divider control,
and PCM clocked self-referenced sensing require their respective circuits.
A scalar amplifier does not implement these nonlinear circuits, nor does it
change the matchline's current 50% timing convention. Supply/process/temperature
sweeps of a scalar amplifier must supply separately characterized values.
None of the seven fixtures is promoted to a validated paper reproduction.
