# Separate 3D NAND CAM model: implementation and validation plan

Status: initial SLC linear RC backend implemented, 2026-09-24. Stages A–C
and the corresponding linear-model integration in E are implemented.
The inputs use a normal `*.memory_device.yaml` file with `type: NAND3D`;
there is no separate `.spec` input. See the [implemented interface](nand-3d-tcam.md)
and [numerical verification](validation/nand3d-rc.md).

The sections below retain the broader design roadmap. Characterized
nonlinear response (D), a complete paper reproduction (F), and multilevel
or reliability models (G) remain future work. The shipped inputs are
synthetic and do not establish publication-level calibration.

Expose the new family as a distinct memory-device type, `type: NAND3D`,
alongside the existing `type: SLCNAND`. Build its separate physical model
behind the shared NAND CAM interface.
Reuse logical search, result plumbing, and block scheduling where their
assumptions hold. Separate array geometry, device response, search protocol,
and sensing so that each paper can be represented explicitly. Implement a
linear transient baseline first, then characterize nonlinear device behavior
before claiming agreement with transistor-level paper results.

## Intended memory-device interface

The memory-device file selects the physical memory family. The existing
`SLCNAND` input keeps its existing model. `NAND3D` selects the new geometry,
device specification, validation rules and backend. Its `storage_mode` is
initially restricted to `SLC`; future multilevel operation belongs to the
same 3D family but requires its own implemented encoding and characterization.

Proposed schema fragment, not a complete runnable input and not yet accepted
by the current parser:

```yaml
schema: memory_device
name: NAND3D_TCAM_example
type: NAND3D
nand3d:
  storage_mode: SLC
  model: transient_rc
  calibration_status: synthetic
  stack:
    storage_layers: 64
    dummy_layers: 0
  # Required geometry, device response, biases, protocol and peripherals follow.
```

`type` chooses the memory family; `model` chooses a supported electrical
solver within that family. In the cell file, `cam_type: TCAM` and
`topology: nand_string` continue to describe the CAM function and connection
topology. A single unambiguous type determines the physical structure; no
second planar/3D flag may contradict it.

Use a separate typed `Nand3dDeviceSpec` and `nand3d` YAML section. Reject
`type: NAND3D` with only old `nand` parameters, and reject `nand3d` on unrelated
device types. Require explicitly populated 3D fields rather than inheriting
planar cell area or wire dimensions. New examples belong under
`config/NAND_3D_TCAM/`, with independent memory-device, cell and architecture
files following the canonical configuration structure.

## 1. Establish the reference experiments

For every target figure or table, record the device structure, layer count,
strings and selected string groups, threshold states, geometry, parasitics,
voltage waveforms, temperature, sensing circuit, and workload. Record exactly
what the reported energy, latency, and area include. Each extracted parameter
needs its source, units, applicable range, and uncertainty. Unknown values
remain unknown; synthetic demonstrations remain labeled synthetic.

Use separate fixture files for different papers and different experiments in
the same paper. A device model from one technology is not automatically the
device used in another paper, even when both are described as 3D NAND.

| Reference | Initial use | Requirements for numerical comparison |
| --- | --- | --- |
| [TCAM-SSD (2024), §3.2](https://arxiv.org/html/2403.06938v1#S3.SS2) | SLC paired-cell encoding, masks, validity, capacity and parallel search | Match physical organization and search scheduling. Its 25 µs search access time is an assumed architectural input, not circuit characterization. |
| [Yang et al. (2023)](https://link.springer.com/article/10.1007/s11432-021-3502-4) | Candidate binary CAM waveform, energy, layer/parasitic sweep and density target | Obtain full circuit parameters and measurement boundaries. Separate its binary and four-level experiments. The public headline alone cannot determine a configuration. |
| [Tseng et al. (IEDM 2020)](https://resourcecenter.eds.ieee.org/conferences/iedm/edsiedm20201400) | Candidate search-periphery and reliability target | Obtain full paper/deck and identify the exact circuit and measurement conditions. |
| [Wang et al. (2020)](https://doi.org/10.1109/LED.2020.3004989) | Later multilevel complementary-search/current target | Add multilevel states, query-bias mapping, and characterized nonlinear currents. Current two-state fixed resistances are insufficient. |

There is also an openly listed device-model starting point: Sahay and
Strukov, [A Behavioral Compact Model of 3D NAND Flash Memory](https://arxiv.org/abs/1812.00290),
published in IEEE Electron Device Letters in 2019. Its abstract describes
vertical charge-trap geometry, parasitic coupling, and parameter extraction.
Review its full equations, data availability, and implementation terms before
selecting it. It is a candidate independent device benchmark, not an assumed
replacement for the devices used by the CAM papers.

**Completion gate:** one target experiment has an explicit comparison contract
and enough data to evaluate its claimed outputs. This gate controls paper
reproduction, not development of the uncalibrated backend.

## 2. Separate the existing model's responsibilities

`NandCamModel::Initialize` currently combines geometry, planar area, wire
lengths, first-moment sensing, peripheral costs, and operation scheduling.
Its planar array height also sets bitline wire resistance/capacitance. Merely
adding a layer count would retain incorrect dependencies.

Introduce a backend boundary for block metrics and pattern evaluation. Keep
the current `analytical_rc` model available, with its current behavior and
configuration defaults. The new `NAND3D` type selects its structure and
backend; its solver can first use a characterized linear network, then a
nonlinear device table or compact model. Shared code is an internal reuse
decision, not a replacement for the separate memory type in user inputs.

Suggested responsibilities, with final class names chosen during implementation:

| Component | Responsibility |
| --- | --- |
| `NandGeometry` / `Nand3dLayout` | Physical inventory, coordinates, footprints, routing dimensions, selected groups |
| `NandSearchEncoding` | Stored states, query biases, validity policy and reserved-cell placement |
| `NandStringCircuit` | Nodes, devices, connections, parasitic and driver loads |
| `NandTransientSolver` | Voltage/current trajectories, convergence and state handoff between phases |
| `NandDeviceCharacteristics` | Linear parameters or characterized current/charge response |
| NAND block backend | Schedule, sensing, area and energy aggregation exposed through shared results |

Do not duplicate the whole bank implementation. Remove assumptions from the
shared path that would incorrectly force every backend to use exactly two
validity cells, voltage-mode discharge sensing, one string per bitline, or
one exponential time constant.

**Completion gate:** existing NAND tests and example results remain equivalent;
the same CLI/Python entry points can select an explicitly identified backend.

## 3. Define physical 3D geometry and capacity

Add `NAND3D` to `MemCellType` and the YAML type mapping, then a typed 3D
geometry/device specification and strict YAML parsing for:

- Active storage wordlines, dummy wordlines, source/drain select devices,
  deck boundaries where needed, and reserved CAM cells.
- String-hole rows and columns, lateral pitches, vertical layer pitch,
  channel dimensions, and applicable device structure.
- Bitlines, strings sharing each bitline, string-select groups, shared
  wordlines, and the number of groups that can be sensed simultaneously.
- Staircase/contact dimensions, wordline partitions and source-line topology.
- Peripheral placement and physical CMOS process, distinct from vertical
  flash dimensions.

Represent storage layers, physical cells, physical program pages, encoded
symbols, and logical key bits separately. Do not infer storage layers by
dividing byte capacities once multilevel states or multiple string groups are
present. A logical page count in TLC is not inherently a wordline count.

For a simple SLC paired-cell profile, validate
`2 * key_bits + reserved_cells + padding_cells = active_storage_cells_per_string`.
Make reserved-cell count and placement part of the profile. A string contributes
one key, but string-select sharing may require multiple search rounds. Selected
page width need not equal the total number of physical strings in the block.

**Completion gate:** hand-calculated fixtures verify physical/logical capacity,
select-group concurrency, cell counts, wordline counts, and reserved-cell
policies; overflow and inconsistent configurations are rejected.

## 4. Implement the 3D footprint and interconnect model

Calculate the array-core footprint from the lateral hole grid and required
spacing. Account separately for staircases, contacts, isolation, decoders,
high-voltage drivers, page buffers and sense amplifiers. Report core area and
complete block footprint separately, plus vertical stack height.

Use explicit floorplan rules for peripherals beside or beneath the array.
If circuits occupy an overlapping tier, calculate the footprint union and
additional routing/keep-out space rather than adding all tier areas. Do not
obtain a 3D footprint by dividing planar area by the number of layers.

Generate electrical lengths from this geometry: lateral bitline/source-line
routing, wordline sheets and stair contacts, vertical channel segments, and
select-group connections. Increasing layer count at fixed hole count adds
vertical devices and stack height; the model must capture any resulting
staircase growth and parasitic changes explicitly. Bank routing must consume
the resulting lateral footprint, not the vertical stack height.

**Completion gate:** independent geometry fixtures check core/peripheral
footprints and interconnect paths. Paper density comparisons use the same
logical bit definition, peripheral scope, and reference technology.

## 5. Build and characterize the electrical network

Start with explicit per-layer read/pass/off resistances and a capacitance
network. Include source/drain selects, bitline load, channel nodes, wordline
driver impedance, and relevant gate-to-channel and neighboring-node coupling.
Allow layer-dependent values. An explicit extracted network is a valid input;
do not invent parasitics from geometry when material/process data are missing.

The first linear version is a numerical baseline. For paper-level device
comparisons, supply a characterized response such as
`I = f(Vgs, Vds, stored_state, layer, temperature)` and terminal charge or
capacitance data. Local channel potentials must determine each device's bias.
One constant `Rpass` cannot reproduce arbitrary bias sweeps or multilevel
threshold windows. Interpolation must preserve physical behavior and reject
unsupported extrapolation; charge models must conserve terminal charge.

Prefer a characterized compact model or tables extracted from it before
developing new semiconductor equations. A table derived only from isolated
cells must also be checked against connected strings and relevant coupling.
[Published BSIM-CMG work](https://pure.uos.ac.kr/en/publications/bsim-cmg-modeling-for-3d-nand-cell-with-macaroni-channel/)
describes this progression from unit-cell extraction to string-position and
neighboring-wordline effects.

**Completion gate:** reproduce device and string current curves across the
target operating region. Separate parameter-fitting points from independent
validation points; record errors and validity limits.

## 6. Simulate the complete search transient

Use a nodal transient solution instead of replacing the string with
`Vpre * exp(-t/tau)`. The general circuit equation is
`dQ(v, inputs)/dt + I(v, inputs) = 0`; for a fixed linear circuit it reduces
to the familiar capacitance/conductance system. Driving wordline transitions
must contribute through the coupling network.

Implement a robust implicit solver with explicit time-step/error tolerances,
convergence diagnostics and handling of floating or zero-capacitance nodes.
Start with the linear case and cross-check it against the existing independent
`scripts/nand_rc_reference.py`, including nonuniform initial states. Then add
the selected nonlinear device representation and appropriate sparse solves.

Represent the protocol explicitly: reset, pass-bias setup, precharge, query
transition, evaluation, sense/latch and recovery. Carry node states between
phases and sense rounds. Support the target paper's current-mode or
voltage-mode sensing with its actual reference and offset definition. Report
intrinsic string response separately from complete search latency.

Select a sensing instant/reference using actual trajectories. Sweep mismatch
positions and relevant state/query patterns; do not reuse the exponential
model's worst-pattern ordering without proving it for the new model. Label
sampled extrema honestly when exhaustive bounds are unavailable.

**Completion gate:** single/two-node analytic limits, full-RC regression,
time-step convergence, and an independently run SPICE comparison agree within
declared numerical tolerances. The existing 512-wordline case must no longer
be falsely accepted at 100 mV margin under its original controlled conditions.
If a new schedule or reference makes it pass, report that changed condition.

Agreement with SPICE on the same declared circuit verifies the solver;
comparison with independent device/paper data establishes physical accuracy.

## 7. Integrate energy and realistic shared resources

Integrate energy at the declared supply rails over the complete protocol:
`E = sum_over_rails integral(Vrail * Irail dt)`. Account for initial and final
stored charge when checking conservation. Include steady conduction where
the chosen sensing circuit supplies it; avoid counting that energy again as
a separate capacitor charging term.

Keep cell/interconnect, driver, sensing, page-buffer and charge-pump costs
identifiable. If only output-rail energy is modeled, apply an explicitly
characterized efficiency model once. If input-supply power is directly
integrated, do not apply that conversion again. Shared wordline charging is
charged per physical event, not once per string. Include inactive-string
loading when those devices share driven rails.

Schedule string-select groups, sense-amplifier sharing and block rounds
separately. Reuse `NandCamBank` scheduling only after its assumptions match
the selected physical organization. Report energy per whole query, per
searched logical bit, and per physical cell with explicit denominators.

**Completion gate:** energy conservation and analytic capacitor limits pass;
sharing and multiplexing fixtures count each event once; reported paper
comparisons use identical rails, integration windows, and peripheral scope.

## 8. Add multilevel search only when required by the target

The first complete 3D backend should support SLC exact/wildcard CAM. Wang's
multilevel experiments and Yang's four-level case require an additional
encoding/device milestone: multiple threshold states, complementary symbol
mapping, query voltages, and the publication's sensing criterion.

Do not enable the existing `MLCNAND` enum globally before those semantics
exist. Introduce a clear symbol-level API or translation layer, validate all
symbol/query combinations, and count logical key bits independently of
physical cells and threshold states. Add variation and correlation when the
chosen reliability comparison requires them. Detailed program/erase,
retention, endurance, ECC and SSD firmware are separate scopes unless a
specific selected validation experiment depends on them.

**Completion gate:** complete supported-symbol truth tables and matched
current/window sweeps pass with characterized data. A multilevel result may
not inherit the SLC backend's validation status automatically.

## 9. Integrate reproducible outputs and paper fixtures

Extend metrics and the shared DTO rather than writing independent CLI and
Python calculations. Report structure, geometry, solver/device-model version,
parameter provenance, waveform measurement definitions, residual/convergence
status, and calibrated operating domain. Only expose exponential time-constant
fields for backends where those fields have a defined meaning. Report current
and voltage sensing margins with distinct units and definitions.

Add an illustrative `config/NAND_3D_TCAM/` configuration. Keep publication
fixtures separate and label partial or synthetic parameter sets. Store
reference points, extraction uncertainty, source figure/table, and numerical
conditions under `docs/validation/`; produce comparison artifacts under
`results/` using the existing audit approach. No paper fixture may claim
reproduction by merely being named after a paper.

Use an explicit progression of evidence per metric:
`synthetic -> numerically verified -> characterized -> independently validated`.
This should be report metadata tied to test evidence and operating conditions,
not an automatic consequence of a user-supplied YAML label.

## 10. Implement in reviewable stages

| Stage | Concrete deliverable | Main repository touch points |
| --- | --- | --- |
| A | Distinct `NAND3D` type, backend boundary and legacy compatibility | `include/circuit/typedef.h`, `src/input/YamlNodeHelpers.cpp`, `include/model/NandCamModel.h`, `src/model/NandCamModel.cpp`, `src/cam/CAM_SubArray.cpp`, `src/factories/`, device printing and type checks |
| B | `Nand3dMemoryDevice`, `nand3d` schema, capacity and layout | `include/technology/Nand3dMemoryDevice.h`, `src/input/MemoryDeviceYamlLoader.cpp`, `src/input/PhysicalDomainValidators.cpp`, `src/config/InputRuleValidator.cpp`, `src/config/EvaCamConfigValidator.cpp`, `src/model/Nand3dCamModel.cpp` |
| C | Linear transient solver and explicit protocol | New circuit/solver files, existing RC reference and regression fixtures |
| D | Characterized nonlinear device response and sensing | Device-characteristics import, solver integration, SPICE/reference comparisons |
| E | Shared-resource scheduling, energy and integration | `src/model/NandCamBank.cpp`, `src/app/EvaCamResultExtractor.cpp`, output/Python DTOs, canonical configuration |
| F | One complete SLC paper reproduction | Reference metadata, parameter fixture, validation script, plots and error report |
| G | Multilevel/reliability targets | Explicit new encoding/state support and separate validation fixtures |

Start collecting reference data before A and continue throughout B–E. Avoid
blocking useful implementation on publisher access, while keeping F gated on
sufficient independent evidence. If the CAM papers remain inaccessible,
first reproduce an accessible device/string benchmark and state that narrower
achievement clearly.

Add focused tests for every new production method, register targets in the
Makefile and `.github/workflows/cpp-tests.yml`, update the unit-test inventory,
and exercise CLI/Python parity and thread safety. Run targeted tests first,
then relevant NAND and existing CAM regressions; use the default and a
3D-NAND-specific Valgrind run for runtime/ownership changes. Document final
schema, output units, unsupported cases and validity domains.

Set numerical and physical acceptance criteria before fitting. Numerical
tolerances should be tighter than reference-data uncertainty. Use absolute
voltage/current error near zero, relative error where meaningful, and report
waveform, timing, energy and area separately. A chosen 5–10% engineering
target is not a published accuracy guarantee and must be justified for each
metric; one matched headline number is insufficient.

The first validated release requires one fully specified SLC 3D string/block
experiment with functional agreement, converged transient behavior, matched
measurement scope, and independent reference points. It does not require
implementing every paper or every NAND storage operation at once.
