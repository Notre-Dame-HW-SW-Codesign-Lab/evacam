# 3D NAND TCAM

EvaCAM's `NAND3D` backend models an SLC NAND TCAM with a vertical string
layout, sequential select groups, and an analytical first-moment RC model.
It uses the ordinary split YAML input format. The shipped electrical and
peripheral parameters are synthetic; numerical circuit tests do not establish
device calibration or reproduce published commercial 3D NAND performance.
The [linear RC verification report](validation/nand3d-rc.md) retains the
independent reference solver and the known limits of the analytical approximation.

EvaCAM is derived from NVSim. Departures in electrical, geometry, peripheral,
and operation-cost modeling require the evidence recorded in the
[NVSim justification audit](validation/nand-nvsim-justification.md).
The [matched NVSim validation](validation/nand-nvsim.md) records the executed
baseline, circuit comparisons, energy reconciliation, and remaining physical
validation limits.
The added detail is not, by itself, evidence of improved physical accuracy.

```sh
make -j
./EvaCAM config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml
```

The canonical example uses `NAND_3D_TCAM.config.yaml`, an architecture YAML,
a cell YAML, and `NAND_3D_TCAM.memory_device.yaml`. There is no separate
`.spec` input format. The cell selects `cam_type: TCAM` and
`topology: nand_string`; its memory device selects `type: NAND3D`,
`nand3d.storage_mode: SLC`, and `nand3d.model: analytical_rc`.

The normal model uses closed-form RC estimates. To migrate an earlier transient
configuration, set `model: analytical_rc` and remove `solver` and
`precharge_driver_resistance` from `nand3d`; those fields are now rejected rather
than silently ignored. Keep the stack, layout, electrical and operation inputs.
The nodal solver remains a separate verification tool.

## Geometry and physical operation units

Each vertical string holds one logical key. Complementary threshold pairs
encode `0=(L,H)`, `1=(H,L)`, and `X=(L,L)`. Queries use read/pass, pass/read,
and pass/pass biases respectively. A reserved validity pair distinguishes
initialized valid and programmed-invalid entries. An erased L,L validity pair
conducts and must not be treated as an invalid marker. The caller must track
occupancy and initialize invalid markers before search.

`nand3d.stack.storage_layers` counts flash storage layers, including encoded
keys, the validity pair, and any padding. Dummy layers and source/drain select
devices are modeled separately from storage bits. Logical keys must satisfy:

```text
2 * (key_width + 1) <= storage_layers
padding_storage_layers = storage_layers - 2 * (key_width + 1)
```

`nand3d.layout.string_rows` is the number of sequential select groups.
`string_columns` is the number of strings within each group. One selected
group contributes one SLC physical page. Consequently:

```text
strings_per_block = string_rows * string_columns
physical_page_bits = string_columns
physical_block_bits = strings_per_block * storage_layers
logical_key_capacity_bits = strings_per_block * key_width * block_count
```

The canonical example has four select groups of 16 strings, 68 storage
layers, two dummy layers, and 32-bit keys. It contains 64 logical entries,
2,048 logical key bits, and 4,352 flash storage cells per block. A physical
page has 16 bits (2 B), and a physical erase block has 4,352 bits (544 B).
These physical capacities do not include dummy or select transistors.

The architecture's fixed `organization.subarray.dimensions` remains
`[total_strings_in_block, logical_key_bits]`. `flash.page_size` and
`flash.block_size` must agree with the group and storage-layer definitions.
They are not derived from logical key capacity. Sense amplifiers are shared
within a select group: `sense_amplifiers = string_columns / mux_sense_amp`
and `sense_rounds_per_block = string_rows * mux_sense_amp`. A full bank query
also includes any block rounds required by its active organization counts.

Physical dimensions come from hole pitches, layer pitch, staircase step and
contact dimensions, isolation width, and peripheral placement. The result
reports the string-grid footprint, staircase and isolation areas, vertical
stack height, lateral bitline length, supplied peripheral area, and occupied
footprint. A cell's `layout.cell_process_node` supplies the peripheral process
context; planar `layout.area` and `layout.aspect_ratio` are rejected for
`NAND3D`. Storage-layer count does not multiply a planar transistor area to
produce the 3D footprint.

The layout approximation uses these explicit dimensions, with
`gate_layers = storage_layers + dummy_layers`:

```text
core_width = string_columns * hole_pitch_x
core_height = string_rows * hole_pitch_y
staircase_width = (gate_layers + 2) * staircase_step_width
inside_width = core_width + staircase_width
inside_height = max(core_height, staircase_contact_length)
array_footprint = (inside_width + 2 * isolation_width)
                * (inside_height + 2 * isolation_width)
vertical_stack_height = (gate_layers + 2) * layer_pitch
```

The two additional layers represent the select devices. `beside` placement
adds the supplied peripheral area to the array footprint; `under_array`
uses the larger of the two areas. This is an explicit rectangular placement
estimate, not a routed layout or a process design-rule check. The lateral
bitline length is `core_height`; vertical stack height is reported separately
and is not used as lateral CMOS wire length.

## Electrical calculation

The `nand3d` mapping supplies read-on, pass, off, and select resistances;
distributed internal, source, bitline, gate, and select capacitances; bias
voltages; and NAND peripheral costs. Missing RC, wordline-driver, and sense
values can use the same warned [CMOS technology fallbacks](nand-tcam.md#technology-library-fallbacks)
as planar NAND. These estimates do not characterize the vertical NAND channel;
all 3D dimensions remain explicit. Both threshold states share the
pass resistance in this initial SLC backend. Threshold values validate
read/pass bias ordering. They do not supply nonlinear channel I-V curves.

The source-to-drain circuit contains the source select resistor and source
capacitance, storage/dummy devices and their internal-node capacitances, drain
select, and the lateral bitline pi section. The model uses the same first-moment
approach as the existing planar analytical backend:

```text
tau = sum(Ci * resistance_from_ground_to_node_i)
Vbitline(t) = Vprecharge * exp(-t / tau)
```

The bitline wire's distributed capacitance sees half its wire resistance.
Dummy devices, padding, the validity pair and the selected complementary-pair
members all contribute to this sum. A pattern evaluation requires one pass over
the string and an exponential; it performs no transient time stepping.

Each round assumes sufficient all-pass precharge to establish a uniform full
rail on the internal nodes and bitline, followed by query evaluation. Recovery
is assumed to reset the string before the next round. Supplied precharge and
recovery durations contribute to operation latency, but their electrical
sufficiency is not verified. Short durations do not reduce the assumed initial
voltage or CV² charging energy. These are the same explicit initialization
assumptions used by the planar analytical path.

The reported conductance is the **DC conductance of the configured resistor
network**. It is not a measured transistor transfer curve or the instantaneous
bitline discharge current. Resistances, capacitances, and peripheral costs
remain supplied parameters. The model does not infer mobility, tunneling,
threshold distributions, channel self-boosting, nonlinear coupling, or
temperature-dependent flash I-V characteristics.

## Sensing and energy scopes

The configured decision time is measured within an evaluation phase. A whole
query includes query setup, electrical precharge/evaluation/recovery phases,
driver and sensing overheads, group/mux rounds, and bank routing. These are
separate quantities in the result.

Sensing uses a common reference and comparator offset. The model constructs the
slowest matching first moment by choosing the source-side read-biased member
of each pair. The fastest nonmatch has one blocking device, with other key
queries masked; the drain-most key mismatch and programmed-invalid marker are
both considered. Positive capacitance weights make these bounds cover the
supported patterns **within the single-exponential approximation**. They do
not bound the full distributed transient or an actual NAND circuit.

A zero `reference_voltage` selects the midpoint of the match and mismatch
voltages. Available margin is the smaller signed distance from the reference,
minus the offset allowance. The run rejects insufficient analytical margin.
Per-query APIs preserve ideal ternary `hit` independently of the calculated
voltage and `sense_margin_pass`.

Bitline/internal-node supply energy is the fully charged CV² sum, multiplied
by strings and sense-mux rounds and divided by the supplied efficiency. Source,
internal and bitline capacitances, including lateral wire loading, are counted.
There is no additional string-conduction term that would count the same
capacitor discharge twice. Wordline/select switching and supplied peripheral
overheads are accounted for separately. Those overheads must exclude charging
energy already modeled. Query masks affect gate/driver energy; full reset and
recharge make the bitline term independent of the stored pattern.

Page program and
block erase retain complete supplied operation
costs: one **selected-group page** and one **physical erase block**,
respectively. Bank totals add the addressed route. The implementation does
not infer a logical-entry rewrite, erase amortization, garbage collection,
verify algorithm, or SSD controller.

## Results and API

The same result extraction feeds console, YAML, and Python. NAND3D metadata
includes:

```yaml
model_identifier: evacam-nand3d-tcam-v1
model_backend: analytical_rc
array_layout: vertical_3d
calibration_status: synthetic
```

The result also preserves the supplied source, analytical delay model,
approximation-bound scope, assumed full-precharge convention and peripheral placement.
Numerical verification and physical calibration are separate claims: the
synthetic fixture remains synthetic after every software or circuit test.
A user-supplied calibration label alone does not provide correlation data.

`geometry` contains logical capacity, physical storage cells, page/block
sizes, group/sense-round counts, and 3D dimensions. `summary.timing` includes
`slowest_match_time_constant_s` and `fastest_mismatch_time_constant_s`.
Transient-solver metadata and diagnostics are absent. Program/erase
and full-query costs use the same SI naming conventions as
[the NAND result contract](nand-tcam.md#results-and-provenance).
Conventional read metrics remain unavailable. Stack, grid, staircase and
peripheral geometry fields describe one block; `physical_cell_count` and
allocated capacities cover all blocks.

```python
import evacam_py

path = "config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml"
run = evacam_py.run(path)
design = run.best_results["SearchLatency"]
print(design.metadata["array_layout"])
print(design.geometry["vertical_stack_height_m"])
print(design.summary["timing.search_latency_s"])

matcher = evacam_py.EvaCAMMatch(path)
key = [0] * matcher.word_width()
decision = matcher.evaluate_nand(key, key, valid=True)
print(decision.hit, decision.matchline_voltage, decision.sense_margin_pass)
```

## Supported release and remaining work

This release supports fixed-geometry, nominal SLC exact/wildcard search with
internal sensing and either bank routing topology. Search, area, leakage, and
physical-page program objectives are available. The shared NAND capability
guards reject conventional-read objectives, generic CAM peripherals,
full/deep exploration, generic dimension overrides, and variation.
The analytical backend caps sense mux at 256 and combined storage/dummy layers
at 4096. It has no integration-step, convergence or transient-sampling budget.

MLC/TLC/QLC storage, approximate/top-k search, segmented keys, nonlinear
device characterization, retention/disturb/endurance, ECC, and controller
simulation are outside this backend. The legacy `SLCNAND` analytical path
remains available with its existing result contract and limitations.

The earlier [linear-RC comparison](validation/nand-yang-2023.md) demonstrated
limits of the one-pole approximation and assumed precharge. Those limitations
remain after restoring the analytical model; analytical sense-margin acceptance
is not a guarantee about the full distributed circuit. The independent nodal
solver retains these counterexamples as verification evidence. The
[matched NVSim comparison](validation/nand-nvsim.md) found small uncorrected
delay errors in its tested circuits and did not establish a physical accuracy
gain that justified making transient integration the normal estimator.

## Experimental nonlinear DC work

A separate [cell-current and DC-string evaluator](validation/nand-nonlinear-dc.md)
now provides the first numerical prototype for the nonlinear roadmap. Its
parameters are uncalibrated, and it is accessible through the C++ electrical API
and a test probe. It does not introduce a new CAM configuration mode or change
this backend's `analytical_rc` model. Further nonlinear integration is outside
the restored analytical scope.
