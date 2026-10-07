# Reference comparison across supported device families

The current models improve direct two-FeFET and MRAM-4T2R latency agreement,
but the additional shared-input runs show worse latency agreement for FeFET-1T
and MRAM-6T2R. There is no established improvement across all devices or metrics.

Values below are **absolute percentage gaps, old executable → current model**.
They are diagnostics from partial reconstructions, not validated accuracy bounds.
The old column means executed public code, not the original paper's reported
EvaCAM predictions. A dash means no defensible comparison is established, not
zero error. Parameter sweeps and alternative array sizes share their device row.

| Device or configuration | Latency gap, old → current | Energy gap, old → current | Area gap, old → current |
| --- | ---: | ---: | ---: |
| Direct two-FeFET, TCAS19 / DATE22 validation | 26.7% → 12.6% | 24.8% → 16.2% | — |
| FeFET 2Fe-1T, DATE21 | 52.3% → 83.4% | — | — |
| Other FeFET TCAM and MCAM variants | — | — | — |
| MRAM 4T2R, VLSIC12 | 69.1% → 47.9% | — | 22.5% → 52.3% |
| MRAM 6T2R, VLSIC11 | 147.6% → 257.9% | — | — |
| MRAM 2T2R, ASPDAC12 | — | — | — |
| PCM 2T2R, JSSC14, generic diagnostic | — → 32.7%* | — | — |
| ReRAM 2.5T1R, ISSCC16 | Broken baseline → 115.6% | — | — |
| ReRAM 3T1R, ISSCC15 | Broken baseline → 59.8% | — | — |
| ReRAM 4T2R, VLSIC14 | Broken baseline → 84.2% | — | — |
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
electrical inputs. The 32.7% generic gap is not validation of that circuit.

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

The expanded table adds two executions using the existing pinned old executable
and the current read-only comparison probe. No production model or config was
changed. The existing current result manifests have the same SHA-256 as the
current executable, so their named-fixture results are still current.

| Case | Paper latency | Old search | Current search | Resolved shared fields checked |
| --- | ---: | ---: | ---: | ---: |
| FeFET 2Fe-1T DATE21 | 252.8 ps | 384.911 ps | 463.648 ps | 81 |
| MRAM 6T2R VLSIC11 | 0.29 ns | 0.718063 ns | 1.037876 ns | 125 |

FeFET-1T uses the current paper-reference fixture's shared geometry, resistances,
ports and biases. The old engine cannot represent `fefet_gate`, its switching
voltage, analytical decision or explicit timing schedule; those model differences
are recorded in the run manifest. Zero read power is written as `0uW` instead of
`0W` for the legacy exporter, with no numerical change. Its energy scope still
cannot be paired with the paper's per-bit energy to make a validated error score.

MRAM-6T2R uses the same generic circuit configuration in both engines. Its old
0.718063 ns result supersedes the earlier 0.745350 ns geometry-only diagnostic
for this shared-input comparison. The older diagnostic did not carry all the
current fixture's inputs.

Evidence and source contracts:

- [Full table data and input manifest hashes](../../output/validation/all-device-summary/summary.json)
- [FeFET-1T resolved inputs, metrics and file hashes](../../output/validation/all-device-summary/fefet1t.json)
- [MRAM-6T2R resolved inputs, metrics and file hashes](../../output/validation/all-device-summary/mram6.json)
- [Original matched old/current audit](matched-legacy-evacam.md)
- [Named paper reference contracts](named-cam.reference.yaml)
- [Original publication reference contracts](original-evacam.reference.yaml)
- [Supported and incomplete device modes](../limitations.md)

The `fefet1t-*` and `mram6-*` directories next to the summary retain both exported
input formats and old/current logs. The existing `make compare-legacy-evacam`
target reproduces the original matched suite; these two additional runs use its
export, probe, and input-check helpers and are preserved separately.
