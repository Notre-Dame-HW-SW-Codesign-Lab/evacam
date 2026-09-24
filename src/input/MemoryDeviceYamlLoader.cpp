#include "input/MemoryDeviceYamlLoader.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "MemCell.h"
#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"
#include "input/PhysicalDomainValidators.h"

namespace {
using YamlHelpers::child_optional;
using YamlHelpers::read_quantity_required;

void ReadResistanceSection(MemCell& cell, const YAML::Node& root) {
    auto res = child_optional(root, "resistance");
    if (!res) return;
    YAML::Node on = child_optional(res, "on");
    if (on) cell.resistanceOn = YamlHelpers::parse_quantity_node(on, YamlHelpers::ResistanceUnits(), 1.0, "resistance.on");
    YAML::Node off = child_optional(res, "off");
    if (off) cell.resistanceOff = YamlHelpers::parse_quantity_node(off, YamlHelpers::ResistanceUnits(), 1.0, "resistance.off");
}
void ReadReadSection(MemCell& cell, const YAML::Node& root) {
    auto read = child_optional(root, "read");
    if (!read) return;
    cell.readMode = YamlHelpers::read_required<std::string>(read, "mode") == "voltage";
    if (child_optional(read, "voltage")) {
        cell.readVoltage = read_quantity_required(
                read, "voltage", YamlHelpers::VoltageUnits(), 1.0,
                "memory_device.read.voltage");
    }
    if (child_optional(read, "current")) cell.readCurrent = read_quantity_required(read, "current", YamlHelpers::CurrentUnits(), 1.0, "read.current");
    if (child_optional(read, "power")) cell.readPower = read_quantity_required(read, "power", YamlHelpers::PowerUnits(), 1.0, "read.power");
    if (child_optional(read, "energy")) cell.readEnergy = read_quantity_required(read, "energy", YamlHelpers::EnergyUnits(), 1.0, "read.energy");
    if (child_optional(read, "min_sense_voltage")) cell.minSenseVoltage = read_quantity_required(read, "min_sense_voltage", YamlHelpers::VoltageUnits(), 1.0, "read.min_sense_voltage");
}
void ReadWriteSection(MemCell& cell, const YAML::Node& root) {
    auto write = child_optional(root, "write");
    if (!write) return;
    auto set = child_optional(write, "set");
    if (set) {
        if (child_optional(set, "voltage")) cell.setVoltage = read_quantity_required(set, "voltage", YamlHelpers::VoltageUnits(), 1.0, "write.set.voltage");
        if (child_optional(set, "current")) cell.setCurrent = read_quantity_required(set, "current", YamlHelpers::CurrentUnits(), 1.0, "write.set.current");
        if (child_optional(set, "pulse")) cell.setPulse = read_quantity_required(set, "pulse", YamlHelpers::TimeUnits(), 1.0, "write.set.pulse");
        if (child_optional(set, "energy")) cell.setEnergy = read_quantity_required(set, "energy", YamlHelpers::EnergyUnits(), 1.0, "write.set.energy");
    }
    auto reset = child_optional(write, "reset");
    if (reset) {
        if (child_optional(reset, "voltage")) cell.resetVoltage = read_quantity_required(reset, "voltage", YamlHelpers::VoltageUnits(), 1.0, "write.reset.voltage");
        if (child_optional(reset, "current")) cell.resetCurrent = read_quantity_required(reset, "current", YamlHelpers::CurrentUnits(), 1.0, "write.reset.current");
        if (child_optional(reset, "pulse")) cell.resetPulse = read_quantity_required(reset, "pulse", YamlHelpers::TimeUnits(), 1.0, "write.reset.pulse");
        if (child_optional(reset, "energy")) cell.resetEnergy = read_quantity_required(reset, "energy", YamlHelpers::EnergyUnits(), 1.0, "write.reset.energy");
    }
}
}  // namespace

namespace YamlHelpers {

void validate_memory_device_keys(const YAML::Node& root) {
    reject_unknown_keys(root,
            {"schema", "name", "type", "cell", "resistance", "capacitance",
             "device", "read", "write", "match", "sram", "flash", "variation",
             "mcam", "dram", "nand", "nand3d"},
            "memory_device");

    reject_unknown_keys(child_optional(root, "cell"),
            {"name", "cam_type", "type", "cell_process_node", "area", "aspect_ratio"},
            "memory_device.cell");

    const YAML::Node resistance = child_optional(root, "resistance");
    reject_unknown_keys(resistance,
            {"on", "off", "at_set", "at_reset", "at_read", "at_half_read",
             "at_half_reset"},
            "memory_device.resistance");
    for (const char* section : {"at_set", "at_reset", "at_read", "at_half_read"}) {
        reject_unknown_keys(child_optional(resistance, section), {"on", "off"},
                std::string("memory_device.resistance.") + section);
    }
    reject_unknown_keys(child_optional(resistance, "at_half_reset"), {"on"},
            "memory_device.resistance.at_half_reset");

    reject_unknown_keys(child_optional(root, "capacitance"), {"on", "off"},
            "memory_device.capacitance");
    reject_unknown_keys(child_optional(root, "device"),
            {"gate_ox_thickness_factor", "soi_width"}, "memory_device.device");
    reject_unknown_keys(child_optional(root, "read"),
            {"mode", "voltage", "current", "power", "energy", "min_sense_voltage",
             "wordline_boost_ratio", "read_floating"},
            "memory_device.read");

    const YAML::Node write = child_optional(root, "write");
    reject_unknown_keys(write, {"set", "reset"}, "memory_device.write");
    for (const char* operation : {"set", "reset"}) {
        reject_unknown_keys(child_optional(write, operation),
                {"mode", "voltage", "current", "pulse", "energy"},
                std::string("memory_device.write.") + operation);
    }

    reject_unknown_keys(child_optional(root, "match"),
            {"cmos_width", "is_nvm_discharge", "additional_cap_on_ml"},
            "memory_device.match");
    reject_unknown_keys(child_optional(root, "sram"), {"nmos_width", "pmos_width"},
            "memory_device.sram");
    reject_unknown_keys(child_optional(root, "flash"),
            {"erase_voltage", "program_voltage", "pass_voltage", "erase_time",
             "program_time", "gate_coupling_ratio"},
            "memory_device.flash");
    reject_unknown_keys(child_optional(root, "variation"),
            {"with_variation", "seed", "mode", "monte_carlo_granularity", "lut_file",
             "samples", "memory_device_resistance_on_stdev",
             "memory_device_resistance_off_stdev", "memory_device_resistance_on_max_var",
             "memory_device_resistance_off_max_var"},
            "memory_device.variation");
    reject_unknown_keys(child_optional(root, "mcam"),
            {"num_resistance_state", "resistance_state", "pair_resistance", "state_variation",
             "ml_precharge_voltage", "searchline_voltage"},
            "memory_device.mcam");
}

namespace {

NandDeviceSpec ReadNandElectrical(const YAML::Node& node, const std::string& prefix) {
    NandDeviceSpec spec;
    spec.model = read_required<std::string>(node, "model");
    spec.calibrationStatus = read_required<std::string>(node, "calibration_status");
    spec.source = read_required<std::string>(node, "source");
    spec.supplyEfficiency = read_optional<double>(node, "supply_efficiency", 1.0);

    auto readGroup = [&](const char* name, const std::vector<UnitSpec>& units,
            std::initializer_list<std::pair<const char*, double*>> fields) {
        const YAML::Node group = child_required(node, name);
        if (!group.IsMap()) {
            throw std::runtime_error(prefix + "." + name + " must be a mapping");
        }
        for (const auto& entry : group) {
            const std::string key = entry.first.as<std::string>();
            if (std::none_of(fields.begin(), fields.end(), [&](const auto& field) {
                    return key == field.first;
                })) {
                throw std::runtime_error("unknown key " + prefix + "." + std::string(name) + "." + key);
            }
        }
        for (const auto& field : fields) {
            const std::string path = prefix + "." + std::string(name) + "." + field.first;
            *field.second = read_quantity_required(group, field.first, units, 1.0, path.c_str());
        }
    };
    readGroup("resistance", ResistanceUnits(), {{"read_on", &spec.resistanceReadOn},
            {"pass", &spec.resistancePass}, {"off", &spec.resistanceOff},
            {"select", &spec.resistanceSelect}});
    readGroup("capacitance", CapacitanceUnits(), {{"gate", &spec.capacitanceGate},
            {"internal", &spec.capacitanceInternal}, {"bitline", &spec.capacitanceBitline},
            {"source", &spec.capacitanceSource}, {"select", &spec.capacitanceSelect}});
    readGroup("threshold", VoltageUnits(), {{"low", &spec.thresholdLow}, {"high", &spec.thresholdHigh}});
    readGroup("bias", VoltageUnits(), {{"read", &spec.voltageRead},
            {"pass", &spec.voltagePass}, {"precharge", &spec.voltagePrecharge}});
    const YAML::Node sensing = child_required(node, "sensing");
    reject_unknown_keys(sensing, {"decision_time", "min_margin", "reference_voltage", "offset"},
            (prefix + ".sensing").c_str());
    spec.decisionTime = read_quantity_required(sensing, "decision_time", TimeUnits(), 1.0,
            (prefix + ".sensing.decision_time").c_str());
    spec.minSenseMargin = read_quantity_required(sensing, "min_margin", VoltageUnits(), 1.0,
            (prefix + ".sensing.min_margin").c_str());
    if (child_optional(sensing, "reference_voltage")) {
        spec.referenceVoltage = read_quantity_required(sensing, "reference_voltage", VoltageUnits(), 1.0,
                (prefix + ".sensing.reference_voltage").c_str());
    }
    if (child_optional(sensing, "offset")) {
        spec.senseOffset = read_quantity_required(sensing, "offset", VoltageUnits(), 1.0,
                (prefix + ".sensing.offset").c_str());
    }
    const std::vector<UnitSpec> areaUnits = {{"m^2", 1.0}, {"cm^2", 1e-4},
            {"mm^2", 1e-6}, {"um^2", 1e-12}, {"nm^2", 1e-18}};
    for (auto field : {std::pair<const char*, NandPeripheralSpec*>{"wordline_driver", &spec.wordlineDriver},
            {"sense", &spec.sense}, {"page_buffer", &spec.pageBuffer}}) {
        const YAML::Node group = child_required(node, field.first);
        const std::string path = prefix + "." + std::string(field.first);
        reject_unknown_keys(group, {"area", "latency", "energy", "leakage"}, path);
        field.second->area = read_quantity_required(group, "area", areaUnits, 1.0, (path + ".area").c_str());
        field.second->latency = read_quantity_required(group, "latency", TimeUnits(), 1.0, (path + ".latency").c_str());
        field.second->energy = read_quantity_required(group, "energy", EnergyUnits(), 1.0, (path + ".energy").c_str());
        field.second->leakage = read_quantity_required(group, "leakage", PowerUnits(), 1.0, (path + ".leakage").c_str());
    }
    for (auto field : {std::pair<const char*, NandOperationSpec*>{"query", &spec.query},
            {"setup", &spec.setup}, {"precharge", &spec.precharge}, {"recovery", &spec.recovery},
            {"program_page", &spec.programPage}, {"erase_block", &spec.eraseBlock}}) {
        const YAML::Node group = child_required(node, field.first);
        const std::string path = prefix + "." + std::string(field.first);
        reject_unknown_keys(group, {"latency", "energy"}, path);
        field.second->latency = read_quantity_required(group, "latency", TimeUnits(), 1.0, (path + ".latency").c_str());
        field.second->energy = read_quantity_required(group, "energy", EnergyUnits(), 1.0, (path + ".energy").c_str());
    }
    spec.configured = true;
    return spec;
}

void RejectNonNandSections(const YAML::Node& root, const char* section) {
    for (const char* key : {"cell", "resistance", "capacitance", "device", "read",
            "write", "match", "sram", "flash", "variation", "mcam", "dram"}) {
        if (child_optional(root, key)) {
            throw std::runtime_error(std::string("memory_device.") + key
                    + " is not supported with " + section + "; provide parameters in memory_device." + section);
        }
    }
}

void SetNandOperationAliases(MemCell& cell, const NandDeviceSpec& spec) {
    cell.flashProgramTime = spec.programPage.latency;
    cell.flashEraseTime = spec.eraseBlock.latency;
    cell.flashPassVoltage = spec.voltagePass;
}

}  // namespace

void ReadNandSection(MemCell& cell, const YAML::Node& root) {
    const YAML::Node node = child_optional(root, "nand");
    if (!node) return;
    if (cell.memCellType != SLCNAND) {
        throw std::runtime_error("memory_device.nand requires type: SLCNAND");
    }
    if (child_optional(root, "nand3d")) {
        throw std::runtime_error("memory_device.nand and nand3d cannot be combined");
    }
    RejectNonNandSections(root, "nand");
    reject_unknown_keys(node, {"model", "calibration_status", "source", "resistance",
            "capacitance", "threshold", "bias", "sensing", "wordline_driver",
            "sense", "page_buffer", "query", "setup", "precharge", "recovery",
            "program_page", "erase_block", "supply_efficiency"}, "memory_device.nand");
    cell.nand = ReadNandElectrical(node, "memory_device.nand");
    SetNandOperationAliases(cell, cell.nand);
}

void ReadNand3dSection(MemCell& cell, const YAML::Node& root) {
    const YAML::Node node = child_optional(root, "nand3d");
    if (!node) return;
    if (cell.memCellType != NAND3D) {
        throw std::runtime_error("memory_device.nand3d requires type: NAND3D");
    }
    if (child_optional(root, "nand")) {
        throw std::runtime_error("memory_device.nand and nand3d cannot be combined");
    }
    RejectNonNandSections(root, "nand3d");
    reject_unknown_keys(node, {"model", "calibration_status", "source", "resistance",
            "capacitance", "threshold", "bias", "sensing", "wordline_driver",
            "sense", "page_buffer", "query", "setup", "precharge", "recovery",
            "program_page", "erase_block", "supply_efficiency", "storage_mode",
            "stack", "layout", "solver", "precharge_driver_resistance"}, "memory_device.nand3d");
    Nand3dMemoryDevice spec;
    spec.storageMode = read_required<std::string>(node, "storage_mode");
    spec.electrical = ReadNandElectrical(node, "memory_device.nand3d");
    const YAML::Node stack = child_required(node, "stack");
    reject_unknown_keys(stack, {"storage_layers", "dummy_layers"}, "memory_device.nand3d.stack");
    spec.storageLayers = read_required<int>(stack, "storage_layers");
    spec.dummyLayers = read_required<int>(stack, "dummy_layers");
    const YAML::Node layout = child_required(node, "layout");
    reject_unknown_keys(layout, {"string_rows", "string_columns", "hole_pitch_x", "hole_pitch_y",
            "layer_pitch", "staircase_step_width", "staircase_contact_length", "isolation_width",
            "peripheral_placement"}, "memory_device.nand3d.layout");
    spec.stringRows = read_required<int>(layout, "string_rows");
    spec.stringColumns = read_required<int>(layout, "string_columns");
    spec.peripheralPlacement = read_required<std::string>(layout, "peripheral_placement");
    for (const auto& field : {std::pair<const char*, double*>{"hole_pitch_x", &spec.holePitchX},
            {"hole_pitch_y", &spec.holePitchY}, {"layer_pitch", &spec.layerPitch},
            {"staircase_step_width", &spec.staircaseStepWidth},
            {"staircase_contact_length", &spec.staircaseContactLength},
            {"isolation_width", &spec.isolationWidth}}) {
        *field.second = read_quantity_required(layout, field.first, LengthUnits(), 1.0,
                ("memory_device.nand3d.layout." + std::string(field.first)).c_str());
    }
    const YAML::Node solver = child_required(node, "solver");
    reject_unknown_keys(solver, {"max_step", "tolerance", "max_steps"}, "memory_device.nand3d.solver");
    spec.solverMaxStep = read_quantity_required(solver, "max_step", TimeUnits(), 1.0,
            "memory_device.nand3d.solver.max_step");
    spec.solverTolerance = read_quantity_required(solver, "tolerance", VoltageUnits(), 1.0,
            "memory_device.nand3d.solver.tolerance");
    spec.solverMaxSteps = read_required<int>(solver, "max_steps");
    spec.prechargeDriverResistance = read_quantity_required(node, "precharge_driver_resistance",
            ResistanceUnits(), 1.0, "memory_device.nand3d.precharge_driver_resistance");
    spec.configured = true;
    cell.nand3d = spec;
    SetNandOperationAliases(cell, cell.nand3d.electrical);
}

void ReadMemoryDeviceFromYaml(MemCell& cell, const std::string& inputFile) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    validate_memory_device_keys(root);
    if (child_optional(root, "type")) {
        cell.memCellType = read_enum_required<MemCellType>(root, "type", false);
    }
    const YAML::Node cellNode = child_optional(root, "cell");
    if (cellNode) {
        cell.memCellType = read_enum_required<MemCellType>(cellNode, "type", false);
        cell.processNode = YamlHelpers::checked_integer<int>(
                read_quantity_required(
                        cellNode, "cell_process_node", LengthUnits(), 1.0,
                        "cell.cell_process_node") / 1e-9,
                "cell.cell_process_node in nanometers");
        cell.area = read_quantity_required(cellNode, "area", YamlHelpers::FeatureAreaUnits(), 1.0, "cell.area");
        cell.aspectRatio = YamlHelpers::read_required<double>(cellNode, "aspect_ratio");
        YamlHelpers::require_positive(cell.processNode, "cell.cell_process_node");
        YamlHelpers::require_positive(cell.area, "cell.area");
        YamlHelpers::require_positive(cell.aspectRatio, "cell.aspect_ratio");
        cell.heightInFeatureSize = std::sqrt(cell.area * cell.aspectRatio);
        cell.widthInFeatureSize = std::sqrt(cell.area / cell.aspectRatio);
    }
    ReadResistanceSection(cell, root);
    ReadReadSection(cell, root);
    ReadWriteSection(cell, root);
    ReadNandSection(cell, root);
    ReadNand3dSection(cell, root);
    PhysicalDomainValidators::ValidateMemCell(cell);
}
}  // namespace YamlHelpers
