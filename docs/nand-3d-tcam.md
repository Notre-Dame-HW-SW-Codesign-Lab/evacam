# 3D NAND TCAM

EvaCAM's `NAND3D` backend models an SLC NAND TCAM with a vertical string
layout, sequential select groups, and a numerical linear RC transient.
It uses the ordinary split YAML input format. The shipped electrical and
peripheral parameters are synthetic; numerical circuit tests do not establish
device calibration or reproduce published commercial 3D NAND performance.
The [linear RC verification report](validation/nand3d-rc.md) records the
numerical checks and their physical limits.

```sh
make -j
./EvaCAM config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml
```

The canonical example uses `NAND_3D_TCAM.config.yaml`, an architecture YAML,
a cell YAML, and `NAND_3D_TCAM.memory_device.yaml`. There is no separate
`.spec` input format. The cell selects `cam_type: TCAM` and
`topology: nand_string`; its memory device selects `type: NAND3D`,
`nand3d.storage_mode: SLC`, and `nand3d.model: transient_rc`.

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
voltages; and explicit NAND peripheral costs. Both threshold states share the
pass resistance in this initial SLC backend. Threshold values validate
read/pass bias ordering. They do not supply nonlinear channel I-V curves.

The transient solver integrates the linear nodal circuit. The physical
orientation is the source select, source capacitance, series flash/dummy
devices with internal-node capacitances, drain select, and loaded bitline.
Wire parasitics follow the configured lateral geometry. The model operates
three electrical phases:

1. Precharge applies pass biases and charges the bitline through the configured
   `precharge_driver_resistance`, with the source select open.
2. Evaluation applies the query and grounds the selected string's source while
   its precharged bitline floats. Matching strings discharge more rapidly.
3. Recovery restores all-pass biases and grounds the source through its select
   resistance and the bitline through the finite precharge-driver resistance.
   The final maximum node residual must not exceed the solver tolerance;
   otherwise the operation fails with a recovery diagnostic.

Internal nodes start from reset, then carry their actual computed voltages
between phases and sense-multiplexing rounds. Separate select groups represent
different strings and start from their own reset states. A finite precharge
time therefore affects the decision waveform and energy; the solver does not
assume that every internal node instantaneously reaches the rail.

`solver.max_step`, `solver.tolerance`, and `solver.max_steps` control the
numerical integration. The backend records its solver name and diagnostic
values in the result. Failure to converge or complete the declared schedule
is reported explicitly. Single-exponential time constants are not emitted for
this backend. `solver.tolerance` and the reported
`transient_absolute_tolerance_v` are local voltage-error controls, not proven
bounds on accumulated endpoint error. Tolerance-convergence checks and the
independent numerical references establish the tested endpoint accuracy.

The reported conductance is the **DC conductance of the configured resistor
network**. It is not a measured transistor transfer curve or the instantaneous
bitline discharge current. Resistances, capacitances, and peripheral costs
remain supplied parameters. The solver does not infer mobility, tunneling,
threshold distributions, channel self-boosting, nonlinear coupling, or
temperature-dependent flash I-V characteristics.

## Sensing and energy scopes

The configured decision time is measured within an evaluation phase. A whole
query includes query setup, electrical precharge/evaluation/recovery phases,
driver and sensing overheads, group/mux rounds, and bank routing. These are
separate quantities in the result.

Sensing uses a common reference and comparator offset. Available margin is
the smaller signed distance from the reference to the sampled match and
mismatch signals, minus the offset allowance. The result labels these checks
as `sensing_bound: sampled_patterns`; they are not an exhaustive guarantee
over every possible key, query, device corner, or operation history.
Per-query APIs return ideal ternary `hit` independently of the modeled
voltage and `sense_margin_pass`. For a matching pattern the reported voltage
is the highest across its mux rounds; for a nonmatching pattern it is the
lowest, so each uses its least favorable sampled decision.

The run samples five matching patterns (all-zero, all-one, all-wildcard, and
both alternating polarities), single mismatches at every key position in both
query polarities with masked and unmasked backgrounds, and three programmed
invalid patterns (all-zero, all-one, and all-wildcard). It uses their least
favorable match/mismatch voltages to choose an automatic midpoint reference
when `reference_voltage` is zero. The sampled-pattern count is reported.

Supply energy for modeled RC phases follows the transient charge delivered by
the precharge supply. Wordline/select switching and explicit peripheral
overheads are accounted for separately. The supplied device/peripheral
parameters must not include a second copy of the capacitance energy already
modeled. Run energy uses the maximum sampled per-string precharge supply
energy, multiplied by all strings, plus the unmasked-query gate/driver cost.
The model's pattern evaluator reports a block cost assuming that every string
has the representative supplied pattern. The public `evaluate_nand` API
scales that cost across the bank and adds routing. It does not infer the
contents or energy distribution of an actual stored array. Page program and
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
model_backend: transient_rc
array_layout: vertical_3d
calibration_status: synthetic
```

The result also preserves the supplied source, solver name, sensing-pattern
scope, electrical initial-condition convention, and peripheral placement.
Numerical verification and physical calibration are separate claims: the
synthetic fixture remains synthetic after every software or circuit test.
A user-supplied calibration label alone does not provide correlation data.

`geometry` contains logical capacity, physical storage cells, page/block
sizes, group/sense-round counts, and 3D dimensions. `summary.diagnostics`
contains finite-precharge and numerical-solver observations. Program/erase
and full-query costs use the same SI naming conventions as
[the NAND result contract](nand-tcam.md#results-and-provenance).
Conventional read metrics and one-pole time constants are absent.
Stack, grid, staircase, and peripheral geometry fields describe one block;
`physical_cell_count` and allocated capacities cover all blocks. Diagnostics
describe the representative string simulations. In particular,
`precharge_min_voltage_v`, `precharge_max_voltage_v`, and
`precharge_source_energy_j_per_string` describe the first precharge from reset;
the search-energy total includes all mux rounds and strings.

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
The transient backend caps sense mux at 256, combined storage/dummy layers
at 4096, and initialization sampling work at 200 million node-step attempts.
The per-phase solver limit can reject a run before that work cap. Large or
stiff circuits may therefore require a smaller geometry or adjusted numerical
settings even when their physical dimensions satisfy the schema.

MLC/TLC/QLC storage, approximate/top-k search, segmented keys, nonlinear
device characterization, retention/disturb/endurance, ECC, and controller
simulation are outside this backend. The legacy `SLCNAND` analytical path
remains available with its existing result contract and limitations.

The earlier [linear-RC comparison](validation/nand-yang-2023.md) demonstrated
limitations of the legacy one-pole approximation and assumed precharge. The
new transient implementation addresses those numerical modeling assumptions;
it does not turn that audit into a reproduction of Yang et al. or validate
the supplied synthetic values against silicon. Independent measured/SPICE
waveforms, circuit parameters, complete operating conditions, and holdout
comparisons are still needed for device-level predictive claims.
