#include "EvaCamConfig.h"
#include "formula.h"
#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "Result.h"
#include "TechnologyTables.h"
#include "YamlHelpers.h"
#include <filesystem>
//#include <magic_enum.hpp>

namespace {
    std::vector<int> BuildPow2Values(int minValue, int maxValue) {
        std::vector<int> values;
        for (int value = minValue; value <= maxValue; value *= 2) {
            values.push_back(value);
            if (value == 0)
                break;
        }
        return values;
    }

    std::vector<int> BuildSequentialValues(int minValue, int maxValue) {
        std::vector<int> values;
        for (int value = minValue; value <= maxValue; value++)
            values.push_back(value);
        return values;
    }

    const char *ToString(DeviceRoadmap roadmap) {
        switch (roadmap) {
            case HP: return "HP";
            case LSTP: return "LSTP";
            case LOP: return "LOP";
            case FEFET: return "FEFET";
            case LP: return "LP";
            default: return "Unknown";
        }
    }

    const char *ToString(MemCellType type) {
        switch (type) {
            case SRAM: return "SRAM";
            case DRAM: return "DRAM";
            case eDRAM: return "eDRAM";
            case MRAM: return "MRAM";
            case PCRAM: return "PCRAM";
            case memristor: return "Memristor";
            case FBRAM: return "FBRAM";
            case SLCNAND: return "SLC NAND";
            case MLCNAND: return "MLC NAND";
            case FEFETRAM: return "FEFET RAM";
            default: return "Unknown";
        }
    }

    const char *ToString(SearchFunction function) {
        switch (function) {
            case EX: return "Exact match";
            case BE: return "Best match";
            case TH: return "Threshold match";
            default: return "Unknown";
        }
    }

    const char *ToString(CacheAccessMode mode) {
        switch (mode) {
            case normal_access_mode: return "Normal";
            case sequential_access_mode: return "Sequential";
            case fast_access_mode: return "Fast";
            default: return "Unknown";
        }
    }

    const char *ToString(WriteScheme scheme) {
        switch (scheme) {
            case set_before_reset: return "Set before reset";
            case reset_before_set: return "Reset before set";
            case erase_before_set: return "Erase before set";
            case erase_before_reset: return "Erase before reset";
            case write_and_verify: return "Write and verify";
            case normal_write: return "Normal write";
            default: return "Unknown";
        }
    }

    const char *ToString(TypeOfInputEncoder type) {
        switch (type) {
            case encoding_two_bit: return "Two-bit";
            default: return "Unknown";
        }
    }

    const char *ToString(TypeOfSenseAmp type) {
        switch (type) {
            case nvsim_voltage_sense: return "Voltage sense";
            case nvsim_current_sense: return "Current sense";
            case self_clock_sense: return "Self-clock sense";
            case dual_threshold_sense: return "Dual-threshold sense";
            case discharge: return "Discharge";
            default: return "Unknown";
        }
    }

    const char *ToString(BufferDesignTarget target) {
        switch (target) {
            case latency_first: return "Latency first";
            case latency_area_trade_off: return "Latency/area trade-off";
            case area_first: return "Area first";
            default: return "Unknown";
        }
    }

    void PrintConstraintIfSet(const char *label, double value) {
        if (value < 1e40)
            std::cout << " - " << label << ": " << value << std::endl;
    }
}

EvaCamConfig::EvaCamConfig() {
    designTarget = cache;
    searchFunction = EX;
    optimizationTarget = read_latency_optimized;
    processNode = 90;
    maxDriverCurrent = 0;

    maxNmosSize = MAX_NMOS_SIZE;

    minNumRowMat = 1;
    maxNumRowMat = 16;
    minNumColumnMat = 1;
    maxNumColumnMat = 16;
    minNumActiveMatPerRow = 1;
    maxNumActiveMatPerRow = maxNumColumnMat;
    minNumActiveMatPerColumn = 1;
    maxNumActiveMatPerColumn = maxNumRowMat;
    minNumRowSubarray = 1;
    maxNumRowSubarray = 16;
    minNumColumnSubarray = 1;
    maxNumColumnSubarray = 16;
    minNumActiveSubarrayPerRow = 1;
    maxNumActiveSubarrayPerRow = maxNumColumnSubarray;
    minNumActiveSubarrayPerColumn = 1;
    maxNumActiveSubarrayPerColumn = maxNumRowSubarray;
    minNumRow = 16;
    maxNumRow = 512;
    minNumColumn = 16;
    maxNumColumn = 512;
    minMuxSenseAmp = 1;
    maxMuxSenseAmp = 32;
    minMuxOutputLev1 = 1;
    maxMuxOutputLev1 = 32;
    minMuxOutputLev2 = 1;
    maxMuxOutputLev2 = 32;
    minNumRowPerSet = 1;
    maxNumRowPerSet = 32;
    minAreaOptimizationLevel = latency_first;
    maxAreaOptimizationLevel = area_first;
    minLocalWireType = local_aggressive;
    maxLocalWireType = local_conservative;
    minGlobalWireType = global_aggressive;
    maxGlobalWireType = global_conservative;
    minLocalWireRepeaterType = repeated_none;
    maxLocalWireRepeaterType = repeated_50;		/* The limit is repeated_50 */
    minGlobalWireRepeaterType = repeated_none;
    maxGlobalWireRepeaterType = repeated_50;	/* The limit is repeated_50 */
    minIsLocalWireLowSwing = false;
    maxIsLocalWireLowSwing = true;
    minIsGlobalWireLowSwing = false;
    maxIsGlobalWireLowSwing = true;

    associativity = 1;				/* Default value for non-cache design */
    routingMode = h_tree;
    internalSensing = true;

    useCactiAssumption = false;

    writeScheme = normal_write;
    cacheAccessMode = normal_access_mode;

    readLatencyConstraint = 1e41;
    writeLatencyConstraint = 1e41;
    readDynamicEnergyConstraint = 1e41;
    writeDynamicEnergyConstraint = 1e41;
    leakageConstraint = 1e41;
    areaConstraint = 1e41;
    readEdpConstraint = 1e41;
    writeEdpConstraint = 1e41;
    isConstraintApplied = false;
    isPruningEnabled = false;

    pageSize = 0;
    flashBlockSize = 0;

    outputFilePrefix = "results/output";	/* Default output file name */
    // TODO: added new initial
    withWriteDriver = false;
    withInputBuffer = false;
    withOutputBuffer = false;
    withInputEnc = false;
    customInputEnc = false;
    typeInputEnc = encoding_two_bit;
    typeSenseAmp = nvsim_voltage_sense;
    customSenseAmp = false;
    withOutputAcc = false;
    withPriorityEnc = false;

    realCapacity = 0;
    NoPrechargeInc = false;
    IncludeLeakge = false;
    scaledVoltage = 0;
    UseUpdatedLib = false;

    minBitSerialWidth = 8;
    maxBitSerialWidth = 1024;
    minRowDriverOptLevel = latency_first;
    maxRowDriverOptLevel = area_first;
    minPriorityOptLevel = latency_first;
    maxPriorityOptLevel = area_first;

    MatchlineSenseMargin = 3e-8;

    // TODO: ensure these are right
    AddCapOnML = 0;

}

void EvaCamConfig::SetDeepExploration(bool enabled) {
    minNumRowMat = 1;
    maxNumRowMat = enabled ? 64 : 4;
    minNumColumnMat = 1;
    maxNumColumnMat = enabled ? 64 : 4;
    minNumActiveMatPerRow = 1;
    maxNumActiveMatPerRow = maxNumColumnMat;
    minNumActiveMatPerColumn = 1;
    maxNumActiveMatPerColumn = maxNumRowMat;
    minNumRowSubarray = 1;
    maxNumRowSubarray = enabled ? 16 : 8;
    minNumColumnSubarray = 1;
    maxNumColumnSubarray = enabled ? 16 : 8;
    minNumActiveSubarrayPerRow = 1;
    maxNumActiveSubarrayPerRow = maxNumColumnSubarray;
    minNumActiveSubarrayPerColumn = 1;
    maxNumActiveSubarrayPerColumn = maxNumRowSubarray;
    minMuxSenseAmp = 1;
    maxMuxSenseAmp = enabled ? 64 : 32;
    minMuxOutputLev1 = 1;
    maxMuxOutputLev1 = enabled ? 64 : 32;
    minMuxOutputLev2 = 1;
    maxMuxOutputLev2 = enabled ? 64 : 32;
    minNumRowPerSet = 1;
    maxNumRowPerSet = enabled ? 1 : associativity;
    minAreaOptimizationLevel = latency_first;
    maxAreaOptimizationLevel = area_first;
    minLocalWireType = local_aggressive;
    maxLocalWireType = enabled ? local_conservative : semi_conservative;
    minGlobalWireType = enabled ? global_aggressive : semi_aggressive;
    maxGlobalWireType = global_conservative;
    minLocalWireRepeaterType = repeated_none;
    maxLocalWireRepeaterType = enabled ? repeated_opt : repeated_50;
    minGlobalWireRepeaterType = repeated_none;
    maxGlobalWireRepeaterType = enabled ? repeated_opt : repeated_50;
    minIsLocalWireLowSwing = false;
    maxIsLocalWireLowSwing = true;
    minIsGlobalWireLowSwing = false;
    maxIsGlobalWireLowSwing = true;
}

std::string EvaCamConfig::DefaultResultsYamlPath(const std::string &inputFile) const {
    std::filesystem::path inputPath(inputFile);
    std::string base = inputPath.stem().string();
    const std::string suffix1 = "_config";
    const std::string suffix2 = "-config";

    if (base.size() > suffix1.size()
            && base.compare(base.size() - suffix1.size(), suffix1.size(), suffix1) == 0) {
        base.resize(base.size() - suffix1.size());
    } else if (base.size() > suffix2.size()
            && base.compare(base.size() - suffix2.size(), suffix2.size(), suffix2) == 0) {
        base.resize(base.size() - suffix2.size());
    }

    return (std::filesystem::path("results") / (base + "_results.yaml")).string();
}

std::string EvaCamConfig::ExplorationCsvPath() const {
    std::stringstream temp;
    temp << outputFilePrefix << "_" << capacity / 1024
        << "K_" << wordWidth << "_" << associativity;

    if (internalSensing) temp << "_IN";
    else                 temp << "_EX";

    if (cell->readMode) temp << "_VOL";
    else                temp << "_CUR";

    temp << ".csv";
    return temp.str();
}

std::shared_ptr<Wire> EvaCamConfig::CreateDefaultLocalWire() const {
    auto wire = std::make_shared<Wire>();
    WireType wireType = (minLocalWireType == maxLocalWireType)
        ? static_cast<WireType>(minLocalWireType)
        : local_aggressive;
    WireRepeaterType repeaterType = (minLocalWireRepeaterType == maxLocalWireRepeaterType)
        ? static_cast<WireRepeaterType>(minLocalWireRepeaterType)
        : repeated_none;
    bool isLowSwing = (minIsLocalWireLowSwing == maxIsLocalWireLowSwing)
        ? static_cast<bool>(minIsLocalWireLowSwing)
        : false;

    wire->Initialize(processNode, wireType, repeaterType, temperature, isLowSwing,
            std::const_pointer_cast<EvaCamConfig>(shared_from_this()));
    return wire;
}

std::shared_ptr<Wire> EvaCamConfig::CreateDefaultGlobalWire() const {
    auto wire = std::make_shared<Wire>();
    WireType wireType = (minGlobalWireType == maxGlobalWireType)
        ? static_cast<WireType>(minGlobalWireType)
        : global_aggressive;
    WireRepeaterType repeaterType = (minGlobalWireRepeaterType == maxGlobalWireRepeaterType)
        ? static_cast<WireRepeaterType>(minGlobalWireRepeaterType)
        : repeated_none;
    bool isLowSwing = (minIsGlobalWireLowSwing == maxIsGlobalWireLowSwing)
        ? static_cast<bool>(minIsGlobalWireLowSwing)
        : false;

    wire->Initialize(processNode, wireType, repeaterType, temperature, isLowSwing,
            std::const_pointer_cast<EvaCamConfig>(shared_from_this()));
    return wire;
}

long long EvaCamConfig::EffectiveCapacityBits() const {
    return static_cast<long long>(capacity) * 8;
}

long EvaCamConfig::EffectiveBlockSizeBits() const {
    return capacity * 8 / wordWidth;
}

int EvaCamConfig::EffectiveAssociativity() const {
    return 1;
}

bool EvaCamConfig::HasFixedOuterGeometry() const {
    return (minNumRowMat == maxNumRowMat)
        && (minNumColumnMat == maxNumColumnMat)
        && (minNumRowSubarray == maxNumRowSubarray)
        && (minNumColumnSubarray == maxNumColumnSubarray);
}

std::vector<int> EvaCamConfig::NumRowMatValues() const {
    return BuildPow2Values(minNumRowMat, maxNumRowMat);
}

std::vector<int> EvaCamConfig::NumColumnMatValues() const {
    return BuildPow2Values(minNumColumnMat, maxNumColumnMat);
}

std::vector<int> EvaCamConfig::NumRowSubarrayValues() const {
    return BuildPow2Values(minNumRowSubarray, maxNumRowSubarray);
}

std::vector<int> EvaCamConfig::NumActiveMatPerRowValues(int numColumnMat) const {
    return BuildPow2Values(MIN(numColumnMat, minNumActiveMatPerRow),
            MIN(numColumnMat, maxNumActiveMatPerRow));
}

std::vector<int> EvaCamConfig::NumActiveMatPerColumnValues(int numRowMat) const {
    return BuildPow2Values(MIN(numRowMat, minNumActiveMatPerColumn),
            MIN(numRowMat, maxNumActiveMatPerColumn));
}

std::vector<int> EvaCamConfig::NumColumnSubarrayValues() const {
    return BuildPow2Values(minNumColumnSubarray, maxNumColumnSubarray);
}

std::vector<int> EvaCamConfig::NumActiveSubarrayPerRowValues(int numColumnSubarray) const {
    return BuildPow2Values(MIN(numColumnSubarray, minNumActiveSubarrayPerRow),
            MIN(numColumnSubarray, maxNumActiveSubarrayPerRow));
}

std::vector<int> EvaCamConfig::NumActiveSubarrayPerColumnValues(int numRowSubarray) const {
    return BuildPow2Values(MIN(numRowSubarray, minNumActiveSubarrayPerColumn),
            MIN(numRowSubarray, maxNumActiveSubarrayPerColumn));
}

std::vector<int> EvaCamConfig::MuxSenseAmpValues() const {
    return BuildPow2Values(minMuxSenseAmp, maxMuxSenseAmp);
}

std::vector<int> EvaCamConfig::MuxOutputLev1Values() const {
    return BuildPow2Values(minMuxOutputLev1, maxMuxOutputLev1);
}

std::vector<int> EvaCamConfig::MuxOutputLev2Values() const {
    return BuildPow2Values(minMuxOutputLev2, maxMuxOutputLev2);
}

std::vector<int> EvaCamConfig::NumRowPerSetValues() const {
    return BuildPow2Values(minNumRowPerSet, MIN(maxNumRowPerSet, associativity));
}

std::vector<int> EvaCamConfig::AreaOptimizationLevels() const {
    return BuildSequentialValues(minAreaOptimizationLevel, maxAreaOptimizationLevel);
}

std::vector<int> EvaCamConfig::RowDriverOptimizationLevels() const {
    return BuildSequentialValues(minRowDriverOptLevel, maxRowDriverOptLevel);
}

std::vector<int> EvaCamConfig::PriorityOptimizationLevels() const {
    return BuildSequentialValues(minPriorityOptLevel, maxPriorityOptLevel);
}

std::vector<int> EvaCamConfig::BitSerialWidthValues() const {
    return BuildPow2Values(minBitSerialWidth, maxBitSerialWidth);
}

bool EvaCamConfig::IsValidPartitioning(long blockSizeBits, int numActiveMatPerRow, int numActiveMatPerColumn,
        int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn) const {
    return blockSizeBits / (numActiveMatPerRow * numActiveMatPerColumn
            * numActiveSubarrayPerRow * numActiveSubarrayPerColumn) != 0;
}

std::shared_ptr<Bank> EvaCamConfig::CreateBank() const {
    if (routingMode == h_tree)
        return std::make_shared<BankWithHtree>();
    return std::make_shared<BankWithoutHtree>();
}

void EvaCamConfig::InitializeBank(const std::shared_ptr<Bank> &bank, int numRowMat, int numColumnMat,
        long long capacityBits, long blockSizeBits, int associativityValue, int numRowPerSet,
        int numActiveMatPerRow, int numActiveMatPerColumn, int muxSenseAmp, int muxOutputLev1,
        int muxOutputLev2, int numRowSubarray, int numColumnSubarray,
        int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn,
        BufferDesignTarget areaOptimizationLevel, MemoryType memoryType,
        const std::shared_ptr<Wire> &localWire, const std::shared_ptr<Wire> &globalWire,
        const std::shared_ptr<CAM_Opt> &camOpt) const {
    bank->Initialize(numRowMat, numColumnMat, capacityBits, blockSizeBits, associativityValue, numRowPerSet,
            numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp, internalSensing, muxOutputLev1,
            muxOutputLev2, numRowSubarray, numColumnSubarray, numActiveSubarrayPerRow,
            numActiveSubarrayPerColumn, areaOptimizationLevel, memoryType, cell->camType,
            searchFunction, std::const_pointer_cast<EvaCamConfig>(shared_from_this()), localWire,
            globalWire, camOpt);
}

bool EvaCamConfig::IsFullExploration() const {
    return optimizationTarget == full_exploration;
}

bool EvaCamConfig::ShouldWriteExplorationCsv() const {
    return IsFullExploration() && !isPruningEnabled;
}

bool EvaCamConfig::IsPruningEnabledForExploration() const {
    return IsFullExploration() && isPruningEnabled;
}

ResultLimits EvaCamConfig::BuildResultLimits(const std::vector<std::shared_ptr<Result>> &bestResults) const {
    ResultLimits limits{};
    limits.readLatency = bestResults[read_latency_optimized]->bank->readLatency * (readLatencyConstraint + 1);
    limits.writeLatency = bestResults[write_latency_optimized]->bank->writeLatency * (writeLatencyConstraint + 1);
    limits.readDynamicEnergy = bestResults[read_energy_optimized]->bank->readDynamicEnergy
        * (readDynamicEnergyConstraint + 1);
    limits.writeDynamicEnergy = bestResults[write_energy_optimized]->bank->writeDynamicEnergy
        * (writeDynamicEnergyConstraint + 1);
    limits.leakage = bestResults[leakage_optimized]->bank->leakage * (leakageConstraint + 1);
    limits.area = bestResults[area_optimized]->bank->area * (areaConstraint + 1);
    limits.readEdp = bestResults[read_edp_optimized]->bank->readLatency
        * bestResults[read_edp_optimized]->bank->readDynamicEnergy * (readEdpConstraint + 1);
    limits.writeEdp = bestResults[write_edp_optimized]->bank->writeLatency
        * bestResults[write_edp_optimized]->bank->writeDynamicEnergy * (writeEdpConstraint + 1);
    return limits;
}

void EvaCamConfig::ApplyResultLimits(const ResultLimits &limits,
        const std::vector<std::shared_ptr<Result>> &results) const {
    for (const auto &result : results) {
        result->reset();
        result->limitReadLatency = limits.readLatency;
        result->limitWriteLatency = limits.writeLatency;
        result->limitReadDynamicEnergy = limits.readDynamicEnergy;
        result->limitWriteDynamicEnergy = limits.writeDynamicEnergy;
        result->limitReadEdp = limits.readEdp;
        result->limitWriteEdp = limits.writeEdp;
        result->limitArea = limits.area;
        result->limitLeakage = limits.leakage;
    }
}

void EvaCamConfig::ReadConfigFromFile(const std::string &inputFile) {

    const YAML::Node config = YAML::LoadFile(inputFile);

    // Design

    auto design = YamlHelpers::child_required(config, "design");

    designTarget = YamlHelpers::read_enum_required<DesignTarget>(design, "target");
    if (designTarget != CAM_chip)
        throw std::runtime_error("[Error]: Only support CAM design.");
    minNumRowPerSet = 1;
    maxNumRowPerSet = 256;

    searchFunction = YamlHelpers::read_enum_required<SearchFunction>(design, "search_function");

    double processNodeM =
        YamlHelpers::read_quantity_required(design, "process_node", YamlHelpers::LengthUnits(), 1e-9, "process_node");
    processNode = (int)std::lround(processNodeM / 1e-9);

    constexpr std::array<int, 9> supportedProcessNodes = {7, 10, 14, 22, 32, 45, 90, 120, 200};
    if (!std::binary_search(supportedProcessNodes.begin(), supportedProcessNodes.end(), processNode))
        logger.Verbose() << "[Warning]: Possible error as " << processNode << "nm proccess node is not supported.";
    if (processNode <= 45) {
        std::string ptm = "[Warning]: When using PTM model, ";
        logger.Verbose() << ptm << "only HP/LSTP model is provided.";
        logger.Verbose() << ptm << "interpolation between 45/32/22/14/10/7nm may lead to error.";
    }

    deviceRoadmap = YamlHelpers::read_enum_required<DeviceRoadmap>(design, "device_roadmap");

    temperature =
        (int)std::lround(YamlHelpers::read_quantity_required(design, "temperature", YamlHelpers::TemperatureUnits(), 1.0, "temperature"));

    // Memory

    auto memory = YamlHelpers::child_required(config, "memory");

    fileMemCell = YamlHelpers::read_required<std::string>(memory, "cell_file");

    capacity = (int64_t)std::llround(
            YamlHelpers::read_quantity_required(memory, "capacity", YamlHelpers::DataSizeUnits(), 1.0, "capacity"));

    wordWidth = (long)std::lround(
            YamlHelpers::read_quantity_required(memory, "word_width", YamlHelpers::BitUnits(), 1.0, "word_width"));
    minBitSerialWidth = maxBitSerialWidth = (int)wordWidth;

    // Routing

    auto routing = YamlHelpers::child_required(config, "routing");

    routingMode = YamlHelpers::read_enum_required<RoutingMode>(routing, "type");

    if (routingMode == non_h_tree) throw std::runtime_error("[Input] Error: non H-tree is under development!");

    // Peripherals

    auto peripherals = YamlHelpers::child_required(config, "peripherals");

    withWriteDriver = YamlHelpers::read_required<bool>(peripherals, "write_driver");

    auto pinput = YamlHelpers::child_required(peripherals, "input");

    withInputBuffer = YamlHelpers::read_required<bool>(pinput, "buffer");
    withInputEnc    = YamlHelpers::read_required<bool>(pinput, "encoder");
    customInputEnc  = YamlHelpers::read_required<bool>(pinput, "custom_encoder");

    auto poutput = YamlHelpers::child_required(peripherals, "output");

    withOutputBuffer = YamlHelpers::read_required<bool>(poutput, "buffer");
    withPriorityEnc  = YamlHelpers::read_required<bool>(poutput, "priority_encoder");
    withOutputAcc    = YamlHelpers::read_required<bool>(poutput, "accumulator");

    if (!withOutputAcc) minBitSerialWidth = maxBitSerialWidth;

    // Sensing

    auto sensing = YamlHelpers::child_required(config, "sensing");

    internalSensing = YamlHelpers::read_required<bool>(sensing, "internal");
    customSenseAmp  = YamlHelpers::read_required<bool>(sensing, "custom_sense_amp");

    typeSenseAmp = YamlHelpers::read_enum_required<TypeOfSenseAmp>(sensing, "amplifier_type");

    // Optimization

    const auto opt = YamlHelpers::child_required(config, "optimization");
    optimizationTarget = YamlHelpers::read_enum_required<OptimizationTarget>(opt, "target");

    auto bufferDesign = YamlHelpers::read_enum_required<BufferDesignTarget>(opt, "buffer_design");
    minAreaOptimizationLevel = maxAreaOptimizationLevel = (int)bufferDesign;

    auto rowDriver = YamlHelpers::read_enum_required<BufferDesignTarget>(opt, "row_driver");
    minRowDriverOptLevel = maxRowDriverOptLevel = (int)rowDriver;

    auto priority = YamlHelpers::read_enum_required<BufferDesignTarget>(opt, "priority_encoder");
    minPriorityOptLevel = maxPriorityOptLevel = (int)priority;

    // Wires

    auto wiresLocal = YamlHelpers::child_required(YamlHelpers::child_required(config, "wires"), "local");
    auto localType = YamlHelpers::read_enum_required<WireType>(wiresLocal, "type");
    minLocalWireType = maxLocalWireType = (int)localType;

    auto localRepeater = YamlHelpers::read_enum_required<WireRepeaterType>(wiresLocal, "repeater");
    minLocalWireRepeaterType = maxLocalWireRepeaterType = (int)localRepeater;

    minIsLocalWireLowSwing = maxIsLocalWireLowSwing = YamlHelpers::read_required<bool>(wiresLocal, "low_swing");

    auto wiresGlobal = YamlHelpers::child_required(YamlHelpers::child_required(config, "wires"), "global");
    auto globalType = YamlHelpers::read_enum_required<WireType>(wiresGlobal, "type");
    minGlobalWireType = maxGlobalWireType = (int)globalType;

    auto globalRepeater = YamlHelpers::read_enum_required<WireRepeaterType>(wiresGlobal, "repeater");
    minGlobalWireRepeaterType = maxGlobalWireRepeaterType = (int)globalRepeater;

    minIsGlobalWireLowSwing = maxIsGlobalWireLowSwing = YamlHelpers::read_required<bool>(wiresGlobal, "low_swing");

    // Array
    // If array is omitted, or if specific subsections are omitted, keep the
    // current exploration ranges/defaults for those dimensions.
    auto array = YamlHelpers::child_optional(config, "array");
    if (array) {
        auto banks = YamlHelpers::child_optional(array, "banks");
        if (banks) {
            auto banks_total = YamlHelpers::child_required(banks, "total");
            auto banks_active = YamlHelpers::child_required(banks, "active");

            minNumRowMat             = maxNumRowMat             = YamlHelpers::read_required_index<int>(banks_total, 0, "array.banks.total[0]");
            minNumColumnMat          = maxNumColumnMat          = YamlHelpers::read_required_index<int>(banks_total, 1, "array.banks.total[1]");
            minNumActiveMatPerColumn = maxNumActiveMatPerColumn = YamlHelpers::read_required_index<int>(banks_active, 0, "array.banks.active[0]");
            minNumActiveMatPerRow    = maxNumActiveMatPerRow    = YamlHelpers::read_required_index<int>(banks_active, 1, "array.banks.active[1]");
        }

        auto mats = YamlHelpers::child_optional(array, "mats");
        if (mats) {
            auto mats_total = YamlHelpers::child_required(mats, "total");
            auto mats_active = YamlHelpers::child_required(mats, "active");

            minNumRowSubarray = maxNumRowSubarray = YamlHelpers::read_required_index<int>(mats_total, 0, "array.mats.total[0]");
            minNumColumnSubarray = maxNumColumnSubarray = YamlHelpers::read_required_index<int>(mats_total, 1, "array.mats.total[1]");
            minNumActiveSubarrayPerColumn = maxNumActiveSubarrayPerColumn = YamlHelpers::read_required_index<int>(mats_active, 0, "array.mats.active[0]");
            minNumActiveSubarrayPerRow = maxNumActiveSubarrayPerRow = YamlHelpers::read_required_index<int>(mats_active, 1, "array.mats.active[1]");
        }

        auto mux = YamlHelpers::child_optional(array, "mux");
        if (mux) {
            if (YamlHelpers::child_optional(mux, "sense_amp")) {
                maxMuxSenseAmp = minMuxSenseAmp = YamlHelpers::read_required<int>(mux, "sense_amp");
            }
            if (YamlHelpers::child_optional(mux, "output_level1")) {
                maxMuxOutputLev1 = minMuxOutputLev1 = YamlHelpers::read_required<int>(mux, "output_level1");
            }
            if (YamlHelpers::child_optional(mux, "output_level2")) {
                maxMuxOutputLev2 = minMuxOutputLev2 = YamlHelpers::read_required<int>(mux, "output_level2");
            }
        }
    }

    // Matchline

    auto matchline = YamlHelpers::child_optional(config, "matchline");
    if (matchline) {
        AddCapOnML =
            YamlHelpers::read_quantity_required(matchline, "additional_cap", YamlHelpers::CapacitanceUnits(), 1e-15, "matchline.additional_cap");
    }

    auto constraints = YamlHelpers::child_optional(config, "constraints");
    if (constraints) {
        bool anyConstraintSet = false;
        bool enabled = YamlHelpers::read_optional<bool>(constraints, "enabled", false);

        if (YamlHelpers::child_optional(constraints, "read_latency")) {
            readLatencyConstraint = YamlHelpers::read_quantity_required(
                    constraints, "read_latency", YamlHelpers::TimeUnits(), 1.0, "constraints.read_latency");
            anyConstraintSet = true;
        }
        if (YamlHelpers::child_optional(constraints, "write_latency")) {
            writeLatencyConstraint = YamlHelpers::read_quantity_required(
                    constraints, "write_latency", YamlHelpers::TimeUnits(), 1.0, "constraints.write_latency");
            anyConstraintSet = true;
        }
        if (YamlHelpers::child_optional(constraints, "read_dynamic_energy")) {
            readDynamicEnergyConstraint = YamlHelpers::read_quantity_required(
                    constraints, "read_dynamic_energy", YamlHelpers::EnergyUnits(), 1.0, "constraints.read_dynamic_energy");
            anyConstraintSet = true;
        }
        if (YamlHelpers::child_optional(constraints, "write_dynamic_energy")) {
            writeDynamicEnergyConstraint = YamlHelpers::read_quantity_required(
                    constraints, "write_dynamic_energy", YamlHelpers::EnergyUnits(), 1.0, "constraints.write_dynamic_energy");
            anyConstraintSet = true;
        }
        if (YamlHelpers::child_optional(constraints, "leakage")) {
            leakageConstraint = YamlHelpers::read_quantity_required(
                    constraints, "leakage", YamlHelpers::PowerUnits(), 1.0, "constraints.leakage");
            anyConstraintSet = true;
        }
        if (YamlHelpers::child_optional(constraints, "area")) {
            areaConstraint = YamlHelpers::read_required<double>(constraints, "area");
            anyConstraintSet = true;
        }
        if (YamlHelpers::child_optional(constraints, "read_edp")) {
            readEdpConstraint = YamlHelpers::read_required<double>(constraints, "read_edp");
            anyConstraintSet = true;
        }
        if (YamlHelpers::child_optional(constraints, "write_edp")) {
            writeEdpConstraint = YamlHelpers::read_required<double>(constraints, "write_edp");
            anyConstraintSet = true;
        }
        isConstraintApplied = enabled || anyConstraintSet;
    }

    auto advanced = YamlHelpers::child_optional(config, "advanced");
    if (advanced) {
        if (YamlHelpers::child_optional(advanced, "max_nmos_size")) {
            maxNmosSize = YamlHelpers::read_quantity_required(
                    advanced, "max_nmos_size", YamlHelpers::FeatureUnits(), 1.0, "advanced.max_nmos_size");
        }
        useCactiAssumption = YamlHelpers::read_optional<bool>(
                advanced, "use_cacti_assumption", useCactiAssumption);
        if (useCactiAssumption) {
            minNumActiveMatPerRow = maxNumColumnMat;
            maxNumActiveMatPerRow = maxNumColumnMat;
            minNumActiveMatPerColumn = 1;
            maxNumActiveMatPerColumn = 1;
            minNumRowSubarray = 2;
            maxNumRowSubarray = 2;
            minNumColumnSubarray = 2;
            maxNumColumnSubarray = 2;
            minNumActiveSubarrayPerRow = 2;
            maxNumActiveSubarrayPerRow = 2;
            minNumActiveSubarrayPerColumn = 2;
            maxNumActiveSubarrayPerColumn = 2;
        }
        isPruningEnabled = YamlHelpers::read_optional<bool>(
                advanced, "enable_pruning", isPruningEnabled);
        if (YamlHelpers::child_optional(advanced, "bit_serial_width")) {
            const long bsw = (long)std::lround(YamlHelpers::read_quantity_required(
                        advanced, "bit_serial_width", YamlHelpers::BitUnits(), 1.0, "advanced.bit_serial_width"));
            minBitSerialWidth = maxBitSerialWidth = (int)bsw;
        }
        if (YamlHelpers::child_optional(advanced, "input_encoder_type")) {
            typeInputEnc = YamlHelpers::read_enum_required<TypeOfInputEncoder>(
                    advanced, "input_encoder_type", false);
        }
        fileCustomSA = YamlHelpers::read_optional<std::string>(
                advanced, "custom_sa_input_file", fileCustomSA);
        UseUpdatedLib = YamlHelpers::read_optional<bool>(
                advanced, "use_updated_lib", UseUpdatedLib);
        NoPrechargeInc = YamlHelpers::read_optional<bool>(
                advanced, "exclude_precharge_latency", NoPrechargeInc);
        IncludeLeakge = YamlHelpers::read_optional<bool>(
                advanced, "include_leakage", IncludeLeakge);
        scaledVoltage = YamlHelpers::read_optional<double>(
                advanced, "scaled_voltage", scaledVoltage);
    }

    auto cacheCfg = YamlHelpers::child_optional(config, "cache");
    if (cacheCfg) {
        associativity = YamlHelpers::read_optional<int>(cacheCfg, "associativity", associativity);
        if (YamlHelpers::child_optional(cacheCfg, "access_mode")) {
            cacheAccessMode = YamlHelpers::read_enum_required<CacheAccessMode>(
                    cacheCfg, "access_mode", false);
        }
        if (YamlHelpers::child_optional(cacheCfg, "write_scheme")) {
            writeScheme = YamlHelpers::read_enum_required<WriteScheme>(
                    cacheCfg, "write_scheme", false);
        }
    }

    auto flash = YamlHelpers::child_optional(config, "flash");
    if (flash) {
        if (YamlHelpers::child_optional(flash, "page_size")) {
            pageSize = (long)std::lround(YamlHelpers::read_quantity_required(
                        flash, "page_size", YamlHelpers::DataSizeUnits(), 1.0, "flash.page_size") * 8.0);
        }
        if (YamlHelpers::child_optional(flash, "block_size")) {
            flashBlockSize = (long)std::lround(YamlHelpers::read_quantity_required(
                        flash, "block_size", YamlHelpers::DataSizeUnits(), 1.0, "flash.block_size") * 8.0);
        }
    }

    auto extra = YamlHelpers::child_optional(config, "extra");
    if (extra) {
        outputFilePrefix = YamlHelpers::read_optional<std::string>(
                extra, "output_file_prefix", outputFilePrefix);
        if (YamlHelpers::child_optional(extra, "worst_case_sense_margin")) {
            MatchlineSenseMargin = YamlHelpers::read_quantity_required(
                    extra, "worst_case_sense_margin", YamlHelpers::VoltageUnits(), 1.0, "extra.worst_case_sense_margin");
            sensemargin = MatchlineSenseMargin;
        }
        if (YamlHelpers::child_optional(extra, "max_driver_current")) {
            maxDriverCurrent = YamlHelpers::read_quantity_required(
                    extra, "max_driver_current", YamlHelpers::CurrentUnits(), 1.0, "extra.max_driver_current");
        }
        // Preferred YAML form, e.g. `real_capacity: 9KB`.
        auto realCapacityNode = YamlHelpers::child_optional(extra, "real_capacity");
        if (realCapacityNode) {
            realCapacity = (int64_t)std::llround(
                    YamlHelpers::parse_quantity_node(
                        realCapacityNode, YamlHelpers::DataSizeUnits(), 1.0, "extra.real_capacity"));
        } else {
            // Backward-compatible aliases used by older configs.
            auto realCapacityB = YamlHelpers::child_optional(extra, "RealCapacity (B)");
            auto realCapacityKB = YamlHelpers::child_optional(extra, "RealCapacity (KB)");
            auto realCapacityMB = YamlHelpers::child_optional(extra, "RealCapacity (MB)");

            if (realCapacityB) {
                realCapacity = (int64_t)std::llround(
                        YamlHelpers::read_scalar_required<double>(realCapacityB, "extra.RealCapacity (B)"));
            } else if (realCapacityKB) {
                realCapacity = (int64_t)std::llround(
                        YamlHelpers::read_scalar_required<double>(realCapacityKB, "extra.RealCapacity (KB)") * 1024.0);
            } else if (realCapacityMB) {
                realCapacity = (int64_t)std::llround(
                        YamlHelpers::read_scalar_required<double>(realCapacityMB, "extra.RealCapacity (MB)") * 1024.0 * 1024.0);
            }
        }
    }

    if (wordWidth <= 0) {
        throw std::runtime_error("[Input] Error: word_width must be > 0.");
    }
    const bool isWordWidthPow2 = (wordWidth & (wordWidth - 1)) == 0;
    if (!isWordWidthPow2 && realCapacity == 0) {
        throw std::runtime_error(
                "[Input] Error: non-power-of-two word_width requires extra.real_capacity to be set.");
    }
    if (realCapacity > 0) {
        if (realCapacity < capacity) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity must be >= memory.capacity.");
        }
        const long long denom =
            (long long)minNumRowSubarray * minNumColumnSubarray *
            minNumActiveMatPerRow * minNumActiveMatPerColumn;
        if (denom <= 0) {
            throw std::runtime_error(
                    "[Input] Error: invalid array geometry while validating extra.real_capacity.");
        }
        if ((realCapacity % denom) != 0) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity is incompatible with array geometry.");
        }
        if (((realCapacity / denom) % wordWidth) != 0) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity is incompatible with word_width.");
        }
    }


    TechSetup();
    MemCellSetup();
    FEFETTechSetup();
}


void EvaCamConfig::ValidateSupportedConfiguration() {
    if (cell->memCellType == DRAM)
        throw std::runtime_error("[ERROR] DRAM model is still under development");

    if (cell->memCellType == eDRAM)
        throw std::runtime_error("[ERROR] Embedded DRAM model is still under development");

    if (cell->memCellType == MLCNAND)
        throw std::runtime_error("[ERROR] MLC NAND flash model is still under development");

    if (designTarget != cache && associativity > 1) {
        logger.Verbose() << "[WARNING] Associativity setting is ignored for non-cache designs.";
        associativity = 1;
    }

    if (!isPow2(associativity))
        throw std::runtime_error("[ERROR] The associativity value has to be a power of 2 in this version.");

    if (routingMode == h_tree && internalSensing == false)
        throw std::runtime_error("[ERROR] H-tree does not support external sensing scheme in this version.");

}

void EvaCamConfig::TechSetup() {
    tech = std::make_shared<Technology>();
    tech->Initialize(processNode, deviceRoadmap, UseUpdatedLib);

    auto techHigh = std::make_shared<Technology>();
    double alpha = 0;

    if (processNode > 200) {
        throw std::runtime_error("[Error] Technology node above 200nm not supported.");
    } else if (processNode > 120) {
        techHigh->Initialize(200, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 120.0) / 60;
    } else if (processNode > 90) {
        techHigh->Initialize(120, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 90.0) / 30;
    } else if (processNode > 65) {
        techHigh->Initialize(90, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 65.0) / 25;
    } else if (processNode > 45) {
        techHigh->Initialize(65, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 45.0) / 20;
    } else if (processNode > 32) {
        techHigh->Initialize(45, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 32.0) / 13;
    } else if (processNode > 22) {
        techHigh->Initialize(32, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 22.0) / 10;
    } else if (processNode > 14) {
        techHigh->Initialize(22, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 14.0) / 8;
    } else if (processNode > 10) {
        techHigh->Initialize(14, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 10.0) / 4;
    } else if (processNode > 7) {
        techHigh->Initialize(10, deviceRoadmap, UseUpdatedLib);
        alpha = (processNode - 10.0) / 3;
    } else if (processNode == 7) {
        techHigh->Initialize(7, deviceRoadmap, UseUpdatedLib);
        alpha = 1;
    } else {
        throw std::runtime_error("[Error]: Technology node below 7nm is not supported.");
    }

    tech->InterpolateWith(techHigh, alpha);
}

void EvaCamConfig::MemCellSetup() {
    cell = std::make_shared<MemCell>();
    cell->ReadCellFromFile(fileMemCell, designTarget, tech->vdd());
}

void EvaCamConfig::FEFETTechSetup() {
    FEFET_tech = std::make_shared<Technology>();
    if (FindTechnologySpec(processNode, FEFET, UseUpdatedLib)) {
        FEFET_tech->Initialize(processNode, FEFET, UseUpdatedLib);
        return;
    }

    if (!UseUpdatedLib && FindTechnologySpec(processNode, FEFET, true)) {
        FEFET_tech->Initialize(processNode, FEFET, true);
        return;
    }

    if (FindTechnologySpec(processNode, deviceRoadmap, UseUpdatedLib)) {
        FEFET_tech->Initialize(processNode, deviceRoadmap, UseUpdatedLib);
        return;
    }

    throw std::runtime_error("[Technology] Unsupported FeFET technology configuration.");
}

void EvaCamConfig::PrintConfig() {
    std::cout << std::endl << "====================" << std::endl << "DESIGN SPECIFICATION" << std::endl << "====================" << std::endl;
    std::cout << "Design Target: ";
    switch (designTarget) {
        case cache:
            std::cout << "Cache" << std::endl;
            break;
        case RAM_chip:
            std::cout << "Random Access Memory" << std::endl;
            break;
        default:	/* CAM */
            std::cout << "Content Addressable Memory" << std::endl;
    }

    std::cout << "Capacity   : ";
    if (capacity < 1024)
        std::cout << capacity << "B" << std::endl;
    else if (capacity < 1024 * 1024)
        std::cout << capacity / 1024 << "KB" << std::endl;
    else if (capacity < 1024 * 1024 * 1024)
        std::cout << capacity / 1024 / 1024 << "MB" << std::endl;
    else
        std::cout << capacity / 1024 / 1024 / 1024 << "GB" << std::endl;

    if (designTarget == cache) {
        std::cout << "Cache Line Size: " << wordWidth / 8 << "Bytes" << std::endl;
        std::cout << "Cache Associativity: " << associativity << " Ways" << std::endl;
    } else {
        std::cout << "Data Width : " << wordWidth << "Bits";
        if (wordWidth % 8 == 0)
            std::cout << " (" << wordWidth / 8 << "Bytes)" << std::endl;
        else
            std::cout << std::endl;
    }
    if (designTarget == RAM_chip && (cell->memCellType == SLCNAND || cell->memCellType == MLCNAND)) {
        std::cout << "Page Size  : " << pageSize / 8 << "Bytes" << std::endl;
        std::cout << "Block Size : " << flashBlockSize / 8 / 1024 << "KB" << std::endl;
    }
    std::cout << "Process Node: " << processNode << "nm" << std::endl;
    std::cout << "Device Roadmap: " << ToString(deviceRoadmap) << std::endl;
    std::cout << "Temperature: " << temperature << "K" << std::endl;
    std::cout << "Memory Cell: " << ToString(cell->memCellType) << std::endl;
    std::cout << "Cell File  : " << fileMemCell << std::endl;

    if (designTarget == cache) {
        std::cout << "Cache Access Mode: " << ToString(cacheAccessMode) << std::endl;
    }

    if (designTarget == CAM_chip) {
        std::cout << "Search Function: " << ToString(searchFunction) << std::endl;
    }

    if (designTarget != RAM_chip || (cell->memCellType != SLCNAND && cell->memCellType != MLCNAND)) {
        std::cout << "Write Scheme: " << ToString(writeScheme) << std::endl;
    }

    std::cout << "Routing Mode: " << (routingMode == h_tree ? "H-tree" : "Non-H-tree") << std::endl;
    std::cout << "Sensing: " << (internalSensing ? "Internal" : "External")
              << ", " << ToString(typeSenseAmp);
    if (customSenseAmp)
        std::cout << " (custom)";
    std::cout << std::endl;

    std::cout << "Peripherals:" << std::endl;
    std::cout << " - Write Driver: " << (withWriteDriver ? "enabled" : "disabled") << std::endl;
    if (withInputBuffer || withInputEnc) {
        std::cout << " - Input Path:";
        if (withInputBuffer)
            std::cout << " buffer";
        if (withInputEnc) {
            std::cout << (withInputBuffer ? "," : "") << " encoder=" << ToString(typeInputEnc);
            if (customInputEnc)
                std::cout << " (custom)";
        }
        std::cout << std::endl;
    }
    if (withOutputBuffer || withPriorityEnc || withOutputAcc) {
        std::cout << " - Output Path:";
        bool needSeparator = false;
        if (withOutputBuffer) {
            std::cout << " buffer";
            needSeparator = true;
        }
        if (withPriorityEnc) {
            std::cout << (needSeparator ? "," : "") << " priority encoder";
            needSeparator = true;
        }
        if (withOutputAcc) {
            std::cout << (needSeparator ? "," : "") << " accumulator";
        }
        std::cout << std::endl;
    }

    std::cout << "Optimization:" << std::endl;
    std::cout << " - Buffer Design: " << ToString((BufferDesignTarget)minAreaOptimizationLevel) << std::endl;
    std::cout << " - Row Driver: " << ToString((BufferDesignTarget)minRowDriverOptLevel) << std::endl;
    if (withPriorityEnc)
        std::cout << " - Priority Encoder: " << ToString((BufferDesignTarget)minPriorityOptLevel) << std::endl;
    if (withOutputAcc)
        std::cout << " - Bit Serial Width: " << minBitSerialWidth << std::endl;

    if (isConstraintApplied) {
        std::cout << "Constraints:" << std::endl;
        PrintConstraintIfSet("Read Latency", readLatencyConstraint);
        PrintConstraintIfSet("Write Latency", writeLatencyConstraint);
        PrintConstraintIfSet("Read Dynamic Energy", readDynamicEnergyConstraint);
        PrintConstraintIfSet("Write Dynamic Energy", writeDynamicEnergyConstraint);
        PrintConstraintIfSet("Read EDP", readEdpConstraint);
        PrintConstraintIfSet("Write EDP", writeEdpConstraint);
        PrintConstraintIfSet("Area", areaConstraint);
        PrintConstraintIfSet("Leakage", leakageConstraint);
    }

    if (UseUpdatedLib || NoPrechargeInc || IncludeLeakge || scaledVoltage != 0 || !fileCustomSA.empty() || isPruningEnabled) {
        std::cout << "Advanced Options:" << std::endl;
        if (UseUpdatedLib)
            std::cout << " - Updated Library Enabled" << std::endl;
        if (NoPrechargeInc)
            std::cout << " - Excluding Precharge Latency" << std::endl;
        if (IncludeLeakge)
            std::cout << " - Including Leakage in Results" << std::endl;
        if (scaledVoltage != 0)
            std::cout << " - Scaled Voltage: " << scaledVoltage << std::endl;
        if (!fileCustomSA.empty())
            std::cout << " - Custom Sense Amp File: " << fileCustomSA << std::endl;
        if (isPruningEnabled)
            std::cout << " - Exploration Pruning Enabled" << std::endl;
    }

    if (optimizationTarget == full_exploration) {
        std::cout << std::endl << "Full design space exploration ... might take hours" << std::endl;
    } else {
        std::cout << std::endl << "Searching for the best solution that is optimized for ";
        switch (optimizationTarget) {
            case read_latency_optimized:
                std::cout << "read latency ..." << std::endl;
                break;
            case write_latency_optimized:
                std::cout << "write latency ..." << std::endl;
                break;
            case read_energy_optimized:
                std::cout << "read energy ..." << std::endl;
                break;
            case write_energy_optimized:
                std::cout << "write energy ..." << std::endl;
                break;
            case read_edp_optimized:
                std::cout << "read energy-delay-product ..." << std::endl;
                break;
            case write_edp_optimized:
                std::cout << "write energy-delay-product ..." << std::endl;
                break;
            case leakage_optimized:
                std::cout << "leakage power ..." << std::endl;
                break;
            default:	/* area */
                std::cout << "area ..." << std::endl;
        }
    }
}
