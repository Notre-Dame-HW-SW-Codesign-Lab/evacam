# EvaCAM Development Roadmap

This document is a working inventory of incomplete features, modeling gaps, and
actionable code debt. The priority roadmap highlights work that can affect
correctness or reported metrics; the source index provides a complete inventory
of inline `TODO`, `FIXME`, and `HACK` markers.

Last reviewed: 2026-08-05.

The inventory can be refreshed with:

```bash
rg -n "TODO|FIXME|HACK" src include --glob '!old_style_config/**'
rg -n "not implemented|under development|not consumed|incomplete" src include docs config \
    --glob '!old_style_config/**'
```

Current scan summary:

- `107` `TODO`/`FIXME`/`HACK` markers in `src/` and `include/`.
- Canonical active configurations use names such as `*.config.yaml`,
  `*.architecture.yaml`, `*.cell.yaml`, and `*.memory_device.yaml`.
- `old_style_config/` is legacy reference material and is excluded.
- Pruning and SPICE-correlation work is tracked separately in
  [Pruning and SPICE Validation Plan](pruning-and-spice-validation.md).

## Priority Roadmap

These items can affect correctness, supported behavior, or confidence in reported metrics.

### 1. Implement or explicitly reject incomplete match APIs

- `src/app/EvaCAM_Match.cpp:259`: exact MCAM vector evaluation is not implemented.
- `src/app/EvaCAM_Match.cpp:278`: best-match MCAM vector evaluation is not implemented.
- `src/app/EvaCAM_Match.cpp:297`: threshold MCAM vector evaluation is not implemented.
- `src/app/EvaCAM_Match.cpp:319`: exact ACAM vector evaluation is not implemented.
- `src/app/EvaCAM_Match.cpp:327`: best-match ACAM vector evaluation is not implemented.
- `src/app/EvaCAM_Match.cpp:335`: threshold ACAM vector evaluation is not implemented.

The current exceptions are intentional and covered by `tests/test_pybind_match.py`; completing a path requires updating those tests and `docs/python-api.md`.

### 2. Replace placeholder energy and leakage values

- `src/circuit/Mux.cpp:155`: mux leakage is forced to `0`.
- `src/cam/CAM_SubArray.cpp:944` and `src/cam/CAM_SubArray.cpp:1175`: write-and-verify programming is not modeled.
- `src/cam/CAM_SubArray.cpp:1185`: CAM cell leakage during read/write is forced to `0`.
- `src/circuit/Wire.cpp:468`: short-circuit wire energy is forced to `0`.
- `src/technology/MemCell.cpp:154`, `190`, and `210`: reset, set, and read energy calculations do not account for charge-pump behavior.

### 3. Complete variation follow-through

- `variation.lut_file` is parsed, stored, and propagated, but no runtime path consumes the LUT (`src/input/CellYamlLoader.cpp:644-645`, `src/config/VariationConfigBuilder.cpp:41`).
- `src/cam/CAM_SubArray.cpp:860`: reference delay still uses a provisional calculation that needs validation.
- `src/cam/CAM_SubArray.cpp:537`: the `1.6` FeFET row-capacitance scaling factor needs validation.
- `src/cam/CAM_SubArray.cpp:1343`: the debug interface remains unfinished.

### 4. Finish or reject partially modeled device and peripheral modes

- `src/config/EvaCamConfigValidator.cpp:9-15`: DRAM, eDRAM, and MLC NAND are rejected as under development.
- Bank-level external sensing remains unsupported for both H-tree and non-H-tree routing.
- `src/cam/CAM_InputEncoder.cpp:29-159`: custom input encoders and encoding lookup tables are unfinished.
- `src/cam/CAM_SenseAmp.cpp:85-194`: self-clocked and dual-threshold sensing paths report that they are under development.
- `src/cam/CAM_SubArray.cpp:1158` and `1192`: MLC MRS set and MLC power calculations are incomplete.

### 5. Replace decoder, driver, and predecoder placeholder arguments

- `src/circuit/PredecodeBlock.cpp:68-172`: thirteen decoder/predecoder calls pass placeholder resistance or load values.
- `src/model/Mat.cpp:41-42`: predecoder initialization passes zero resistance.
- `src/cam/CAM_MMR.cpp:41`, `src/cam/CAM_Encoder.cpp:48`, and `src/cam/CAM_PriorityEncoder.cpp:33`: encoder/MMR initialization passes zero gate resistance.
- `src/cam/CAM_SubArray.cpp:507-528`, `653`, `660`, and `673`: buffer, level-shifter, encoder, and output-peripheral initialization still uses provisional arguments or omits output drivers.
- `src/circuit/Precharger.cpp:53`: output-driver initialization passes a placeholder value.

## Technical Debt by Area

### CAM peripheral variants

- `src/cam/CAM_MMR.cpp:26-40`: only two-level folding and an 8-to-3 `BasicMMR` are supported; the three-LA-input NOR is missing.
- `src/cam/CAM_BasicMMR.cpp:50`, `95`, `117`, `175`, and `218`: 4-input and 2-input MMR variants are missing.
- `src/cam/CAM_BasicEncoder.cpp:39`, `81`, `124`, and `161`: 4-to-2 and 2-to-1 encoder variants are missing.
- `src/cam/CAM_BasicEncoder.cpp:104`: output-data latency is omitted based on an array-size assumption.
- `src/cam/CAM_Encoder.cpp:84`: ramp input is approximate.
- `src/circuit/SenseAmp.cpp:149`: sense-amp behavior below 22 nm needs calibration.

### Wire and device models

- `src/circuit/Wire.cpp:119`: only copper wire is modeled.
- `src/circuit/Wire.cpp:344`: one calculation is documented as inconsistent with CACTI 6.5.
- `include/circuit/constant.h:43`: IV-converter area is a fixed technology/design-dependent constant.
- `include/technology/MemCell.h:39`: wordline boost ratio is declared but not realized.

### CAM subarray assumptions

- `include/cam/CAM_SubArray.h:82`: a calculation path can leave a value unresolved.
- `include/cam/CAM_SubArray.h:92`: `voltageMemCellOn` is initialized to zero instead of calculated.
- `include/cam/CAM_SubArray.h:93`: `WriteDriverArea` uses a defensive zero initialization despite being calculated elsewhere.
- `src/cam/CAM_SubArray.cpp:394`: CMOS/diode cell sensing constraints need validation.
- `src/cam/CAM_SubArray.cpp:776`: layout is provisional.

### Array organization and result accounting

- `src/model/Bank.cpp:95` and `125`: result breakdowns are acknowledged as inaccurate.
- `src/model/Mat.cpp:148`: vague placeholder requires clarification or removal.
- `include/model/Result.h:31`: `localWire` conflicts conceptually with a global variable name.

### Physical layout approximations

These are lower priority unless area results are being calibrated.

- `src/cam/CAM_OutputAccumulator.cpp:41`
- `src/cam/CAM_PriorityEncoder.cpp:47`
- `src/cam/CAM_MMR.cpp:55`
- `src/cam/CAM_BasicEncoder.cpp:63` and `77`
- `src/cam/CAM_Encoder.cpp:63`
- `src/cam/CAM_BasicMMR.cpp:88`
- `src/cam/CAM_SubArray.cpp:776`

### Cleanup-only or low-signal comments

These should be converted into specific work items or removed.

- `src/cam/CAM_LevelShifter.cpp:46` and `51`
- `src/cam/CAM_Line.cpp:65`, `99`, `100`, `135`, and `137`
- `src/cam/CAM_BasicMMR.cpp:130`, `154`, and `199`
- `src/circuit/Precharger.cpp:127`
- `src/model/Mat.cpp:148`

## Source Marker Index

This index covers every current `TODO`/`FIXME`/`HACK` marker. Repeated markers with the same meaning are grouped into ranges.

### Headers

- `include/cam/CAM_BasicEncoder.h:49`: decoder optimization priority is not configurable.
- `include/cam/CAM_SubArray.h:82`, `92`, `93`: missing calculation and defensive zero initializations.
- `include/circuit/BasicDecoder.h:42`: decoder optimization priority is not configurable.
- `include/circuit/PredecodeBlock.h:65`: predecoder optimization priority is not configurable.
- `include/circuit/constant.h:43`: fixed IV-converter area.
- `include/model/Result.h:31`: `localWire` naming conflict.
- `include/technology/MemCell.h:39`: unrealized wordline boost ratio.

### `src/technology`

- `src/technology/MemCell.cpp:154`, `190`, `210`: charge-pump effects are omitted.

### `src/model`

- `src/model/Bank.cpp:95`, `125`: inaccurate breakdown comments.
- `src/model/Mat.cpp:41-42`: placeholder predecoder resistance.
- `src/model/Mat.cpp:148`: vague placeholder.

### `src/circuit`

- `src/circuit/Mux.cpp:155`: missing leakage model.
- `src/circuit/Precharger.cpp:53`: placeholder initialization argument.
- `src/circuit/Precharger.cpp:127`: vague reference to preceding code.
- `src/circuit/PredecodeBlock.cpp:68`, `72`, `104`, `108`, `112`, `123`, `134`, `146`, `156`, `158`, `164`, `170`, `172`: placeholder decoder/predecoder values.
- `src/circuit/SenseAmp.cpp:149`: calibration below 22 nm.
- `src/circuit/Wire.cpp:119`: copper-only model.
- `src/circuit/Wire.cpp:344`: CACTI inconsistency.
- `src/circuit/Wire.cpp:468`: omitted short-circuit energy.

### `src/cam`

- `src/cam/CAM_BasicEncoder.cpp:26`, `39`, `63`, `77`, `81`, `104`, `124`, `161`: driver, variant, layout, and latency assumptions.
- `src/cam/CAM_BasicMMR.cpp:50`, `88`, `95`, `117`, `130`, `154`, `175`, `199`, `218`: missing variants, layout assumptions, and cleanup.
- `src/cam/CAM_Encoder.cpp:48`, `63`, `84`: placeholder resistance, layout, and ramp approximation.
- `src/cam/CAM_InputEncoder.cpp:29`, `44`, `55`, `70`, `82`, `91`, `104`, `130`, `142`, `159`: custom encoder and lookup-table support.
- `src/cam/CAM_LevelShifter.cpp:46`, `51`: unused variable and unknown initialization.
- `src/cam/CAM_Line.cpp:65`, `99`, `100`, `135`, `137`: unvalidated electrical/encoding assumptions and vague cleanup.
- `src/cam/CAM_MMR.cpp:26`, `27`, `40`, `41`, `55`: folding limitations, missing NOR, placeholder resistance, and layout.
- `src/cam/CAM_OutputAccumulator.cpp:41`: provisional layout.
- `src/cam/CAM_PriorityEncoder.cpp:33`, `47`: placeholder resistance and layout.
- `src/cam/CAM_SenseAmp.cpp:85`, `90`, `116`, `121`, `147`, `152`, `187`, `192`: unfinished self-clock sense path.
- `src/cam/CAM_SubArray.cpp:394`, `507`, `510`, `511`, `514`, `528`, `537`, `653`, `660`, `673`, `776`, `860`, `944`, `1158`, `1175`, `1185`, `1192`, `1343`: sensing constraints, placeholder initialization, FeFET scaling, layout, reference delay, write/MLC/leakage modeling, and debug support.

## User-Visible Runtime Limits

These are user-visible limitations and should remain tracked even though the code uses exceptions rather than TODO comments.

- DRAM, eDRAM, and MLC NAND models are rejected as under development.
- H-tree and non-H-tree routing require internal sensing; bank-level external sensing is not modeled.
- MCAM binary-vector exact, best, and threshold match evaluation is not implemented.
- ACAM exact, best, and threshold match evaluation is not implemented.
- `variation.lut_file` is accepted and propagated but not consumed.
