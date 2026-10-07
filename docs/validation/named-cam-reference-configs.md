# Runnable CAM paper references

For the subsequent original-DATE-2022 reconstruction, full-paper evidence and
additional circuit/accounting corrections, see [the original validation audit](original-evacam-validation.md).
The numerical tables below describe the earlier checkpoint.

Added 2026-10-07 after the [CAM repairs](named-cam-fixes.md). Seven separate
fixtures under [`config/paper_reference/`](../../config/paper_reference/README.md)
now pin the published dimensions, process nodes and cell areas where the sources
establish them. All seven run successfully and satisfy their configured sensing
margins. **None is yet a complete reproduction of its paper.**

The subsequent [sensing and topology repairs](named-cam-sensing-topology.md)
update the DATE21 model and 180nm interpolation. That report contains the
before/after table; the initial results below are preserved as a baseline.
The later [analytical timing report](analytical-cam-timing.md) contains the
current comparison and separates decision, schedule and amplifier effects.

The [machine-readable reference manifest](named-cam.reference.yaml) records each
paper, reference value, source location, matched fields, inherited assumptions and
expected modeled geometry. Its `partial` status is independent of numerical
agreement. No model equation, resistance, sense margin or delay constant was tuned
to reduce an error percentage in this work.

## Reproduce the comparison

```sh
make validate-named-cam
```

This builds EvaCAM if needed and runs the seven fixtures with one thread using
`scripts/validate_named_cam.py` (Python 3 and PyYAML). Outputs are
`output/validation/paper-configs/comparison.md` and `runs.json`; each execution
also preserves its own directory with logs, result YAMLs, command arguments,
binary/runner hashes, input hashes and a copy of the reference manifest.
Fresh per-run paths prevent failed runs from reusing stale output.

The command fails for invalid execution, missing numerical results, geometry or
physical-node drift, failed sensing margins, or nonpositive/nonfinite principal
metrics. It does **not** fail because a partial model misses a paper headline.
Those are separate questions: regression correctness and physical calibration.

## Initial results

Search latency below is the complete configured bank search, including precharge
and peripherals. The model-only matchline column is diagnostic; it must not be
substituted for bank latency to improve apparent agreement. Paper delay boundaries
remain unresolved. Values come from rounded YAML outputs.

| Design | Paper ns | Exploration example ns | Reference fixture ns | Matchline only ns | Reference gap |
| --- | ---: | ---: | ---: | ---: | ---: |
| FeFET DATE21 | 0.2528 | 0.768466 | 0.644592 | 0.473444 | +155.0% |
| MRAM4 VLSI12 | 2.50 | 2.945 | 0.885443 | 0.380127 | -64.6% |
| MRAM6 VLSI11 | 0.29 | 2.685 | 1.038 | 0.476190 | +257.9% |
| PCM JSSC14 | 1.90 | 1.276 | 1.278 | 0.648275 | -32.7% |
| ReRAM ISSCC16 | 1.00 | 2.156 | 2.156 | 1.149 | +115.6% |
| ReRAM ISSCC15 | 0.96 | 1.178 | 1.348 | 0.414371 | +40.4% |
| ReRAM VLSI14 | 1.20 | 1.380 | 2.430 | 1.109 | +102.5% |

The exploration column is the saved post-repair baseline in
[named-cam-fixes.md](named-cam-fixes.md); it is not rerun by this command.
MRAM6 and FeFET move closer with supported configuration changes. Other examples
move farther away: their earlier apparent agreement used different hardware.
This shows why the prior raw gaps cannot establish device accuracy.

Energy and area are recorded for model regression and future scope reconciliation,
without measured-error scores:

| Design | Modeled search pJ | Modeled total area um^2 |
| --- | ---: | ---: |
| FeFET DATE21 | 4.062 | 5,874.454 |
| MRAM4 VLSI12 | 18.397 | 28,476.070 |
| MRAM6 VLSI11 | 23.192 | 48,542.854 |
| PCM JSSC14 | 21,996 | 1,451,000 |
| ReRAM ISSCC16 | 883.063 | 72,776.377 |
| ReRAM ISSCC15 | 249.788 | 45,388.875 |
| ReRAM VLSI14 | 224.912 | 134,100.723 |

## Sourced changes and remaining assumptions

- **FeFET:** 45nm and the 0.36um^2 footprint follow [DATE21 Table I](https://past.date-conference.com/proceedings-archive/2021/pdf/1478.pdf).
  This converts to 177.777777778 F^2, replacing 350 F^2. The inherited 64x64
  geometry is still an assumption: Table I does not identify its array size.
  Device characterization, peripheral scope and activity remain unmatched.
- **MRAM4:** 64 entries x 32 bits at 90nm follow [NVSim-CAM Table 2](https://miglopst.github.io/files/li_iccad2016.pdf).
  The [original paper title](https://doi.org/10.1109/VLSIC.2012.6243781) supplies
  3.14um^2/cell. The 2.50ns measurement is reported through NVSim-CAM; the
  original full chip paper was unavailable. Inherited diode/device sizes and
  generic sensing remain uncharacterized against the chip.
- **MRAM6:** 64x32, 90nm, 1.2V and 10.35um^2/cell follow the
  [author presentation, slide 11](https://www.csis.tohoku.ac.jp/files/Matsunaga_2011_SymposiumonVLSI_Circuits.pdf).
  Slide 7 confirms a diode-connected matchline keeper. Its nonlinear I-V and
  self-discharge timing are still approximated. Slide 19's energy number belongs
  to a different segmented array and is excluded from this prototype comparison.
- **PCM:** corrected the fixture's name to JSSC14 and the cell node to 90nm;
  the footprint is 0.41um^2 and nominal capacity is 1Mbit, from the
  [author publication](https://li.seas.upenn.edu/publication/li-2014-jssc/).
  The inherited 64-bit organization, 32 subarrays and physical/logical capacity
  mapping do not reconstruct the paper's two-bit encoding and clocked
  self-referenced sensing.
- **ReRAM16:** explicitly pins the [published 256-bit word](https://scholar.nycu.edu.tw/en/publications/a-256b-wordlength-reram-based-tcam-with-1ns-search-time-and-14-im/).
  Its inherited 64 entries, 65nm process and footprint remain assumptions.
  The region-splitter sense amplifier remains absent from the model.
- **ReRAM15:** allocates two complete 64x64 blocks at 90nm, matching the
  [author institution abstract](https://scholar.nycu.edu.tw/en/publications/a-3t1r-nonvolatile-tcam-using-mlc-reram-with-sub-1ns-search-time/).
  This replaces the exploration input's two 64x32 fragments. Representing the
  blocks as entry partitions in one mat is an implementation assumption;
  the bi-directional voltage-divider control still needs characterization.
- **ReRAM14:** sets both process labels and physical dimensions to 180nm and
  128x32, following [official program paper 12.2](https://archive.vlsisymposium.org/14web/wp-content/uploads/2013/06/Circ-14-program.pdf).
  The cell footprint and transistor widths remain inherited F-unit assumptions.
  Generic current sensing does not reproduce the RC-filtered stress-decoupled
  circuit, and the electrical process parameters remain interpolated PTM values.

Every fixture uses `SearchLatency`, explicit physical subarray dimensions and
full-word comparison. Temperature and unspecified electrical settings retain
their exploration-example values and are labeled as assumptions. Original
exploration inputs and old-style configs were not modified by this initial
fixture-only work. The subsequent topology repair also updates the canonical
DATE21 exploration config.

ASPDAC12, SRAM ESSCIRC15 and ReRAM VLSI21 are explicitly excluded in the manifest:
respectively, unavailable absolute timing, tentative paper identification with a
clock-frequency-only reference, and missing topology with a different search metric.

## What remains to improve agreement

First establish each paper's timing endpoints, precharge inclusion, array activity,
supply and temperature. Then characterize the named sensing circuit and its
matchline loading from a published schematic or SPICE reference. A reported
whole-search delay cannot serve as an independently characterized amplifier delay.

The current CAM backend supports generic NVSim voltage/current sensing. Other
parsed sense modes are rejected, so paper-specific sensing may require model
work in addition to YAML inputs. Preserve the separate matchline and peripheral
breakdowns when validating that extension. Device I-V, layout parasitics and
encoded-capacity accounting remain independent sources of uncertainty.

## Checks

- `make test-named-cam-regression`: resolved dimensions, physical process,
  cell area, sensing mode, supply where sourced, subarray count and full capacity.
- `make test-named-cam-validation`: units, raw-gap computation, invalid quantities,
  geometry drift, sensing failure and stale-output rejection.
- `make test-generated-v2-configs`: parsing of every shipped config, including
  the seven new fixtures.
- `make validate-named-cam`: actual binary runs, sensing margins and report generation.

The tests enforce supported physical contracts without asserting that these
partial reconstructions reproduce measured performance.
