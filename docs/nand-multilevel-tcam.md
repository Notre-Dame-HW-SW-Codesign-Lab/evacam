# Table-based multilevel NAND TCAM

Planar and 3D NAND support exact matching of multilevel symbols using supplied
per-transistor effective resistances. The existing analytical first-moment RC
model handles the series string. Nonlinear and transient solvers are outside EvaCAM’s scope and have been removed.

Runnable synthetic examples:

```sh
./EvaCAM config/NAND_MLC_TCAM/NAND_MLC_TCAM.config.yaml
./EvaCAM config/NAND_3D_MLC_TCAM/NAND_3D_MLC_TCAM.config.yaml
```

Both examples store 64 entries of 32 logical bits: 16 four-level symbols per
entry. Their table and operation costs are illustrative, not flash calibration.

## Table contract

Within `memory_device.nand` or `memory_device.nand3d`, add:

```yaml
model: analytical_rc
query_voltages: [0.5V, 1V, 1.8V, 2.5V]
resistance_table:
  - [10kohm, 8kohm, 6kohm, 5kohm]
  - [1Gohm, 11kohm, 8kohm, 6kohm]
  - [2Gohm, 1Gohm, 12kohm, 8kohm]
  - [3Gohm, 2Gohm, 1Gohm, 13kohm]
```

Rows index the programmed state of **one physical transistor**; columns index
its applied query voltage. An entry is a resistance in ohms, not conductance
and not the total resistance of a two-cell symbol. Numeric SI values and
resistance/voltage unit strings are accepted.

The matrix must be square with 2, 4, 8, or 16 states. Query voltages must have
the same length and increase strictly within `[0, bias.pass)`. Every resistance
must be finite, positive, and at least `resistance.pass`. State-dependent pass
resistance is not modeled: the scalar pass resistance is shared by all states.
No symmetry, distance dependence, or uniform diagonal resistance is inferred.

For planar flash, use `type: MLCNAND` for 4/8/16 states, or `SLCNAND` for two
states. For 3D flash, retain `type: NAND3D` and select `storage_mode: SLC`, `MLC`,
`TLC`, or `QLC` for 2, 4, 8, or 16 states respectively. Multilevel modes require
both table fields. Existing SLC inputs without a table retain their original
binary resistance model and encoding.

## Encoding, masks, and validity

For `N` states, symbol `s` is stored in two adjacent devices as `(s, N-1-s)`.
Query `q` drives those devices with `(query_voltages[q], query_voltages[N-1-q])`.
The model looks up their resistances independently:

```text
Rfirst  = resistance_table[s][q]
Rsecond = resistance_table[N-1-s][N-1-q]
```

They remain separate series elements with their own downstream capacitance
weights. The model does not sum parallel conductances as the FeFET MCAM path
does. The same symbol mismatch can therefore produce different electrical
responses depending on its position and the surrounding symbols.

A query symbol of `-1` masks both devices by applying pass bias. A stored `-1`
uses `(0, 0)`, the two lowest-threshold states. Its query-dependent resistances
come from row zero; it is not replaced with an ideal conducting branch.

The reserved binary validity pair remains controlled by `resistance.read_on`,
`resistance.off`, `threshold.low/high`, and `bias.read`. Those fields describe
the validity marker when a table is present; they do not define the multilevel
data states. Padding and dummy devices remain pass-biased. `valid=false`
selects the programmed-invalid marker. Erasing alone does not establish that
marker or track occupancy.

`cam_type: TCAM` supports exact search (`EX`) and explicit nominal distance
threshold analysis (`TH`, described below). Equality and wildcards determine
the exact-search logical `hit`; electrical margin is reported separately.
A multilevel table does not guarantee electrically separable distance classes.

## Geometry and physical operation units

Let `b = log2(N)` and `K` be the number of symbols per key:

```text
memory.word_width bits = K * b
organization.subarray.dimensions = [strings_per_block, K]
required storage wordlines = 2 * K + 2
logical capacity bits = blocks * strings_per_block * K * b
physical cell count = blocks * strings_per_block * storage_wordlines
flash.block_size bits = strings_per_block * storage_wordlines * b
```

`memory.word_width` must contain whole symbols. Complementary encoding stores
`b` logical bits across **two** physical devices; it does not provide `b`
logical bits per device after encoding. Validity and padding further reduce
usable capacity.

A physical page is one **bit plane** at a selected wordline: `page_size` in bits
is the planar string count or the 3D `string_columns` count. A multilevel
wordline has `b` such pages. The physical block capacity counts all bit planes,
including unused capacity in the binary validity marker and padding.

The four-level examples use 36 storage wordlines: 32 for symbols, two for
validity, and two for padding. Each block contains 2,304 physical storage cells,
4,608 nominal physical bits, and 2,048 usable logical key bits. The planar page
is 64 bits; the 3D page is 16 bits across four select groups. YAML page/block
quantities remain byte quantities (`8B`/`2B` pages and `576B` blocks here).

Program latency/energy remains a supplied complete cost for **one physical
bit-plane page**. Erase remains one block. Users must supply costs appropriate
to their multilevel programming protocol; the table does not predict charge
injection, verify loops, page-order dependencies, or programming history.

## Sensing, energy, and scope

The model computes the slowest logical match and fastest logical nonmatch over
all supported symbol pairs, wildcard cases, positions, and invalid entries.
It combines per-position extrema of the affine first-moment equation, including
all patterns with at least one mismatch. This does not assume that a single
fixed table entry represents every mismatch. In EX mode, a table whose classes overlap
produces an infeasible design. TH mode retains it for diagnostic analysis.

These are global bounds **within the single-exponential RC approximation**.
They are not bounds on a full transient simulation or measured hardware.
Effective resistances remain fixed during evaluation and do not respond to
changing local source/drain voltages. Characterize the table for the intended
bias range and retain its provenance in `calibration_status` and `source`.

Full reset/precharge and the existing capacitor inventory determine bitline
energy. Wordline energy uses the actual two query voltages per unmasked symbol;
driver energy counts both updated lines. Aggregate search energy uses the
worst-energy query symbol, which need not be zero. Query-specific evaluation
uses the supplied query and masks. Global routing budgets two digital bias
codes per symbol, each `b+1` bits to include pass masking, plus two validity
control bits. Voltage generation/decoding and peripheral characterization
remain supplied overheads, not a transistor-level DAC model.

## C++ and Python API

```python
import evacam_py

matcher = evacam_py.EvaCAMMatch(
    "config/NAND_3D_MLC_TCAM/NAND_3D_MLC_TCAM.config.yaml"
)
assert matcher.word_width() == 32          # Logical bits
assert matcher.bits_per_symbol() == 2
assert matcher.symbol_width() == 16       # Input vector length
stored = [0, 1, 2, 3] * 4
result = matcher.evaluate_nand(stored, stored, valid=True)
assert result.hit and result.sense_margin_pass
```

`evaluate_vector`, `evaluate_symbols`, and row-array matching accept symbol
indices. `evaluate_bits` accepts the logical bit width and packs each symbol
MSB-first; it accepts binary bits only. Use symbol APIs for wildcards. The same
contracts apply to the C++ `EvaCAM_Match` interface.

Results expose `bits_per_cell`, `bits_per_symbol`, `symbols_per_key`, logical
bit width, physical cell counts and physical bit capacity separately. Metadata
identifies the table response, complementary encoding and fixed-bias assumption,
including in no-solution YAML assumptions.

Verification: `make test-nand-resistance-table test-pybind-nand` covers all state
counts, strict parsing, masks/validity, independent series-RC and supply-energy
calculations, exhaustive short-word bounds for asymmetric tables, both bank
routes, and structured YAML/Python output.

## Nominal distance and threshold analysis

Table-based planar and 3D NAND also expose squared symbol distance
`D = sum((stored[i] - query[i])**2)`. Set `design.search_function: TH` in the
architecture to load exploratory designs even when their exact-search classes
overlap. Keep `sensing.strict_sense_margin: false` to inspect failed thresholds;
`true` makes individual threshold evaluations throw if their margin fails.
The aggregate CLI run has no query or distance threshold, so it reports timing,
energy and **exact-search diagnostic** margins, not threshold feasibility.
`EX` retains its existing exact-search feasibility requirement.

```python
query = [0] * matcher.symbol_width()
stored = query.copy()
stored[0] = 1
value = matcher.evaluate_distance(stored, query)
bounds = matcher.distance_voltage_bounds(query, include_variation=False)
threshold = matcher.evaluate_distance_threshold(stored, query, 1)
print(threshold.hit, threshold.sense_margin, threshold.sense_margin_pass)
```

`hit` is the ideal decision `D <= threshold`. It does not assert that the
circuit can distinguish the classes. The returned electrical diagnostics use
all ordinary stored words for the supplied query, including distances farther
away than the nearest rejected distance. Programmed-invalid entries remain
in the rejected class even when the threshold accepts every valid word.
Wildcards are excluded from distance APIs; they retain their exact-search use.
Use `evaluate_distance_threshold` explicitly in TH mode; generic vector/array
threshold calls have no NAND distance cutoff and are rejected. Best-match and
k-nearest-neighbor NAND search remain unsupported.

For every reachable integer distance, dynamic programming gives the minimum
and maximum first-moment time constants, voltages and DC conductances. An empty
query in `distance_voltage_bounds([], False)` gives global bounds over ordinary
stored/query words. Voltage extrema include replayable
`minimum_voltage_stored/query` and `maximum_voltage_stored/query` witnesses.
The DC extrema can come from different words: their witness fields are unused
for NAND. Very large requests are rejected before allocation, rather than
silently sampled (2 million cumulative DP state slots / 30 million transitions).
These are nominal combinatorial bounds, not a statistical variation envelope;
`include_variation` cannot enable NAND variation.

The position effect comes from the affine RC expression
`tau = baseline + sum((R[i] - Rpass) * downstream_capacitance[i])`.
Swapping two unequal resistances preserves total series resistance and DC
conductance, but changes tau when their downstream capacitances differ.
Equal weights remove this effect. The single-exponential sensing model is
`V(t) = Vpre * exp(-t/tau)`, so greater tau gives higher sampled voltage.
A fixed table alone does not model local transistor-bias feedback.

For a threshold `T`, let `A` be the highest accepted voltage and `B` the lowest
rejected voltage (including invalid entries). A zero configured reference uses
`(A+B)/2` **for that query and threshold**; a positive reference is fixed.
Margin is `min(reference-A, B-reference) - offset`, and slack subtracts the
configured required margin. Negative overlap is preserved. A tunable midpoint
is an analytical reference choice; hardware calibration and reference generation
are not implemented by this API.

Generate plots and CSV/JSON exports using the supplied synthetic example:

```sh
make test-nand-distance test-nand-resistance-table test-nand-distance-plot
python3 scripts/plot_nand_distance.py --output-dir results/nand-distance-study \
    --decision-times-ns 50 500 5000
```

The output directory must be new. Each case contains a copied configuration,
original-file hashes, fixed-query distance bounds, voltage witnesses, every
reachable threshold cut, and a sweep moving one adjacent-state mismatch through
the string. PNGs show full/low-distance voltage ranges, signed margin slack,
and position dependence. `--query` accepts one ordinary state per symbol;
default is all zero. With a constant query the position sweep preserves the
same resistor multiset; a nonconstant query can also change the physical pair
resistances. Use separate consistent input configurations to compare string
lengths or state counts. Source configurations are left unchanged; plotting
snapshots use TH with diagnostic margins. The example remains synthetic and
no statistical variation, nonlinear flash physics, or correction calibration
is implied.
