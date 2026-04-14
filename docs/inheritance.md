# Inheritance Notes

This document is a quick reference for the major inheritance relationships in the EvaCAM class hierarchy.

## Hierarchy

- `FunctionUnit`
- `Bank`
- `BankWithHtree`
- `BankWithoutHtree`
- `RowDecoder`
- `CAM_RowNand`
- `Precharger`
- `CAM_Precharger`
- `CAM_LevelShifter`
- `CAM_InputEncoder`
- `SubArray`
- `CAM_SubArray`
- `SenseAmp`
- `CAM_SenseAmp`
- `Mux`
- `Mat`
- `Comparator`
- `OutputDriver`
- `CAM_DataBuffer`
- `CAM_Controller`
- `CAM_OutputAccumulator`
- `CAM_MMR`
- `CAM_BasicMMR`
- `PredecodeBlock`
- `CAM_PriorityEncoder`
- `CAM_BasicEncoder`
- `CAM_Encoder`
- `BasicDecoder`
- `Result`
- `CAM_Result`
- `MemCell`
- `CAM_MemCell`
- `Technology`
- `Wire`
- `CAM_Line`

## Tree

```text
FunctionUnit
    Bank
        BankWithHtree
        BankWithoutHtree
    RowDecoder
        CAM_RowNand
    Precharger
        CAM_Precharger
    CAM_LevelShifter
    CAM_InputEncoder
    SubArray
    CAM_SubArray
    SenseAmp
        CAM_SenseAmp
    Mux
    Mat
    Comparator
    OutputDriver
    CAM_DataBuffer
    CAM_Controller
    CAM_OutputAccumulator
    CAM_MMR
    CAM_BasicMMR
    PredecodeBlock
    CAM_PriorityEncoder
    CAM_BasicEncoder
    CAM_Encoder
    BasicDecoder
Result
    CAM_Result
MemCell
    CAM_MemCell
Technology
Wire
CAM_Line
```

Note: `CAM_SubArray` does not inherit from `SubArray`; both derive from `FunctionUnit`.

## Classes Outside The Repo-Local Inheritance Tree

These classes do not have a base class or derived class within EvaCAM's own class hierarchy.

- `EvaCAM_Match`
- `EvaCamExplorer`
- `EvaCamOutput`
- `EvaCamContextBuilder`
- `CliOptionsParser`
- `Logger`
- `Technology`
- `Wire`
- `CAM_Line`
- `DerivedValueHelpers`
- `EvaCamConfigValidator`
- `EvaCamConfigPrinter`
- `EvaCamYamlLoader`
- `ConfigSectionReaders`
- `ConfigNormalizer`
- `InputRuleValidator`
- `OutputPathBuilder`
- `TechnologyLoader`
- `VariationConfigBuilder`
- `CellYamlLoader`
- `YamlNodeHelpers`
- `YamlUnitParsers`
- `ExplorationSpaceResolver`
- `IntValueDomain`
- `BankFactory`
- `WireFactory`

- `EvaCamConfig`
