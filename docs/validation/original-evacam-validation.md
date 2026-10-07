# Original EvaCAM validation: reconstruction and analytical circuit corrections

This audit uses the **DATE 2022** paper, Tables I–II, as the historical baseline.
The newer TCAD draft and the old executable are different baselines. The original
45 nm validation is a **direct two-FeFET 64×64 TCAM**, not the DATE 2021
2FeFET-1T circuit. See the [reference contract](original-evacam.reference.yaml),
[complete input ledger](original-evacam.inputs.yaml), and generated
[comparison table](../../output/validation/original-evacam/comparison.md).

The subsequent [matched executable comparison](matched-legacy-evacam.md) establishes
that the public code also fails to recover the paper's reported predictions.
With shared reconstructed inputs, old/current search is 256.649/305.742 ps for
two-FeFET and 0.772795/1.303268 ns for MRAM. The current circuit models improve
latency agreement over those old-code runs, although they still fall short of
the paper's claimed accuracy. MRAM area agreement remains worse. The comparison
also isolates the old doubled diode-capacitance factor and mislabelled latency.

## Disposition of the four requested steps

| Step | Delivered | What cannot yet be claimed |
| --- | --- | --- |
| Reconstruct the original validation setups | Separate original-paper fixtures for direct 2FeFET, MRAM and PCM; all four original cases and published EvaCAM values in a machine-readable manifest; SAPIENS capacity/clock/workload reconciliation | Exact publication decks are unavailable. SAPIENS is a serial distance-computation architecture, not the generic exact-TCAM fixture. |
| Correct circuit-specific analytical modeling | Direct NVM access paths; physical terminal inventory; two-bit PCM discharge-path count; inverter sensing with a derived trip point; linear and square-law diode keepers; supply-recharge energy | PCM's clocked charge-sharing/latch/reference circuitry and SAPIENS's complete divider/accumulator backend are still not reproduced. The new reductions do not conceal these omissions. |
| Replace inherited electrical assumptions | Paper-sourced geometry, minimum inverter sizing, complementary FeFET search inputs, MRAM DC nodes and conservative bias power; every fixture leaf has a checked provenance record | FeFET R/C, keeper sizing/control-node load, PCM CSRSS sizes/reference programming and SAPIENS charger/threshold data remain unavailable or inherited. |
| Compare energy and area at matching scope | Independent latency/energy/area comparisons, explicit measurement kind and scope, original published values, input hashes, fresh results/logs and regression tests | None of these partial reconstructions establishes a full matched chip-level accuracy score. Missing observations are null, bounds are not point targets, and cell-area inputs are not independent validation. |

No device resistance, capacitance, transistor width or extra delay was optimized
against a published macro result. All runtime models remain analytical. Numerical
integration is confined to the unit-test oracle.

## Source findings

Full primary PDFs were found in the local research library. Their titles,
locations relative to that library, public URLs and SHA-256 digests are recorded
in the manifest; the PDFs are not redistributed in this repository.

- [EvaCAM, DATE 2022](https://past.date-conference.com/proceedings-archive/2022/pdf/0130.pdf),
  Tables I–II: RRAM, PCM and MRAM silicon comparisons; a separate two-FeFET
  HSPICE comparison. MRAM's printed `ps` table unit conflicts with the original
  chip's 2.5 ns result. The reported error column sometimes differs from arithmetic
  on rounded values (e.g. 2.1/1.9 gives +10.5%, while the paper prints 9.4%).
- [Two-FeFET, TCAS-II 2019](https://doi.org/10.1109/TCSII.2018.2889225),
  experimental setup and Fig. 6: 45 nm PTM, 64×64, 1 V search, minimum MOS
  W/L=65/45 nm, inverter SA and 0.15 µm² cell. The 25°C condition is approximated
  by the current technology table's minimum supported 300 K. The DATE 2022
  350 ps/1.5 pJ HSPICE point is retained as an observation, not used to infer a
  resistance or output load.
- [MRAM, VLSIC 2012](https://doi.org/10.1109/VLSIC.2012.6243781),
  Figs. 1, 4–7: 64×32, 90 nm CMOS, 1.2 V, 1.1/2.5 kΩ MTJs, 3.14 µm² cell,
  measured DC cell-node separation 0.39 V and ML_ST-to-OUT delay 2.5 ns. Approximate
  DC intersections are 0.32/0.71 V and 73/63 µA. The macro bounding box
  152.6×135.9 µm includes blank space; it is not the 17,200 µm² target used by
  EvaCAM. The source-node midpoint is an explicit SA-trip assumption.
- [PCM, JSSC 2014](https://doi.org/10.1109/JSSC.2013.2292055),
  Tables I–III and Figs. 10–17: eight 2048×64 banks, four 512×64 subarrays per
  bank, 0.41 µm² two-transistor/two-resistor cell, one selected branch per encoded
  two-bit pair, 1.9 ns at 1.2 V after screening defective bits. CSRSS precharges
  and evaluates at different inverter thresholds and uses an externally adjusted
  reference array. Approximately 0.55/0.45 V in the illustrative plots does not
  specify the fabricated SA sizing. The 2.2×2.7 mm die includes reference/test/IO.
- [SAPIENS, TED 2021](https://doi.org/10.1109/TED.2021.3110464),
  Figs. 2, 5–6 and Section IV: 65,536 RRAM devices encode 32,768 logical bits;
  eight 32×128 subarrays share 32 SAs through 8:1 BL muxes. The paper reports
  a 640 ns full 32-vector query and a 200 MHz clock, and supports one or two
  bits per sensing cycle. The 5 ns clock is not a full-query latency. Eight
  sequential mux groups would require 5,120 ns at 128 cycles per group; that is
  schedule arithmetic, not an independently measured ensemble latency.

SAPIENS Fig. 6 gives about 270 pJ for its normalized 32-vector comparison.
The separately reported 3.39 mW × 640 ns gives 2,169.6 pJ. Dividing by eight
would happen to produce 271.2 pJ, but the paper does not establish that conversion
for this comparison. The audit retains this unresolved normalization rather than
using that coincidence as validation. Similarly, 98,000 µm² is EvaCAM's
die-photo subset, while the SAPIENS paper describes a 0.2 mm² core.

## Analytical models and accounting

`sensing.circuit` is opt-in. Existing fixtures keep their legacy electrical
models. Circuit options require nominal exact TCAM, explicit search timing,
valid voltage ordering and whole encoded pairs; variation and approximate
matching are rejected for these reductions.

```yaml
# Direct 2T2R/2FeFET, or two-bit encoded PCM:
circuit:
  model: direct_nvm
  precharge_voltage: 0.55V
  bits_per_discharge_path: 2
# Fixed decision; inverter_threshold can instead derive a trip from its SA model.
decision:
  model: voltage_threshold
  threshold: 0.45V
```

For direct FeFET, the selected memory device itself is the conducting path.
For PCM/ReRAM, the selected access FET stays **on** in both the matching and
mismatching states: Rpath=Raccess,on+Rmemory,state. Two physical drain terminals
per logical cell contribute capacitance, even when only one selected branch per
pair conducts. The physical capacity is unchanged by two-bit encoding. Encoder
area and energy count one encoder per pair. The nominal exact-match API still
returns the common one-miss decision time and conservative energy; a multi-bit
sensed voltage cannot be inferred from mismatch count alone when pair locations
are unknown and is explicitly rejected.

A second initialization used to overwrite the topology-specific matchline
capacitance before precharger sizing. This is removed for all generic CAMs.
The line, precharger, discharge and energy calculations now retain the same
physical terminal inventory. This correctness fix also changes legacy examples.

`analytical_inverter` is a sense-amplifier file model with required positive
`n_sense_width`, `p_sense_width` and nonnegative `output_capacitance`. Its input
load is the gate capacitance, its output load includes both drains, its rising
output delay is ln(2) R_P C_out, and area/leakage use the existing technology
formulas. It does not use a regenerative latch logarithm or a current converter.
The trip estimate solves equal square-law currents:

```
r = sqrt(mu_p W_p / (mu_n W_n))
Vtrip = (Vth + r (Vdd - Vth)) / (1 + r)
```

This is an analytical technology approximation with symmetric threshold
magnitudes. An unspecified external output load is represented by an explicitly
recorded 0 fF assumption, not presented as a measured value. Internal drain
capacitance and energy are still included.

`clamped_keeper` uses piecewise linear resistors that switch off below two
configured clamp voltages; its two exponential segments include a shared Elmore
wire term. `diode_keeper` instead interprets `keeper_high_clamp` and
`keeper_low_clamp` as the **DC source-node voltages** and adds the technology
MOS threshold. `decision.model: keeper_midpoint` derives an assumed decision
voltage from the source-node midpoint plus Vth. The serialized assumptions state
which convention is used.

For the diode model each path contributes `I=k max(VML-Vsource-Vth,0)^2`, where
`k=Ion/(Vdd-Vth)^2` comes from the existing technology table and configured
keeper width. Above the high clamp, the parallel paths sum to a quadratic and
integrate with an arctangent. Below it, matching paths cut off and the remaining
response is reciprocal in time. Both have closed-form inverses. The existing
linear activation exposure handles input slew without a transient solver.
Wire capacitance is included; wire resistance, body effect, bias-source impedance
and source-node settling are omitted in this reduction and remain model limits.

For the new circuits, matchline recharge supply work is
`C_total Vdd (Vpre - Vfinal)`. The nominal report uses full discharge (direct
NVM) or the low clamp (keeper) as a conservative bound; it does not claim a
paper workload activity. Direct floating-ML discharge no longer also incurs a
second `read_power × number_of_cells × time` supply term. MRAM retains a separate
bias supply term using the approximate worst DC operating current from Fig. 1(c).
Explicit search timing now also includes enabled level-shifter, row-control and
output-mux energy. Precharger control energy is separate from its ML recharge.

## Current results

With the reconstructed fixtures, direct two-FeFET search is 0.305742 ns versus
0.350 ns HSPICE (raw difference −12.6%), and 1.743 pJ versus 1.5 pJ (+16.2%).
MRAM search is 1.303 ns versus 2.5 ns (−47.9%). Replacing the preliminary linear
keeper by the analytical diode law moves MRAM from about 0.87 ns to 1.30 ns;
this improvement uses technology parameters, not the macro delay target.
These differences are diagnostics because the input and measurement contracts
remain incomplete. They do **not** establish better accuracy than the original
EvaCAM paper's reported 0.345 ns and 2.72 ns, respectively.

The original-paper PCM fixture is infeasible with inherited electrical inputs,
and SAPIENS has no native estimate. The seven older named diagnostics remain
available through `make validate-named-cam`; their latency values did not change
at the displayed precision in this pass. Do not substitute those older generic
PCM/MRAM estimates for the new circuit reconstruction when claiming validation.

## Reproduction and checks

```sh
make -j4
make validate-original-evacam
make validate-named-cam
```

The original suite records binary, script, library and fixture hashes, and writes
fresh per-run YAML and console logs under `output/validation/original-evacam/`.
The input audit rejects missing, duplicated or changed provenance entries.
Unexpected feasibility changes fail the suite. PCM's inherited-input
infeasibility is an explicit expected result, not an omitted case or a passing
accuracy result. SAPIENS reports unsupported architecture rather than a fabricated
native estimate. Runtime crashes, malformed output and missing results still fail.

Focused checks cover independent numerical integration of the linear and
square-law keeper equations, threshold crossing, inverter load scaling, energy
supply work, parser rejection, direct-device resistance, encoded access leakage,
source/match voltages, preserved capacitance and scope-aware metric comparisons.
Integration tests use a reduced PCM margin **only inside the test** to inspect
its electrical accounting; the paper fixture retains its original 70 mV
requirement and remains infeasible.

## Remaining work is visible, not calibrated away

The next engineering work is the complete PCM CSRSS charge-sharing/latch/reference
path and SAPIENS divider/serial-distance backend. Their characterization inputs
must be independent of the aggregate target numbers. Existing scalar peripheral
interfaces can accept such independent characterization; substituting a scalar
chosen to close the paper gap would not validate the tool. The original release's
exact validation decks, paper-specific wire/load assumptions and test activity
would be needed before claiming the original accuracy has been reproduced or
surpassed.
