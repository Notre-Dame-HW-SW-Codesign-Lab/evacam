# Analytical CAM decision timing and search scheduling

For the subsequent original-DATE-2022 reconstruction, full-paper evidence and
additional circuit/accounting corrections, see [the original validation audit](original-evacam-validation.md).
The numerical tables below describe the earlier checkpoint.

Updated 2026-10-07, following the [sensing/topology repairs](named-cam-sensing-topology.md).
EvaCAM remains an analytical estimator. The new runtime path evaluates closed-form
RC responses and a bounded scalar root, without transient integration, device
physics, SPICE, or fitted whole-macro delays.

## Changes and results

The model can now time a sensing decision instead of always using a 50% Horowitz
event. Search control, precharge overlap, calculated driver load and recovery are
explicit configuration choices. Two reference fixtures opt in; other configs
retain legacy defaults, including MCAM, variation and NAND backends.

Before means the immediately preceding sensing/topology version, not the original
historical executable. Gap is `100 * (EvaCAM / paper - 1)`. All seven references
remain partial: unmatched operating corners, circuits and timing boundaries
prevent these raw differences from being accuracy scores.

| Configuration | Paper ns | Before ns | Updated reference ns | Before gap | Updated gap |
| --- | ---: | ---: | ---: | ---: | ---: |
| FeFET-2Fe1T-DATE21 | 0.2528 | 0.490475 | 0.463648 | +94.0% | +83.4% |
| MRAM-4T2R-VLSIC12 | 2.50 | 0.885443 | 0.885443 | -64.6% | -64.6% |
| MRAM-6T2R-VLSIC11 | 0.29 | 1.038 | 1.038 | +257.9% | +257.9% |
| PCM-2T2R-JSSC14 | 1.90 | 1.278 | 1.278 | -32.7% | -32.7% |
| ReRAM-2.5T1R-ISSCC16 | 1.00 | 2.156 | 2.156 | +115.6% | +115.6% |
| ReRAM-3T1R-ISSCC15 | 0.96 | 1.348 | 0.385813 | +40.4% | -59.8% |
| ReRAM-4T2R-VLSIC14 | 1.20 | 2.211 | 2.211 | +84.2% | +84.2% |

ReRAM15 becomes more optimistic. Controlled runs with the same executable,
geometry and device data explain the change:

| Selected change | FeFET ns | ReRAM15 ns |
| --- | ---: | ---: |
| Legacy decision, schedule and amplifier proxy | 0.490475 | 1.348 |
| Decision only | 0.490997 | 0.979409 |
| Schedule and calculated driver load only | 0.463512 | 1.293 |
| Both changes, inherited amplifier proxy | 0.463648 | 0.916134 |
| Updated reference, including voltage-sense proxy | 0.463648 | 0.385813 |

The inherited ReRAM current converter contributes 0.540515 ns; the generic
voltage amplifier contributes 0.010515 ns. The inherited proxy yields a -4.6%
total gap after the timing changes, but near agreement cannot validate the
absent control/replica path. No resistance, width, delay or recovery constant was
adjusted to fit a headline. FeFET's fixed-voltage decision alone does not improve
agreement; its improvement comes mainly from scheduling and removing the
historical 1.6 driver-load multiplier.

## Source basis and remaining inputs

The [DATE21 paper](https://past.date-conference.com/proceedings-archive/2021/pdf/1478.pdf)
uses an inverter sense amplifier. The reference selects a fixed-voltage event,
with an explicit unverified 0.5 V trip point and generic amplifier sizing.
Array size, effective device strengths and operating conditions still need
independent evidence.

The [ISSCC15 paper, 17.5](https://picture.iczhiku.com/resource/eetop/syIFhqJDqDgdSnvB.pdf)
describes voltage separation, dual replica rows that track the slower search
polarity, and a 1 V supply. It excludes external test-path delay. This supports
voltage sensing in the reference. Missing numerical inputs include replica and
voltage-divider control costs, reference offset, MRS/X leakage and internal-node
capacitance. The inherited 25 mV separation is an assumption. The generic 90 nm
current tables use 1.2 V; changing Vdd alone would make resistance inconsistent
with those tables.

Broadcast control and overlapped precharge are architectural assumptions in
both references. Zero recovery means uncharacterized recovery contributes zero;
it does not establish sustainable throughput. The [manifest](named-cam.reference.yaml)
records matched fields and unresolved inputs. The other five references need
circuit-specific decision/control evidence before opting in: diode/keeper
timing, self-referenced sensing and specialized amplifiers should not acquire
generic constants merely to reduce their gaps.

Missing circuit costs can still use analytical equivalent circuits or
independently characterized scalar peripherals. Better source data is needed
to choose those equivalents and parameters credibly; this does not require a
physics simulator.

## Equations and timing boundaries

Let `a` be the reciprocal of the input slope and `tau` the existing effective
matchline RC time constant. Conductance rises linearly over activation time `a`:

```text
u(t) = t^2/(2a)   for 0 <= t < a
       t - a/2   for t >= a
V(t) = Vpre * exp(-u(t)/tau)
```

For `a = 0`, exposure `u(t) = t`. Threshold sensing uses
`u = tau_miss * ln(Vpre / Vthreshold)` and analytically inverts `u(t)`.
It checks all-match/one-miss separation at that event. Differential sensing
finds the first rising crossing of `Vmatch(t) - Vmiss(t) = required_margin`.
The root is bounded by the exact peak of the difference of exponentials;
unreachable margins reject the candidate. Dimensionless exposure and `expm1`
avoid cancellation for small differences.

This is an ideal differential reference, without replica mismatch or sensing
offset. The actual calculated separation reaches the amplifier calculation.
Query matching uses a common one-miss completion event for all rows, including
all-match and multiple-miss cases.

The optional phase schedule is:

```text
query_ready = input + control
evaluation_start = max(query_ready, precharge)  # overlap_input
                 = query_ready + precharge    # serial
result_ready = evaluation_start + control_node + matchline + output
cycle = result_ready + recovery
```

Broadcast bypasses the row-address merge. Only enabled search muxes contribute
column-select control latency; unmultiplexed searches do not wait for write
address decoding. Physical driver load uses terminal and wire capacitance from
the cell ports. Shared driver sizing can also change read/write estimates.

Bank timing repeats all sense groups and comparison steps, inserts recovery
between them, and includes existing routing costs. Later sense groups reuse
the latched query but cannot subtract input time hidden behind precharge.
Cycle time includes final recovery and assumes serialized operation. Generic
energy retains its conservative activity budget; timing verification does not
establish data-dependent energy or area accuracy.

## Validation and reproduction

```sh
make validate-analytical-cam
make validate-named-cam
```

The first command runs focused checks and writes isolated decision, schedule
and amplifier-proxy comparisons under `output/validation/analytical-cam/`.
The second runs all seven fixtures under `output/validation/paper-configs/`.
Each run preserves generated configs, results, logs and hashes of the binary,
runner and inputs. An absent result cannot reuse an earlier success. Geometry,
topology, decision model, schedule and feasible sensing are checked before
reporting a comparison.

Equation tests cover exact step-response and quadratic differential limits,
time/voltage scaling, unreachable margins and invalid domains. Independent
test-only RK4 integration checks 54 combinations of RC ratio, time scale,
activation and decision mode. Maximum relative crossing discrepancy was
`5.27937e-10`. This verifies the equations, not agreement with silicon. No
numerical integration is linked into the production model.

Integration tests cover both routing modes, common decision time, margin
sensitivity, mux/bit-serial recovery, broadcast/decoded control, precharge
overlap, invalid configurations and YAML diagnostics. The full unit suite,
generated v2 configs, Monte Carlo/corner tests, decoder/routing regressions,
Python run/match bindings and `make test` passed; Valgrind reported zero errors.
Focused parser/subarray tests additionally cover resetting defaults, slowest
driver selection and cached-query timing.
