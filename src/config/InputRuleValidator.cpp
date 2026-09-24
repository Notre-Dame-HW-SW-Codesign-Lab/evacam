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
#include "MemCell.h"
#include "input/PhysicalDomainValidators.h"
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

void ValidateScalarDomains(const EvaCamConfig &config, bool isMcam) {
    YamlHelpers::require_range(
            config.input.temperature, 300, 400, "design.temperature",
            "between 300K and 400K");
    YamlHelpers::require_range(
            config.input.processNode, 7, 200, "design.system_process_node",
            "between 7nm and 200nm");
    if (isMcam) {
        if (config.input.wordWidth != 0) {
            throw std::runtime_error(
                    "[Input] Error: memory.word_width is not defined for MCAM; use memory.vector_dimensions.");
        }
        YamlHelpers::require_positive(
                config.input.vectorDimensions, "memory.vector_dimensions");
    } else {
        YamlHelpers::require_positive(config.input.wordWidth, "memory.word_width");
        if (config.input.vectorDimensions != 0) {
            throw std::runtime_error(
                    "[Input] Error: memory.vector_dimensions is only valid for MCAM.");
        }
    }
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

void ValidateDerivedInputs(const EvaCamConfig &config, bool isMcam) {
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
    const long storageWidthBits = config.wordGeometry.storageWidthBits;
    const bool isStorageWidthPow2 =
        (storageWidthBits & (storageWidthBits - 1)) == 0;
    if (!isMcam && !isStorageWidthPow2 && config.wordGeometry.bitsPerCell == 1
            && config.runtimeSizing.realCapacity == 0) {
        throw std::runtime_error(
                "[Input] Error: non-power-of-two word_width requires extra.real_capacity to be set.");
    }
    if (config.wordGeometry.allocatedCapacityBits % storageWidthBits != 0) {
        throw std::runtime_error(
                "[Input] Error: resolved capacity must contain a whole number of configured vectors or words.");
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
        if (((allocatedBits / denom) % storageWidthBits) != 0) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity is incompatible with the configured vector or word width.");
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
                    "[Input] Error: organization.comparison_columns_per_step must be a positive divisor of the physical columns per stored entry.");
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
    const long long requiredPhysicalColumns =
        config.wordGeometry.physicalColumnsPerWord;
    const long long suppliedPhysicalColumns = CheckedMultiply(
            subarrayColumns, dataPartitions, "supplied physical word columns");
    if (suppliedPhysicalColumns != requiredPhysicalColumns) {
        const std::string logicalDescription = isMcam
            ? std::to_string(config.wordGeometry.vectorDimensions) + " MCAM vector dimensions"
            : std::to_string(config.wordGeometry.storageWidthBits) + " logical bits";
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions provides "
                + std::to_string(subarrayColumns) + " columns across each of "
                + std::to_string(dataPartitions) + " active data partitions ("
                + std::to_string(suppliedPhysicalColumns) + " per stored entry), but "
                + logicalDescription + " require exactly "
                + std::to_string(requiredPhysicalColumns)
                + " physical columns per stored entry.");
    }

    const long long entryCount = CheckedMultiply(
            subarrayRows, addressPartitions, "derived entry count");
    long long capacityBits = CheckedMultiply(
            entryCount, config.wordGeometry.storageWidthBits, "derived capacity");
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
            static_cast<long>(suppliedPhysicalColumns), isMcam);

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
    const bool nandString = YamlHelpers::read_optional<std::string>(
            root, "topology", "parallel") == "nand_string";
    if (nandString) {
        if ((memCellType != SLCNAND && memCellType != NAND3D)
                || LoadCamTypeForValidation(root, config.input.fileMemCell) != TCAM) {
            throw std::runtime_error(
                    "[Input] Error: cell.topology nand_string requires SLCNAND or NAND3D TCAM.");
        }
        if (!config.input.internalSensing) {
            throw std::runtime_error("[Input] Error: NAND TCAM requires internal sensing.");
        }
        return;
    }
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

void ValidateAndResolveNandGeometry(EvaCamConfig &config, const YAML::Node& cellRoot) {
    const MemCellType type = LoadMemCellTypeForValidation(cellRoot, config.input.fileMemCell);
    const bool isNand3d = type == NAND3D;
    Nand3dMemoryDevice nand3d;
    if (isNand3d) {
        // Parse the same typed device contract used by the runtime loader, so
        // geometry validation cannot accept a different stack interpretation.
        MemCell cell;
        cell.memCellType = NAND3D;
        cell.nandString = true;
        cell.camType = TCAM;
        cell.accessType = none_access;
        cell.processNode = 1;
        YamlHelpers::ReadNand3dSection(cell,
                LoadMemoryDeviceForValidation(cellRoot, config.input.fileMemCell));
        PhysicalDomainValidators::ValidateMemCell(cell);
        nand3d = cell.nand3d;
    }
    if (!config.runtimeSizing.hasFixedSubarrayDimensions) {
        throw std::runtime_error(
                "[Input] Error: NAND TCAM requires explicit organization.subarray.dimensions.");
    }
    if (config.input.pageSize <= 0 || config.input.flashBlockSize <= 0
            || config.input.flashBlockSize % config.input.pageSize != 0) {
        throw std::runtime_error(
                "[Input] Error: NAND flash requires positive page_size and block_size, with whole pages per block.");
    }
    const long rows = config.runtimeSizing.fixedSubarrayRows;
    const long columns = config.runtimeSizing.fixedSubarrayColumns;
    const long wordlines = isNand3d ? nand3d.storageLayers
            : config.input.flashBlockSize / config.input.pageSize;
    const long sensingColumns = isNand3d ? nand3d.stringColumns : rows;
    if (isNand3d) {
        const long long strings = CheckedMultiply(nand3d.stringRows, nand3d.stringColumns,
                "NAND3D strings per block");
        const long long blockBits = CheckedMultiply(strings, nand3d.storageLayers,
                "NAND3D physical block bits");
        if (rows != strings || config.input.pageSize != nand3d.stringColumns
                || config.input.flashBlockSize != blockBits) {
            throw std::runtime_error("[Input] Error: NAND3D subarray rows must equal string_rows * string_columns; flash.page_size must equal string_columns bits and block_size must equal strings * storage_layers bits.");
        }
    } else if (rows != config.input.pageSize || rows < 8 || rows > 1048576) {
        throw std::runtime_error(
                "[Input] Error: NAND subarray rows must equal flash.page_size in bits (8..1048576 strings).");
    }
    if (columns != config.input.wordWidth || columns <= 0
            || wordlines > 4096 || wordlines < 2
            || columns > (wordlines - 2) / 2) {
        throw std::runtime_error(
                "[Input] Error: NAND subarray columns must equal memory.word_width and fit complementary pairs plus a validity pair within at most 4096 wordlines.");
    }
    const auto &geometry = config.exploration.geometry;
    for (const auto *domain : {&geometry.numRowMat, &geometry.numColumnMat,
            &geometry.numRowSubarray, &geometry.numColumnSubarray,
            &geometry.numActiveMatPerRow, &geometry.numActiveMatPerColumn,
            &geometry.numActiveSubarrayPerRow, &geometry.numActiveSubarrayPerColumn}) {
        // Check bounds before materializing the domain: the bank also caps
        // total blocks, and oversized powers-of-two bounds can overflow the
        // generic domain enumerator before geometry validation runs.
        if (domain->Min() <= 0 || domain->Max() > 65536 || domain->Min() != domain->Max()) {
            throw std::runtime_error(
                    "[Input] Error: NAND TCAM requires fixed positive bank/mat totals and active counts of at most 65536.");
        }
    }
    for (const auto &partition : {
            std::pair<int, int>{geometry.numColumnMat.Min(), geometry.numActiveMatPerRow.Min()},
            {geometry.numRowMat.Min(), geometry.numActiveMatPerColumn.Min()},
            {geometry.numColumnSubarray.Min(), geometry.numActiveSubarrayPerRow.Min()},
            {geometry.numRowSubarray.Min(), geometry.numActiveSubarrayPerColumn.Min()}}) {
        if (partition.second > partition.first || partition.first % partition.second != 0) {
            throw std::runtime_error(
                    "[Input] Error: NAND active bank/mat counts must divide their totals.");
        }
    }
    const long long blocks = CheckedMultiply(
            CheckedTotalProduct(geometry.numRowMat, geometry.numColumnMat, "NAND mats"),
            CheckedTotalProduct(geometry.numRowSubarray, geometry.numColumnSubarray, "NAND blocks per mat"),
            "NAND total block count");
    if (blocks > 65536) {
        throw std::runtime_error("[Input] Error: NAND TCAM supports at most 65536 physical blocks.");
    }
    const long long logicalBits = CheckedMultiply(
            CheckedMultiply(blocks, rows, "NAND entry count"), columns, "NAND logical capacity");
    CheckedMultiply(blocks, config.input.flashBlockSize, "NAND physical capacity");
    const long long logicalBytes = logicalBits / 8;
    if (logicalBits % 8 != 0 || (config.runtimeSizing.hasExplicitCapacity
            && !config.runtimeSizing.capacityIsAuto && config.input.capacity != logicalBytes)) {
        throw std::runtime_error(
                "[Input] Error: memory.capacity must equal NAND logical key capacity (all blocks, independent of active counts).");
    }
    if (config.runtimeSizing.realCapacity != 0 && config.runtimeSizing.realCapacity != logicalBytes) {
        throw std::runtime_error(
                "[Input] Error: NAND memory.physical_capacity is a legacy logical allocation override; omit it and use flash geometry for physical cells.");
    }
    config.input.capacity = logicalBytes;
    config.ResolveWordGeometry(1, columns, false);
    if (geometry.muxSenseAmp.Min() <= 0 || geometry.muxSenseAmp.Max() > sensingColumns) {
        throw std::runtime_error("[Input] Error: NAND sense amplifier mux must divide the selected page string count.");
    }
    for (int mux : geometry.muxSenseAmp.Values()) {
        if (mux <= 0 || sensingColumns % mux != 0) {
            throw std::runtime_error("[Input] Error: NAND sense amplifier mux must divide the selected page string count.");
        }
    }
    for (const auto* domain : {&geometry.muxOutputLev1, &geometry.muxOutputLev2}) {
        if (domain->Min() != 1 || domain->Max() != 1) {
            throw std::runtime_error("[Input] Error: NAND CAM requires output_level1 and output_level2 mux values of 1.");
        }
    }
    if (config.runtimeSizing.hasExplicitComparisonColumns
            && (config.exploration.cam.bitSerialWidth.Min() != columns
                    || config.exploration.cam.bitSerialWidth.Max() != columns)) {
        throw std::runtime_error("[Input] Error: NAND comparison_columns_per_step must equal the complete key width; segmented search is not supported.");
    }
}

}  // namespace

void InputRuleValidator::Validate(EvaCamConfig &config) {
    const YAML::Node cellRoot = LoadCellFileForValidation(config.input.fileMemCell);
    const bool isMcam =
        LoadCamTypeForValidation(cellRoot, config.input.fileMemCell) == MCAM;
    const bool isNand = YamlHelpers::read_optional<std::string>(
            cellRoot, "topology", "parallel") == "nand_string";
    ValidateScalarDomains(config, isMcam);
    ValidateMemCellSupport(config);
    if (isNand) {
        ValidateAndResolveNandGeometry(config, cellRoot);
        ResolveComparisonColumns(config);
        ValidatePeripheralSupport(config);
        return; // NAND device/capability validation follows in the typed loader.
    }
    const int bitsPerCell = LoadBitsPerCellForValidation(config);
    config.ResolveWordGeometry(bitsPerCell, 0, isMcam);
    ValidateAndResolveExplicitSubarrayDimensions(config, isMcam);
    ResolveComparisonColumns(config);
    ValidateDerivedInputs(config, isMcam);
    ValidatePeripheralSupport(config);
}
