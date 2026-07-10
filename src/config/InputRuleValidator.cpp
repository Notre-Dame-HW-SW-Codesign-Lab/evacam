#include "config/InputRuleValidator.h"

#include <cctype>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

#include "EvaCamConfig.h"
#include "SenseAmp.h"
#include "input/CustomSenseAmpYamlLoader.h"
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

bool IsCellV2(const YAML::Node &root) {
    return YamlHelpers::schema_matches(root, "cell");
}

YAML::Node LoadV2MemoryDeviceForValidation(const YAML::Node &cellRoot,
        const std::string &cellFile) {
    const YAML::Node reference = YamlHelpers::child_required(cellRoot, "memory_device");
    return YAML::LoadFile(ResolveReference(cellFile,
            YamlHelpers::read_scalar_required<std::string>(
                reference, "memory_device")));
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
    if (IsCellV2(root)) {
        const YAML::Node memoryDevice = LoadV2MemoryDeviceForValidation(root, cellFile);
        return YamlHelpers::read_enum_required<MemCellType>(
                memoryDevice, "type", false);
    }
    const YAML::Node cellNode = YamlHelpers::child_required(root, "cell");
    return YamlHelpers::read_enum_required<MemCellType>(cellNode, "type", false);
}

CAMType LoadCamTypeForValidation(const YAML::Node &root, const std::string &cellFile) {
    if (IsCellV2(root)) {
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
    const YAML::Node cellNode = YamlHelpers::child_required(root, "cell");
    if (YamlHelpers::child_optional(cellNode, "cam_type")) {
        return YamlHelpers::read_enum_required<CAMType>(cellNode, "cam_type", false);
    }
    const std::string camType = InferCamTypeToken(cellNode, cellFile);
    if (camType == "MCAM") {
        return MCAM;
    }
    if (camType == "ACAM") {
        return ACAM;
    }
    return TCAM;
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

CAM_CmosRegion LoadPortConnectionRegion(const YAML::Node &portNode,
        const std::string &cellFile) {
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
    if (kind == "access_device") {
        const std::string accessFile = ResolveReference(
                cellFile, YamlHelpers::read_required<std::string>(connection, "device"));
        const YAML::Node accessRoot = YAML::LoadFile(accessFile);
        const YAML::Node accessNode = YamlHelpers::child_optional(accessRoot, "access_device")
                ? YamlHelpers::child_optional(accessRoot, "access_device")
                : accessRoot;
        return YamlHelpers::read_enum_required<CAM_CmosRegion>(
                accessNode, "connected_terminal", false);
    }
    throw std::runtime_error("[Input] Error: cell.ports.column has unsupported connection.kind.");
}

void ValidateCamColumnTopology(const YAML::Node &root, const std::string &cellFile) {
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
        const CAM_CmosRegion region = LoadPortConnectionRegion(portNode, cellFile);
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

void ValidateMcamResistanceStates(const EvaCamConfig &config, const YAML::Node &root) {
    const CAMType camType = LoadCamTypeForValidation(root, config.input.fileMemCell);
    if (camType != MCAM) {
        return;
    }

    const YAML::Node ownerRoot = IsCellV2(root)
            ? LoadV2MemoryDeviceForValidation(root, config.input.fileMemCell)
            : root;
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
    if (numStates < 2 || numStates > 64) {
        throw std::runtime_error(
                "[Input] Error: mcam.num_resistance_state must be between 2 and 64.");
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

    const YAML::Node mlPrechargeVoltages = YamlHelpers::child_optional(mcam, "ml_precharge_voltage");
    if (mlPrechargeVoltages) {
        if (!mlPrechargeVoltages.IsSequence() && !mlPrechargeVoltages.IsMap()) {
            throw std::runtime_error(
                    "[Input] Error: mcam.ml_precharge_voltage must be a sequence or map.");
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
    if (searchlineVoltages) {
        if (!searchlineVoltages.IsSequence() && !searchlineVoltages.IsMap()) {
            throw std::runtime_error(
                    "[Input] Error: mcam.searchline_voltage must be a sequence or map.");
        }
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
        }
    }

    const YAML::Node centerVoltage = YamlHelpers::child_optional(mcam, "center_voltage");
    if (centerVoltage) {
        const double voltage = YamlHelpers::parse_quantity_node(
                centerVoltage, YamlHelpers::McamCenterVoltageUnits(), 1.0, "mcam.center_voltage");
        if (voltage < 0) {
            throw std::runtime_error(
                    "[Input] Error: mcam.center_voltage must be non-negative.");
        }
    }

    if (static_cast<bool>(searchlineVoltages) != static_cast<bool>(centerVoltage)) {
        throw std::runtime_error(
                "[Input] Error: mcam.searchline_voltage and mcam.center_voltage must be provided together.");
    }
    if (searchlineVoltages && centerVoltage) {
        const YAML::Node ports = YamlHelpers::child_required(root, "ports");
        const YAML::Node rowPorts = YamlHelpers::child_required(ports, "row");
        int searchlineCount = 0;
        for (auto it = rowPorts.begin(); it != rowPorts.end(); ++it) {
            const CAM_PortType type =
                    YamlHelpers::read_enum_required<CAM_PortType>(it->second, "type", false);
            if (type == Searchline) {
                searchlineCount++;
            }
        }
        if (searchlineCount != 2) {
            throw std::runtime_error(
                    "[Input] Error: MCAM state voltage mapping requires exactly two searchline row ports.");
        }

        const double center = YamlHelpers::parse_quantity_node(
                centerVoltage, YamlHelpers::McamCenterVoltageUnits(), 1.0, "mcam.center_voltage");
        for (int state = 0; state < numStates; state++) {
            const YAML::Node voltageNode = searchlineVoltages[state];
            const double primary = YamlHelpers::parse_quantity_node(
                    voltageNode, YamlHelpers::VoltageUnits(), 1.0, "mcam.searchline_voltage");
            if (2 * center - primary < 0) {
                throw std::runtime_error(
                        "[Input] Error: MCAM derived complementary searchline voltage must be non-negative.");
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

void ValidateDerivedInputs(const EvaCamConfig &config) {
    if (config.input.wordWidth <= 0) {
        throw std::runtime_error("[Input] Error: word_width must be > 0.");
    }
    if (config.input.capacity <= 0) {
        throw std::runtime_error(
                "[Input] Error: memory.capacity must be > 0 unless organization.subarray.dimensions derives it.");
    }
    const bool isWordWidthPow2 = (config.input.wordWidth & (config.input.wordWidth - 1)) == 0;
    if (!isWordWidthPow2 && config.runtimeSizing.realCapacity == 0) {
        throw std::runtime_error(
                "[Input] Error: non-power-of-two word_width requires extra.real_capacity to be set.");
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
        if (((config.runtimeSizing.realCapacity / denom) % config.input.wordWidth) != 0) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity is incompatible with word_width.");
        }
    }
}

void ValidateAndResolveExplicitSubarrayDimensions(EvaCamConfig &config) {
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
    if (subarrayRows < 16 || subarrayRows > 512) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions row count must be between 16 and 512.");
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

    const long long partitionFactor = CheckedMultiply(banksTotal / banksActive,
            matsTotal / matsActive, "active partitioning");
    if (config.input.wordWidth % partitionFactor != 0) {
        throw std::runtime_error(
                "[Input] Error: word_width is incompatible with active bank/mat partitioning.");
    }
    const long long effectiveSubarrayRows = config.input.wordWidth / partitionFactor;
    if (effectiveSubarrayRows != subarrayRows) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions row count is incompatible with word_width.");
    }
    if ((effectiveSubarrayRows & (effectiveSubarrayRows - 1)) != 0) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions row count must match a power-of-two effective row count.");
    }

    long long capacityBits = subarrayRows;
    capacityBits = CheckedMultiply(capacityBits, subarrayColumns, "derived capacity");
    capacityBits = CheckedMultiply(capacityBits, banksTotal, "derived capacity");
    capacityBits = CheckedMultiply(capacityBits, matsTotal, "derived capacity");
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

    if (config.runtimeSizing.realCapacity > 0
            && config.runtimeSizing.realCapacity != derivedCapacityBytes) {
        throw std::runtime_error(
                "[Input] Error: extra.real_capacity must match capacity derived from organization.subarray.dimensions.");
    }
}

void ValidateMemCellSupport(const EvaCamConfig &config) {
    const YAML::Node root = LoadCellFileForValidation(config.input.fileMemCell);
    const MemCellType memCellType = LoadMemCellTypeForValidation(root, config.input.fileMemCell);
    ValidateCamPortPresence(root);
    ValidateCamColumnTopology(root, config.input.fileMemCell);
    ValidateCamModelSupport(config, root, memCellType);
    ValidateMcamResistanceStates(config, root);

    if (!IsCamModelMemCellTypeSupported(memCellType)) {
        throw std::runtime_error(
                "[Input] Error: memory.cell.type is not supported for CAM modeling.");
    }

    if (!config.input.internalSensing && memCellType != SRAM) {
        throw std::runtime_error(
                "[Input] Error: external sensing is only supported for SRAM CAM cells.");
    }
}

}  // namespace

void InputRuleValidator::Validate(EvaCamConfig &config) {
    ValidateAndResolveExplicitSubarrayDimensions(config);
    ValidateDerivedInputs(config);
    ValidatePeripheralSupport(config);
    ValidateMemCellSupport(config);
}
