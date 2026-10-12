# FeFET energy and sensing audit

Updated 2026-10-08. This follows the [component accounting audit](accounting-and-timing-audit.md).
EvaCAM remains analytical. No parameter was fitted to a paper's aggregate delay
or energy. Both reference fixtures remain partial reconstructions.

## Corrections and configuration contract

- `fefet_gate` now uses the cell matchline port's width for pull-down on/off
  resistance, drain capacitance, control-node gate capacitance and leakage.
  The previous off-resistance expression omitted width scaling. The resolved
  legacy width field is derived from this port; a conflicting architecture
  declaration is rejected. Identical declarations remain accepted for compatibility.
- Removed the contradictory 10F architecture declarations from both current
  FeFET-1T examples. The retained 1F port is an inherited, unverified assumption.
  Neither 1F nor 10F has been established as the DATE21 paper's width.
- The DATE21 paper-reference fixture now uses the existing analytical inverter
  for input loading, output delay, output energy and the decision threshold.
  Its two widths are explicitly assumed to be 1.5F, following EvaCAM's minimum
  NMOS convention, with zero characterized external output load. Intrinsic
  output drain capacitances are still included. Equal widths produce a modeled
  trip voltage of 0.396479V; the old fixed 0.5V value remains a diagnostic case.
- FeFET-1T recharge energy now includes the same capacitor inventory as matchline
  timing: wire, cell drains, mux load, precharger drain, SA input and added load.
  The previous energy expression omitted some of these loads. An analytical
  inverter's own energy accounts for its output; its input is charged by the ML.
- Result YAML records the canonical width and FeFET query-energy assumptions.
  The named-reference runner checks the inverter model, decision source and
  resolved pull-down width, preventing silent regression to generic sensing.

The [DATE21 paper, Section IV-A and Fig. 4](https://past.date-conference.com/proceedings-archive/2021/pdf/1478.pdf)
establishes the inverter topology and minimized PTM MOSFETs, but not explicit
widths or output loads. The 1.5F inverter assumption is not evidence that the
cell pull-down should also be 1.5F. The exploration example retains its generic
SA; only the paper-reference fixture changes sensing topology.

## Before and after

Before means immediately before this patch, after the component-accounting work.
Values are full configured bank searches. The saved baseline is
[`before.json`](../../output/validation/fefet-energy-sensing/before.json).

| Fixture | Search ps, before → after | Energy pJ, before → after | Area µm², before → after |
| --- | ---: | ---: | ---: |
| Direct 2FeFET TCAS19 | 303.235 → 303.235 | 2.805970 → 2.805970 | 1338.653 → 1338.653 |
| 2FeFET-1T DATE21 | 461.120 → 495.393 | 4.323472 → 4.125970 | 2896.023 → 2800.122 |

The DATE21 latency diagnostic gap increases from **82.4% to 96.0%** against
252.8ps. At the old fixed 0.5V threshold, replacing only the sensing circuit
reduces delay to **412.973ps**; deriving the threshold from the assumed inverter
requires a deeper discharge and raises delay to **495.393ps**. This exposes a
decision assumption rather than establishing improved paper accuracy.

The direct 2FeFET energy gap against the DATE22 1.5pJ observation remains
**87.1%**. Its supplied cell area is not an independent area validation target.

## Energy ownership and query protocol

| Native energy component | Direct 2FeFET pJ | 2FeFET-1T pJ |
| --- | ---: | ---: |
| Matchline recharge | 1.217499 | 1.234726 |
| External searchline capacitance | 0.704686 | 0.863701 |
| Driver-internal switching | 0.374725 | 0.420790 |
| Other configured subarray components | 0.509061 | 1.606753 |

External searchline charging is already included in `row_driver`; the audit
splits it out and reconciles the sum rather than adding another cost. The
current full-reset assumption charges one complementary line per compared bit.
For a binary capacitor charged from zero, the supply provides `V × Q = C V²`;
the stored energy alone (`½ C V²`) is not the full supply work.

An independent transition enumeration checks all four previous/current bit
pairs. With equiprobable independent queries and retained searchlines, external
line charging halves to **0.352343pJ** for direct 2FeFET. Repeating the query
charges neither line; complementing it charges one line per bit. Resetting both
lines before every query charges one line even for a repeated query.

Changing only external line activity projects direct 2FeFET bank energy from
2.805970 to **2.453627pJ**. This is **not a workload-energy prediction**: internal
driver transitions, ML final voltage, control-node activity, sensing and other
peripherals remain at their native assumptions. No activity multiplier is
silently applied to production energy. The two level-shifter proxies alone cost
0.217391pJ here; their necessity and the paper's peripheral boundary remain
unverified, so they have not been removed to close the gap.

## Sensitivity and remaining evidence

`make audit-fefet-energy` saves ten FeFET-1T alternatives: the reference, generic
SA/fixed threshold, inverter/fixed threshold, pull-down widths 1.5/2/10F,
inverter output loads 1/5fF, and inverter widths 1/3F. Each case records its
changed inputs. The generic-SA case changes both sensing and decision relative
to the reference; the fixed-threshold inverter case separates those effects.
These are illustrative alternatives, not probability distributions or verified
uncertainty bounds. A 10F pull-down gives 223.411ps but raises energy to 6.838292pJ;
closer delay alone cannot identify the correct device.

Remaining evidence includes the actual pull-down/inverter widths, inverter output
load, device R/C, query/reset protocol, ML discharge duration, peripheral inventory,
and paper timing/energy measurement endpoints. DATE21's inherited 64×64 array
size also remains unresolved. Unit and independent circuit checks establish
implementation consistency; these missing conditions still prevent a complete
paper-accuracy claim.

The old/current runner now supplies the same canonical **1F** pull-down to both
engines. Old search is 383.294ps, compared with the historical 384.911ps run that
used the conflicting old 10F field. Those are different baseline configurations;
the earlier audit is retained rather than relabelled as an equal-width comparison.

## Reproduction and checks

```sh
make -j4 test-cam-accounting test-named-cam-regression test-fefet-energy-audit
make audit-fefet-energy validate-named-cam validate-original-evacam compare-legacy-evacam
```

The audit saves fresh configs, raw probe logs/values, component reconciliation,
query scenarios, changed-input descriptions, and input/runner/probe hashes.
Failed runs replace the latest summary with explicit failures while preserving
earlier run directories. Tests cover capacitor ownership by independent load
perturbations, width scaling and conflict rejection, query transitions with
unequal line capacitances, sensing/load sensitivity and stale-output rejection.

Verification passed: the full `make -j4 test-unit test` suite, Python run/match
bindings, all three existing reference runners, the new audit runner and the
inventory freshness check. Valgrind reported zero errors and no leaks. The
inventory has 1,329 callables, 1,121 covered, 89 pre-existing missing cases and
119 exemptions; this patch adds no uncovered production callable.

- [Generated audit table](../../output/validation/fefet-energy-sensing/comparison.md)
- [Full evidence](../../output/validation/fefet-energy-sensing/runs.json)
- [All-device reference comparison](all-device-reference-comparison.md)
