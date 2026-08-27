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

## Memory Technologies

Accepted cell types include `SRAM`, `MRAM`, `PCRAM`, `ReRAM`, `FBRAM`, `SLCNAND`, and `FEFETRAM`.

Known unsupported or incomplete modes:

- `DRAM` is under development
- `eDRAM` is under development
- `MLCNAND` is under development

## MCAM

- MCAM uses unitless `memory.vector_dimensions`; `memory.word_width` is rejected
  because bit-word width has no MCAM meaning. Each vector dimension occupies
  one multi-level cell, and encoded storage is dimensions multiplied by
  `log2(num_resistance_state)`.
- MCAM is limited to the shipped two-FeFET topology: `FEFETRAM`, no access device, two gate-connected searchlines, and two drain-connected matchlines.
- Exact, best-match, and threshold integer-vector evaluation is supported for
  symbols in `0..num_resistance_state-1`. Distance is squared Euclidean, not
  Hamming; the physical result also depends on the configured resistance curve
  and sense margin.
- The shipped resistance states and eight-state searchline voltages are provisional infrastructure examples, not calibrated device-correlation data.
- The shipped MCAM fixture assumes a `70mV` minimum detectable voltage. MCAM
  reports actual margin, required margin, signed slack, and pass/fail without
  rejecting the result by default; `sensing.strict_sense_margin: true` makes
  the same requirement mandatory.
- Best-match margin depends on the actual best and runner-up vectors. Threshold
  margin is query dependent because state-range endpoints change which symbol
  deltas are reachable. The reported ideal hit still uses squared Euclidean
  distance and is distinct from electrical detectability.

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
