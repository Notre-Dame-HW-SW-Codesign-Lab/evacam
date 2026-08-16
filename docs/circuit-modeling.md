# Circuit Modeling

This document explains how EvaCAM models CAM circuitry once the YAML inputs have been parsed. It focuses on the electrical and timing model used to estimate latency, dynamic energy, leakage, and sensing margin.

## Scope

This document covers the current circuit model for:

- technology and cell parameter loading
- wire and device loading on CAM rows and columns
- matchline delay and sensing
- search/read latency composition
- dynamic read/search/write energy
- leakage estimation
- the built-in resistance-variation model

## Modeling Boundary

EvaCAM models the array as a composition of:

- a cell/device description from the cell config
- a peripheral CMOS technology model from built-in technology tables
- explicit row and column wires
- attached peripheral blocks such as decoders, muxes, prechargers, and sense amplifiers

At runtime, the main CAM-specific electrical work is performed in [src/cam/CAM_SubArray.cpp](../src/cam/CAM_SubArray.cpp), with support from:

- [src/cam/CAM_Line.cpp](../src/cam/CAM_Line.cpp) for line RC and line loading
- [src/technology/MemCell.cpp](../src/technology/MemCell.cpp) for cell-level read/write quantities
- [src/circuit/formula.cpp](../src/circuit/formula.cpp) for transistor, wire, capacitance, and delay helper equations
- [src/cam/CAM_SenseAmp.cpp](../src/cam/CAM_SenseAmp.cpp) for sense amplifier abstraction

The implementation is centered on TCAM-style search with matchline discharge. Other modes exist in the input schema, but some are incomplete or explicitly unsupported. Those limitations are listed near the end of this document.

## Inputs That Drive the Circuit Model

The circuit model is driven by the architecture config, cell config, technology
config, and nested device/sensing configs referenced by the run config.

Important top-level inputs include:

- `design.system_process_node`
- `design.device_roadmap`
- `design.temperature`
- `design.search_function`
- run-level `cell`
- `routing` and `wires` settings used to build wire objects
- architecture-level `sensing` file reference
- sensing-level `sense_amplifier` file reference
- peripheral optimization choices such as buffer, row-driver, and priority-encoder targets
- array organization values such as bank, mat, and subarray geometry

Important cell-YAML inputs include:

- `cam_type`
- `layout.area`
- `ports`
- `memory_device`
- cell-level `access_device` and per-port CMOS fields

Important device inputs, split between the referenced memory-device YAML file
and the cell file's inline `access_device` mapping, include:

- `type`
- `voltage_drop`
- `resistance.on`
- `resistance.off`
- `read.mode`
- `read.voltage`
- `read.current`
- `read.power`
- `read.energy`
- `read.min_sense_voltage`
- `write.set.*`
- `write.reset.*`
- `variation.*`
- `ports.row`
- `ports.column`

The current repository-level input guide is in [input-files.md](input-files.md). That file explains where fields live; this document explains how they affect the circuit equations.

## Bank Routing Models

EvaCAM supports two bank-to-mat routing topologies. `H-tree` recursively splits
address and data wires through balanced horizontal and vertical branches, so mats
at a given tree depth see the same modeled route length. `non_h_tree` gives each
mat row a direct route from the bank interface. Those routes have different
lengths; the farthest route determines latency, active routes contribute
read/write dynamic energy, and every physical route contributes repeater
leakage. A CAM search activates every physical mat and subarray in the selected
bank, independent of the read/write `active` organization fields, so search
dynamic energy includes every local subarray and every bank-to-mat search route.

Both topologies keep matchline sensing inside each mat. Bank-level external
matchline sensing is rejected because its global mux and sense-amplifier path is
inherited from conventional-memory modeling and has not been made CAM-correct.
The direct model treats columns in one mat row as parallel route groups and does
not separately model horizontal placement within that row.

## Technology and Cell Loading

Technology loading is handled by [src/config/TechnologyLoader.cpp](../src/config/TechnologyLoader.cpp).

### Peripheral CMOS Technology

EvaCAM first loads a CMOS technology model for the requested process node. The technology tables are discrete, so intermediate process nodes are produced by interpolation between supported anchor nodes. The resulting `Technology` object supplies quantities such as:

- `vdd`
- threshold-voltage-related parameters
- effective on/off currents
- transconductance-related data
- wire and device capacitance coefficients
- feature-size-dependent geometry values

These parameters feed the transistor sizing, gate/drain capacitance, on-resistance, leakage, and delay helper formulas.

### Cell Model

The cell config is parsed into a `MemCell` object. The fields most relevant to the circuit model are:

- low-resistance and high-resistance states
- read voltage/current/energy/power settings
- minimum sensible voltage
- write set/reset voltages, currents, pulse widths, and optional explicit energies
- access-device voltage drop and leakage
- CAM port topology
- variation enablement and sigma values

The cell model is not just a metadata container. It also computes fallback read power and write energy when the cell config does not provide explicit energy values.

### FeFET Technology Selection

A separate FeFET technology object may also be loaded when needed. This matters because some capacitance and geometry calculations switch to FeFET-specific technology parameters for FeFET-based cells instead of reusing the main CMOS technology object.

## Line and Device Abstraction

Rows and columns are modeled by `CAM_Line`. Each line starts with wire resistance and wire capacitance based on line length and the selected wire model, then adds the device loading implied by the corresponding port description in the cell config.

### Line Geometry

For a subarray:

- row length is proportional to the number of columns times cell width
- column length is proportional to the number of rows times cell height

Optional stitching overhead is added to row length when the cell description requests it.

### Base Wire RC

Each line begins with:

- `R_wire = length * wire_resistance_per_unit`
- `C_wire = length * wire_capacitance_per_unit`

If the port requests a non-default wire width, the code recomputes the wire resistance and capacitance from explicit geometry-based helper formulas instead of using the default per-unit values.

### Added Device Capacitance

The port connectivity declared in the cell config determines what extra capacitance is attached to a row or column:

- gate-connected ports add gate capacitance
- drain- or source-connected ports add drain capacitance
- diode-style connections add both gate and drain contributions
- FeFET cells use the FeFET technology object for these calculations when applicable

The `ports` section matters electrically because it determines both the topology and the parasitic load seen by the peripheral circuits and the matchline.

### Current Capability and Minimum Mux Width

For each column, EvaCAM also estimates a worst-case current based on the line type:

- write-related lines use set/reset voltage or current information
- matchlines use read-mode assumptions
- leakage from unselected devices may be added in some cases

That current estimate is converted into a minimum mux width by dividing by the technology’s NMOS on-current. If a required mux width exceeds the configured implementation limit, the configuration is rejected as invalid.

## Subarray-Level Circuit Composition

`CAM_SubArray::Initialize()` builds the full modeled datapath for the subarray. The structure includes:

- input buffer and level shifters
- optional input encoder
- row decode and row drivers
- precharger
- column decoder merge logic
- column muxes
- sense amplifier and sense-amplifier muxing
- optional output accumulator and priority encoder
- optional write drivers
- optional output buffer

Each of these blocks has its own area, latency, and energy model. The CAM subarray model composes them with the line and cell model to obtain subarray-level metrics.

## Matchline and Cell Electrical Model

The core CAM-specific behavior is the matchline discharge model.

### Precharge

The precharge voltage is currently assumed to be:

`Vprecharge = Vdd`

This is a simplifying assumption. Other precharge schemes are possible, but the present model uses full-Vdd precharge for generality and simplicity.

### Extracting the Effective Matchline Cell Path

During initialization, EvaCAM scans the column ports to find the matchline-bearing column. From the port connectivity and cell type it derives:

- access resistance contribution
- match-device or memory-device resistance contribution
- effective low-resistance-state path
- effective high-resistance-state path
- effective cell access capacitance

The exact composition depends on whether the matchline path uses:

- CMOS access
- diode-like access
- no explicit access device
- a nonvolatile discharge path

The implementation stores both nominal values and, when variation is enabled, sampled values.

### Nominal Resistive States

The subarray model keeps separate quantities for:

- `resMemCellOn`
- `resMemCellOff`
- `resCellAccess`
- `resMatchTran`
- `matchlineWireRes`

These quantities represent the effective resistance seen by the matchline under different cell states and are used to build the RC model for both mismatch and all-match conditions.

## Search and Read Latency Model

`CAM_SubArray::CalculateLatency()` computes the read/search timing.

### Peripheral Timing Before Matchline Sensing

Latency is first computed for the non-matchline path:

- input buffer
- input level shifter
- output level shifter
- input encoder
- row-decode merge NAND
- row drivers
- precharger
- column decode merge NAND
- sense-amp mux control stages

The subarray then computes:

- the worst row-driver latency
- the worst column-decoder-side latency
- `decoderLatency = max(row-side path, column-side path)`

### One-Mismatch Matchline Delay

For TCAM, the primary matchline delay is estimated for the one-mismatch case. The code forms an effective resistance for a word with one discharging cell and the remaining cells in the opposite state:

`R_total_cell = (R_on * R_off) / ((N - 1) * R_on + R_off)`

where `N` is the bit-serial width used for the comparison.

The effective cell capacitance is:

`C_total_cell = C_cell_access * N`

The matchline time constant is then modeled as:

`tau = R_total_cell * (C_total_cell + C_mux + C_extra_ML + C_precharge + C_sense)`
`    + R_wire_ML * (C_mux + C_extra_ML + C_precharge + C_sense + C_wire_ML / 2)`

This is a distributed-RC-style approximation that combines:

- cell path resistance
- matchline wire resistance
- matchline wire capacitance
- mux loading
- explicit extra matchline capacitance from peripheral config
- precharger output capacitance
- sense-amplifier input load

The delay is then passed through the Horowitz delay model using a transistor transconductance-derived `beta` term and the selected row-driver output ramp.

### All-Match Sensing Instant and Sense Margin

EvaCAM separately estimates the all-match case. In that case the effective matchline resistance is:

`R_all_match = R_off / N`

The corresponding all-match time constant is:

`tau_all_match = R_all_match * (C_wire_ML + C_mux)`
`              + R_wire_ML * (C_mux + C_wire_ML / 2)`

The code then defines a reference sensing delay:

`referDelay = tau_all_match * ln(2)`

At that instant, the expected all-match voltage drop is used to compute:

`senseMargin = Vprecharge / 2 - Vmatch_drop`

If `senseMargin < senseVoltage`, the configuration is marked invalid because the model predicts that the matchline is too long or too heavily loaded to sense reliably.

This sense-margin check is one of the main validity gates in the current model.

### MCAM Matchline Delay

For MCAM, EvaCAM sorts the configured `mcam.resistance_state` values from HRS
to LRS. The sorted HRS entry is distance `0`, representing equality. Nonzero
absolute symbol distances select the progressively lower-resistance entries.
For each candidate one-mismatch distance, that mismatch branch is placed in
parallel with the HRS branches of the other cells in the word.

That selected state drives:

- `resTotalCell`
- matchline `tau`
- `matchlineDelay`
- total search/read latency composition

This makes the nominal MCAM latency path worst-case across configured nonzero
MCAM distances. Matchline precharge uses the per-distance
`mcam.ml_precharge_voltage` value when supplied and technology `Vdd` otherwise.
`read.min_sense_voltage` remains the acceptance threshold; a provisional value
of `0V` disables that rejection gate, while sense-amplifier latency still uses
the positive modeled boundary margin rather than dividing by zero.

Exact MCAM vector evaluation is query-aware. Inputs are integers from `0` to
`num_resistance_state - 1`; a hit requires element-wise equality across the
whole word. Each cell contributes the conductance selected by
`abs(stored_symbol - query_symbol)`, and the row resistance is the parallel
reduction of those cell branches. This produces query-specific matchline delay,
sense margin, and discharge energy. Best-match and threshold MCAM evaluation
remain unimplemented.

### Total Search and Read Latency

Once the matchline and sensing stages are available, the total search latency is formed by summing:

- input buffer latency
- the slower of precharge latency and decode-plus-encoding latency
- matchline delay
- column mux latency
- sense-amplifier latency
- sense-amp mux latencies
- output accumulator latency
- priority encoder latency
- output buffer latency
- input/output level shifter latency

The read latency uses nearly the same composition. In the current implementation, search and read are largely the same subarray path, with small differences in which shifter terms are included.

## Exact Match, Approximate Match, and Per-Query Evaluation

The timing model above produces a nominal subarray-level search latency. EvaCAM also contains two more query-oriented modeling paths.

### Per-Query Binary Match Evaluation

`EvaluateBinaryMatch()` evaluates a concrete stored word and query word.

For a given mismatch count:

- the effective matchline resistance is recomputed from the mismatch count
- a mismatch-specific matchline delay is computed, with exact-hit latency reported at the one-mismatch sensing delay
- the total search latency is adjusted by replacing the nominal matchline delay with the returned per-query matchline delay
- the effective sense margin is reported as the voltage separation from the previous mismatch-count case at the one-mismatch sensing instant
- search energy is scaled upward with mismatch ratio as a simple approximation of deeper matchline discharge

This path is intentionally simpler than a full transient simulation. It gives a query-dependent timing and energy estimate using the same RC abstractions as the nominal model, and is intended as a mode to pass values to other tools.

### Threshold Match Evaluation

`EvaCAM_Match::evaluate_threshold()` adds an explicit TCAM threshold hit rule on top of the per-query binary match model. The caller supplies an inclusive `maxMismatches` value:

- `maxMismatches = 0` is exact-match behavior
- `maxMismatches = 2` accepts `0`, `1`, and `2` mismatches
- `maxMismatches = word_width` accepts all mismatch counts

The physical timing, energy, matchline delay, and sense-margin fields still come from the modeled result for the actual mismatch count. The threshold rule changes only the returned `hit` field.

EvaCAM validates that the threshold can be separated by the modeled sense margin. For inclusive `maxMismatches = N`, the relevant decision boundary is between `N` mismatches and `N + 1` mismatches. If `N < word_width`, the modeled boundary sense margin must meet the cell's minimum sense voltage; otherwise threshold evaluation raises an error. If `N == word_width`, there is no miss boundary to sense.

For `search_function: TH`, plain vector evaluation still requires an explicit threshold. Use `evaluate_threshold(..., maxMismatches)` rather than `evaluate_vector(...)` so the hit rule is unambiguous.

### Best Match Evaluation

TCAM best match is an array-level operation. `EvaCAM_Match::evaluate_array()` evaluates either stored rows plus one query or a list of known mismatch counts. For `search_function: BE`, EvaCAM finds the minimum mismatch count across the supplied rows and marks every row with that count as a hit. All tied best rows are hits; no priority selector or multiple-match resolver is applied.

As with threshold evaluation, timing, energy, matchline delay, and sense-margin fields come from the modeled result for each row's actual mismatch count. Best-match evaluation changes only the returned `hit` field.

EvaCAM validates best-match detectability with the modeled sense margin. If the best mismatch count is outside the maximum detectable mismatch distance, evaluation raises an error. If there is a distinct non-best class, EvaCAM also checks the conservative adjacent boundary between `bestMismatches` and `bestMismatches + 1`. If that boundary cannot be sensed, evaluation raises an error rather than reporting a best match the model cannot distinguish.

Plain scalar vector evaluation is not supported for `search_function: BE` because a single row cannot establish which row is best.

### Approximate Search Modes

For BE and TH search modes, the code sweeps mismatch counts and compares adjacent mismatch-delay cases. If the delay separation is greater than the configured matchline sensing margin, the mismatch count is considered distinguishable. The results are stored in lookup-like arrays for approximate-search latency estimation.

This is not a full analog classifier model. It is a timing-separation heuristic layered on the same matchline RC equations.

## Sense-Amplifier Modeling

Sense-amplifier behavior is wrapped by [src/cam/CAM_SenseAmp.cpp](../src/cam/CAM_SenseAmp.cpp).

### Supported Sense Modes

The currently implemented built-in paths are:

- NVSim-style voltage sense
- NVSim-style current sense
- discharge sense

The following modes exist in the type system but are not modeled as complete production paths:

- self-clock sense
- dual-threshold sense

Those modes currently emit warnings or throw errors depending on how they are used.

### YAML-Backed Sense Amplifier

EvaCAM loads the sense-amplifier model referenced by the v2 sensing file and uses its:

- area
- input capacitance
- read/write latency
- dynamic energy
- leakage

Legacy `custom_sense_amplifier_file` inputs remain compatibility-only migration fixtures. New active configs should use `*.sensing.yaml` plus `config/lib/sense_amp/*.sense_amp.yaml`.

### Role in the Matchline Model

The sense amplifier affects:

- the load capacitance on the matchline
- the post-matchline latency
- the search/read dynamic energy

The minimum sensible voltage from the cell config is also passed into the sense-amp path as the sense threshold used by the current high-level model.

## Dynamic Energy Model

`CAM_SubArray::CalculatePower()` computes the dynamic energy terms.

### Search Dynamic Energy

The search energy is built from a matchline-centric term plus peripheral activity.

The exact matchline energy equation depends on cell type and sense mode.

#### Discharge-Based Model

For discharge sensing, the model uses:

- matchline wire capacitance
- mux capacitance
- total cell capacitance
- the difference between precharge voltage squared and read-voltage squared

This approximates the energy removed from the matchline/load capacitance during discharge.

#### SRAM and FeFET Search

For SRAM and FeFETRAM search, the model uses a simpler full-swing charging expression based on:

- matchline wire capacitance
- mux capacitance
- total cell capacitance
- `Vprecharge^2`

#### NVM Search

For MRAM, PCRAM, and memristor cells:

- current-sensing uses an ICCAD 2009-style precharge/discharge approximation with `vpreMin` and `vpreMax`
- voltage-sensing uses a swing-based expression between `Vprecharge` and the modeled on-cell voltage

### Cell Read Energy

Cell read energy is determined in priority order:

1. use explicit `read.energy` if provided
2. otherwise use `read.power * active_read_time` if provided
3. otherwise derive from the device model

The derived path depends on memory type and read mode:

- SRAM uses capacitance-based charging
- current-in voltage sensing uses `Vdd * Iread * t`
- voltage-divider sensing uses a derived maximum matchline current
- current sensing uses `(Vread - Vdrop) / Ron`

This cell-read energy is then scaled by the number of columns sharing the active sense path.

### Searchline and Row-Driver Energy

The model separately accumulates energy to drive the search values on the row-side lines. For ordinary binary ports, it scales the row-driver dynamic energy by the configured search voltages for logic `0` and logic `1`, then averages the two squared-voltage terms.

For MCAM, `mcam.searchline_voltage` is required. EvaCAM sorts the configured
values from low to high. For state `s`, the two FeFET gates receive `V[s]` and
`V[N - 1 - s]`, respectively, matching the paper's analog-inverse scheme. The
common analog center is derived from half the reversed-pair sum, and validation
requires every reversed pair to produce that same center. Nominal exploration
averages the squared voltage across all states for each searchline. Exact match
evaluation instead uses the concrete query symbols, so row-driver energy
changes with the query. The matchline energy is the modeled capacitive energy
lost between precharge and the voltage reached at the boundary sensing time.

### Read Dynamic Energy Aggregation

The total read energy is formed by adding:

- search dynamic energy
- input buffer energy
- input encoder energy
- cell read energy
- column decode and precharge energy
- column mux energy
- sense-amp energy
- output accumulation and priority-encoding energy
- output buffer energy
- level shifter energy

### Write Energy

Write energy is modeled separately for reset and set operations.

At the cell level:

- explicit set/reset energies are honored when present
- otherwise `MemCell::CalculateWriteEnergy()` derives them from voltage/current/pulse settings

At the interconnect level, EvaCAM adds charging energy for the driven lines based on:

- access capacitance
- bitline capacitance
- mux capacitance
- set/reset voltages from the cell port description

For PCRAM, the result is divided by a conservative shaper-efficiency constant. For MRAM, memristor, and FeFETRAM, the result is divided by a more aggressive shaper-efficiency constant.

The final write dynamic energy also includes:

- row-driver write energy
- mux write energy
- optional write-driver energy
- column-merge and sense-amp-mux-stage energy
- input level-shifter write energy

The implementation keeps separate set and reset energy paths, then reports the maximum as the main write energy.

## Leakage Model

Leakage is modeled differently for SRAM and nonvolatile cells.

### SRAM Leakage

For SRAM, EvaCAM estimates leakage from:

- the two inverter devices in the SRAM cell
- the access transistors
- the match transistor

The model uses gate-leakage helper formulas parameterized by transistor widths, temperature, and the technology object.

### NVM Leakage

For NVM-style cells, EvaCAM does not directly model resistive-state standby leakage through the memory element itself in detail. Instead, it mainly counts gate leakage from the transistors identified through the row and column port descriptions, plus the match transistor for CMOS-access cells.

### Peripheral Leakage

Peripheral leakage contributions are then added from instantiated blocks such as:

- buffers
- precharger
- mux stages
- sense amplifier
- output accumulator
- priority encoder
- write drivers

As with other metrics, this is a compact analytical estimate, not a transistor-level standby simulation.

## Variation Model

Variation is controlled by the presence of a memory-device `variation` section
and loaded through [src/config/TechnologyLoader.cpp](../src/config/TechnologyLoader.cpp).

### Supported Modes

The current implementation supports:

- variation omitted
- `single_point`
- `monte_carlo`
- `corner`

If the memory-device config omits `variation`, the model uses nominal
resistances everywhere.

If `single_point` is selected, EvaCAM draws one deterministic sample from the configured seeded RNG streams and overwrites the nominal timing and search-energy results with that single sampled point.

If `monte_carlo` is selected, EvaCAM draws `N` samples and computes summary statistics. `variation.monte_carlo_granularity: cell` samples each modeled matchline cell branch independently and reduces those sampled branches into the aggregate matchline resistance. `variation.monte_carlo_granularity: effective` samples one effective on/off resistance pair per Monte Carlo sample and applies it across the modeled matchline cells.

If `corner` is selected, EvaCAM enumerates independent deterministic low/high corners for each active effective resistance component. The `*_max_var` inputs are raw fractional bounds, so `5%` applies `nominal * 0.95` and `nominal * 1.05`. Corner mode ignores user-provided `seed` and `samples`; the sample count is derived from the number of active corner dimensions.

### Sampled Quantities

The current sampled quantities are the effective memory-device resistance in the on-state path and the effective memory-device resistance in the off-state path. Access-device resistance, wire resistance, and peripheral circuit parameters remain nominal.

For Monte Carlo `cell` granularity, the number of sampled matchline branches is `CAM_opt.BitSerialWidth`. The representative one-mismatch path uses one sampled on branch and the remaining sampled off branches. The all-match/reference path reuses the same sampled off-branch population from that Monte Carlo sample. For Monte Carlo `effective` granularity and corner mode, EvaCAM varies the effective on/off resistance values directly before the existing aggregate matchline equations consume them.

### Sampling Method

Each resistance category uses a stable deterministic RNG stream derived from:

- the base variation seed
- the sample index
- a stream offset specific to that resistance category

The sampled values are produced by `VariationSampler::SampleResistance()`. The user interface describes this as a bounded-Gaussian resistance-variation model.

Corner mode does not use the RNG. It applies deterministic low/high scaling to the active memory-device resistance components in this bit order: on-state resistance, off-state resistance.

### Effect on Timing and Energy

For each sampled point, the model recomputes:

- effective on/off resistances
- matchline delay
- search latency
- all-match sensing delay
- exact match sense margin
- search dynamic energy

If a sampled exact match sense margin falls below the sensing threshold, the sampled timing result is forced to an invalid large-number sentinel.

### Reported Statistics

For Monte Carlo mode, EvaCAM stores:

- nominal value
- mean
- standard deviation
- minimum
- maximum
- 95th percentile

for the currently summarized metrics:

- matchline delay
- search latency
- search dynamic energy
- exact match sense margin

The subarray’s exported timing and search-energy values are then replaced by the sample mean.

## Configuration Validity Checks

The circuit model rejects or invalidates some configurations before producing results. Important checks include:

- missing matchline port
- unsupported device type
- unsupported external sense-amp use for non-SRAM cells
- unsupported column bitline topology in the current matchline model
- sense margin below the minimum sensible voltage
- mux width exceeding the configured maximum device size

These checks are important because a successful parse does not guarantee a physically modeled configuration.

## Supported Modes and Important Limitations

The current implementation has several important boundaries.

### Well-Supported Path

The strongest support today is for:

- TCAM-style search
- analytical matchline RC delay
- built-in sense amplifier
- analytical read/search/write energy
- resistance-based variation on the matchline path

### Explicitly Limited or Unsupported Paths

- ACAM is not supported.
- MCAM has an experimental exact-match timing and energy path for the shipped 2FeFET topology, including integer vector/query evaluation, resistance-distance matchline behavior, query-specific searchline energy, and per-state sampled variation. Best-match and threshold MCAM APIs remain unsupported.
- External sense amplifiers are rejected for non-SRAM cells.
- Some sense-amp modes exist in the type system but are not fully implemented.
- Some topologies are rejected early because the current matchline model does not cover them.

### Simplifying Assumptions Worth Remembering

- precharge is full `Vdd` unless MCAM supplies `mcam.ml_precharge_voltage`
- matchline timing is an analytical RC approximation, not a SPICE transient
- query-dependent energy for mismatches is scaled by a simple mismatch-ratio heuristic in `EvaluateBinaryMatch()`
- write energy uses analytical pulse-energy formulas and line-charging approximations
- leakage is primarily transistor gate leakage plus peripheral leakage, not a full standby-current simulation

These assumptions are not bugs by themselves. They define the intended fidelity level of the model.

## Interpreting Results

When calibrating or debugging a new cell description, these relationships are the most important.

### What Usually Increases Search Latency

- more columns per compared word
- larger cell access capacitance
- larger matchline wire resistance or capacitance
- higher mux loading
- slower row-driver output ramp
- smaller sense margin

### What Usually Reduces Sense Margin

- longer matchlines
- larger wire resistance
- larger mux or sense-amp load
- weaker separation between on-state and off-state resistance
- larger sampled resistance variation

### What Usually Increases Search Energy

- larger matchline capacitance
- larger cell capacitance
- wider words
- higher precharge voltage swing
- stronger driver activity on row/search lines

### What Usually Increases Write Energy

- larger set/reset voltages
- longer write pulses
- lower-resistance write path
- larger bitline and mux capacitance
- more columns behind the active write path

## Worked Example: 2FeFET TCAM

The shipped `config/2FeFET_TCAM/` example is a good reference point for understanding the modeling flow.

At a high level:

1. The run config selects the inputs, and the architecture config supplies process node, search mode, organization, and peripheral options.
2. The cell and memory-device configs supply the device resistances, read/write settings, minimum sense voltage, and the row/column port description.
3. EvaCAM converts the cell geometry into row and column lengths.
4. Each line receives wire RC plus device capacitance from its attached ports.
5. The matchline-bearing column determines the effective on-state and off-state matchline path resistance.
6. The subarray builds a one-mismatch RC delay for nominal search timing and an all-match RC delay for sense-margin validation.
7. Peripheral latencies are added around the matchline event.
8. Matchline, cell, driver, and peripheral energies are summed into search/read/write metrics.
9. If the memory-device config contains `variation`, the matchline-path resistances are resampled and the timing and search-energy summaries are recomputed.

For a new cell technology, the most important calibration knobs are usually:

- `resistance.on`
- `resistance.off`
- `read.mode`
- `read.voltage` or `read.current`
- `read.min_sense_voltage`
- port topology and device widths
- write pulse and energy settings
- variation sigma values

## File Map

For future edits, the most relevant implementation files are:

- [src/cam/CAM_SubArray.cpp](../src/cam/CAM_SubArray.cpp)
- [include/cam/CAM_SubArray.h](../include/cam/CAM_SubArray.h)
- [src/cam/CAM_Line.cpp](../src/cam/CAM_Line.cpp)
- [src/cam/CAM_SenseAmp.cpp](../src/cam/CAM_SenseAmp.cpp)
- [src/technology/MemCell.cpp](../src/technology/MemCell.cpp)
- [src/config/TechnologyLoader.cpp](../src/config/TechnologyLoader.cpp)
- [src/circuit/formula.cpp](../src/circuit/formula.cpp)

## Open Documentation Gaps

This document reflects the current implementation, including places where the model is intentionally approximate or where the code comments note missing support. If the model changes, the sections most likely to need updates are:

- supported sense modes
- matchline-delay equations
- variation summary behavior
- MCAM and ACAM support status
- write-energy assumptions
