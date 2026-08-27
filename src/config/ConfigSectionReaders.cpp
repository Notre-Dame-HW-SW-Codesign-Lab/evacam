#include "config/ConfigSectionReaders.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

#include "EvaCamConfig.h"
#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

namespace {

bool TryReadOptionalChild(const YAML::Node &parent, const char *key, YAML::Node &child) {
    if (!parent || !parent.IsMap()) {
        return false;
    }

    for (auto it = parent.begin(); it != parent.end(); ++it) {
        if (it->first.IsScalar() && it->first.as<std::string>() == key) {
            child = it->second;
            return true;
        }
    }
    return false;
}

std::array<int, 2> ReadCoordinatePair(const YAML::Node &node, const char *what) {
    if (!node || !node.IsSequence() || node.size() != 2) {
        throw std::runtime_error(
                std::string("[Input] Error: ") + what
                + " must be a two-value sequence [x, y].");
    }
    return {
        YamlHelpers::read_required_index<int>(node, 0, what),
        YamlHelpers::read_required_index<int>(node, 1, what),
    };
}

}  // namespace

namespace ConfigSectionReaders {

void ReadDesignSection(const YAML::Node &root, EvaCamConfig &config) {
    auto design = YamlHelpers::child_required(root, "design");
    config.input.designTarget = YamlHelpers::read_enum_required<DesignTarget>(design, "target");
    config.input.searchFunction = YamlHelpers::read_enum_required<SearchFunction>(design, "search_function");

    const double processNodeM = YamlHelpers::read_quantity_required(
            design, "system_process_node", YamlHelpers::LengthUnits(), 1e-9, "system_process_node");
    config.input.processNode = YamlHelpers::checked_integer<int>(
            processNodeM / 1e-9, "design.system_process_node in nanometers");

    constexpr std::array<int, 9> supportedProcessNodes = {7, 10, 14, 22, 32, 45, 90, 120, 200};
    if (!std::binary_search(supportedProcessNodes.begin(), supportedProcessNodes.end(), config.input.processNode)) {
        config.logger.Verbose() << "[Warning]: Possible error as " << config.input.processNode
                                << "nm proccess node is not supported.";
    }
    if (config.input.processNode <= 45) {
        const std::string ptm = "[Warning]: When using PTM model, ";
        config.logger.Verbose() << ptm << "only HP/LSTP model is provided.";
        config.logger.Verbose() << ptm << "interpolation between 45/32/22/14/10/7nm may lead to error.";
    }

    config.input.deviceRoadmap = YamlHelpers::read_enum_required<DeviceRoadmap>(design, "device_roadmap");
    config.input.temperature = YamlHelpers::checked_integer<int>(
            YamlHelpers::read_quantity_required(
                    design, "temperature", YamlHelpers::TemperatureUnits(), 1.0,
                    "design.temperature"),
            "design.temperature in kelvin");
}

void ReadMemorySection(const YAML::Node &root, EvaCamConfig &config) {
    auto memory = YamlHelpers::child_required(root, "memory");
    config.input.fileMemCell = YamlHelpers::read_required<std::string>(memory, "cell_file");
    auto capacityNode = YamlHelpers::child_optional(memory, "capacity");
    config.runtimeSizing.hasExplicitCapacity = static_cast<bool>(capacityNode);
    config.runtimeSizing.capacityIsAuto = false;
    if (capacityNode) {
        if (capacityNode.IsScalar() && capacityNode.as<std::string>() == "auto") {
            config.runtimeSizing.capacityIsAuto = true;
            config.input.capacity = 0;
        } else {
            config.input.capacity = YamlHelpers::checked_integer<int64_t>(
                    YamlHelpers::parse_quantity_node(
                            capacityNode, YamlHelpers::DataSizeUnits(), 1.0,
                            "memory.capacity"),
                    "memory.capacity in bytes");
        }
    } else {
        config.input.capacity = 0;
    }
    const YAML::Node wordWidth = YamlHelpers::child_optional(memory, "word_width");
    config.input.wordWidth = wordWidth
        ? YamlHelpers::checked_integer<long>(
                YamlHelpers::parse_quantity_node(
                        wordWidth, YamlHelpers::BitUnits(), 1.0,
                        "memory.word_width"),
                "memory.word_width in bits")
        : 0;
    config.input.vectorDimensions = YamlHelpers::read_optional<long>(
            memory, "vector_dimensions", 0);
    const long comparisonWidth = config.input.vectorDimensions > 0
        ? config.input.vectorDimensions : config.input.wordWidth;
    if (comparisonWidth > 0) {
        config.exploration.cam.bitSerialWidth =
            IntValueDomain::PowersOfTwo((int)comparisonWidth, (int)comparisonWidth);
    }
}

void ReadRoutingSection(const YAML::Node &root, EvaCamConfig &config) {
    auto routing = YamlHelpers::child_required(root, "routing");
    config.input.routingMode = YamlHelpers::read_enum_required<RoutingMode>(routing, "type");
}

void ReadPeripheralSection(const YAML::Node &root, EvaCamConfig &config) {
    auto peripheralsNode = YamlHelpers::child_required(root, "peripherals");
    config.peripherals.withWriteDriver = YamlHelpers::read_required<bool>(peripheralsNode, "write_driver");

    auto inputNode = YamlHelpers::child_required(peripheralsNode, "input");
    config.peripherals.withInputBuffer = YamlHelpers::read_required<bool>(inputNode, "buffer");
    config.peripherals.withInputEnc = YamlHelpers::read_required<bool>(inputNode, "encoder");
    config.peripherals.customInputEnc = YamlHelpers::read_required<bool>(inputNode, "custom_encoder");

    auto outputNode = YamlHelpers::child_required(peripheralsNode, "output");
    config.peripherals.withOutputBuffer = YamlHelpers::read_required<bool>(outputNode, "buffer");
    config.peripherals.withPriorityEnc = YamlHelpers::read_required<bool>(outputNode, "priority_encoder");
    config.peripherals.withOutputAcc = YamlHelpers::read_required<bool>(outputNode, "accumulator");

    if (!config.peripherals.withOutputAcc) {
        const long comparisonWidth = config.input.vectorDimensions > 0
            ? config.input.vectorDimensions : config.input.wordWidth;
        if (comparisonWidth > 0) {
            config.exploration.cam.bitSerialWidth =
                IntValueDomain::PowersOfTwo((int)comparisonWidth, (int)comparisonWidth);
        }
    }
}

void ReadSensingSection(const YAML::Node &root, EvaCamConfig &config) {
    auto sensing = YamlHelpers::child_required(root, "sensing");
    config.input.internalSensing = YamlHelpers::read_required<bool>(sensing, "internal");
    config.peripherals.customSenseAmp = YamlHelpers::read_required<bool>(sensing, "custom_sense_amp");
    config.peripherals.typeSenseAmp = YamlHelpers::read_enum_required<TypeOfSenseAmp>(sensing, "sensing_mode");
    config.peripherals.strictSenseMargin = YamlHelpers::read_optional<bool>(
            sensing, "strict_sense_margin", false);
}

void ReadOptimizationSection(const YAML::Node &root, EvaCamConfig &config) {
    auto optimization = YamlHelpers::child_required(root, "optimization");
    config.input.optimizationTarget =
        YamlHelpers::read_enum_required<OptimizationTarget>(optimization, "target");
    config.requestDeepExploration = YamlHelpers::read_optional<bool>(
            optimization, "deep_exploration", false);

    auto bufferDesign = YamlHelpers::read_enum_required<BufferDesignTarget>(optimization, "buffer_design");
    config.exploration.cam.areaOptimizationLevel =
        IntValueDomain::Sequential((int)bufferDesign, (int)bufferDesign);

    auto rowDriver = YamlHelpers::read_enum_required<BufferDesignTarget>(optimization, "row_driver");
    config.exploration.cam.rowDriverOptLevel = IntValueDomain::Sequential((int)rowDriver, (int)rowDriver);

    auto priority = YamlHelpers::read_enum_required<BufferDesignTarget>(optimization, "priority_encoder");
    config.exploration.cam.priorityOptLevel = IntValueDomain::Sequential((int)priority, (int)priority);
}

void ReadWireSection(const YAML::Node &root, EvaCamConfig &config) {
    auto wires = YamlHelpers::child_required(root, "wires");
    auto local = YamlHelpers::child_required(wires, "local");
    auto localType = YamlHelpers::read_enum_required<WireType>(local, "type");
    config.exploration.wires.localWireType = IntValueDomain::Sequential((int)localType, (int)localType);
    auto localRepeater = YamlHelpers::read_enum_required<WireRepeaterType>(local, "repeater");
    config.exploration.wires.localWireRepeaterType =
        IntValueDomain::Sequential((int)localRepeater, (int)localRepeater);
    const int isLocalWireLowSwing = YamlHelpers::read_required<bool>(local, "low_swing");
    config.exploration.wires.isLocalWireLowSwing =
        IntValueDomain::Sequential(isLocalWireLowSwing, isLocalWireLowSwing);

    auto global = YamlHelpers::child_required(wires, "global");
    auto globalType = YamlHelpers::read_enum_required<WireType>(global, "type");
    config.exploration.wires.globalWireType = IntValueDomain::Sequential((int)globalType, (int)globalType);
    auto globalRepeater = YamlHelpers::read_enum_required<WireRepeaterType>(global, "repeater");
    config.exploration.wires.globalWireRepeaterType =
        IntValueDomain::Sequential((int)globalRepeater, (int)globalRepeater);
    const int isGlobalWireLowSwing = YamlHelpers::read_required<bool>(global, "low_swing");
    config.exploration.wires.isGlobalWireLowSwing =
        IntValueDomain::Sequential(isGlobalWireLowSwing, isGlobalWireLowSwing);
}

void ReadOrganizationSection(const YAML::Node &root, EvaCamConfig &config) {
    YAML::Node organization;
    if (!TryReadOptionalChild(root, "organization", organization)
            && !TryReadOptionalChild(root, "array", organization)) {
        return;
    }

    auto subarray = YamlHelpers::child_optional(organization, "subarray");
    const bool hasSubarrayDimensions = subarray
        && static_cast<bool>(YamlHelpers::child_optional(subarray, "dimensions"));

    auto banks = hasSubarrayDimensions
        ? YamlHelpers::child_required(organization, "banks")
        : YamlHelpers::child_optional(organization, "banks");
    if (banks) {
        auto banksTotal = YamlHelpers::child_required(banks, "total");
        auto banksActive = YamlHelpers::child_required(banks, "active");
        const auto total = ReadCoordinatePair(banksTotal, "organization.banks.total");
        const auto active = ReadCoordinatePair(banksActive, "organization.banks.active");
        const int numRowMat = total[0];
        const int numColumnMat = total[1];
        const int numActiveMatPerColumn = active[0];
        const int numActiveMatPerRow = active[1];
        config.exploration.geometry.numRowMat = IntValueDomain::PowersOfTwo(numRowMat, numRowMat);
        config.exploration.geometry.numColumnMat = IntValueDomain::PowersOfTwo(numColumnMat, numColumnMat);
        config.exploration.geometry.numActiveMatPerColumn =
            IntValueDomain::PowersOfTwo(numActiveMatPerColumn, numActiveMatPerColumn);
        config.exploration.geometry.numActiveMatPerRow =
            IntValueDomain::PowersOfTwo(numActiveMatPerRow, numActiveMatPerRow);
    }

    auto mats = hasSubarrayDimensions
        ? YamlHelpers::child_required(organization, "mats")
        : YamlHelpers::child_optional(organization, "mats");
    if (mats) {
        auto matsTotal = YamlHelpers::child_required(mats, "total");
        auto matsActive = YamlHelpers::child_required(mats, "active");
        const auto total = ReadCoordinatePair(matsTotal, "organization.mats.total");
        const auto active = ReadCoordinatePair(matsActive, "organization.mats.active");
        const int numRowSubarray = total[0];
        const int numColumnSubarray = total[1];
        const int numActiveSubarrayPerColumn = active[0];
        const int numActiveSubarrayPerRow = active[1];
        config.exploration.geometry.numRowSubarray = IntValueDomain::PowersOfTwo(numRowSubarray, numRowSubarray);
        config.exploration.geometry.numColumnSubarray =
            IntValueDomain::PowersOfTwo(numColumnSubarray, numColumnSubarray);
        config.exploration.geometry.numActiveSubarrayPerColumn =
            IntValueDomain::PowersOfTwo(numActiveSubarrayPerColumn, numActiveSubarrayPerColumn);
        config.exploration.geometry.numActiveSubarrayPerRow =
            IntValueDomain::PowersOfTwo(numActiveSubarrayPerRow, numActiveSubarrayPerRow);
    }

    if (hasSubarrayDimensions) {
        auto dimensions = YamlHelpers::child_required(subarray, "dimensions");
        const auto coordinate =
                ReadCoordinatePair(dimensions, "organization.subarray.dimensions");
        const int numRow = coordinate[0];
        const int numColumn = coordinate[1];
        if (numRow <= 0 || numColumn <= 0) {
            throw std::runtime_error(
                    "[Input] Error: organization.subarray.dimensions values must be positive.");
        }
        config.exploration.geometry.numRow = IntValueDomain::FixedSet({numRow});
        config.exploration.geometry.numColumn = IntValueDomain::FixedSet({numColumn});
        config.runtimeSizing.hasFixedSubarrayDimensions = true;
        config.runtimeSizing.fixedSubarrayRows = numRow;
        config.runtimeSizing.fixedSubarrayColumns = numColumn;
    }

    auto mux = YamlHelpers::child_optional(organization, "mux");
    if (!mux) {
        return;
    }
    if (YamlHelpers::child_optional(mux, "sense_amp")) {
        const int muxSenseAmp = YamlHelpers::read_required<int>(mux, "sense_amp");
        config.exploration.geometry.muxSenseAmp = IntValueDomain::PowersOfTwo(muxSenseAmp, muxSenseAmp);
    }
    if (YamlHelpers::child_optional(mux, "output_level1")) {
        const int muxOutputLev1 = YamlHelpers::read_required<int>(mux, "output_level1");
        config.exploration.geometry.muxOutputLev1 = IntValueDomain::PowersOfTwo(muxOutputLev1, muxOutputLev1);
    }
    if (YamlHelpers::child_optional(mux, "output_level2")) {
        const int muxOutputLev2 = YamlHelpers::read_required<int>(mux, "output_level2");
        config.exploration.geometry.muxOutputLev2 = IntValueDomain::PowersOfTwo(muxOutputLev2, muxOutputLev2);
    }
}

void ReadMatchlineSection(const YAML::Node &root, EvaCamConfig &config) {
    auto matchline = YamlHelpers::child_optional(root, "matchline");
    if (!matchline) {
        return;
    }
    if (YamlHelpers::child_optional(matchline, "additional_cap")) {
        config.peripherals.addCapOnML = YamlHelpers::read_quantity_required(
                matchline, "additional_cap", YamlHelpers::CapacitanceUnits(), 1e-15,
                "matchline.additional_cap");
    }
    auto matchTransistor = YamlHelpers::child_optional(matchline, "match_transistor");
    if (matchTransistor && YamlHelpers::child_optional(matchTransistor, "cmos_width")) {
        config.input.camWidthMatchTran = YamlHelpers::read_quantity_required(
                matchTransistor, "cmos_width", YamlHelpers::FeatureUnits(), 1.0,
                "matchline.match_transistor.cmos_width");
        config.input.hasCamWidthMatchTran = true;
    }
}

void ReadConstraintSection(const YAML::Node &root, EvaCamConfig &config) {
    auto constraintsNode = YamlHelpers::child_optional(root, "constraints");
    if (!constraintsNode) {
        return;
    }
    bool anyConstraintSet = false;
    const bool enabled = YamlHelpers::read_optional<bool>(constraintsNode, "enabled", false);
    if (YamlHelpers::child_optional(constraintsNode, "read_latency")) {
        config.constraints.readLatency = YamlHelpers::read_quantity_required(
                constraintsNode, "read_latency", YamlHelpers::TimeUnits(), 1.0, "constraints.read_latency");
        anyConstraintSet = true;
    }
    if (YamlHelpers::child_optional(constraintsNode, "write_latency")) {
        config.constraints.writeLatency = YamlHelpers::read_quantity_required(
                constraintsNode, "write_latency", YamlHelpers::TimeUnits(), 1.0, "constraints.write_latency");
        anyConstraintSet = true;
    }
    if (YamlHelpers::child_optional(constraintsNode, "read_dynamic_energy")) {
        config.constraints.readDynamicEnergy = YamlHelpers::read_quantity_required(
                constraintsNode, "read_dynamic_energy", YamlHelpers::EnergyUnits(), 1.0, "constraints.read_dynamic_energy");
        anyConstraintSet = true;
    }
    if (YamlHelpers::child_optional(constraintsNode, "write_dynamic_energy")) {
        config.constraints.writeDynamicEnergy = YamlHelpers::read_quantity_required(
                constraintsNode, "write_dynamic_energy", YamlHelpers::EnergyUnits(), 1.0, "constraints.write_dynamic_energy");
        anyConstraintSet = true;
    }
    if (YamlHelpers::child_optional(constraintsNode, "leakage")) {
        config.constraints.leakage = YamlHelpers::read_quantity_required(
                constraintsNode, "leakage", YamlHelpers::PowerUnits(), 1.0, "constraints.leakage");
        anyConstraintSet = true;
    }
    if (YamlHelpers::child_optional(constraintsNode, "area")) {
        config.constraints.area = YamlHelpers::read_required<double>(constraintsNode, "area");
        anyConstraintSet = true;
    }
    if (YamlHelpers::child_optional(constraintsNode, "read_edp")) {
        config.constraints.readEdp = YamlHelpers::read_required<double>(constraintsNode, "read_edp");
        anyConstraintSet = true;
    }
    if (YamlHelpers::child_optional(constraintsNode, "write_edp")) {
        config.constraints.writeEdp = YamlHelpers::read_required<double>(constraintsNode, "write_edp");
        anyConstraintSet = true;
    }
    config.constraints.enabled = enabled || anyConstraintSet;
}

void ReadAdvancedSection(const YAML::Node &root, EvaCamConfig &config) {
    auto advanced = YamlHelpers::child_optional(root, "advanced");
    if (!advanced) {
        return;
    }
    if (YamlHelpers::child_optional(advanced, "max_nmos_size")) {
        config.input.maxNmosSize = YamlHelpers::read_quantity_required(
                advanced, "max_nmos_size", YamlHelpers::FeatureUnits(), 1.0, "advanced.max_nmos_size");
    }
    config.useCactiAssumption = YamlHelpers::read_optional<bool>(
            advanced, "use_cacti_assumption", config.useCactiAssumption);
    config.exploration.useCactiAssumption = config.useCactiAssumption;
    config.exploration.pruningEnabled = YamlHelpers::read_optional<bool>(
            advanced, "enable_pruning", config.exploration.pruningEnabled);
    if (YamlHelpers::child_optional(advanced, "bit_serial_width")) {
        const long bsw = YamlHelpers::checked_integer<long>(
                YamlHelpers::read_quantity_required(
                        advanced, "bit_serial_width", YamlHelpers::BitUnits(), 1.0,
                        "advanced.bit_serial_width"),
                "advanced.bit_serial_width in bits");
        config.exploration.cam.bitSerialWidth = IntValueDomain::PowersOfTwo((int)bsw, (int)bsw);
        config.runtimeSizing.hasExplicitComparisonColumns = true;
    }
    if (YamlHelpers::child_optional(advanced, "input_encoder_type")) {
        config.peripherals.typeInputEnc =
            YamlHelpers::read_enum_required<TypeOfInputEncoder>(advanced, "input_encoder_type", false);
    }
    config.peripherals.fileCustomSA = YamlHelpers::read_optional<std::string>(
            advanced, "custom_sa_input_file", config.peripherals.fileCustomSA);
    config.peripherals.fileSenseAmp = YamlHelpers::read_optional<std::string>(
            advanced, "sense_amp_input_file", config.peripherals.fileSenseAmp);
    config.peripherals.useUpdatedLib = YamlHelpers::read_optional<bool>(
            advanced, "use_updated_lib", config.peripherals.useUpdatedLib);
    config.peripherals.noPrechargeInc = YamlHelpers::read_optional<bool>(
            advanced, "exclude_precharge_latency", config.peripherals.noPrechargeInc);
    config.peripherals.includeLeakage = YamlHelpers::read_optional<bool>(
            advanced, "include_leakage", config.peripherals.includeLeakage);
    config.peripherals.scaledVoltage = YamlHelpers::read_optional<double>(
            advanced, "scaled_voltage", config.peripherals.scaledVoltage);
}

void ReadFlashSection(const YAML::Node &root, EvaCamConfig &config) {
    auto flash = YamlHelpers::child_optional(root, "flash");
    if (!flash) {
        return;
    }
    if (YamlHelpers::child_optional(flash, "page_size")) {
        config.input.pageSize = YamlHelpers::checked_integer<long>(
                YamlHelpers::read_quantity_required(
                        flash, "page_size", YamlHelpers::DataSizeUnits(), 1.0,
                        "flash.page_size") * 8.0,
                "flash.page_size in bits");
    }
    if (YamlHelpers::child_optional(flash, "block_size")) {
        config.input.flashBlockSize = YamlHelpers::checked_integer<long>(
                YamlHelpers::read_quantity_required(
                        flash, "block_size", YamlHelpers::DataSizeUnits(), 1.0,
                        "flash.block_size") * 8.0,
                "flash.block_size in bits");
    }
}

void ReadExtraSection(const YAML::Node &root, EvaCamConfig &config) {
    auto extra = YamlHelpers::child_optional(root, "extra");
    if (!extra) {
        return;
    }
    config.input.outputFilePrefix = YamlHelpers::read_optional<std::string>(
            extra, "output_file_prefix", config.input.outputFilePrefix);
    config.input.outputYamlFileName = YamlHelpers::read_optional<std::string>(
            extra, "output_yaml_file", config.input.outputYamlFileName);
    if (YamlHelpers::read_optional<bool>(extra, "output_yaml_file_from_config", false)) {
        config.logger.Log()
            << "[Input] Warning: output.results is deprecated; prefer CLI --output for YAML results paths.";
    }
    config.input.fileTechnology = YamlHelpers::read_optional<std::string>(
            extra, "technology_file", config.input.fileTechnology);
    if (YamlHelpers::child_optional(extra, "worst_case_sense_margin")) {
        config.peripherals.matchlineSenseMargin = YamlHelpers::read_quantity_required(
                extra, "worst_case_sense_margin", YamlHelpers::VoltageUnits(), 1.0, "extra.worst_case_sense_margin");
    }
    if (YamlHelpers::child_optional(extra, "max_driver_current")) {
        config.input.maxDriverCurrent = YamlHelpers::read_quantity_required(
                extra, "max_driver_current", YamlHelpers::CurrentUnits(), 1.0, "extra.max_driver_current");
    }
    auto realCapacityNode = YamlHelpers::child_optional(extra, "real_capacity");
    if (realCapacityNode) {
        config.runtimeSizing.realCapacity = YamlHelpers::checked_integer<int64_t>(
                YamlHelpers::parse_quantity_node(
                        realCapacityNode, YamlHelpers::DataSizeUnits(), 1.0,
                        "extra.real_capacity"),
                "extra.real_capacity in bytes");
        return;
    }
    auto realCapacityB = YamlHelpers::child_optional(extra, "RealCapacity (B)");
    auto realCapacityKB = YamlHelpers::child_optional(extra, "RealCapacity (KB)");
    auto realCapacityMB = YamlHelpers::child_optional(extra, "RealCapacity (MB)");
    if (realCapacityB) {
        config.runtimeSizing.realCapacity = YamlHelpers::checked_integer<int64_t>(
                YamlHelpers::read_scalar_required<double>(
                        realCapacityB, "extra.RealCapacity (B)"),
                "extra.RealCapacity (B)");
    } else if (realCapacityKB) {
        config.runtimeSizing.realCapacity = YamlHelpers::checked_integer<int64_t>(
                YamlHelpers::read_scalar_required<double>(
                        realCapacityKB, "extra.RealCapacity (KB)") * 1024.0,
                "extra.RealCapacity (KB) in bytes");
    } else if (realCapacityMB) {
        config.runtimeSizing.realCapacity = YamlHelpers::checked_integer<int64_t>(
                YamlHelpers::read_scalar_required<double>(
                        realCapacityMB, "extra.RealCapacity (MB)") * 1024.0 * 1024.0,
                "extra.RealCapacity (MB) in bytes");
    }
}

}  // namespace ConfigSectionReaders
