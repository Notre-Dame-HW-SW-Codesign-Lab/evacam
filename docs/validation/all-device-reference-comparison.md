# Reference comparison across supported device families

The current models improve direct two-FeFET and MRAM-4T2R latency agreement,
but the additional shared-input runs show worse latency agreement for FeFET-1T
and MRAM-6T2R. The [accounting and timing audit](accounting-and-timing-audit.md)
corrects component counts and removes idle write lines from search timing.
The subsequent [FeFET audit](fefet-energy-sensing-audit.md) reconciles pull-down
sizing and uses an analytical inverter; its assumed trip point increases the
FeFET-1T latency gap. Direct two-FeFET energy remains unchanged.
There is no established improvement across all devices or metrics.

Values below are **absolute percentage gaps, old executable → current model**.
They are diagnostics from partial reconstructions, not validated accuracy bounds.
The old column means executed public code, not the original paper's reported
EvaCAM predictions. A dash means no defensible comparison is established, not
zero error. Parameter sweeps and alternative array sizes share their device row.

| Device or configuration | Latency gap, old → current | Energy gap, old → current | Area gap, old → current |
| --- | ---: | ---: | ---: |
| Direct two-FeFET, TCAS19 / DATE22 validation | 26.7% → 13.4% | 24.8% → 87.1% | — |
| FeFET 2Fe-1T, DATE21 | 51.6% → 96.0% | — | — |
| Other FeFET TCAM and MCAM variants | — | — | — |
| MRAM 4T2R, VLSIC12 | 69.1% → 49.1% | — | 22.5% → 22.0% |
| MRAM 6T2R, VLSIC11 | 147.6% → 244.2% | — | — |
| MRAM 2T2R, ASPDAC12 | — | — | — |
| PCM 2T2R, JSSC14, generic diagnostic | — → 34.9%* | — | — |
| ReRAM 2.5T1R, ISSCC16 | Broken baseline → 115.5% | — | — |
| ReRAM 3T1R, ISSCC15 | Broken baseline → 60.8% | — | — |
| ReRAM 4T2R, VLSIC14 | Broken baseline → 83.8% | — | — |
| ReRAM 2T2R, generic | — | — | — |
| ReRAM 2T2R, VLSI21 / SAPIENS | — | — | — |
| SRAM 8T BCAM, 65 nm | — | — | — |
| SRAM 10T BCAM, 28 nm | — | — | — |
| SRAM 16T TCAM, 28 nm / ESSCIRC15 example | — | — | — |
| Planar SLC NAND TCAM | — | — | — |
| 3D NAND TCAM | — | — | — |
| FBRAM, accepted legacy type | — | — | — |

*PCM's generic fixture omits the complete two-bit encoding/CSRSS circuit. The
more detailed original-validation fixture has no valid solution with its inherited
electrical inputs. The 34.9% generic gap is not validation of that circuit.

## What the missing entries mean

- The old current-mode ReRAM RC expression adds a dimensionless word length to
  a capacitance. Its huge outputs cannot serve as a meaningful physical accuracy
  baseline; the previous old-code audit records the failure.
- MRAM-2T2R has no comparable absolute timing/energy observation established in
  the existing source audit. Generic ReRAM and the other FeFET variants likewise
  have no matched publication fixture.
- SAPIENS requires the serial divider/distance/accumulator architecture. Its
  clock period is not whole-query latency, and that backend remains incomplete.
- SRAM examples have no matched reference metric/operating point. The ESSCIRC15
  attribution is tentative; the candidate paper's clock frequency and process
  differ from the example. A supplied cell footprint is not independent area
  validation.
- NAND has circuit and numerical verification, but no matched published CAM
  latency/energy/area validation for these configurations. The older Kondo
  transient comparison and NVSim memory-operation checks must not be substituted
  for current analytical whole-CAM accuracy.
- FBRAM is an accepted legacy type without a paper-validation fixture in this
  suite. Parser acceptance is not a validation claim.
- Energy comparisons need matching query/activity boundaries; area comparisons
  need matching included circuits and blank-area conventions. Only the two
  diagnostic comparisons shown above currently have numerical pairs in this
  audit. MRAM area still has an unresolved measurement-scope difference.

DRAM, eDRAM and multilevel NAND remain under development and are excluded from
supported-device accuracy claims.

## Additional controlled runs

The matched runner now includes FeFET-1T and MRAM-6T2R permanently, with
stage breakdowns, isolated configuration ablations and component-budget checks.
The current column includes the accounting/timing corrections; old source and
shared electrical inputs remain pinned. All values are full configured searches.

| Case | Paper latency | Old search | Current search | Resolved shared fields checked |
| --- | ---: | ---: | ---: | ---: |
| FeFET 2Fe-1T DATE21 | 252.8 ps | 383.294 ps | 495.393 ps | 81 |
| MRAM 6T2R VLSIC11 | 0.29 ns | 0.718063 ns | 0.998131 ns | 125 |

FeFET-1T uses the current paper-reference fixture's shared geometry, resistances,
ports and biases, with the canonical 1F pull-down exported to both engines.
The earlier 384.911ps old result used the conflicting 10F global field and remains
historical evidence. The old engine cannot represent `fefet_gate`, its switching
voltage, analytical inverter/decision or explicit timing schedule; those model differences
are recorded in the run manifest. Zero read power is written as `0uW` instead of
`0W` for the legacy exporter, with no numerical change. Its energy scope still
cannot be paired with the paper's per-bit energy to make a validated error score.

MRAM-6T2R uses the same generic circuit configuration in both engines. Its old
0.718063 ns result supersedes the earlier 0.745350 ns geometry-only diagnostic
for this shared-input comparison. The older diagnostic did not carry all the
current fixture's inputs.

Evidence and source contracts:

- [Full table data and input manifest hashes](../../output/validation/all-device-summary/summary.json)
- [Fresh matched inputs, stage metrics and hashes](../../output/validation/matched-legacy/runs.json)
- [Before/after accounting results](../../output/validation/accounting-timing/after.json)
- [Original matched old/current audit](matched-legacy-evacam.md)
- [Named paper reference contracts](named-cam.reference.yaml)
- [Original publication reference contracts](original-evacam.reference.yaml)
- [Supported and incomplete device modes](../limitations.md)

`make compare-legacy-evacam` reproduces both additional cases and their ten
isolated native stages. The earlier `fefet1t-*` and `mram6-*` directories are
historical evidence; use the fresh matched report for current values.
