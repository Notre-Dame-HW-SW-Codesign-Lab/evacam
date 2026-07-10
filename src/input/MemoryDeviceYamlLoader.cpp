#include "input/MemoryDeviceYamlLoader.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "MemCell.h"
#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

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
void ReadMemoryDeviceFromYaml(MemCell& cell, const std::string& inputFile) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    if (child_optional(root, "type")) {
        cell.memCellType = read_enum_required<MemCellType>(root, "type", false);
    }
    const YAML::Node cellNode = child_optional(root, "cell");
    if (cellNode) {
        cell.memCellType = read_enum_required<MemCellType>(cellNode, "type", false);
        cell.processNode = static_cast<int>(std::lround(read_quantity_required(cellNode, "cell_process_node", LengthUnits(), 1.0, "cell.cell_process_node") / 1e-9));
        cell.area = read_quantity_required(cellNode, "area", YamlHelpers::FeatureAreaUnits(), 1.0, "cell.area");
        cell.aspectRatio = YamlHelpers::read_required<double>(cellNode, "aspect_ratio");
    }
    ReadResistanceSection(cell, root);
    ReadReadSection(cell, root);
    ReadWriteSection(cell, root);
    ReadMcamSection(cell, root);
}
}  // namespace YamlHelpers
