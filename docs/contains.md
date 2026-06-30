# Containment Notes

This document is a quick reference for the main ownership and member relationships between the repo's core runtime classes. It tracks the concrete storage style where that affects ownership:

- `shared_ptr`: shared long-lived context or result objects that are still shared by design
- `unique_ptr`: a single owner holds a heap-allocated child object
- by value: the object is stored directly as a member or vector element
- `const &`: the object is required by an API but not owned by the callee

## High-Level Ownership

- `FunctionUnit` stores `EvaCamConfig` as `std::shared_ptr<EvaCamConfig>`.
- `TechnologyContext` stores `Technology tech`, `Technology fefetTech`, and `MemCell cell` as `shared_ptr`.
- The main model hierarchy is single-owner: `Bank` owns `Mat` through `unique_ptr`, `Mat` owns `CAM_SubArray` and its predecoder blocks through `unique_ptr`, and subarrays own their internal circuit blocks through `unique_ptr` or direct members.
- Wire settings are no longer owned through smart pointers in the model tree. `Wire` objects are direct value members or passed by `const Wire&`.
- `CAM_Opt` is a value-like configuration bundle. It is stored by value and passed as `const CAM_Opt&`.
- `Result` still stores `EvaCamConfig` and the selected `Bank` as `shared_ptr`; `Result::localWire` and `Result::globalWire` are value snapshots.
- Explorer best-result buffers still use `std::vector<std::shared_ptr<Result>>`.

## Helper API Notes

- Circuit formula helpers take `const Technology&`.
- `Technology::InterpolateWith` takes `const Technology&`.
- `BankFactory::CreateBank` takes `const EvaCamConfig&`.
- `BankFactory::InitializeBank` still takes `const std::shared_ptr<EvaCamConfig>&` because it forwards config into model initialization paths that store shared config.
- `WireFactory::{CreateDefaultLocalWire, CreateDefaultGlobalWire}` return `Wire` values, but still take shared config because `Wire::Initialize` stores the config context.

## Class Containment

### `EvaCamConfig`

- `Logger logger` (by value)
- `InputConfig input` (by value)
- `PeripheralConfig peripherals` (by value)
- `TechnologyContext technology` (by value)
- `ConstraintConfig constraints` (by value)
- `RuntimeSizingConfig runtimeSizing` (by value)
- `ExplorationSpec exploration` (by value)
- `ResolvedExplorationSpace resolvedExploration` (by value)

### `TechnologyContext`

- `Technology tech` (`shared_ptr`)
- `Technology fefetTech` (`shared_ptr`)
- `MemCell cell` (`shared_ptr`)

### `FunctionUnit`

- `EvaCamConfig config` (`shared_ptr`)

### `Wire`

- `EvaCamConfig config` (`shared_ptr`)

### `CAM_Line`

- `EvaCamConfig config` (`shared_ptr`)
- `Wire localWire` (by value)

### `OutputDriver`

- No notable contained runtime objects tracked in this summary

### `Mux`

- No notable contained runtime objects tracked in this summary

### `SenseAmp`

- No notable contained runtime objects tracked in this summary

### `BasicDecoder`

- `OutputDriver outputDriver` (by value)

### `RowDecoder`

- `OutputDriver outputDriver` (by value)

### `Precharger`

- `OutputDriver outputDriver` (by value)
- `Wire localWire` (by value)

### `PredecodeBlock`

- `RowDecoder rowDecoderStage1A` (`unique_ptr`)
- `RowDecoder rowDecoderStage1B` (`unique_ptr`)
- `RowDecoder rowDecoderStage1C` (`unique_ptr`)
- `RowDecoder rowDecoderStage2` (`unique_ptr`)
- `BasicDecoder basicDecoderA1` (`unique_ptr`)
- `BasicDecoder basicDecoderA2` (`unique_ptr`)
- `BasicDecoder basicDecoderB` (`unique_ptr`)
- `BasicDecoder basicDecoderC` (`unique_ptr`)

### `CAM_DataBuffer`

- `OutputDriver outputDriver` (by value)

### `CAM_InputEncoder`

- `OutputDriver outputDriver` (by value)

### `CAM_BasicEncoder`

- `OutputDriver outputDriver` (by value)

### `CAM_Encoder`

- `CAM_BasicEncoder BasicEncoder` (by value)
- `OutputDriver outputDriver` (by value)

### `CAM_BasicMMR`

- `OutputDriver LookAheadDriver` (by value)

### `CAM_MMR`

- `OutputDriver outputDriver` (by value)
- `CAM_BasicMMR BasicMMR` (by value)

### `CAM_SenseAmp`

- `SenseAmp customSA` (`unique_ptr`)
- `SenseAmp normalSenseAmp` (`unique_ptr`)

### `CAM_OutputAccumulator`

- `OutputDriver outputDriver` (by value)

### `CAM_PriorityEncoder`

- `CAM_MMR MMR` (by value)
- `CAM_Encoder Encoder` (by value)

### `CAM_SubArray`

- `CAM_DataBuffer inputBuf` (`unique_ptr`)
- `CAM_DataBuffer outputBuf` (`unique_ptr`)
- `CAM_LevelShifter inputLS` (`unique_ptr`)
- `CAM_LevelShifter outputLS` (`unique_ptr`)
- `CAM_InputEncoder inputEnc` (`unique_ptr`)
- `RowDecoder RowDecMergeNand` (`unique_ptr`)
- `RowDecoder RowDriver` (`vector<unique_ptr<RowDecoder>>`)
- `Precharger precharger` (`unique_ptr`)
- `RowDecoder ColDecMergeNand` (`unique_ptr`)
- `RowDecoder WriteDriver` (`vector<unique_ptr<RowDecoder>>`)
- `Mux ColMux` (`vector<unique_ptr<Mux>>`)
- `CAM_SenseAmp senseAmp` (`unique_ptr`)
- `RowDecoder senseAmpMuxLev1Nand` (`unique_ptr`)
- `Mux senseAmpMuxLev1` (`unique_ptr`)
- `RowDecoder senseAmpMuxLev2Nand` (`unique_ptr`)
- `Mux senseAmpMuxLev2` (`unique_ptr`)
- `CAM_OutputAccumulator outputAcc` (`unique_ptr`)
- `CAM_PriorityEncoder priorityEnc` (`unique_ptr`)
- `CAM_Line Row` (`vector<CAM_Line>`)
- `CAM_Line Col` (`vector<CAM_Line>`)
- `RowDecoder rowDecoder` (`unique_ptr`)
- `RowDecoder bitlineMuxDecoder` (`unique_ptr`)
- `RowDecoder senseAmpMuxLev1Decoder` (`unique_ptr`)
- `RowDecoder senseAmpMuxLev2Decoder` (`unique_ptr`)
- `Mux bitlineMux` (`unique_ptr`)
- `Wire localWire` (by value)
- `CAM_Opt CAM_opt` (by value)

### `Mat`

- `CAM_SubArray subarray` (`unique_ptr`)
- `PredecodeBlock rowPredecoderBlock1` (`unique_ptr`)
- `PredecodeBlock rowPredecoderBlock2` (`unique_ptr`)
- `PredecodeBlock bitlineMuxPredecoderBlock1` (`unique_ptr`)
- `PredecodeBlock bitlineMuxPredecoderBlock2` (`unique_ptr`)
- `PredecodeBlock senseAmpMuxLev1PredecoderBlock1` (`unique_ptr`)
- `PredecodeBlock senseAmpMuxLev1PredecoderBlock2` (`unique_ptr`)
- `PredecodeBlock senseAmpMuxLev2PredecoderBlock1` (`unique_ptr`)
- `PredecodeBlock senseAmpMuxLev2PredecoderBlock2` (`unique_ptr`)
- `Wire localWire` (by value)
- `CAM_Opt CAM_opt` (by value)

### `Bank`

- `Mat mat` (`unique_ptr`)
- `Wire localWire` (by value)
- `Wire globalWire` (by value)
- `CAM_Opt CAM_opt` (by value)

### `BankWithHtree`

- No additional notable contained runtime objects beyond `Bank` in this summary

### `BankWithoutHtree`

- `Mux globalBitlineMux` (`unique_ptr`)
- `SenseAmp globalSenseAmp` (`unique_ptr`)

### `Result`

- `EvaCamConfig config` (`shared_ptr`)
- `Bank bank` (`shared_ptr`)
- `Wire localWire` (by value)
- `Wire globalWire` (by value)
