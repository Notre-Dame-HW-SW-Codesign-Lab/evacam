# CAM component accounting and timing audit

This is the accounting checkpoint. The subsequent [FeFET energy and sensing
audit](fefet-energy-sensing-audit.md) updates FeFET-1T sensing, width consistency
and recharge energy; the before/after numbers below are preserved historically.

Updated 2026-10-08. This implements the first two next steps: component accounting
and an explanation of the FeFET-1T/MRAM-6T2R regressions. No paper fixture, device
resistance, threshold, activity factor or circuit model was fitted to a reference.

## Accounting corrections

- Subarray area now adds each aggregate decoder, write driver and mux once. Their
  classes already own the physical instance counts. Removed extra ×4/×9/×3 factors
  and the extra output-driver addition on consecutive bitlines.
- Input buffers count once per physical query bit. Level-shifter dimensions use
  their own areas, with input-bit and sense-output counts respectively. Disabled
  output-mux selectors contribute no area. Rectangular layout starts from `lenRow`,
  and width × height equals the full component sum.
- Search-driver energy counts every compared bit. The driver primitive reports one
  active line, whereas a CAM activates many query lines in parallel. The existing
  mean-squared-voltage activity assumption is retained; it represents charging from
  the reset level, not independently characterized successive-query activity.
- Column mux power is evaluated before aggregation. Accumulator/buffer energy and
  leakage use the same sense-output counts as area; input buffer/encoder leakage
  uses physical input counts. Decoder/level-shifter costs are included consistently.
- Reused input-buffer/encoder energy is subtracted for the complete compared query
  when scheduling additional sense groups. Both bank routes, YAML and the Python
  result extractor use the same cached-input budget.
- Native results, YAML and Python expose additive subarray area, search energy,
  leakage and critical-path budgets. These are local budgets; bank routing and
  repeated sensing/comparison operations are accounted at the bank level.

Level shifters remain the existing analytical proxy, including on nominal-voltage
search paths. Their circuit need and characterization are not established for each
paper. The accumulator/buffer inventory retains one copy per sense output. These
assumptions are now visible rather than removed solely to improve agreement.
The conventional read/write circuitry and write-energy measurement scope have not
been independently revalidated by this search-focused audit.

## Before and after

Before is the current tool immediately before this patch, not the released old
executable. Original-validation fixtures are marked **original**; others are the
named partial reference fixtures. Bank area includes modeled peripherals.

| Fixture | Search ns, before → after | Search pJ, before → after | Area µm², before → after |
| --- | ---: | ---: | ---: |
| FeFET-2Fe1T-DATE21 | 0.463648 → 0.461120 | 3.059 → 4.323 | 5760.4 → 2896.0 |
| MRAM-4T2R-VLSIC12 | 0.885443 → 0.849986 | 18.397 → 29.712 | 28476.1 → 13985.0 |
| MRAM-6T2R-VLSIC11 | 1.037876 → 0.998131 | 23.192 → 44.287 | 48542.9 → 30512.4 |
| PCM-2T2R-JSSC14 | 1.278061 → 1.235977 | 21995.565 → 23060.885 | 1450891.4 → 990313.4 |
| ReRAM-2.5T1R-ISSCC16 | 2.156157 → 2.154524 | 883.063 → 989.542 | 72776.4 → 27573.5 |
| ReRAM-3T1R-ISSCC15 | 0.385813 → 0.376263 | 21.749 → 32.485 | 32512.3 → 20352.3 |
| ReRAM-4T2R-VLSIC14 | 2.211143 → 2.206099 | 209.483 → 274.888 | 134100.7 → 89151.9 |
| FeFET-2Fe-TCAS19 **original** | 0.305742 → 0.303235 | 1.743 → 2.806 | 2402.8 → 1338.7 |
| MRAM-4T2R-VLSIC12 **original** | 1.303268 → 1.271603 | 160.154 → 166.159 | 26195.1 → 13414.7 |

The MRAM4 original-fixture area gap against 17,200 µm² falls from **52.3% to
22.0%**. FeFET-1T and MRAM6 latency gaps fall from **83.4% to 82.4%** and
**257.9% to 244.2%**. These remain partial-reconstruction diagnostic gaps.
Correct accounting does not uniformly improve publication agreement: direct two-FeFET
energy rises from 1.743 to 2.806 pJ versus the 1.5 pJ observation, increasing the gap
from **16.2% to 87.1%**. Its latency gap rises slightly (12.6% to 13.4%), as does
MRAM4 latency (47.9% to 49.1%), because their predictions already underestimate delay.
Area changes also alter peripheral layout loads and upstream timing, so even
unchanged local circuit equations can produce small timing changes.

## Why the two latency regressions occurred

### FeFET-1T

The old executable uses the separate `match_transistor.cmos_width=10F` for its
CMOS discharge path, whereas the fixture describes a **1F** matchline-port device.
The evaluated path resistance is 1.840 kΩ old versus 18.399 kΩ current. It also
uses a different sensing event and omits the current level-shifter proxies.
The current model includes a separate 1.265 ps FeFET control-node delay.

The native one-change audit gives:

- Reference: **461.120 ps** total, including 317.480 ps matchline and 0.734 ps SA.
- Add 1 fF: **474.066 ps** total.
- Change only decision to legacy Horowitz: **460.993 ps**.
- Restore only legacy scheduling: **488.610 ps**.
- Change the pull-down port to the old 10F sizing: **216.374 ps**.

The 10F run is a diagnostic, not a proposed paper configuration. It also changes
drain and gate capacitance, so it measures the effect of device sizing rather
than resistance alone. The SA intrinsic delay is small; its 3.926 fF input load
still contributes to matchline delay. The remaining work is the paper inverter
and characterization of its load, the minimum transistor drive and timing boundary.

### MRAM-6T2R

The old executable evaluates a voltage-divider sensing event at the requested
70 mV swing; the current generic path uses a 50% Horowitz discharge event. These
are different events and circuit abstractions. The old diode-capacitance bug
inflates line loading, while its effective conducting path is smaller (4.577 kΩ
versus 10.431 kΩ). The current model also includes level-shifter delay absent from
the public source. Both inherit a 64:1 output readout mux that adds 79.097 ps.

A real search-path bug was corrected: write-only wordlines held at zero during
search no longer select the slowest search driver or slew. Addressed read/write
decoder timing remains separate. Absent mux-select paths are also excluded.

- Reference: **998.131 ps** total, including 466.216 ps matchline and 7.139 ps SA.
- Add 1 fF: **1005.449 ps**.
- Replace only the decision with an ideal differential margin event: **634.818 ps**.
- Use the explicit broadcast schedule: **920.558 ps**.
- Remove only the output readout mux: **918.654 ps**.

These alternatives identify timing-boundary sensitivity. None implements the
paper’s complete self-match-line-discharge control, so they are not silently
promoted into the reference configuration or substituted into the accuracy table.

## Reproduction and checks

```sh
make -j4 test-cam-accounting test-legacy-comparison
make compare-legacy-evacam
make validate-named-cam validate-original-evacam
```

`make compare-legacy-evacam` permanently includes both regression cases (81 and
125 resolved shared inputs checked) and ten native stages. Old FeFET-1T exports
explicitly disclose the control-node/decision/schedule features the old engine
cannot represent. Failed runs invalidate the latest summary; prior evidence is kept.

Focused tests verify additive budgets, independent area perturbations, rectangular
layout, disabled blocks, compared-bit scaling, both bank routes with cached inputs,
YAML/Python consistency, active-search driver selection, repeated calculation,
and sensitivity to added capacitance. They are included in unit tests and CI.
A stale NAND rejection test now names an existing generic amplifier file so it
reaches the intended unsupported-amplifier check instead of failing file loading.
No NAND production model changed.

- [Before snapshot and probe hash](../../output/validation/accounting-timing/before.json)
- [After snapshot and component budgets](../../output/validation/accounting-timing/after.json)
- [Pinned old/native executions and stage hashes](../../output/validation/matched-legacy/runs.json)
- [Generated stage comparison](../../output/validation/matched-legacy/comparison.md)
- [All-device reference table](all-device-reference-comparison.md)

Final verification passed: `make -j4 test-unit test`, the Python run/match binding
tests, all three reference runners above, and the inventory freshness check.
Valgrind reported zero errors and no leaks. The inventory has 1,324 callables,
1,116 covered, 89 pre-existing missing cases and 119 exemptions; no new missing
production callable was introduced.
