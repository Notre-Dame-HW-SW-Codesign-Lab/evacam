# NAND flash TCAM

EvaCAM supports an explicit, exploratory NAND-string TCAM model using
two-state flash devices and exact search with stored and query wildcards.
The shipped values are synthetic. This is an analytical first-moment RC
approximation, not a calibrated prediction of a commercial NAND device or SSD.

Run the complete example from the repository root:

```sh
make -j
./EvaCAM config/NAND_TCAM/NAND_TCAM.config.yaml
```

The implementation uses `type: SLCNAND` together with `cam_type: TCAM`,
`topology: nand_string`, and an explicit `nand` electrical model. An SLC NAND
device cannot silently use the ordinary parallel-matchline CAM equations.

## Encoding and geometry

Each logical entry occupies one NAND string. Two flash transistors encode
each key symbol. With threshold states `L` and `H`, the convention is:

| Symbol | Stored threshold pair | Query voltage pair |
| --- | --- | --- |
| 0 | L, H | read, pass |
| 1 | H, L | pass, read |
| X | L, L | pass, pass |

The configured biases must satisfy `VtL < Vread < VtH < Vpass`. A matching
string conducts and discharges its bitline. A mismatch blocks that discharge.
In the matcher API, `-1` denotes a stored wildcard or masked query bit.

One pair per string is reserved for validity, with a fixed query of zero.
`valid=false` represents a **programmed invalid marker** whose read-biased
device blocks conduction. An erased L,L pair conducts: blocks must be
initialized with invalid markers before use. The stateless matcher takes
validity from its caller and cannot infer occupancy or simulate that
initialization sequence. `evaluate_vector` assumes a valid stored entry;
`evaluate_nand(stored, query, valid=False)` evaluates the programmed invalid
case. A valid all-X key intentionally matches every query.

`organization.subarray.dimensions: [S, W]` means `S` physical strings and a
logical key width of `W` bits. One subarray is one erase block. The architecture
must also define `flash.page_size` and `flash.block_size`, with capacity units
such as `8B` and `544B`. Runtime converts these to physical bit counts. For
this SLC full-page organization:

```text
physical_page_bits = S
data_wordlines_per_string = physical_block_bits / S = L
physical_flash_cells_per_block = S * L
maximum_key_width = floor((L - 2) / 2)
padding_wordlines = L - 2 * W - 2
logical_capacity_bits = allocated_entries * W
```

Here `L` includes key, validity, and padding flash cells. Two select devices
are additional transistors, not additional stored bits. Padding devices are
pass biased and contribute area, capacitance, and energy. NAND logical
capacity is reported separately from its encoded physical flash capacity.

The example has 64 strings, 32-bit keys, and 68 physical data wordlines per
string: 64 key wordlines, two validity wordlines, and two padding wordlines.
It therefore represents 2,048 logical key bits and 4,352 physical flash cells.
`memory.physical_capacity`, when supplied, remains allocated **logical** key
capacity; it is not a substitute for `flash.block_size`.

All outer organization dimensions must be fixed. The implementation bounds
each block to 1,048,576 strings and 4,096 wordlines, and the bank to 65,536
physical blocks. `organization.mux.sense_amp`
is a positive divisor of the string count and gives the number of sequential
sense rounds per block. `output_level1` and `output_level2` must both be one.
The active mat/subarray counts schedule block rounds for a whole-array query;
every allocated block is searched. This differs from silently applying
ordinary CAM word partitioning to NAND strings.
The generic subarray dimension tester and in-memory dimension overrides are
not supported for this topology; update the fixed architecture and physical
flash quantities together.

## Electrical and sensing model

`nand.resistance` supplies the resistance of a conducting read-biased device,
a pass-biased device, a blocking device, and each select device. Both threshold
states share the pass resistance in this backend. The model requires
`Roff > Rread_on >= Rpass > 0`; it does not infer flash resistance from generic
CMOS drive-current tables.

The string is a series ladder. Each internal-node capacitance is weighted by
the resistance between that node and the source. Bitline loading includes the
configured capacitance and the local wire model. This gives a position-sensitive
first-moment time constant `tau`, followed by a single-exponential approximation:

```text
Vbitline(t) = Vprecharge * exp(-t / tau)
```

This approximation assumes precharged internal nodes: every precharge round
applies the pass bias to all wordlines before applying the query biases. The
supplied precharge duration is assumed sufficient to establish this initial
state; the model does not verify that transient. It omits nonlinear channel
current, charge redistribution beyond the first moment, threshold
distributions, temperature dependence of flash resistances, retention,
read/program disturb, endurance, and device history. The area model is planar:
`layout.area` is the footprint of **one flash transistor**, and the model adds
select devices and supplied peripheral areas. It does not predict 3D NAND
vertical geometry or layer-dependent behavior.

The model derives a slowest-match bound and fastest-mismatch bound across
supported symbols and mismatch positions, including invalid strings. These
bounds are conservative **within the single-exponential approximation**; they
do not bound the full RC transient or a physical NAND circuit. Both are
evaluated at `nand.sensing.decision_time`. A zero
`reference_voltage` chooses the midpoint of those two voltages; a positive
value supplies an explicit reference. Available margin is:

```text
min(reference - slowest_match_voltage,
    fastest_mismatch_voltage - reference) - offset
```

This is the smaller **per-class distance from the reference**, after comparator
offset allowance. It is not the full match/mismatch voltage gap. The result
must meet `nand.sensing.min_margin`; infeasible candidates are rejected.
Negative margins remain signed. The matcher reports ideal logical `hit`
separately from actual pattern voltage, reference margin, and
`sense_margin_pass`.

An independent nodal RC reference in `scripts/nand_rc_reference.py` demonstrates
this distinction. With the shipped synthetic resistances and capacitances,
512 data wordlines, a 255-bit key, no wire parasitics, and every node initially
at 0.8 V, a matching string reaches 0.582868 V at 50 ns; the exponential predicts
0.538209 V. At the model's 0.668113 V reference and 10 mV offset, this tested
match has 75.245 mV margin, although the model reports 119.903 mV. A required
100 mV margin therefore passes the approximation but fails this linear-network
check. This is a counterexample for one valid pattern, not a proof of the exact
worst-case pattern. Even with an ideal bitline precharge driver and an open
source select, the source-end node of that 512-wordline string reaches only
0.012972 V in the configured 5 ns precharge interval. The uniform initial state
used above is a separate, more favorable assumption.

The numerical reference and its circuit-identity tests check the approximation;
they do not establish device calibration. Paper-level validation, including
architecture and measurement-boundary differences, is documented separately in
[`validation/nand-yang-2023.md`](validation/nand-yang-2023.md).

## Cost accounting

The model takes explicit area, latency, dynamic energy, and leakage for
wordline drivers, sense amplifiers, and page buffers. These are supplied model
parameters, not automatically characterized high-voltage transistor circuits.
The fixed setup/query/recovery costs can include rail startup and local query
or output handling. No priority encoder or top-k result sorter is modeled.

Local block search latency includes shared query/setup time, pass/query
wordline settling and select-gate switching per round, and a final wordline
reset. Each sense round also includes precharge, evaluation, sensing,
page-buffer, and recovery time. The output is one match bit per physical entry.
A whole-query bank latency includes the necessary sequential block rounds and
bank routing.

Dynamic energy includes explicit peripheral costs and supply-side capacitive
charging costs. All data wordlines charge to `Vpass` initially. Query-selected
wordlines then drop to `Vread` without energy recovery. Between rounds they
must be restored to `Vpass`. For `r` sense rounds and `n` selected wordlines
(one validity wordline plus the number of unmasked query bits), wordline energy
is:

```text
Cwordline * [L * Vpass^2 + (r - 1) * n * Vpass * (Vpass - Vread)] / efficiency
```

Bitline/internal-node and select-gate charging costs use complete supply-side
`C * V^2 / efficiency` cycles. The model does not also add a string
`I * V * t` term for the same discharge. Full-run search energy uses the
conservative unmasked-query bound; per-query matcher energy uses the actual
mask. Setup is shared within a block, while reset/top-up, precharge, and
sensing costs follow the modeled switching events and rounds. Whole-query
energy includes all allocated blocks and query/result routing. Supplied
peripheral energies must exclude the capacitance terms already calculated by
the model.

`nand.program_page` and `nand.erase_block` supply complete local operation
latency and energy for one physical page and one physical block respectively.
The caller must choose values appropriate for the configured geometry,
including rail generation and verify loops. Bank operation totals add one
addressed-route overhead: programming transfers a physical-page payload and
address, while erase transfers only a block command/address. They do not multiply these costs by the number of
blocks searched. Programming is not a whole-logical-entry update: an entry
spans multiple wordlines/pages. No automatic read-modify-write, erase
amortization, garbage collection, ECC, or SSD firmware is inferred. Changing
an invalid marker into a valid pair may require erase under actual NAND
programming constraints; the stateless evaluator does not schedule that update.

Conventional NAND memory reads are unavailable. `WriteLatency`,
`WriteDynamicEnergy`, and `WriteEDP` optimize the bank's one-page program
operation. Search objectives, area, and leakage are also supported. Read
objectives, full/deep exploration, design constraints, and the legacy
read-oriented exploration CSV are rejected for NAND.

## Results and provenance

NAND console output identifies the model, calibration status, source,
logical/physical geometry, complete query costs, physical page/block operation
costs, and sensing bounds. YAML and Python are produced from one shared
structured result. NAND YAML uses numeric SI values with unit suffixes in field
names, rather than unit-formatted strings. For example:

```yaml
metadata:
  model_identifier: "evacam-nand-tcam-v1"
  topology: "nand_string"
  model_backend: "analytical_rc"
  read_metrics: "unavailable"
geometry:
  strings_per_block: 64
  data_wordlines_per_string: 68
  logical_capacity_bits: 2048
  physical_cell_count: 4352
summary:
  timing:
    search_latency_s: ...
    subarray_search_latency_s: ...
    program_page_latency_s: ...
    bank_program_page_latency_s: ...
    erase_block_latency_s: ...
    bank_erase_block_latency_s: ...
  energy:
    search_dynamic_j: ...
    program_page_dynamic_j: ...
    erase_block_dynamic_j: ...
```

`metadata.model_source` preserves the supplied provenance. The accepted
calibration labels are `synthetic`, `uncalibrated`, and `calibrated`; a
`calibrated` label is a declaration supplied with the parameters, not a
calibration performed or verified by EvaCAM. The shipped fixture
must remain labeled as uncalibrated/synthetic; passing analytical tests is not
physical calibration. `breakdown.block_search_*` contains local block costs,
while `breakdown.search_*` separates all-block local work from bank routing.
Geometry reports both `sense_rounds_per_block` and `block_rounds` so that a
single electrical decision time cannot be mistaken for a whole-query latency.

In Python, the corresponding field is a dotted key such as
`design.summary["timing.search_latency_s"]`; string provenance is in
`design.metadata`. Read metrics are absent rather than fabricated as zero.

The earlier [implementation plan](nand-flash-cam-plan.md) remains the historical
investigation and roadmap. This implementation supplies the analytical
infrastructure and synthetic regression fixture. Independent SPICE/device
correlation, response-table backends, segmented keys, variation, multilevel
storage, approximate search, and validated 3D NAND remain future work.
