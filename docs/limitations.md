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

## Geometry And Sizing Rules

- Fixed geometry values in `organization.*` are interpreted as powers-of-two domains
- Fixed physical subarray dimensions in `organization.subarray.dimensions` are exact values and may be non-powers-of-two, subject to the supported row/column limits
- `organization.subarray.dimensions` requires explicit `organization.banks` and `organization.mats` totals and active values
- `organization.subarray.dimensions` is rejected with `optimization.target: Exploration` or `optimization.deep_exploration: true`
- `memory.capacity` may be omitted or set to exact scalar `auto` only when `organization.subarray.dimensions` is supplied
- Non-power-of-two `memory.word_width` requires `memory.physical_capacity`
- `memory.physical_capacity` must be at least `memory.capacity`
- `memory.physical_capacity` must be compatible with the selected organization geometry
- With `organization.subarray.dimensions`, `memory.physical_capacity` must exactly match the derived capacity if supplied

## Practical Guidance

- Start from a known-good file under `config/`
- Change one axis at a time: technology, organization geometry, or peripheral options
- Use `./EvaCAM -v <config>` when testing new combinations
- If a run ends with `No valid solutions.`, the YAML may be valid but the design point is illegal or unsupported
