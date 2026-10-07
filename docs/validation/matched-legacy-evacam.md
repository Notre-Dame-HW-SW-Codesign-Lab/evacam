# Reproducing and comparing the public EvaCAM executable

**The public executable does not reproduce the original paper's reported model
values. With shared reconstructed inputs, the current circuit models improve
FeFET and MRAM latency agreement over that executable.** This does not establish
the original paper's claimed accuracy, or an improvement across every metric.

The [all-device comparison](all-device-reference-comparison.md) adds shared-input
FeFET-1T and MRAM-6T2R runs, whose latency agreement is worse in the current model,
and records reference coverage for the remaining device families.

The [generated comparison](../../output/validation/matched-legacy/comparison.md)
and [machine-readable evidence](../../output/validation/matched-legacy/runs.json)
separate published values, the executable's actual search metrics, and its
mislabelled console metrics. The [original validation audit](original-evacam-validation.md)
records the underlying paper observations and remaining input limitations.

## What was reproduced

The runner extracts the committed source at
`3289a855c7ad4b74edd60920a9c5c6dc2edcad9b` from the local upstream Git repository.
Its only changes since the August 2022 source upload `28cd15e` are to `README.md`.
It builds with `g++ -std=c++11 -O0 -g`; only the reporter is instrumented. The
original checkout, its local modifications, and `old_style_config/` are untouched.

The public release contains just the root `2FeFET_TCAM.cfg` and `.cell` example.
The named MRAM/PCM/RRAM examples in the local old checkout are untracked additions,
not recovered publication decks. The committed FeFET example evaluates to:

| Metric | Public executable | DATE 2022 reported EvaCAM |
| --- | ---: | ---: |
| Actual bank search latency | 313.776 ps | 345 ps |
| Console field labelled Search Latency | 379.913 ps | 345 ps |
| Bank search energy | 2.24043 pJ | 1.48 pJ |
| Subarray area | 3112.32 µm² | 3274 µm² |

Neither latency field recovers 345 ps. This example also uses 350 K, a 300 F²
cell and generic latch sensing; it is not the reconstructed 300 K, 0.15 µm²
minimum-inverter validation circuit. The root config specifies **0 fF** additional
ML capacitance. The **33 fF** entry in its cell file is ignored by that reader.
The current ordinary example's 6 fF is therefore an input change. Changing only
that input in the old executable moves actual search from 313.776 to 355.349 ps;
this sensitivity is not evidence that 6 fF was the publication's setting.

The exporter writes shared YAML and legacy decks from the same restricted inputs.
At runtime it checks 59 resolved fields for FeFET/PCM and 92 for MRAM: geometry,
process, supply, temperature, resistance, margin, added capacitance, access sizing,
ports and their terminal/voltage definitions, and mux/write-driver choices.
The exported public deck must also reproduce the committed root deck's latency,
energy and area. No config or circuit parameter is fitted to a paper target.

## Shared reconstruction inputs and the latest circuit models

These compare bank search to the same paper observations used in the earlier
audit. The two executable columns share reconstructed device/geometry inputs;
the current circuit model additionally uses its documented circuit-specific
assumptions, which the old model cannot express. Signed gaps use
`100 × (estimate/reference − 1)`. They remain diagnostic gaps because the exact
publication decks, loads and timing boundaries are incomplete.

| Case | Paper reference | Paper's EvaCAM prediction | Old executable | Current circuit | Old gap | Current gap |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Direct two-FeFET, 64×64 | 350 ps | 345 ps | 256.649 ps | 305.742 ps | −26.7% | −12.6% |
| MRAM 4T2R, 64×32 | 2.5 ns | 2.72 ns | 0.772795 ns | 1.303268 ns | −69.1% | −47.9% |

FeFET energy improves from **1.87189 pJ** in the old executable to
**1.74343 pJ**, versus 1.5 pJ: +24.8% to +16.2%. There is no matching MRAM
energy observation in this contract.

MRAM area goes the other direction: **13,337.9 to 26,195.1 µm²**, versus
EvaCAM's 17,200 µm² measurement scope (−22.5% to +52.3% raw gaps). That scope
excludes blank area and is not fully matched by either generic floorplan.
FeFET has no independent reference subarray area in the original validation table;
3274 µm² is the old paper's model prediction, not measured ground truth.

Thus the earlier statement that the new reconstructions agree less closely than
the original paper's *reported predictions* remains true. It did not establish
that the new implementation is less accurate than the *released executable*.
The controlled latency comparison now shows improvement over that executable for
these two reconstructions.

## Why the numbers move

### The old console reports a different metric

`CAM_Result::print()` labels `bank->readLatency` as Search Latency.
The actual `bank->searchLatency` uses a different schedule: it adds precharge
and row drive, and omits output-SA mux delays included in the read path.
The current code's search path includes explicit control and enabled mux stages.
The generated table retains both old fields; it never compares an old printed
read delay silently against a current search delay.

The old `CAM_SenseAmp::CalculatePower()` also zeroes its latency after timing
has been accumulated. The instrumentation uses the saved subarray
`senseAmpLatency`, not that cleared field. With generic sensing and shared
inputs, both versions' intrinsic SA delays are equal in these cases. The large
latency differences come from paths, loads and schedules.

### FeFET path resistance and capacitance differ

On the public FeFET inputs the old model uses the configured **10 kΩ** directly;
the current generic path uses **29.221 kΩ**, adding a CMOS access resistance.
On the smaller paper-input fixture it uses **17.738 kΩ**. A direct two-FeFET
cell should use the selected device path, which the opt-in `direct_nvm` model
now does. The generic configuration is therefore a continuity diagnostic,
not the recommended model for this circuit.

The old FeFET mismatch RC includes the line's already counted drain capacitance
and another cell-capacitance term, but omits SA/precharger end loads that appear
in its all-match expression. The current model uses one physical load inventory
for all states. Reproducing that older expression would recover a number without
establishing a consistent circuit model.

The current staged reconstruction gives:

| FeFET stage, shared physical inputs | Search latency | Search energy |
| --- | ---: | ---: |
| Generic circuit and default schedule | 426.306 ps | 8.58527 pJ |
| Explicit broadcast/search schedule | 400.231 ps | 8.74601 pJ |
| Direct NVM, generic SA, explicit 0.5 V decision | 288.850 ps | 2.25469 pJ |
| Final inverter SA with derived trip point | 305.742 ps | 1.74343 pJ |

The intermediate 0.5 V decision is an ablation, not a sourced calibration.
The final fixture derives the inverter trip from its analytical sizing model.
Removing the second DC supply-power term from a floating matchline explains
much of the energy correction. The old model also counted two powered devices
per word where the later generic model counts the comparison width; these
energy conventions cannot be compared as if they represented identical activity.

### Old MRAM agreement depends strongly on a capacitance bug and margin input

The old diode terminal load contains `Cgate × N × N + Cdrain × N`.
For 32 bits this makes the ML capacitance **306.456 fF**, compared with
**35.830 fF** using linear terminal counting. A separate diagnostic copy of the
old source removes only the inner `N`; no transistor sizes or other equations
are changed.

| MRAM input | Unmodified old search | Old with only diode count corrected |
| --- | ---: | ---: |
| Shared 70 mV minimum sense requirement | 0.772795 ns | 0.347587 ns |
| Inherited 500 mV requirement | 2.912116 ns | 0.595451 ns |

The 2.912 ns result looks close to the 2.5 ns observation, but the agreement is
not robust to fixing this counting error. The inherited 500 mV requirement also
exceeds the approximately 390 mV cell-node separation reported for the chip;
it cannot be used uncritically with the reconstructed keeper circuit.

The current diode model additionally accounts for diminishing overdrive as the
ML approaches its source voltage plus threshold. That moves the current
reconstruction from **0.891472 ns** with linear clamps to **1.303268 ns** with
the diode law. The two reductions use different voltage conventions as documented
in the original circuit audit. Body effect, source-node settling and the exact
keeper/sensing dimensions remain unresolved; the result is not yet validated.

### Area accounting contains inherited multipliers

The current area path multiplies several row/control/write-driver blocks by four
and the first SA-output mux by nine. The public source does not. These factors
already exist in the local HV snapshot and the current repository's initial
February 2026 implementation; they were not introduced by the recent analytical
circuit work. For the shared MRAM fixture the write-driver subtotal alone changes
from **1653.696 to 6623.872 µm²**, while the single row-driver area is identical.
These factors need an explicit circuit replication/floorplan basis before the
current area estimate can be called more accurate. They are not silently removed
in this comparison to make area agree.

### The old executable has an uninitialized leakage input

Valgrind finds an uninitialized `Row[i].CellPort.leak` branch in the committed
FeFET run, as well as leaked allocations. The example optimizes leakage, so this
is a reproducibility defect. In the tested native and Valgrind executions the
selected latency and energy agreed; it does not explain away their discrepancy
with the paper. The captured Valgrind log is included in each completed audit
when Valgrind is installed.

## What this changes about the next work

1. Use this pinned executable comparison as the software baseline. Keep the
   paper's reported predictions as a separate historical claim.
2. Keep direct two-FeFET and diode-MRAM circuit fixtures explicit. Finish their
   control/load assumptions before interpreting gaps as validated errors.
3. Audit the inherited area multipliers against actual repeated circuitry;
   the MRAM area discrepancy is a code/accounting question as well as a scope
   question. Do not tune the cell footprint to hide it.
4. Complete the PCM CSRSS/reference and SAPIENS serial-distance models. The
   matched 512×64 PCM local-subarray run here is only a software diagnostic,
   and no SAPIENS whole-query estimate is fabricated from its clock period.

## Reproduce the comparison

```sh
make compare-legacy-evacam LEGACY_REPO=/path/to/public/EvaCAM/checkout
```

The checkout needs the pinned commit in its Git object database; it is never
modified. The target builds the current probe and runs its focused tests. The
runner creates a fresh evidence directory containing exported inputs, source
and input hashes, old build logs, component-level results and the isolated diode
ablation. It preserves prior runs. The normal production executable and equations
are unchanged by this audit.
