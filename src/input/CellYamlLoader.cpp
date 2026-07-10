#include "input/CellYamlLoader.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "MemCell.h"
#include "input/AccessDeviceYamlLoader.h"
#include "input/MemoryDeviceYamlLoader.h"
#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    return s.substr(start, end - start);
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

double parse_fraction_or_percent_node(const YAML::Node& node, const char* what) {
    if (!node) {
        throw std::runtime_error(std::string("Missing value for ") + what);
    }

    if (node.IsScalar()) {
        const std::string raw = node.as<std::string>();
        std::string s = trim(raw);
        if (s.empty()) {
            throw std::runtime_error(std::string("Empty value for ") + what);
        }

        bool isPercent = !s.empty() && s.back() == '%';
        if (isPercent) {
            s.pop_back();
            s = trim(s);
        }

        char* end = nullptr;
        errno = 0;
        const double value = std::strtod(s.c_str(), &end);
        if (end == s.c_str() || errno != 0) {
            throw std::runtime_error(std::string("Invalid numeric value for ") + what + ": " + raw);
        }
        while (*end == ' ' || *end == '\t')
            ++end;
        if (*end != '\0') {
            throw std::runtime_error(std::string("Invalid suffix for ") + what + ": " + raw);
        }
        return isPercent ? (value / 100.0) : value;
    }

    throw std::runtime_error(std::string("Invalid node type for ") + what);
}

std::string resolve_reference(const std::string& ownerFile, const std::string& reference) {
    const std::filesystem::path referencePath(reference);
    if (referencePath.is_absolute()) {
        return referencePath.lexically_normal().string();
    }

    const std::filesystem::path ownerPath = std::filesystem::absolute(ownerFile);
    return (ownerPath.parent_path() / referencePath).lexically_normal().string();
}

bool is_v2_schema(const YAML::Node& root, const std::string& schema) {
    return YamlHelpers::schema_matches(root, schema);
}

bool has_key(const YAML::Node& node, const char* key) {
    if (!node || !node.IsMap()) {
        return false;
    }
    for (const auto& it : node) {
        if (it.first.IsScalar() && it.first.as<std::string>() == key) {
            return true;
        }
    }
    return false;
}

CAM_CmosRegion terminal_to_region(const YAML::Node& connection, const char* what) {
    const std::string terminal = to_lower(YamlHelpers::read_required<std::string>(
            connection, "terminal"));
    if (terminal == "gate") {
        return gate;
    }
    if (terminal == "source") {
        return source;
    }
    if (terminal == "drain") {
        return drain;
    }
    throw std::runtime_error(std::string("Invalid ") + what + ".connection.terminal: "
            + terminal);
}

void apply_access_spec(MemCell& cell, CAMPort& port,
        const YamlHelpers::AccessDeviceSpec& spec, int count) {
    cell.accessType = spec.type;
    cell.widthAccessCMOS = spec.width;
    cell.voltageDropAccessDevice = spec.voltageDrop;
    cell.leakageCurrentAccessDevice = spec.leakageCurrent;

    port.ConnectedRegion = spec.connectedTerminal;
    port.numCmos = count;
    port.isNMOS = spec.isNmos;
    port.widthCmos = spec.width;
}

void parse_voltage_fields(const YAML::Node& volts, CAMPort& port) {
    if (!volts || !volts.IsMap())
        return;
    if (YamlHelpers::child_optional(volts, "set_lrs"))
        port.volSetLRS = YamlHelpers::parse_quantity_node(
                YamlHelpers::child_optional(volts, "set_lrs"), YamlHelpers::VoltageUnits(), 1.0, "voltages.set_lrs");
    if (YamlHelpers::child_optional(volts, "set_mrs"))
        port.volSetMRS = YamlHelpers::parse_quantity_node(
                YamlHelpers::child_optional(volts, "set_mrs"), YamlHelpers::VoltageUnits(), 1.0, "voltages.set_mrs");
    if (YamlHelpers::child_optional(volts, "reset"))
        port.volReset = YamlHelpers::parse_quantity_node(
                YamlHelpers::child_optional(volts, "reset"), YamlHelpers::VoltageUnits(), 1.0, "voltages.reset");
    if (YamlHelpers::child_optional(volts, "search0"))
        port.volSearch0 = YamlHelpers::parse_quantity_node(
                YamlHelpers::child_optional(volts, "search0"), YamlHelpers::VoltageUnits(), 1.0, "voltages.search0");
    if (YamlHelpers::child_optional(volts, "search1"))
        port.volSearch1 = YamlHelpers::parse_quantity_node(
                YamlHelpers::child_optional(volts, "search1"), YamlHelpers::VoltageUnits(), 1.0, "voltages.search1");
}

void parse_port_connection(const YAML::Node& p, CAMPort& port, MemCell& cell,
        const std::string& ownerFile, bool isV2, const char* what) {
    const YAML::Node connection = YamlHelpers::child_optional(p, "connection");
    if (connection) {
        const std::string kind = to_lower(YamlHelpers::read_required<std::string>(
                connection, "kind"));
        if (kind == "memory_terminal") {
            if (isV2 && (has_key(p, "cmos_region")
                        || has_key(p, "num_cmos")
                        || has_key(p, "cmos_width")
                        || has_key(p, "is_nmos"))) {
                throw std::runtime_error(std::string("Legacy CMOS port fields are not allowed in ")
                        + what + " for cell");
            }
            port.ConnectedRegion = terminal_to_region(connection, what);
            port.numCmos = 1;
            port.isNMOS = true;
            port.widthCmos = 1.0;
            return;
        }
        if (kind == "access_device") {
            if (!isV2) {
                throw std::runtime_error(std::string("access_device connections require cell in ")
                        + what);
            }
            const std::string deviceFile = resolve_reference(ownerFile,
                    YamlHelpers::read_required<std::string>(connection, "device"));
            const int count = YamlHelpers::read_optional<int>(connection, "count", 1);
            if (count <= 0) {
                throw std::runtime_error(std::string("Invalid ") + what
                        + ".connection.count: must be positive");
            }
            const YamlHelpers::AccessDeviceSpec spec =
                    YamlHelpers::ReadAccessDeviceSpecFromYaml(deviceFile);
            if (spec.type == none_access) {
                throw std::runtime_error(std::string("Invalid ") + what
                        + ".connection.device: access type cannot be none");
            }
            if (spec.connectedTerminal == none) {
                throw std::runtime_error(std::string("Invalid ") + what
                        + ".connection.device: connected_terminal is required");
            }
            apply_access_spec(cell, port, spec, count);
            return;
        }
        if (kind != "access_terminal") {
            throw std::runtime_error(std::string("Invalid ") + what + ".connection.kind: "
                    + kind);
        }
        if (isV2) {
            throw std::runtime_error(std::string("access_terminal is not allowed in ")
                    + what + " for cell; use access_device");
        }
        port.ConnectedRegion = terminal_to_region(connection, what);
        port.numCmos = YamlHelpers::read_optional<int>(p, "num_cmos", 1);
        port.isNMOS = YamlHelpers::read_optional<bool>(p, "is_nmos", true);
        port.widthCmos = 1.0;
        if (YamlHelpers::child_optional(p, "cmos_width")) {
            port.widthCmos = YamlHelpers::read_quantity_required(
                    p, "cmos_width", YamlHelpers::FeatureUnits(), 1.0,
                    what);
        }
        return;
    }

    port.ConnectedRegion = YamlHelpers::read_enum_required<CAM_CmosRegion>(p, "cmos_region", false);
    port.numCmos = YamlHelpers::read_required<int>(p, "num_cmos");
    port.isNMOS = YamlHelpers::read_required<bool>(p, "is_nmos");
    port.widthCmos = YamlHelpers::read_quantity_required(
            p, "cmos_width", YamlHelpers::FeatureUnits(), 1.0,
            what);
}

void parse_ports(const YAML::Node& ports, MemCell& cell, const std::string& inputFile,
        bool isV2) {
    CAMPort (&camPort)[2][MAX_PORT] = cell.camPort;
    int& numRow = cell.camNumRow;
    int& numCol = cell.camNumCol;
    numRow = 0;
    numCol = 0;

    auto rows = YamlHelpers::child_optional(ports, "row");
    if (rows && rows.IsMap()) {
        int max_idx = -1;
        for (const auto& it : rows) {
            int idx = YamlHelpers::read_scalar_required<int>(it.first, "ports.row index");
            if (idx < 0 || idx >= MAX_PORT)
                throw std::runtime_error("row port index out of range");
            if (idx > max_idx)
                max_idx = idx;
        }
        if (max_idx >= 0) {
            for (int i = 0; i <= max_idx; i++) {
                camPort[0][i] = CAMPort{};
                camPort[0][i].IsCol = false;
                camPort[0][i].leak = false;
                camPort[0][i].isNVMdischarge = false;
            }
        }
        for (const auto& it : rows) {
            int idx = YamlHelpers::read_scalar_required<int>(it.first, "ports.row index");
            const YAML::Node p = it.second;
            CAMPort& port = camPort[0][idx];
            port.Type = YamlHelpers::read_enum_required<CAM_PortType>(p, "type", false);
            parse_port_connection(p, port, cell, inputFile, isV2, "ports.row");
            port.leak = YamlHelpers::read_optional<bool>(p, "leak", false);
            port.isNVMdischarge = YamlHelpers::read_optional<bool>(p, "is_nvm_discharge", false);
            port.widthWire = YamlHelpers::read_quantity_required(p, "wire_width", YamlHelpers::FeatureUnits(), 1.0, "ports.row.wire_width");

            parse_voltage_fields(YamlHelpers::child_optional(p, "voltages"), port);
        }
        numRow = max_idx + 1;
    }

    auto cols = YamlHelpers::child_optional(ports, "column");
    if (cols && cols.IsMap()) {
        int max_idx = -1;
        for (const auto& it : cols) {
            int idx = YamlHelpers::read_scalar_required<int>(it.first, "ports.column index");
            if (idx < 0 || idx >= MAX_PORT)
                throw std::runtime_error("column port index out of range");
            if (idx > max_idx)
                max_idx = idx;
        }
        if (max_idx >= 0) {
            for (int i = 0; i <= max_idx; i++) {
                camPort[1][i] = CAMPort{};
                camPort[1][i].IsCol = true;
                camPort[1][i].leak = false;
                camPort[1][i].isNVMdischarge = false;
            }
        }
        for (const auto& it : cols) {
            int idx = YamlHelpers::read_scalar_required<int>(it.first, "ports.column index");
            const YAML::Node p = it.second;
            CAMPort& port = camPort[1][idx];
            port.Type = YamlHelpers::read_enum_required<CAM_PortType>(p, "type", false);
            parse_port_connection(p, port, cell, inputFile, isV2, "ports.column");
            port.leak = YamlHelpers::read_optional<bool>(p, "leak", false);
            port.isNVMdischarge = YamlHelpers::read_optional<bool>(p, "is_nvm_discharge", false);
            port.widthWire = YamlHelpers::read_quantity_required(p, "wire_width", YamlHelpers::FeatureUnits(), 1.0, "ports.column.wire_width");

            parse_voltage_fields(YamlHelpers::child_optional(p, "voltages"), port);
        }
        numCol = max_idx + 1;
    }
}

void ReadCellSection(MemCell& cell, const YAML::Node& cellNode, const std::string& inputFile) {
    cell.memCellType = YamlHelpers::read_enum_required<MemCellType>(cellNode, "type", false);
    cell.processNode = (int)std::lround(YamlHelpers::read_quantity_required(
            cellNode, "cell_process_node", YamlHelpers::LengthUnits(), 1e-9, "cell.cell_process_node") / 1e-9);
    cell.area = YamlHelpers::read_quantity_required(cellNode, "area", YamlHelpers::FeatureAreaUnits(), 1.0, "cell.area");
    cell.aspectRatio = YamlHelpers::read_required<double>(cellNode, "aspect_ratio");
    cell.heightInFeatureSize = sqrt(cell.area * cell.aspectRatio);
    cell.widthInFeatureSize = sqrt(cell.area / cell.aspectRatio);

    std::string cellName;
    if (YamlHelpers::child_optional(cellNode, "name"))
        cellName = YamlHelpers::read_required<std::string>(cellNode, "name");

    cell.camType = TCAM;
    if (YamlHelpers::child_optional(cellNode, "cam_type")) {
        cell.camType = YamlHelpers::read_enum_required<CAMType>(cellNode, "cam_type", false);
    } else {
        std::string probe = to_lower(cellName + " " + inputFile);
        if (probe.find("mcam") != std::string::npos)
            cell.camType = MCAM;
        else if (probe.find("acam") != std::string::npos)
            cell.camType = ACAM;
        else if (probe.find("tcam") != std::string::npos)
            cell.camType = TCAM;
    }
}

void ReadV2CellSection(MemCell& cell, const YAML::Node& root, const std::string& inputFile) {
    if (has_key(root, "cell")) {
        throw std::runtime_error("cell uses top-level name/cam_type/layout, not cell:");
    }

    const YAML::Node layout = YamlHelpers::child_required(root, "layout");
    cell.processNode = (int)std::lround(YamlHelpers::read_quantity_required(
            layout, "cell_process_node", YamlHelpers::LengthUnits(), 1e-9,
            "layout.cell_process_node") / 1e-9);
    cell.area = YamlHelpers::read_quantity_required(layout, "area",
            YamlHelpers::FeatureAreaUnits(), 1.0, "layout.area");
    cell.aspectRatio = YamlHelpers::read_required<double>(layout, "aspect_ratio");
    cell.heightInFeatureSize = sqrt(cell.area * cell.aspectRatio);
    cell.widthInFeatureSize = sqrt(cell.area / cell.aspectRatio);

    std::string cellName;
    if (YamlHelpers::child_optional(root, "name")) {
        cellName = YamlHelpers::read_required<std::string>(root, "name");
    }

    cell.camType = TCAM;
    if (YamlHelpers::child_optional(root, "cam_type")) {
        cell.camType = YamlHelpers::read_enum_required<CAMType>(root, "cam_type", false);
    } else {
        std::string probe = to_lower(cellName + " " + inputFile);
        if (probe.find("mcam") != std::string::npos)
            cell.camType = MCAM;
        else if (probe.find("acam") != std::string::npos)
            cell.camType = ACAM;
        else if (probe.find("tcam") != std::string::npos)
            cell.camType = TCAM;
    }
}

void RejectV2DeviceSections(const YAML::Node& root) {
    const char* sections[] = {
        "access_device", "resistance", "capacitance", "device", "read", "write",
        "match", "dram", "sram", "flash", "variation", "mcam"
    };
    for (const char* section : sections) {
        if (has_key(root, section)) {
            throw std::runtime_error(std::string("cell does not allow top-level ")
                    + section + "; move it to the referenced device or architecture file");
        }
    }
}

void ReadAccessDeviceSection(MemCell& cell, const YAML::Node& root) {
    auto access = YamlHelpers::child_optional(root, "access_device");
    if (!access || !access.IsMap()) {
        return;
    }

    cell.accessType = YamlHelpers::read_enum_required<CellAccessType>(access, "type", false);
    if (YamlHelpers::child_optional(access, "cmos_width"))
        cell.widthAccessCMOS = YamlHelpers::read_quantity_required(
                access, "cmos_width", YamlHelpers::FeatureUnits(), 1.0, "access_device.cmos_width");
    if (YamlHelpers::child_optional(access, "voltage_drop"))
        cell.voltageDropAccessDevice = YamlHelpers::read_quantity_required(
                access, "voltage_drop", YamlHelpers::VoltageUnits(), 1.0, "access_device.voltage_drop");
    if (YamlHelpers::child_optional(access, "leakage_current"))
        cell.leakageCurrentAccessDevice = YamlHelpers::read_quantity_required(
                access, "leakage_current", YamlHelpers::CurrentUnits(), 1.0, "access_device.leakage_current");
}

void ReadResistanceSection(MemCell& cell, const YAML::Node& root) {
    auto res = YamlHelpers::child_optional(root, "resistance");
    if (!res) {
        return;
    }

    YAML::Node on = YamlHelpers::child_optional(res, "on");
    if (!on)
        on = YamlHelpers::child_optional_bool_key(res, true);
    YAML::Node off = YamlHelpers::child_optional(res, "off");
    if (!off)
        off = YamlHelpers::child_optional_bool_key(res, false);
    if (on)
        cell.resistanceOn = YamlHelpers::parse_quantity_node(on, YamlHelpers::ResistanceUnits(), 1.0, "resistance.on");
    if (off)
        cell.resistanceOff = YamlHelpers::parse_quantity_node(off, YamlHelpers::ResistanceUnits(), 1.0, "resistance.off");

    auto atSet = YamlHelpers::child_optional(res, "at_set");
    if (atSet) {
        if (YamlHelpers::child_optional(atSet, "on"))
            cell.resistanceOnAtSetVoltage = YamlHelpers::read_quantity_required(
                    atSet, "on", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_set.on");
        if (YamlHelpers::child_optional(atSet, "off"))
            cell.resistanceOffAtSetVoltage = YamlHelpers::read_quantity_required(
                    atSet, "off", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_set.off");
    }
    auto atReset = YamlHelpers::child_optional(res, "at_reset");
    if (atReset) {
        if (YamlHelpers::child_optional(atReset, "on"))
            cell.resistanceOnAtResetVoltage = YamlHelpers::read_quantity_required(
                    atReset, "on", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_reset.on");
        if (YamlHelpers::child_optional(atReset, "off"))
            cell.resistanceOffAtResetVoltage = YamlHelpers::read_quantity_required(
                    atReset, "off", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_reset.off");
    }
    auto atRead = YamlHelpers::child_optional(res, "at_read");
    if (atRead) {
        if (YamlHelpers::child_optional(atRead, "on"))
            cell.resistanceOnAtReadVoltage = YamlHelpers::read_quantity_required(
                    atRead, "on", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_read.on");
        if (YamlHelpers::child_optional(atRead, "off"))
            cell.resistanceOffAtReadVoltage = YamlHelpers::read_quantity_required(
                    atRead, "off", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_read.off");
    }
    auto atHalfRead = YamlHelpers::child_optional(res, "at_half_read");
    if (atHalfRead) {
        if (YamlHelpers::child_optional(atHalfRead, "on"))
            cell.resistanceOnAtHalfReadVoltage = YamlHelpers::read_quantity_required(
                    atHalfRead, "on", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_half_read.on");
        if (YamlHelpers::child_optional(atHalfRead, "off"))
            cell.resistanceOffAtHalfReadVoltage = YamlHelpers::read_quantity_required(
                    atHalfRead, "off", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_half_read.off");
    }
    auto atHalfReset = YamlHelpers::child_optional(res, "at_half_reset");
    if (atHalfReset && YamlHelpers::child_optional(atHalfReset, "on")) {
        cell.resistanceOnAtHalfResetVoltage = YamlHelpers::read_quantity_required(
                atHalfReset, "on", YamlHelpers::ResistanceUnits(), 1.0, "resistance.at_half_reset.on");
    }
}

void ReadCapacitanceSection(MemCell& cell, const YAML::Node& root) {
    auto cap = YamlHelpers::child_optional(root, "capacitance");
    if (!cap) {
        return;
    }

    if (YamlHelpers::child_optional(cap, "on"))
        cell.capacitanceOn = YamlHelpers::read_quantity_required(cap, "on", YamlHelpers::CapacitanceUnits(), 1.0, "capacitance.on");
    if (YamlHelpers::child_optional(cap, "off"))
        cell.capacitanceOff = YamlHelpers::read_quantity_required(cap, "off", YamlHelpers::CapacitanceUnits(), 1.0, "capacitance.off");
}

void ReadDeviceSection(MemCell& cell, const YAML::Node& root) {
    auto device = YamlHelpers::child_optional(root, "device");
    if (!device) {
        return;
    }

    cell.gateOxThicknessFactor = YamlHelpers::read_optional<double>(device, "gate_ox_thickness_factor", cell.gateOxThicknessFactor);
    if (YamlHelpers::child_optional(device, "soi_width")) {
        cell.widthSOIDevice = YamlHelpers::read_quantity_required(device, "soi_width", YamlHelpers::FeatureUnits(), 1.0, "device.soi_width");
    }
}

void ReadReadSection(MemCell& cell, const YAML::Node& root) {
    auto read = YamlHelpers::child_optional(root, "read");
    if (!read) {
        return;
    }

    const std::string mode = to_lower(YamlHelpers::read_required<std::string>(read, "mode"));
    if (mode == "voltage")
        cell.readMode = true;
    else if (mode == "current")
        cell.readMode = false;
    else
        throw std::runtime_error("Invalid read.mode: " + mode);

    if (YamlHelpers::child_optional(read, "voltage"))
        cell.readVoltage = YamlHelpers::read_quantity_required(read, "voltage", YamlHelpers::VoltageUnits(), 1.0, "read.voltage");
    if (YamlHelpers::child_optional(read, "current"))
        cell.readCurrent = YamlHelpers::read_quantity_required(read, "current", YamlHelpers::CurrentUnits(), 1.0, "read.current");
    if (YamlHelpers::child_optional(read, "power"))
        cell.readPower = YamlHelpers::read_quantity_required(read, "power", YamlHelpers::PowerUnits(), 1.0, "read.power");
    if (YamlHelpers::child_optional(read, "energy"))
        cell.readEnergy = YamlHelpers::read_quantity_required(read, "energy", YamlHelpers::EnergyUnits(), 1.0, "read.energy");
    if (YamlHelpers::child_optional(read, "min_sense_voltage"))
        cell.minSenseVoltage = YamlHelpers::read_quantity_required(
                read, "min_sense_voltage", YamlHelpers::VoltageUnits(), 1.0, "read.min_sense_voltage");
    cell.wordlineBoostRatio = YamlHelpers::read_optional<double>(read, "wordline_boost_ratio", cell.wordlineBoostRatio);
    cell.readFloating = YamlHelpers::read_optional<bool>(read, "read_floating", cell.readFloating);
}

void ReadWriteSection(MemCell& cell, const YAML::Node& root) {
    auto write = YamlHelpers::child_optional(root, "write");
    if (!write) {
        return;
    }

    auto set = YamlHelpers::child_optional(write, "set");
    if (set) {
        const std::string mode = to_lower(YamlHelpers::read_required<std::string>(set, "mode"));
        if (mode == "voltage")
            cell.setMode = true;
        else if (mode == "current")
            cell.setMode = false;
        else
            throw std::runtime_error("Invalid write.set.mode: " + mode);

        if (YamlHelpers::child_optional(set, "voltage"))
            cell.setVoltage = YamlHelpers::read_quantity_required(set, "voltage", YamlHelpers::VoltageUnits(), 1.0, "write.set.voltage");
        if (YamlHelpers::child_optional(set, "current"))
            cell.setCurrent = YamlHelpers::read_quantity_required(set, "current", YamlHelpers::CurrentUnits(), 1.0, "write.set.current");
        if (YamlHelpers::child_optional(set, "pulse"))
            cell.setPulse = YamlHelpers::read_quantity_required(set, "pulse", YamlHelpers::TimeUnits(), 1.0, "write.set.pulse");
        if (YamlHelpers::child_optional(set, "energy"))
            cell.setEnergy = YamlHelpers::read_quantity_required(set, "energy", YamlHelpers::EnergyUnits(), 1.0, "write.set.energy");
    }

    auto reset = YamlHelpers::child_optional(write, "reset");
    if (reset) {
        const std::string mode = to_lower(YamlHelpers::read_required<std::string>(reset, "mode"));
        if (mode == "voltage")
            cell.resetMode = true;
        else if (mode == "current")
            cell.resetMode = false;
        else
            throw std::runtime_error("Invalid write.reset.mode: " + mode);

        if (YamlHelpers::child_optional(reset, "voltage"))
            cell.resetVoltage = YamlHelpers::read_quantity_required(reset, "voltage", YamlHelpers::VoltageUnits(), 1.0, "write.reset.voltage");
        if (YamlHelpers::child_optional(reset, "current"))
            cell.resetCurrent = YamlHelpers::read_quantity_required(reset, "current", YamlHelpers::CurrentUnits(), 1.0, "write.reset.current");
        if (YamlHelpers::child_optional(reset, "pulse"))
            cell.resetPulse = YamlHelpers::read_quantity_required(reset, "pulse", YamlHelpers::TimeUnits(), 1.0, "write.reset.pulse");
        if (YamlHelpers::child_optional(reset, "energy"))
            cell.resetEnergy = YamlHelpers::read_quantity_required(reset, "energy", YamlHelpers::EnergyUnits(), 1.0, "write.reset.energy");
    }
}

void ReadMatchSection(MemCell& cell, const YAML::Node& root) {
    auto match = YamlHelpers::child_optional(root, "match");
    if (!match) {
        return;
    }

    if (YamlHelpers::child_optional(match, "cmos_width")) {
        cell.camWidthMatchTran = YamlHelpers::read_quantity_required(match, "cmos_width", YamlHelpers::FeatureUnits(), 1.0, "match.cmos_width");
    }
    cell.isNVMdischarge = YamlHelpers::read_optional<bool>(match, "is_nvm_discharge", cell.isNVMdischarge);
}

void ReadDramSection(MemCell& cell, const YAML::Node& root) {
    auto dram = YamlHelpers::child_optional(root, "dram");
    if (dram && YamlHelpers::child_optional(dram, "cell_capacitance")) {
        cell.capDRAMCell = YamlHelpers::read_quantity_required(
                dram, "cell_capacitance", YamlHelpers::CapacitanceUnits(), 1.0, "dram.cell_capacitance");
    }
}

void ReadSramSection(MemCell& cell, const YAML::Node& root) {
    auto sram = YamlHelpers::child_optional(root, "sram");
    if (!sram) {
        return;
    }

    if (YamlHelpers::child_optional(sram, "nmos_width")) {
        cell.widthSRAMCellNMOS = YamlHelpers::read_quantity_required(
                sram, "nmos_width", YamlHelpers::FeatureUnits(), 1.0, "sram.nmos_width");
    }
    if (YamlHelpers::child_optional(sram, "pmos_width")) {
        cell.widthSRAMCellPMOS = YamlHelpers::read_quantity_required(
                sram, "pmos_width", YamlHelpers::FeatureUnits(), 1.0, "sram.pmos_width");
    }
}

void ReadFlashSection(MemCell& cell, const YAML::Node& root) {
    auto flash = YamlHelpers::child_optional(root, "flash");
    if (!flash) {
        return;
    }

    if (YamlHelpers::child_optional(flash, "erase_voltage")) {
        cell.flashEraseVoltage = YamlHelpers::read_quantity_required(
                flash, "erase_voltage", YamlHelpers::VoltageUnits(), 1.0, "flash.erase_voltage");
    }
    if (YamlHelpers::child_optional(flash, "program_voltage")) {
        cell.flashProgramVoltage = YamlHelpers::read_quantity_required(
                flash, "program_voltage", YamlHelpers::VoltageUnits(), 1.0, "flash.program_voltage");
    }
    if (YamlHelpers::child_optional(flash, "pass_voltage")) {
        cell.flashPassVoltage = YamlHelpers::read_quantity_required(
                flash, "pass_voltage", YamlHelpers::VoltageUnits(), 1.0, "flash.pass_voltage");
    }
    if (YamlHelpers::child_optional(flash, "erase_time")) {
        cell.flashEraseTime = YamlHelpers::read_quantity_required(
                flash, "erase_time", YamlHelpers::TimeUnits(), 1.0, "flash.erase_time");
    }
    if (YamlHelpers::child_optional(flash, "program_time")) {
        cell.flashProgramTime = YamlHelpers::read_quantity_required(
                flash, "program_time", YamlHelpers::TimeUnits(), 1.0, "flash.program_time");
    }
    cell.gateCouplingRatio = YamlHelpers::read_optional<double>(flash, "gate_coupling_ratio", cell.gateCouplingRatio);
}

void ReadVariationSection(MemCell& cell, const YAML::Node& root) {
    auto variation = YamlHelpers::child_optional(root, "variation");
    if (!variation) {
        return;
    }

    cell.withVariation = YamlHelpers::read_optional<bool>(variation, "with_variation", true);
    if (YamlHelpers::child_optional(variation, "seed")) {
        cell.variationSeed = YamlHelpers::read_required<uint32_t>(variation, "seed");
        cell.hasVariationSeed = true;
    }
    if (YamlHelpers::child_optional(variation, "mode")) {
        cell.variationMode = YamlHelpers::read_required<std::string>(variation, "mode");
    }
    if (YamlHelpers::child_optional(variation, "monte_carlo_granularity")) {
        cell.monteCarloGranularity = YamlHelpers::read_required<std::string>(
                variation,
                "monte_carlo_granularity");
    }
    if (YamlHelpers::child_optional(variation, "lut_file")) {
        cell.variationLutFile = YamlHelpers::read_required<std::string>(variation, "lut_file");
    }
    if (YamlHelpers::child_optional(variation, "samples")) {
        cell.variationSamples = YamlHelpers::read_required<int>(variation, "samples");
        cell.hasVariationSamples = true;
    }
    if (YamlHelpers::child_optional(variation, "memory_device_resistance_on_stdev")) {
        cell.resistanceOnVariation = parse_fraction_or_percent_node(
                YamlHelpers::child_optional(variation, "memory_device_resistance_on_stdev"),
                "variation.memory_device_resistance_on_stdev");
    }
    if (YamlHelpers::child_optional(variation, "memory_device_resistance_off_stdev")) {
        cell.resistanceOffVariation = parse_fraction_or_percent_node(
                YamlHelpers::child_optional(variation, "memory_device_resistance_off_stdev"),
                "variation.memory_device_resistance_off_stdev");
    }
    if (YamlHelpers::child_optional(variation, "memory_device_resistance_on_max_var")) {
        cell.resistanceOnMaxVariation = parse_fraction_or_percent_node(
                YamlHelpers::child_optional(variation, "memory_device_resistance_on_max_var"),
                "variation.memory_device_resistance_on_max_var");
    }
    if (YamlHelpers::child_optional(variation, "memory_device_resistance_off_max_var")) {
        cell.resistanceOffMaxVariation = parse_fraction_or_percent_node(
                YamlHelpers::child_optional(variation, "memory_device_resistance_off_max_var"),
                "variation.memory_device_resistance_off_max_var");
    }
}

void ReadMcamSection(MemCell& cell, const YAML::Node& root) {
    auto mcam = YamlHelpers::child_optional(root, "mcam");
    if (!mcam) {
        return;
    }

    cell.numResistanceState = YamlHelpers::read_optional<int>(mcam, "num_resistance_state", cell.numResistanceState);

    auto states = YamlHelpers::child_optional(mcam, "resistance_state");
    if (states) {
        if (states.IsSequence()) {
            const int n = std::min<int>(states.size(), 64);
            for (int i = 0; i < n; i++) {
                cell.ResistanceState[i] = YamlHelpers::parse_quantity_node(states[i], YamlHelpers::ResistanceUnits(), 1.0, "mcam.resistance_state");
            }
            if (cell.numResistanceState == 0)
                cell.numResistanceState = n;
        } else if (states.IsMap()) {
            for (const auto& it : states) {
                const int idx = YamlHelpers::read_scalar_required<int>(it.first, "mcam.resistance_state index");
                if (idx < 0 || idx >= 64)
                    throw std::runtime_error("mcam.resistance_state index out of range");
                cell.ResistanceState[idx] = YamlHelpers::parse_quantity_node(
                        it.second, YamlHelpers::ResistanceUnits(), 1.0, "mcam.resistance_state");
            }
        } else {
            throw std::runtime_error("mcam.resistance_state must be sequence or map");
        }
    }

    auto stateVar = YamlHelpers::child_optional(mcam, "state_variation");
    if (stateVar) {
        if (stateVar.IsSequence()) {
            const int n = std::min<int>(stateVar.size(), 64);
            for (int i = 0; i < n; i++) {
                cell.resStateVariation[i] = parse_fraction_or_percent_node(stateVar[i], "mcam.state_variation");
            }
        } else if (stateVar.IsMap()) {
            for (const auto& it : stateVar) {
                const int idx = YamlHelpers::read_scalar_required<int>(it.first, "mcam.state_variation index");
                if (idx < 0 || idx >= 64)
                    throw std::runtime_error("mcam.state_variation index out of range");
                cell.resStateVariation[idx] = parse_fraction_or_percent_node(it.second, "mcam.state_variation");
            }
        } else {
            throw std::runtime_error("mcam.state_variation must be sequence or map");
        }
    }

    auto mlPrechargeVoltage = YamlHelpers::child_optional(mcam, "ml_precharge_voltage");
    if (mlPrechargeVoltage) {
        if (mlPrechargeVoltage.IsSequence()) {
            const int n = std::min<int>(mlPrechargeVoltage.size(), 64);
            for (int i = 0; i < n; i++) {
                cell.mlPrechargeVoltage[i] = YamlHelpers::parse_quantity_node(
                        mlPrechargeVoltage[i], YamlHelpers::VoltageUnits(), 1.0, "mcam.ml_precharge_voltage");
            }
        } else if (mlPrechargeVoltage.IsMap()) {
            for (const auto& it : mlPrechargeVoltage) {
                const int idx = YamlHelpers::read_scalar_required<int>(it.first, "mcam.ml_precharge_voltage index");
                if (idx < 0 || idx >= 64)
                    throw std::runtime_error("mcam.ml_precharge_voltage index out of range");
                cell.mlPrechargeVoltage[idx] = YamlHelpers::parse_quantity_node(
                        it.second, YamlHelpers::VoltageUnits(), 1.0, "mcam.ml_precharge_voltage");
            }
        } else {
            throw std::runtime_error("mcam.ml_precharge_voltage must be sequence or map");
        }
    }

    auto searchlineVoltage = YamlHelpers::child_optional(mcam, "searchline_voltage");
    if (searchlineVoltage) {
        cell.hasMcamSearchlineVoltages = true;
        if (searchlineVoltage.IsSequence()) {
            const int n = std::min<int>(searchlineVoltage.size(), 64);
            for (int i = 0; i < n; i++) {
                cell.searchlineVoltage[i] = YamlHelpers::parse_quantity_node(
                        searchlineVoltage[i], YamlHelpers::VoltageUnits(), 1.0, "mcam.searchline_voltage");
            }
        } else if (searchlineVoltage.IsMap()) {
            for (const auto& it : searchlineVoltage) {
                const int idx = YamlHelpers::read_scalar_required<int>(it.first, "mcam.searchline_voltage index");
                if (idx < 0 || idx >= 64)
                    throw std::runtime_error("mcam.searchline_voltage index out of range");
                cell.searchlineVoltage[idx] = YamlHelpers::parse_quantity_node(
                        it.second, YamlHelpers::VoltageUnits(), 1.0, "mcam.searchline_voltage");
            }
        } else {
            throw std::runtime_error("mcam.searchline_voltage must be sequence or map");
        }
    }

    if (YamlHelpers::child_optional(mcam, "center_voltage")) {
        cell.hasMcamCenterVoltage = true;
        cell.centerVoltage = YamlHelpers::read_quantity_required(
                mcam, "center_voltage", YamlHelpers::McamCenterVoltageUnits(), 1.0, "mcam.center_voltage");
    }
}

void ReadPortsSection(MemCell& cell, const YAML::Node& root,
        const std::string& inputFile, bool isV2) {
    auto ports = YamlHelpers::child_optional(root, "ports");
    if (ports)
        parse_ports(ports, cell, inputFile, isV2);
}

void ReadMemoryDeviceReference(MemCell& cell, const YAML::Node& root,
        const std::string& inputFile) {
    const YAML::Node reference = YamlHelpers::child_optional(root, "memory_device");
    if (!reference || !reference.IsScalar()) {
        return;
    }

    const YAML::Node deviceRoot = YAML::LoadFile(resolve_reference(
            inputFile, reference.as<std::string>()));
    YamlHelpers::require_schema(deviceRoot, "memory_device", "memory device config");
    cell.memCellType = YamlHelpers::read_enum_required<MemCellType>(
            deviceRoot, "type", false);
    ReadResistanceSection(cell, deviceRoot);
    ReadCapacitanceSection(cell, deviceRoot);
    ReadDeviceSection(cell, deviceRoot);
    ReadReadSection(cell, deviceRoot);
    ReadWriteSection(cell, deviceRoot);
    ReadDramSection(cell, deviceRoot);
    ReadSramSection(cell, deviceRoot);
    ReadFlashSection(cell, deviceRoot);
    ReadVariationSection(cell, deviceRoot);
    ReadMcamSection(cell, deviceRoot);
}

void ReadAccessDeviceReference(MemCell& cell, const YAML::Node& root,
        const std::string& inputFile) {
    const YAML::Node reference = YamlHelpers::child_optional(root, "access_device");
    if (!reference || !reference.IsScalar()) {
        return;
    }

    const YAML::Node accessRoot = YAML::LoadFile(resolve_reference(
            inputFile, reference.as<std::string>()));
    ReadAccessDeviceSection(cell, accessRoot);
}

}  // namespace

namespace YamlHelpers {

void ReadMemCellFromYaml(MemCell& cell, const std::string& inputFile) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    YamlHelpers::require_schema(root, "cell", "cell config");
    const bool isV2 = is_v2_schema(root, "cell");
    if (isV2) {
        cell.accessType = none_access;
        RejectV2DeviceSections(root);
        ReadV2CellSection(cell, root, inputFile);
    } else {
        const YAML::Node cellNode = child_required(root, "cell");
        ReadCellSection(cell, cellNode, inputFile);
    }
    ReadMemoryDeviceReference(cell, root, inputFile);
    ReadAccessDeviceReference(cell, root, inputFile);
    if (!isV2) {
        ReadAccessDeviceSection(cell, root);
        ReadResistanceSection(cell, root);
        ReadCapacitanceSection(cell, root);
        ReadDeviceSection(cell, root);
        ReadReadSection(cell, root);
        ReadWriteSection(cell, root);
        ReadMatchSection(cell, root);
        ReadDramSection(cell, root);
        ReadSramSection(cell, root);
        ReadFlashSection(cell, root);
        ReadVariationSection(cell, root);
        ReadMcamSection(cell, root);
    }
    ReadPortsSection(cell, root, inputFile, isV2);
}

}  // namespace YamlHelpers
