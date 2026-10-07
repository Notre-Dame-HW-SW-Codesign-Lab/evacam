# Supported Modes And Limits

This file summarizes current runtime restrictions enforced by the code.

## Design And Routing

- `design.target` must be `CAM`
- `routing.type` may be `H-tree` or `non_h_tree`
- Both routing modes currently require `sensing.internal: true`
- Non-H-tree uses direct, unequal-length bank routes from the bank interface to each mat;
  it does not model bank-level external matchline sensing

## Technology Range

- Process nodes below `7nm` are rejected
- Process nodes above `200nm` are rejected
- Intermediate nodes are interpolated between built-in technology tables
- The supported anchor nodes are `7`, `10`, `14`, `22`, `32`, `45`, `65`, `90`, `120`, and `200` nm
- Physical dimensions use the requested process node even when electrical
  parameters use nearby tables. Result assumptions report physical feature
  size, electrical table bounds, and the interpolation coefficient separately.

## Memory Technologies

Accepted cell types include `SRAM`, `MRAM`, `PCRAM`, `ReRAM`, `FBRAM`, `SLCNAND`, `NAND3D`, and `FEFETRAM`.

Known unsupported or incomplete modes:

- `DRAM` is under development
- `eDRAM` is under development
- `MLCNAND` is under development

The named non-flash examples are design exploration inputs, not complete
paper reproductions. The [paper comparison](validation/named-cam-papers.md)
records their different sizes, circuits, and measurement scopes. The
[CAM repair notes](validation/named-cam-fixes.md) describe corrected input
semantics and capacitance accounting. The independent RC check validates
the linear network's first moment; it does not validate a single-exponential
waveform or paper-specific sensing circuits.

Separate [paper reference fixtures](validation/named-cam-reference-configs.md)
pin sourced dimensions, process and cell footprints and can be rerun with
`make validate-named-cam`. Their manifest labels every case as a partial
reconstruction and records unmatched sensing, operating conditions and metric
scope. Small numerical gaps do not change that status.

The [analytical decision and scheduling model](validation/analytical-cam-timing.md)
adds explicit sensing events and phase dependencies for nominal exact TCAM.
Its one-pole equations and ideal differential reference are analytical
approximations, not a characterization of the named chip. Replica/control
delays, sensing offsets, MRS/X-state leakage, actual inverter trip points and
operating-corner current tables remain required for tighter paper agreement.
Generic search energy retains its conservative activity budget; timing changes
do not establish data-dependent energy accuracy. Other modes keep legacy timing.

The [sensing and topology repairs](validation/named-cam-sensing-topology.md)
restore scalar-amplifier inputs and give DATE21 an explicit `fefet_gate`
control-node model. That model supports nominal exact TCAM search and uses
a resistive divider followed by CMOS discharge, with an assumed switching
voltage. Its independent numerical checks establish equation correctness,
not compact-model or silicon calibration. Scalar amplifier values do not
automatically scale across load, voltage, process or temperature.

## NAND String TCAM

- `SLCNAND` requires `cam_type: TCAM`, `topology: nand_string`, and an explicit
  `nand` device model. Exact search (`EX`), stored wildcards, and query masks
  are supported by a series-string analytical RC approximation.
- The canonical `config/NAND_TCAM/` values are synthetic, with no measured or
  SPICE correlation. Planar area and a first-moment transient approximation
  must not be presented as validated commercial or 3D NAND predictions.
- The [independent RC audit](validation/nand-yang-2023.md) finds that a
  512-wordline matching string can pass the approximation's 100 mV margin
  requirement while providing only 75 mV in the full linear RC calculation.
  The supplied precharge time is also assumed sufficient, not verified.
- Fixed `[strings, logical key bits]` subarray dimensions describe one erase
  block. Physical pages contain one bit per string. Keys must fit in one
  string after complementary encoding and a validity pair.
- NAND uses supplied query, wordline-driver, sense, page-buffer, and operation
  costs. Generic CAM peripheral toggles, external sensing, generic matchline
  overrides, and variation are rejected.
- Missing RC, wordline-driver, and sense parameters can use warned CMOS
  [technology-library estimates](nand-tcam.md#technology-library-fallbacks).
  These do not establish NAND calibration or high-voltage circuit costs.
  Geometry, biases, sensing requirements, page buffers, and operation costs
  still need explicit inputs.
- Search/area/leakage objectives are supported; write objectives mean one
  physical page program. Conventional read, full/deep exploration, design
  constraints, and legacy exploration CSV are unsupported.
- The generic subarray dimension tester and independent dimension overrides
  are unsupported for NAND; change fixed NAND architecture configurations
  together with their physical page/block geometry.
- No multilevel, approximate/top-k, segmented-word, conventional storage-read,
  retention/disturb/endurance, ECC, or SSD-controller model is provided.
- See [NAND TCAM](nand-tcam.md) for geometry, sensing margins, operation units,
  provenance, and the supported example.

## 3D NAND String TCAM

- `NAND3D` is a separate SLC-mode backend with explicit vertical stack and
  lateral layout geometry and the same first-moment, single-exponential RC
  approach as the planar analytical model.
- The supplied 3D example is synthetic and uncalibrated. Numerical convergence
  checks of the separate nodal reference verify circuit equations; they do not validate
  flash device physics, fabricated area, or measurements from a paper.
- Sense bounds cover supported patterns within the analytical approximation,
  not the full distributed RC transient. Full precharge and recovery reset are
  assumed each round; phase durations are supplied and their sufficiency is not
  simulated. The separate nodal reference retains known approximation failures.
- A physical page contains one bit per string in one select group. Groups are
  searched sequentially; each logical entry occupies a complete vertical string.
- Only SLC resistance states and linear capacitances are modeled. No
  nonlinear transistor I–V, charge trapping, process variation, coupling noise,
  high-voltage programming waveform, retention, or endurance model is included.
- The operation and runtime restrictions listed for NAND string TCAM above also
  apply. See [3D NAND TCAM](nand-3d-tcam.md) for the supported contract.

## MCAM

- MCAM uses unitless `memory.vector_dimensions`; `memory.word_width` is rejected
  because bit-word width has no MCAM meaning. Each vector dimension occupies
  one multi-level cell, and encoded storage is dimensions multiplied by
  `log2(num_resistance_state)`.
- MCAM is limited to the shipped two-FeFET topology: `FEFETRAM`, no access device, two gate-connected searchlines, and two drain-connected matchlines.
- Exact, best-match, k-nearest-neighbor, and threshold integer-vector
  evaluation is supported for symbols in `0..num_resistance_state-1`. Distance
  is squared Euclidean, not Hamming; the physical result also depends on the
  configured resistance curve and sense margin.
- The shipped resistance states and eight-state searchline voltages are provisional infrastructure examples, not calibrated device-correlation data.
- The shipped MCAM fixture assumes a `70mV` minimum detectable voltage. MCAM
  reports actual margin, required margin, signed slack, and pass/fail without
  rejecting the result by default; `sensing.strict_sense_margin: true` makes
  the same requirement mandatory.
- Best-match margin depends on the actual best and runner-up vectors. Threshold
  margin is query dependent because state-range endpoints change which symbol
  deltas are reachable. The reported ideal hit still uses squared Euclidean
  distance and is distinct from electrical detectability.
- MCAM k-nearest-neighbor evaluation ranks modeled row conductances, includes
  every electrical tie at the kth boundary, and reports the k/k+1 voltage gap.
  It does not model a hardware top-k sorter, priority resolver, or tie breaker.

## Geometry And Sizing Rules

- Fixed geometry values in `organization.*` are interpreted as powers-of-two domains
- Fixed physical subarray dimensions in `organization.subarray.dimensions` are exact values and may be non-powers-of-two, subject to the supported row/column limits
- `organization.subarray.dimensions` requires explicit `organization.banks` and `organization.mats` totals and active values
- `organization.subarray.dimensions` is rejected with `optimization.target: Exploration` or `optimization.deep_exploration: true`
- `memory.capacity` may be omitted or set to exact scalar `auto` only when `organization.subarray.dimensions` is supplied
- Non-power-of-two `memory.word_width` requires `memory.physical_capacity` for
  single-bit CAM; MCAM resolves storage from vector dimensions and cell states
- `memory.physical_capacity` must be at least `memory.capacity`
- `memory.physical_capacity` must be compatible with the selected organization geometry
- With `organization.subarray.dimensions`, `memory.physical_capacity` must exactly match the derived capacity if supplied

## Practical Guidance

- Start from a known-good file under `config/`
- Change one axis at a time: technology, organization geometry, or peripheral options
- Use `./EvaCAM -v <config>` when testing new combinations
- If a run ends with `No valid solutions.`, the YAML may be valid but the design point is illegal or unsupported. The console and no-solution YAML still report the configured minimum required sense margin.

The [original EvaCAM validation audit](validation/original-evacam-validation.md)
separates implementation checks from paper accuracy. The diode-keeper reduction
uses fixed DC source nodes, square-law current and lumped capacitance; it omits
wire resistance, body effect and source-node settling. PCM CSRSS/reference and
SAPIENS's complete serial divider/accumulator architecture remain unreproduced.
Neither an unavailable metric nor a mismatched metric scope has an error score.
