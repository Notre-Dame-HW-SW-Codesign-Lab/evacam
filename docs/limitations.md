# Supported Modes And Limits

This file summarizes current runtime restrictions enforced by the code.

## Design And Routing

- `design.target` must be `CAM`
- `routing.type: non_h_tree` is rejected as under development
- H-tree requires `sensing.internal: true`

## Technology Range

- Process nodes below `7nm` are rejected
- Process nodes above `200nm` are rejected
- Intermediate nodes are interpolated between built-in technology tables
- The supported anchor nodes are `7`, `10`, `14`, `22`, `32`, `45`, `90`, `120`, and `200` nm

## Memory Technologies

Accepted cell types include `SRAM`, `MRAM`, `PCRAM`, `ReRAM`, `FBRAM`, `SLCNAND`, and `FEFETRAM`.

Known unsupported or incomplete modes:

- `DRAM` is under development
- `eDRAM` is under development
- `MLCNAND` is under development

## Geometry And Sizing Rules

- Fixed geometry values in `array.*` are interpreted as powers-of-two domains
- `cache.associativity` must be a power of two
- Non-power-of-two `memory.word_width` requires `extra.real_capacity`
- `extra.real_capacity` must be at least `memory.capacity`
- `extra.real_capacity` must be compatible with the selected array geometry

## Practical Guidance

- Start from a known-good file under `config/`
- Change one axis at a time: technology, array geometry, or peripheral options
- Use `./EvaCAM -v <config>` when testing new combinations
- If a run ends with `No valid solutions.`, the YAML may be valid but the design point is illegal or unsupported
