# Schema Reference

This document covers the YAML fields currently parsed by EvaCAM. Treat this file and the shipped examples under `config/` as the source of truth for real runs.

## Top-Level Config

Required sections:

- `design`
- `memory`
- `routing`
- `peripherals`
- `sensing`
- `optimization`
- `wires`

Optional sections:

- `array`
- `matchline`
- `constraints`
- `advanced`
- `cache`
- `flash`
- `extra`

Common required keys:

- `design.target`: currently only `CAM`
- `design.search_function`: `EX`, `BE`, or `TH`
- `design.process_node`: for example `45nm`
- `design.device_roadmap`: `HP`, `LSTP`, `LOP`, `FEFET`, or `LP`
- `memory.cell_file`, `memory.capacity`, `memory.word_width`
- `routing.type`: currently only `H-tree`
- `optimization.target`: `ReadLatency`, `WriteLatency`, `ReadDynamicEnergy`, `WriteDynamicEnergy`, `ReadEDP`, `WriteEDP`, `LeakagePower`, `Area`, `SearchLatency`, `SearchEnergy`, `SearchEDP`, or `Exploration`

Useful optional keys:

- `array.banks.*`, `array.mats.*`, `array.mux.*`: pin exploration to fixed powers-of-two values
- `constraints.*`: result limits; setting any value enables constraints
- `advanced.enable_pruning`, `advanced.bit_serial_width`, `advanced.use_cacti_assumption`
- `cache.associativity`, `cache.access_mode`, `cache.write_scheme`
- `extra.real_capacity`: required when `memory.word_width` is not a power of two
- `extra.output_file_prefix`: affects exploration CSV naming

## Cell File

Required section:

- `cell` with `type`, `process_node`, `area`, and `aspect_ratio`

Common implemented optional sections:

- `access_device`
- `resistance`
- `capacitance`
- `device`
- `read`
- `write`
- `match`
- `dram`
- `sram`
- `flash`
- `variation`
- `mcam`
- `ports`

Important notes:

- `cell.cam_type` is optional; EvaCAM infers it from the file name if omitted.
- `ports.row` and `ports.column` are maps keyed by integer index.
- `docs/config_full_example.yaml` and `docs/cell_full_example.yaml` include reference-only or proposed fields. Do not assume every commented field in those files is implemented.
