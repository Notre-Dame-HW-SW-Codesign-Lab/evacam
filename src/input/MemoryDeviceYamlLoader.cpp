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
    if (child_optional(read, "voltage")) cell.readVoltage = read_quantity_required(read, "voltage", YamlHelpers::VoltageUnits(), 1.0, "read.voltage");
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
void ReadMcamSection(MemCell& cell, const YAML::Node& root) {
    auto mcam = child_optional(root, "mcam");
    if (!mcam) return;
    if (child_optional(mcam, "center_voltage")) {
        cell.hasMcamCenterVoltage = true;
        cell.centerVoltage = read_quantity_required(mcam, "center_voltage", YamlHelpers::McamCenterVoltageUnits(), 1.0, "mcam.center_voltage");
    }
}
}  // namespace

namespace YamlHelpers {

void validate_memory_device_keys(const YAML::Node& root) {
    reject_unknown_keys(root,
            {"schema", "name", "type", "cell", "resistance", "capacitance",
             "device", "read", "write", "match", "sram", "flash", "variation",
             "mcam", "dram"},
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
            {"num_resistance_state", "resistance_state", "state_variation",
             "ml_precharge_voltage", "searchline_voltage", "center_voltage"},
            "memory_device.mcam");
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
    ReadMcamSection(cell, root);
    PhysicalDomainValidators::ValidateMemCell(cell);
}
}  // namespace YamlHelpers
