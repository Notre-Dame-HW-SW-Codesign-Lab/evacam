# Containment Notes

This document is a quick reference for the main ownership and member relationships between the repo's core runtime classes.

## Notes

- `FunctionUnit` contains `EvaCamConfig` (`shared_ptr`)
- `Bank` contains `Mat` (`shared_ptr`), `Wire localWire` (`shared_ptr`), `Wire globalWire` (`shared_ptr`), and `CAM_Opt` (`shared_ptr`)
- `Result` contains `EvaCamConfig` (`shared_ptr`), `Bank` (`shared_ptr`), `Wire localWire` (`shared_ptr`), and `Wire globalWire` (`shared_ptr`)
- `TechnologyContext` contains `Technology tech` (`shared_ptr`), `Technology fefetTech` (`shared_ptr`), and `MemCell cell` (`shared_ptr`)

## Class Containment

### `EvaCamConfig`

- `Logger logger`
- `InputConfig input`
- `PeripheralConfig peripherals`
- `TechnologyContext technology`
- `ConstraintConfig constraints`
- `RuntimeSizingConfig runtimeSizing`
- `ExplorationSpec exploration`
- `ResolvedExplorationSpace resolvedExploration`

### `CAM_Line`

- `EvaCamConfig` (`shared_ptr`)
- `Wire` (`shared_ptr`)

### `MemCell`

- No notable contained runtime objects tracked in this summary

### `CAM_MemCell`

- No notable contained runtime objects tracked in this summary

### `FunctionUnit`

- `EvaCamConfig` (`shared_ptr`)

### `OutputDriver`

- No notable contained runtime objects tracked in this summary

### `Mux`

- No notable contained runtime objects tracked in this summary

### `SenseAmp`

- No notable contained runtime objects tracked in this summary

### `CAM_Controller`

- No notable contained runtime objects tracked in this summary

### `Comparator`

- No notable contained runtime objects tracked in this summary

### `CAM_LevelShifter`

- No notable contained runtime objects tracked in this summary

### `Wire`

- `SenseAmp` (`unique_ptr`)
- `EvaCamConfig` (`shared_ptr`)

### `CAM_SenseAmp`

- `SenseAmp normalSenseAmp` (`shared_ptr`)
- `SenseAmp customSA` (`shared_ptr`)

### `BasicDecoder`

- `OutputDriver` (`unique_ptr`)

### `CAM_InputEncoder`

- `OutputDriver` (`shared_ptr`)

### `CAM_BasicEncoder`

- `OutputDriver` (by value)

### `CAM_Encoder`

- `CAM_BasicEncoder` (by value)
- `OutputDriver` (by value)

### `CAM_BasicMMR`

- `OutputDriver LookAheadDriver` (by value)

### `CAM_MMR`

- `OutputDriver` (by value)
- `CAM_BasicMMR` (by value)

### `RowDecoder`

- `OutputDriver` (`shared_ptr`)

### `Precharger`

- `OutputDriver` (`shared_ptr`)
- `Wire localWire` (`shared_ptr`)

### `CAM_Precharger`

- No notable contained runtime objects tracked in this summary

### `CAM_RowNand`

- No notable contained runtime objects tracked in this summary

### `PredecodeBlock`

- `RowDecoder rowDecoderStage1A` (`shared_ptr`)
- `RowDecoder rowDecoderStage1B` (`shared_ptr`)
- `RowDecoder rowDecoderStage1C` (`shared_ptr`)
- `RowDecoder rowDecoderStage2` (`shared_ptr`)
- `BasicDecoder basicDecoderA1` (`shared_ptr`)
- `BasicDecoder basicDecoderA2` (`shared_ptr`)
- `BasicDecoder basicDecoderB` (`shared_ptr`)
- `BasicDecoder basicDecoderC` (`shared_ptr`)

### `CAM_OutputAccumulator`

- `OutputDriver` (by value)

### `CAM_PriorityEncoder`

- `CAM_MMR` (by value)
- `CAM_Encoder` (by value)

### `SubArray`

- `RowDecoder rowDecoder` (`shared_ptr`)
- `RowDecoder bitlineMuxDecoder` (`shared_ptr`)
- `RowDecoder senseAmpMuxLev1Decoder` (`shared_ptr`)
- `RowDecoder senseAmpMuxLev2Decoder` (`shared_ptr`)
- `Mux bitlineMux` (`shared_ptr`)
- `Mux senseAmpMuxLev1` (`shared_ptr`)
- `Mux senseAmpMuxLev2` (`shared_ptr`)
- `Precharger` (`shared_ptr`)
- `SenseAmp` (`shared_ptr`)
- `Wire localWire` (`shared_ptr`)

### `CAM_DataBuffer`

- `OutputDriver` (`shared_ptr`)

### `CAM_SubArray`

- `CAM_DataBuffer inputBuf` (`shared_ptr`)
- `CAM_DataBuffer outputBuf` (`shared_ptr`)
- `CAM_LevelShifter inputLS` (`shared_ptr`)
- `CAM_LevelShifter outputLS` (`shared_ptr`)
- `CAM_InputEncoder inputEnc` (`shared_ptr`)
- `CAM_RowNand RowDecMergeNand` (`shared_ptr`)
- `CAM_RowNand RowDriver` (`shared_ptr`, vector)
- `CAM_Precharger precharger` (`shared_ptr`)
- `RowDecoder ColDecMergeNand` (`shared_ptr`)
- `RowDecoder WriteDriver` (`shared_ptr`, vector)
- `Mux ColMux` (`shared_ptr`, vector)
- `CAM_SenseAmp senseAmp` (`shared_ptr`)
- `RowDecoder senseAmpMuxLev1Nand` (`shared_ptr`)
- `Mux senseAmpMuxLev1` (`shared_ptr`)
- `RowDecoder senseAmpMuxLev2Nand` (`shared_ptr`)
- `Mux senseAmpMuxLev2` (`shared_ptr`)
- `CAM_OutputAccumulator outputAcc` (`shared_ptr`)
- `CAM_PriorityEncoder priorityEnc` (`shared_ptr`)
- `CAM_Line Row` (`shared_ptr`, vector)
- `CAM_Line Col` (`shared_ptr`, vector)
- `Wire localWire` (`shared_ptr`)
- `CAM_Opt` (`shared_ptr`)

### `Mat`

- `CAM_SubArray subarray` (`shared_ptr`)
- `PredecodeBlock rowPredecoderBlock1` (`shared_ptr`)
- `PredecodeBlock rowPredecoderBlock2` (`shared_ptr`)
- `PredecodeBlock bitlineMuxPredecoderBlock1` (`shared_ptr`)
- `PredecodeBlock bitlineMuxPredecoderBlock2` (`shared_ptr`)
- `PredecodeBlock senseAmpMuxLev1PredecoderBlock1` (`shared_ptr`)
- `PredecodeBlock senseAmpMuxLev1PredecoderBlock2` (`shared_ptr`)
- `PredecodeBlock senseAmpMuxLev2PredecoderBlock1` (`shared_ptr`)
- `PredecodeBlock senseAmpMuxLev2PredecoderBlock2` (`shared_ptr`)
- `Wire localWire` (`shared_ptr`)
- `CAM_Opt` (`shared_ptr`)
- `Comparator` (by value)

### `Bank`

- `Mat` (`shared_ptr`)
- `Wire localWire` (`shared_ptr`)
- `Wire globalWire` (`shared_ptr`)
- `CAM_Opt` (`shared_ptr`)

### `BankWithHtree`

- No additional notable contained runtime objects beyond `Bank` in this summary

### `BankWithoutHtree`

- `Mux globalBitlineMux` (`shared_ptr`)
- `SenseAmp globalSenseAmp` (`shared_ptr`)
- `Comparator globalComparator` (`shared_ptr`)

### `Result`

- `EvaCamConfig` (`shared_ptr`)
- `Bank` (`shared_ptr`)
- `Wire localWire` (`shared_ptr`)
- `Wire globalWire` (`shared_ptr`)

### `CAM_Result`

- No additional notable contained runtime objects beyond `Result` in this summary
