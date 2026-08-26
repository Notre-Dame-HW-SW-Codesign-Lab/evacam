#include "config/InputRuleValidator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "EvaCamConfig.h"
#include "SenseAmp.h"
#include "input/CustomSenseAmpYamlLoader.h"
#include "input/MemoryDeviceYamlLoader.h"
#include "input/SenseAmpYamlLoader.h"
#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

namespace {

bool IsCamModelMemCellTypeSupported(MemCellType type) {
    switch (type) {
        case SRAM:
        case MRAM:
        case PCRAM:
        case memristor:
        case FEFETRAM:
            return true;
        default:
            return false;
    }
}

YAML::Node LoadCellFileForValidation(const std::string &cellFile) {
    return YAML::LoadFile(cellFile);
}

std::string ResolveReference(const std::string &ownerFile, const std::string &reference) {
    const std::filesystem::path referencePath(reference);
    if (referencePath.is_absolute()) {
        return referencePath.lexically_normal().string();
    }
    return (std::filesystem::absolute(ownerFile).parent_path() / referencePath)
            .lexically_normal().string();
}

YAML::Node LoadMemoryDeviceForValidation(const YAML::Node &cellRoot,
        const std::string &cellFile) {
    const YAML::Node reference = YamlHelpers::child_required(cellRoot, "memory_device");
    const YAML::Node memoryDevice = YAML::LoadFile(ResolveReference(cellFile,
            YamlHelpers::read_scalar_required<std::string>(
                reference, "memory_device")));
    YamlHelpers::require_schema(
            memoryDevice, "memory_device", "memory device config");
    YamlHelpers::validate_memory_device_keys(memoryDevice);
    return memoryDevice;
}

std::string InferCamTypeToken(const YAML::Node &cellNode, const std::string &cellFile) {
    if (YamlHelpers::child_optional(cellNode, "cam_type")) {
        return YamlHelpers::read_required<std::string>(cellNode, "cam_type");
    }

    std::string probe = cellFile;
    if (YamlHelpers::child_optional(cellNode, "name")) {
        probe = YamlHelpers::read_required<std::string>(cellNode, "name") + " " + probe;
    }

    for (char &c : probe) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }

    if (probe.find("mcam") != std::string::npos) {
        return "MCAM";
    }
    if (probe.find("acam") != std::string::npos) {
        return "ACAM";
    }
    return "TCAM";
}

MemCellType LoadMemCellTypeForValidation(const YAML::Node &root,
        const std::string &cellFile) {
    const YAML::Node memoryDevice = LoadMemoryDeviceForValidation(root, cellFile);
    return YamlHelpers::read_enum_required<MemCellType>(
            memoryDevice, "type", false);
}

CAMType LoadCamTypeForValidation(const YAML::Node &root, const std::string &cellFile) {
    if (YamlHelpers::child_optional(root, "cam_type")) {
        return YamlHelpers::read_enum_required<CAMType>(root, "cam_type", false);
    }
    const std::string camType = InferCamTypeToken(root, cellFile);
    if (camType == "MCAM") {
        return MCAM;
    }
    if (camType == "ACAM") {
        return ACAM;
    }
    return TCAM;
}

int LoadBitsPerCellForValidation(const EvaCamConfig &config) {
    const YAML::Node root = LoadCellFileForValidation(config.input.fileMemCell);
    if (LoadCamTypeForValidation(root, config.input.fileMemCell) != MCAM) {
        return 1;
    }
    const YAML::Node memoryDevice = LoadMemoryDeviceForValidation(
            root, config.input.fileMemCell);
    const YAML::Node mcam = YamlHelpers::child_required(memoryDevice, "mcam");
    int numStates = YamlHelpers::read_optional<int>(
            mcam, "num_resistance_state", 0);
    if (numStates == 0) {
        const YAML::Node states = YamlHelpers::child_required(mcam, "resistance_state");
        if (states.IsSequence()) {
            numStates = static_cast<int>(states.size());
        }
    }
    int bitsPerCell = 0;
    for (int states = numStates; states > 1; states >>= 1) {
        bitsPerCell++;
    }
    return bitsPerCell;
}

void ValidateCamPortPresence(const YAML::Node &root) {
    const YAML::Node ports = YamlHelpers::child_required(root, "ports");
    const YAML::Node rowPorts = YamlHelpers::child_optional(ports, "row");
    const YAML::Node columnPorts = YamlHelpers::child_optional(ports, "column");

    if (!rowPorts || !rowPorts.IsMap() || rowPorts.size() == 0) {
        throw std::runtime_error(
                "[Input] Error: cell.ports.row must define at least one CAM row port.");
    }

    if (!columnPorts || !columnPorts.IsMap() || columnPorts.size() == 0) {
        throw std::runtime_error(
                "[Input] Error: cell.ports.column must define at least one CAM column port.");
    }
}

CAM_CmosRegion LoadPortConnectionRegion(const YAML::Node &portNode) {
    const YAML::Node connection = YamlHelpers::child_optional(portNode, "connection");
    if (!connection) {
        return YamlHelpers::read_enum_required<CAM_CmosRegion>(
                portNode, "cmos_region", false);
    }

    const std::string kind = YamlHelpers::read_required<std::string>(
            connection, "kind");
    if (kind == "memory_terminal" || kind == "access_terminal") {
        return YamlHelpers::read_enum_required<CAM_CmosRegion>(
                connection, "terminal", false);
    }
    throw std::runtime_error("[Input] Error: cell.ports.column has unsupported connection.kind.");
}

void ValidateCamColumnTopology(const YAML::Node &root) {
    const YAML::Node ports = YamlHelpers::child_required(root, "ports");
    const YAML::Node columnPorts = YamlHelpers::child_required(ports, "column");
    bool foundMatchline = false;

    for (auto it = columnPorts.begin(); it != columnPorts.end(); ++it) {
        const YAML::Node portNode = it->second;
        const CAM_PortType portType =
                YamlHelpers::read_enum_required<CAM_PortType>(portNode, "type", false);

        // Plain bitline columns are used by some shipped CAM cell configs for
        // write paths. Keep validating that a matchline exists, but do not
        // reject the topology before the model sees it.
        // if (portType == Bitline) {
        //     throw std::runtime_error(
        //             "[Input] Error: cell.ports.column does not support Bitline topology for CAM modeling.");
        // }

        if (portType != Matchline && portType != Matchline_Bitline) {
            continue;
        }

        foundMatchline = true;
        const CAM_CmosRegion region = LoadPortConnectionRegion(portNode);
        if (region == gate) {
            throw std::runtime_error(
                    "[Input] Error: cell.ports.column matchline connection cannot use cmos_region gate.");
        }
        if (region != drain && region != source && region != diode && region != none) {
            throw std::runtime_error(
                    "[Input] Error: cell.ports.column matchline connection uses unsupported cmos_region.");
        }
    }

    if (!foundMatchline) {
        throw std::runtime_error(
                "[Input] Error: cell.ports.column must define at least one CAM matchline port.");
    }
}

void ValidateCamModelSupport(const EvaCamConfig &config, const YAML::Node &root,
        MemCellType memCellType) {
    const CAMType camType = LoadCamTypeForValidation(root, config.input.fileMemCell);
    if (camType == ACAM) {
        throw std::runtime_error("[Input] Error: ACAM is not supported at this time.");
    }

    if (camType == MCAM && memCellType != FEFETRAM) {
        throw std::runtime_error(
                "[Input] Error: only 2FeFET MCAM design has limited support.");
    }

    if (camType == MCAM) {
        config.logger.Log() << "[Input] Warning: 2FeFET MCAM support is experimental; "
            "latency and power models are still being validated.";
    }
}

void ValidateSupportedMcamTopology(const EvaCamConfig &config, const YAML::Node &root) {
    if (LoadCamTypeForValidation(root, config.input.fileMemCell) != MCAM) {
        return;
    }

    const YAML::Node accessDevice = YamlHelpers::child_required(root, "access_device");
    const CellAccessType accessType =
            YamlHelpers::read_enum_required<CellAccessType>(accessDevice, "type", false);
    if (accessType != none_access) {
        throw std::runtime_error(
                "[Input] Error: the supported 2FeFET MCAM topology requires access_device.type: none.");
    }

    const YAML::Node ports = YamlHelpers::child_required(root, "ports");
    const YAML::Node rowPorts = YamlHelpers::child_required(ports, "row");
    if (rowPorts.size() != 2 || !rowPorts[0] || !rowPorts[1]) {
        throw std::runtime_error(
                "[Input] Error: the supported 2FeFET MCAM topology requires exactly two row ports indexed 0 and 1.");
    }
    for (int index = 0; index < 2; index++) {
        const YAML::Node port = rowPorts[index];
        const CAM_PortType type =
                YamlHelpers::read_enum_required<CAM_PortType>(port, "type", false);
        const CAM_CmosRegion region = LoadPortConnectionRegion(port);
        if (type != Searchline || region != gate) {
            throw std::runtime_error(
                    "[Input] Error: the supported 2FeFET MCAM topology requires both row ports to be gate-connected searchlines.");
        }
    }

    const YAML::Node columnPorts = YamlHelpers::child_required(ports, "column");
    if (columnPorts.size() != 2 || !columnPorts[0] || !columnPorts[1]) {
        throw std::runtime_error(
                "[Input] Error: the supported 2FeFET MCAM topology requires exactly two column ports indexed 0 and 1.");
    }
    for (int index = 0; index < 2; index++) {
        const YAML::Node port = columnPorts[index];
        const CAM_PortType type =
                YamlHelpers::read_enum_required<CAM_PortType>(port, "type", false);
        const CAM_CmosRegion region = LoadPortConnectionRegion(port);
        if (type != Matchline || region != drain) {
            throw std::runtime_error(
                    "[Input] Error: the supported 2FeFET MCAM topology requires both column ports to be drain-connected matchlines.");
        }
    }
}

void ValidateMcamResistanceStates(const EvaCamConfig &config, const YAML::Node &root) {
    const CAMType camType = LoadCamTypeForValidation(root, config.input.fileMemCell);
    if (camType != MCAM) {
        return;
    }

    const YAML::Node ownerRoot = LoadMemoryDeviceForValidation(
            root, config.input.fileMemCell);
    const YAML::Node mcam = YamlHelpers::child_required(ownerRoot, "mcam");
    int numStates = YamlHelpers::read_optional<int>(mcam, "num_resistance_state", 0);
    const YAML::Node states = YamlHelpers::child_required(mcam, "resistance_state");
    if (!states.IsSequence() && !states.IsMap()) {
        throw std::runtime_error(
                "[Input] Error: mcam.resistance_state must be a sequence or map.");
    }

    if (numStates == 0 && states.IsSequence()) {
        numStates = static_cast<int>(states.size());
    }
    if (numStates < 2 || numStates > 64
            || (numStates & (numStates - 1)) != 0) {
        throw std::runtime_error(
                "[Input] Error: mcam.num_resistance_state must be a power of two between 2 and 64.");
    }
    if (static_cast<int>(states.size()) != numStates) {
        throw std::runtime_error(
                "[Input] Error: mcam.resistance_state must contain exactly "
                "mcam.num_resistance_state entries.");
    }

    for (int state = 0; state < numStates; state++) {
        YAML::Node stateNode;
        if (states.IsSequence()) {
            if (state >= static_cast<int>(states.size())) {
                throw std::runtime_error(
                        "[Input] Error: mcam.resistance_state must define every configured resistance state.");
            }
            stateNode = states[state];
        } else {
            stateNode = states[state];
            if (!stateNode) {
                throw std::runtime_error(
                        "[Input] Error: mcam.resistance_state must define every configured resistance state.");
            }
        }

        const double resistance = YamlHelpers::parse_quantity_node(
                stateNode, YamlHelpers::ResistanceUnits(), 1.0, "mcam.resistance_state");
        if (resistance <= 0) {
            throw std::runtime_error(
                    "[Input] Error: mcam.resistance_state values must be positive.");
        }
    }

    const YAML::Node stateVariations = YamlHelpers::child_optional(mcam, "state_variation");
    if (stateVariations) {
        if ((!stateVariations.IsSequence() && !stateVariations.IsMap())
                || static_cast<int>(stateVariations.size()) != numStates) {
            throw std::runtime_error(
                    "[Input] Error: mcam.state_variation must define every configured resistance state.");
        }
        for (int state = 0; state < numStates; state++) {
            if (!stateVariations[state]) {
                throw std::runtime_error(
                        "[Input] Error: mcam.state_variation must define every configured resistance state.");
            }
        }
    }

    const YAML::Node mlPrechargeVoltages = YamlHelpers::child_optional(mcam, "ml_precharge_voltage");
    if (mlPrechargeVoltages) {
        if (!mlPrechargeVoltages.IsSequence() && !mlPrechargeVoltages.IsMap()) {
            throw std::runtime_error(
                    "[Input] Error: mcam.ml_precharge_voltage must be a sequence or map.");
        }
        if (static_cast<int>(mlPrechargeVoltages.size()) != numStates) {
            throw std::runtime_error(
                    "[Input] Error: mcam.ml_precharge_voltage must define every configured resistance state.");
        }
        for (int state = 0; mlPrechargeVoltages.size() != 0 && state < numStates; state++) {
            YAML::Node voltageNode;
            if (mlPrechargeVoltages.IsSequence()) {
                if (state >= static_cast<int>(mlPrechargeVoltages.size())) {
                    throw std::runtime_error(
                            "[Input] Error: mcam.ml_precharge_voltage must define every configured resistance state.");
                }
                voltageNode = mlPrechargeVoltages[state];
            } else {
                voltageNode = mlPrechargeVoltages[state];
                if (!voltageNode) {
                    throw std::runtime_error(
                            "[Input] Error: mcam.ml_precharge_voltage must define every configured resistance state.");
                }
            }

            const double voltage = YamlHelpers::parse_quantity_node(
                    voltageNode, YamlHelpers::VoltageUnits(), 1.0, "mcam.ml_precharge_voltage");
            if (voltage < 0) {
                throw std::runtime_error(
                        "[Input] Error: mcam.ml_precharge_voltage values must be non-negative.");
            }
        }
    }

    const YAML::Node searchlineVoltages = YamlHelpers::child_optional(mcam, "searchline_voltage");
    if (!searchlineVoltages) {
        throw std::runtime_error(
                "[Input] Error: MCAM requires mcam.searchline_voltage with one value per resistance state.");
    }
    std::vector<double> orderedSearchlineVoltages;
    if (searchlineVoltages) {
        if (!searchlineVoltages.IsSequence() && !searchlineVoltages.IsMap()) {
            throw std::runtime_error(
                    "[Input] Error: mcam.searchline_voltage must be a sequence or map.");
        }
        if (static_cast<int>(searchlineVoltages.size()) != numStates) {
            throw std::runtime_error(
                "[Input] Error: mcam.searchline_voltage must define every configured resistance state.");
        }
        orderedSearchlineVoltages.reserve(numStates);
        for (int state = 0; searchlineVoltages.size() != 0 && state < numStates; state++) {
            YAML::Node voltageNode;
            if (searchlineVoltages.IsSequence()) {
                if (state >= static_cast<int>(searchlineVoltages.size())) {
                    throw std::runtime_error(
                            "[Input] Error: mcam.searchline_voltage must define every configured resistance state.");
                }
                voltageNode = searchlineVoltages[state];
            } else {
                voltageNode = searchlineVoltages[state];
                if (!voltageNode) {
                    throw std::runtime_error(
                            "[Input] Error: mcam.searchline_voltage must define every configured resistance state.");
                }
            }

            const double voltage = YamlHelpers::parse_quantity_node(
                    voltageNode, YamlHelpers::VoltageUnits(), 1.0, "mcam.searchline_voltage");
            if (voltage < 0) {
                throw std::runtime_error(
                        "[Input] Error: mcam.searchline_voltage values must be non-negative.");
            }
            orderedSearchlineVoltages.push_back(voltage);
        }
    }

    if (!orderedSearchlineVoltages.empty()) {
        std::sort(orderedSearchlineVoltages.begin(), orderedSearchlineVoltages.end());
        for (size_t state = 1; state < orderedSearchlineVoltages.size(); state++) {
            if (orderedSearchlineVoltages[state]
                    == orderedSearchlineVoltages[state - 1]) {
                throw std::runtime_error(
                        "[Input] Error: mcam.searchline_voltage values must be distinct.");
            }
        }

        const double analogInverseSum = orderedSearchlineVoltages.front()
            + orderedSearchlineVoltages.back();
        const double tolerance = std::max(1e-12,
                std::abs(analogInverseSum) * 1e-9);
        for (int state = 0; state < numStates; state++) {
            const double pairSum = orderedSearchlineVoltages[state]
                + orderedSearchlineVoltages[numStates - state - 1];
            if (std::abs(pairSum - analogInverseSum) > tolerance) {
                throw std::runtime_error(
                        "[Input] Error: mcam.searchline_voltage must satisfy the "
                        "paper's analog-inverse mapping: every reversed pair "
                        "must have the same derived center.");
            }
        }
    }
}

bool IsSupportedCamSenseAmpType(TypeOfSenseAmp type) {
    return type == nvsim_voltage_sense
        || type == nvsim_current_sense
        || type == discharge;
}

void ValidateCustomSenseAmpFile(const std::string &filePath) {
    if (filePath.empty()) {
        throw std::runtime_error(
                "[Input] Error: sensing.custom_sense_amp requires advanced.custom_sa_input_file.");
    }

    try {
        SenseAmp customSenseAmp;
        YamlHelpers::ReadCustomSenseAmpFromYaml(customSenseAmp, filePath, 1.0);
    } catch (const YAML::BadFile &) {
        throw std::runtime_error(
                "[Input] Error: custom sense amp file cannot be found: " + filePath);
    }
}

void ValidateDefaultSenseAmpFile(const std::string &filePath) {
    if (filePath.empty()) {
        return;
    }

    try {
        (void)YamlHelpers::ReadSenseAmpModelFromYaml(filePath);
    } catch (const YAML::BadFile &) {
        throw std::runtime_error(
                "[Input] Error: sense amp file cannot be found: " + filePath);
    }
}

void ValidatePeripheralSupport(const EvaCamConfig &config) {
    if (config.peripherals.customInputEnc) {
        throw std::runtime_error(
                "[Input] Error: custom input encoder is not supported.");
    }

    if (config.peripherals.typeInputEnc != encoding_two_bit) {
        throw std::runtime_error(
                "[Input] Error: input encoder type must be encoding_two_bit.");
    }

    if (!IsSupportedCamSenseAmpType(config.peripherals.typeSenseAmp)) {
        throw std::runtime_error(
                "[Input] Error: sensing.sensing_mode is not supported for CAM modeling.");
    }

    if (config.peripherals.customSenseAmp) {
        ValidateCustomSenseAmpFile(config.peripherals.fileCustomSA);
    } else {
        ValidateDefaultSenseAmpFile(config.peripherals.fileSenseAmp);
    }

    if (config.exploration.wires.isLocalWireLowSwing.Min() != 0
            && config.exploration.wires.localWireRepeaterType.Min() != repeated_none) {
        throw std::runtime_error(
                "[Input] Error: wires.local.low_swing is not supported with repeaters.");
    }

    if (config.exploration.wires.isGlobalWireLowSwing.Min() != 0
            && config.exploration.wires.globalWireRepeaterType.Min() != repeated_none) {
        throw std::runtime_error(
                "[Input] Error: wires.global.low_swing is not supported with repeaters.");
    }
}

long long CheckedMultiply(long long lhs, long long rhs, const char *what) {
    if (lhs <= 0 || rhs <= 0) {
        throw std::runtime_error(std::string("[Input] Error: ") + what + " factors must be positive.");
    }
    if (lhs > std::numeric_limits<long long>::max() / rhs) {
        throw std::runtime_error(std::string("[Input] Error: ") + what + " exceeds int64_t range.");
    }
    return lhs * rhs;
}

long long CheckedTotalProduct(const IntValueDomain &first, const IntValueDomain &second,
        const char *what) {
    return CheckedMultiply(first.Min(), second.Min(), what);
}

void ValidateScalarDomains(const EvaCamConfig &config) {
    YamlHelpers::require_range(
            config.input.temperature, 300, 400, "design.temperature",
            "between 300K and 400K");
    YamlHelpers::require_range(
            config.input.processNode, 7, 200, "design.system_process_node",
            "between 7nm and 200nm");
    YamlHelpers::require_positive(config.input.wordWidth, "memory.word_width");
    YamlHelpers::require_positive(config.input.maxNmosSize, "advanced.max_nmos_size");
    YamlHelpers::require_non_negative(
            config.input.maxDriverCurrent, "extra.max_driver_current");
    YamlHelpers::require_non_negative(
            config.peripherals.addCapOnML, "matchline.additional_cap");
    YamlHelpers::require_positive(
            config.peripherals.matchlineSenseMargin, "sensing.worst_case_sense_margin");
    YamlHelpers::require_non_negative(
            config.peripherals.scaledVoltage, "modeling.scaled_voltage");

    if (config.input.hasCamWidthMatchTran) {
        YamlHelpers::require_positive(
                config.input.camWidthMatchTran, "matchline.match_transistor.cmos_width");
    }
    if (config.input.pageSize != 0) {
        YamlHelpers::require_positive(config.input.pageSize, "flash.page_size");
    }
    if (config.input.flashBlockSize != 0) {
        YamlHelpers::require_positive(config.input.flashBlockSize, "flash.block_size");
    }
    if (config.input.pageSize > 0 && config.input.flashBlockSize > 0
            && config.input.flashBlockSize % config.input.pageSize != 0) {
        throw std::runtime_error(
                "[Input] Error: flash.block_size must be an integer multiple of "
                "flash.page_size.");
    }

    if (config.constraints.enabled) {
        YamlHelpers::require_positive(
                config.constraints.readLatency, "design_constraints.read_latency");
        YamlHelpers::require_positive(
                config.constraints.writeLatency, "design_constraints.write_latency");
        YamlHelpers::require_positive(
                config.constraints.readDynamicEnergy,
                "design_constraints.read_dynamic_energy");
        YamlHelpers::require_positive(
                config.constraints.writeDynamicEnergy,
                "design_constraints.write_dynamic_energy");
        YamlHelpers::require_positive(
                config.constraints.readEdp, "design_constraints.read_edp");
        YamlHelpers::require_positive(
                config.constraints.writeEdp, "design_constraints.write_edp");
        YamlHelpers::require_positive(config.constraints.area, "design_constraints.area");
        YamlHelpers::require_positive(
                config.constraints.leakage, "design_constraints.leakage");
    }
}

void ValidateDerivedInputs(const EvaCamConfig &config) {
    if (config.exploration.pruningEnabled
            && config.input.optimizationTarget != full_exploration) {
        throw std::runtime_error(
                "[Input] Error: exploration.enable_pruning requires "
                "optimization.target: Exploration.");
    }
    if (config.input.capacity <= 0) {
        throw std::runtime_error(
                "[Input] Error: memory.capacity must be > 0 unless organization.subarray.dimensions derives it.");
    }
    const bool isWordWidthPow2 = (config.input.wordWidth & (config.input.wordWidth - 1)) == 0;
    if (!isWordWidthPow2 && config.wordGeometry.bitsPerCell == 1
            && config.runtimeSizing.realCapacity == 0) {
        throw std::runtime_error(
                "[Input] Error: non-power-of-two word_width requires extra.real_capacity to be set.");
    }
    if (config.wordGeometry.allocatedCapacityBits % config.input.wordWidth != 0) {
        throw std::runtime_error(
                "[Input] Error: resolved capacity must contain a whole number of memory.word_width logical words.");
    }
    if (config.runtimeSizing.realCapacity > 0) {
        if (config.runtimeSizing.realCapacity < config.input.capacity) {
            throw std::runtime_error("[Input] Error: extra.real_capacity must be >= memory.capacity.");
        }
        const long long denom =
            (long long)config.exploration.geometry.numRowSubarray.Min()
            * config.exploration.geometry.numColumnSubarray.Min()
            * config.exploration.geometry.numActiveMatPerRow.Min()
            * config.exploration.geometry.numActiveMatPerColumn.Min();
        if (denom <= 0) {
            throw std::runtime_error(
                    "[Input] Error: invalid organization geometry while validating extra.real_capacity.");
        }
        if ((config.runtimeSizing.realCapacity % denom) != 0) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity is incompatible with organization geometry.");
        }
        const long long allocatedBits = CheckedMultiply(
                config.runtimeSizing.realCapacity, 8, "extra.real_capacity");
        if (((allocatedBits / denom) % config.input.wordWidth) != 0) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity is incompatible with word_width.");
        }
    }
}

void ResolveComparisonColumns(EvaCamConfig &config) {
    const long physicalColumns = config.wordGeometry.physicalColumnsPerWord;
    if (!config.runtimeSizing.hasExplicitComparisonColumns) {
        config.exploration.cam.bitSerialWidth = IntValueDomain::FixedSet(
                {static_cast<int>(physicalColumns)});
        return;
    }
    for (int width : config.exploration.cam.bitSerialWidth.Values()) {
        if (width <= 0 || width > physicalColumns || physicalColumns % width != 0) {
            throw std::runtime_error(
                    "[Input] Error: organization.comparison_columns_per_step must be a positive divisor of the physical columns per word.");
        }
    }
}

void ValidateAndResolveExplicitSubarrayDimensions(
        EvaCamConfig &config, bool isMcam) {
    if (!config.runtimeSizing.hasFixedSubarrayDimensions) {
        if (!config.runtimeSizing.hasExplicitCapacity || config.runtimeSizing.capacityIsAuto) {
            throw std::runtime_error(
                    "[Input] Error: memory.capacity is required unless organization.subarray.dimensions is supplied.");
        }
        return;
    }

    if (config.input.optimizationTarget == full_exploration || config.exploration.deepExploration) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions is only supported for fixed non-DSE configs.");
    }

    const int subarrayRows = config.runtimeSizing.fixedSubarrayRows;
    const int subarrayColumns = config.runtimeSizing.fixedSubarrayColumns;
    if (subarrayRows < 8 || subarrayRows > 512) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions row count must be between 8 and 512.");
    }
    if (subarrayColumns < 8 || subarrayColumns > 512) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions column count must be between 8 and 512.");
    }

    const long long banksTotal = CheckedTotalProduct(config.exploration.geometry.numRowMat,
            config.exploration.geometry.numColumnMat, "organization.banks.total");
    const long long banksActive = CheckedTotalProduct(config.exploration.geometry.numActiveMatPerColumn,
            config.exploration.geometry.numActiveMatPerRow, "organization.banks.active");
    const long long matsTotal = CheckedTotalProduct(config.exploration.geometry.numRowSubarray,
            config.exploration.geometry.numColumnSubarray, "organization.mats.total");
    const long long matsActive = CheckedTotalProduct(config.exploration.geometry.numActiveSubarrayPerColumn,
            config.exploration.geometry.numActiveSubarrayPerRow, "organization.mats.active");

    if (banksTotal % banksActive != 0 || matsTotal % matsActive != 0) {
        throw std::runtime_error(
                "[Input] Error: active bank/mat partitioning must divide total bank/mat geometry.");
    }

    const long long dataPartitions = CheckedMultiply(
            banksActive, matsActive, "active data partitioning");
    const long long addressPartitions = CheckedMultiply(banksTotal / banksActive,
            matsTotal / matsActive, "address partitioning");
    const long long minimumPhysicalColumns =
        config.wordGeometry.physicalColumnsPerWord;
    const long long suppliedPhysicalColumns = CheckedMultiply(
            subarrayColumns, dataPartitions, "supplied physical word columns");
    if (suppliedPhysicalColumns < minimumPhysicalColumns) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions provides "
                + std::to_string(subarrayColumns) + " columns across each of "
                + std::to_string(dataPartitions) + " active data partitions ("
                + std::to_string(suppliedPhysicalColumns) + " per word), but "
                + std::to_string(config.input.wordWidth) + " logical bits at "
                + std::to_string(config.wordGeometry.bitsPerCell)
                + " bits per cell require "
                + std::to_string(minimumPhysicalColumns)
                + " physical columns per word.");
    }
    if (!isMcam && suppliedPhysicalColumns != minimumPhysicalColumns) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions must provide exactly "
                + std::to_string(minimumPhysicalColumns)
                + " columns per word for a single-bit CAM; oversized physical-word "
                  "allocation is supported only for MCAM.");
    }

    const long long entryCount = CheckedMultiply(
            subarrayRows, addressPartitions, "derived entry count");
    long long capacityBits = CheckedMultiply(
            entryCount, config.input.wordWidth, "derived capacity");
    if (capacityBits % 8 != 0) {
        throw std::runtime_error(
                "[Input] Error: derived capacity from organization.subarray.dimensions is not byte-addressable.");
    }
    const int64_t derivedCapacityBytes = capacityBits / 8;

    if (!config.runtimeSizing.hasExplicitCapacity || config.runtimeSizing.capacityIsAuto) {
        config.input.capacity = derivedCapacityBytes;
    } else if (config.input.capacity != derivedCapacityBytes) {
        throw std::runtime_error(
                "[Input] Error: memory.capacity does not match organization.subarray.dimensions.");
    }

    config.ResolveWordGeometry(config.wordGeometry.bitsPerCell,
            static_cast<long>(suppliedPhysicalColumns));

    if (config.runtimeSizing.realCapacity > 0
            && config.runtimeSizing.realCapacity != derivedCapacityBytes) {
        throw std::runtime_error(
                "[Input] Error: extra.real_capacity must match capacity derived from organization.subarray.dimensions.");
    }
}

void ValidateMemCellSupport(const EvaCamConfig &config) {
    const YAML::Node root = LoadCellFileForValidation(config.input.fileMemCell);
    YamlHelpers::require_schema(root, "cell", "cell config");
    const YAML::Node memoryDevice = LoadMemoryDeviceForValidation(
            root, config.input.fileMemCell);
    YamlHelpers::require_schema(memoryDevice, "memory_device", "memory device config");
    if (YamlHelpers::child_optional(memoryDevice, "dram")) {
        throw std::runtime_error(
                "[Input] Error: dram is not supported and must not be specified.");
    }
    const MemCellType memCellType = LoadMemCellTypeForValidation(root, config.input.fileMemCell);
    ValidateCamPortPresence(root);
    ValidateCamColumnTopology(root);
    ValidateCamModelSupport(config, root, memCellType);
    ValidateSupportedMcamTopology(config, root);
    ValidateMcamResistanceStates(config, root);

    if (!IsCamModelMemCellTypeSupported(memCellType)) {
        throw std::runtime_error(
                "[Input] Error: memory.cell.type is not supported for CAM modeling.");
    }

    if (!config.input.internalSensing) {
        throw std::runtime_error(
                "[Input] Error: CAM bank routing requires internal sensing in this version.");
    }
}

}  // namespace

void InputRuleValidator::Validate(EvaCamConfig &config) {
    ValidateScalarDomains(config);
    ValidateMemCellSupport(config);
    const YAML::Node cellRoot = LoadCellFileForValidation(config.input.fileMemCell);
    const bool isMcam =
        LoadCamTypeForValidation(cellRoot, config.input.fileMemCell) == MCAM;
    const int bitsPerCell = LoadBitsPerCellForValidation(config);
    config.ResolveWordGeometry(bitsPerCell);
    ValidateAndResolveExplicitSubarrayDimensions(config, isMcam);
    ResolveComparisonColumns(config);
    ValidateDerivedInputs(config);
    ValidatePeripheralSupport(config);
}
