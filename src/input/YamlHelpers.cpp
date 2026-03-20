#include "input/YamlHelpers.h"
#include "MemCell.h"


namespace YamlHelpers {

    static std::string trim(const std::string& s) {
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
            start++;
        size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
            end--;
        return s.substr(start, end - start);
    }

    std::string kind(const YAML::Node& n) {
        if (!n)
            return "missing";
        if (n.IsNull())
            return "null";
        if (n.IsScalar())
            return "scalar";
        if (n.IsSequence())
            return "sequence";
        if (n.IsMap())
            return "map";
        return "other";
    }

    YAML::Node child_required(const YAML::Node& parent, const char* key) {
        if (!parent) {
            throw std::runtime_error(std::string("Parent node missing; cannot read key: ") + key);
        }
        if (!parent.IsMap()) {
            throw std::runtime_error(
                    std::string("Cannot read key '") + key + "' from parent of type " + kind(parent) + " (expected map)");
        }

        const YAML::Node cparent = parent;
        YAML::Node child = cparent[key];

        if (!child) {
            std::ostringstream oss;
            oss << "Missing key: " << key;
            if (parent.Mark().line != -1) {
                oss << " (near line " << (parent.Mark().line + 1) << ", col " << (parent.Mark().column + 1) << ")";
            }
            throw std::runtime_error(oss.str());
        }
        return child;
    }

    YAML::Node child_optional(const YAML::Node& parent, const char* key) {
        if (!parent || !parent.IsMap())
            return YAML::Node();
        const YAML::Node cparent = parent;
        return cparent[key];
    }

    YAML::Node child_optional_bool_key(const YAML::Node& parent, bool key) {
        if (!parent || !parent.IsMap())
            return YAML::Node();
        const YAML::Node cparent = parent;
        return cparent[key];
    }

    YAML::Node child_required_index(const YAML::Node& parent, size_t idx, const char* what) {
        if (!parent) {
            throw std::runtime_error(std::string("Parent node missing; cannot read index for ") + what);
        }
        if (!parent.IsSequence()) {
            throw std::runtime_error(
                    std::string("Cannot read index for ") + what + " from parent of type " + kind(parent) + " (expected sequence)");
        }
        if (idx >= parent.size()) {
            throw std::runtime_error(std::string("Index out of range for ") + what);
        }
        const YAML::Node cparent = parent;
        YAML::Node child = cparent[idx];
        if (!child) {
            throw std::runtime_error(std::string("Missing value for ") + what);
        }
        return child;
    }

    bool is_yaml_file(const std::string& path) {
        if (path.size() < 4)
            return false;
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".yaml") ||
            (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".yml");
    }

    static double parse_quantity_string(
            const std::string& raw,
            const std::vector<UnitSpec>& units,
            double default_unit_to_base,
            const char* what) {

        std::string s = trim(raw);
        if (s.empty())
            throw std::runtime_error(std::string("Empty value for ") + what);

        const char* begin = s.c_str();
        char* end = nullptr;
        double value = std::strtod(begin, &end);
        if (end == begin) {
            throw std::runtime_error(std::string("Invalid numeric value for ") + what + ": " + raw);
        }

        std::string unit = trim(std::string(end));
        if (unit.empty()) {
            return value * default_unit_to_base;
        }

        for (const auto& u : units) {
            if (unit == u.suffix) {
                return value * u.to_base;
            }
        }

        throw std::runtime_error(std::string("Unknown unit '") + unit + "' for " + what);
    }

    double parse_quantity_node(
            const YAML::Node& node,
            const std::vector<UnitSpec>& units,
            double default_unit_to_base,
            const char* what) {

        if (!node) {
            throw std::runtime_error(std::string("Missing value for ") + what);
        }

        if (node.IsScalar()) {
            const std::string raw = node.as<std::string>();
            return parse_quantity_string(raw, units, default_unit_to_base, what);
        }

        if (node.IsNull()) {
            throw std::runtime_error(std::string("Null value for ") + what);
        }

        throw std::runtime_error(std::string("Invalid node type for ") + what + ": " + kind(node));
    }

    double read_quantity_required(
            const YAML::Node& parent,
            const char* key,
            const std::vector<UnitSpec>& units,
            double default_unit_to_base,
            const char* what) {

        YAML::Node n = child_required(parent, key);
        return parse_quantity_node(n, units, default_unit_to_base, what);
    }

    const std::vector<UnitSpec>& VoltageUnits() {
        static const std::vector<UnitSpec> k = {
            {"V", 1.0},
            {"mV", 1e-3},
            {"uV", 1e-6},
            {"nV", 1e-9},
            {"kV", 1e3},
        };
        return k;
    }

    const std::vector<UnitSpec>& CurrentUnits() {
        static const std::vector<UnitSpec> k = {
            {"A", 1.0},
            {"mA", 1e-3},
            {"uA", 1e-6},
            {"nA", 1e-9},
            {"pA", 1e-12},
        };
        return k;
    }

    const std::vector<UnitSpec>& TimeUnits() {
        static const std::vector<UnitSpec> k = {
            {"s", 1.0},
            {"ms", 1e-3},
            {"us", 1e-6},
            {"ns", 1e-9},
            {"ps", 1e-12},
            {"fs", 1e-15},
        };
        return k;
    }

    const std::vector<UnitSpec>& CapacitanceUnits() {
        static const std::vector<UnitSpec> k = {
            {"F", 1.0},
            {"mF", 1e-3},
            {"uF", 1e-6},
            {"nF", 1e-9},
            {"pF", 1e-12},
            {"fF", 1e-15},
        };
        return k;
    }

    const std::vector<UnitSpec>& ResistanceUnits() {
        static const std::vector<UnitSpec> k = {
            {"ohm", 1.0},
            {"kohm", 1e3},
            {"Mohm", 1e6},
            {"Gohm", 1e9},
        };
        return k;
    }

    const std::vector<UnitSpec>& PowerUnits() {
        static const std::vector<UnitSpec> k = {
            {"W", 1.0},
            {"mW", 1e-3},
            {"uW", 1e-6},
            {"nW", 1e-9},
            {"pW", 1e-12},
        };
        return k;
    }

    const std::vector<UnitSpec>& EnergyUnits() {
        static const std::vector<UnitSpec> k = {
            {"J", 1.0},
            {"mJ", 1e-3},
            {"uJ", 1e-6},
            {"nJ", 1e-9},
            {"pJ", 1e-12},
            {"fJ", 1e-15},
        };
        return k;
    }

    const std::vector<UnitSpec>& TemperatureUnits() {
        static const std::vector<UnitSpec> k = {
            {"K", 1.0},
        };
        return k;
    }

    const std::vector<UnitSpec>& DataSizeUnits() {
        static const std::vector<UnitSpec> k = {
            {"B", 1.0},
            {"b", 1.0},
            {"Byte", 1.0},
            {"byte", 1.0},
            {"KB", 1024.0},
            {"kB", 1024.0},
            {"kb", 1024.0},
            {"MB", 1024.0 * 1024.0},
            {"mB", 1024.0 * 1024.0},
            {"mb", 1024.0 * 1024.0},
            {"GB", 1024.0 * 1024.0 * 1024.0},
            {"gB", 1024.0 * 1024.0 * 1024.0},
            {"gb", 1024.0 * 1024.0 * 1024.0},
        };
        return k;
    }

    const std::vector<UnitSpec>& BitUnits() {
        static const std::vector<UnitSpec> k = {
            {"bit", 1.0},
            {"bits", 1.0},
        };
        return k;
    }

    const std::vector<UnitSpec>& LengthUnits() {
        static const std::vector<UnitSpec> k = {
            {"m", 1.0},
            {"cm", 1e-2},
            {"mm", 1e-3},
            {"um", 1e-6},
            {"nm", 1e-9},
        };
        return k;
    }

    const std::vector<UnitSpec>& FeatureUnits() {
        static const std::vector<UnitSpec> k = {
            {"F", 1.0},
        };
        return k;
    }

    const std::vector<UnitSpec>& FeatureAreaUnits() {
        static const std::vector<UnitSpec> k = {
            {"F^2", 1.0},
        };
        return k;
    }

    namespace {

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

        void parse_voltage_fields(const YAML::Node& volts, CAMPort& port) {
            if (!volts || !volts.IsMap())
                return;
            if (child_optional(volts, "set_lrs"))
                port.volSetLRS = parse_quantity_node(child_optional(volts, "set_lrs"), VoltageUnits(), 1.0, "voltages.set_lrs");
            if (child_optional(volts, "set_mrs"))
                port.volSetMRS = parse_quantity_node(child_optional(volts, "set_mrs"), VoltageUnits(), 1.0, "voltages.set_mrs");
            if (child_optional(volts, "reset"))
                port.volReset = parse_quantity_node(child_optional(volts, "reset"), VoltageUnits(), 1.0, "voltages.reset");
            if (child_optional(volts, "search0"))
                port.volSearch0 = parse_quantity_node(child_optional(volts, "search0"), VoltageUnits(), 1.0, "voltages.search0");
            if (child_optional(volts, "search1"))
                port.volSearch1 = parse_quantity_node(child_optional(volts, "search1"), VoltageUnits(), 1.0, "voltages.search1");
        }

        void parse_ports(const YAML::Node& ports, CAMPort camPort[2][MAX_PORT], int& numRow, int& numCol) {
            numRow = 0;
            numCol = 0;

            auto rows = child_optional(ports, "row");
            if (rows && rows.IsMap()) {
                int max_idx = -1;
                for (const auto& it : rows) {
                    int idx = read_scalar_required<int>(it.first, "ports.row index");
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
                    int idx = read_scalar_required<int>(it.first, "ports.row index");
                    const YAML::Node p = it.second;
                    CAMPort& port = camPort[0][idx];
                    port.Type = read_enum_required<CAM_PortType>(p, "type", false);
                    port.ConnectedRegion = read_enum_required<CAM_CmosRegion>(p, "cmos_region", false);
                    port.numCmos = read_required<int>(p, "num_cmos");
                    port.isNMOS = read_required<bool>(p, "is_nmos");
                    port.leak = read_optional<bool>(p, "leak", false);
                    port.isNVMdischarge = read_optional<bool>(p, "is_nvm_discharge", false);
                    port.widthCmos = read_quantity_required(p, "cmos_width", FeatureUnits(), 1.0, "ports.row.cmos_width");
                    port.widthWire = read_quantity_required(p, "wire_width", FeatureUnits(), 1.0, "ports.row.wire_width");

                    parse_voltage_fields(child_optional(p, "voltages"), port);
                }
                numRow = max_idx + 1;
            }

            auto cols = child_optional(ports, "column");
            if (cols && cols.IsMap()) {
                int max_idx = -1;
                for (const auto& it : cols) {
                    int idx = read_scalar_required<int>(it.first, "ports.column index");
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
                    int idx = read_scalar_required<int>(it.first, "ports.column index");
                    const YAML::Node p = it.second;
                    CAMPort& port = camPort[1][idx];
                    port.Type = read_enum_required<CAM_PortType>(p, "type", false);
                    port.ConnectedRegion = read_enum_required<CAM_CmosRegion>(p, "cmos_region", false);
                    port.numCmos = read_required<int>(p, "num_cmos");
                    port.isNMOS = read_required<bool>(p, "is_nmos");
                    port.leak = read_optional<bool>(p, "leak", false);
                    port.isNVMdischarge = read_optional<bool>(p, "is_nvm_discharge", false);
                    port.widthCmos = read_quantity_required(p, "cmos_width", FeatureUnits(), 1.0, "ports.column.cmos_width");
                    port.widthWire = read_quantity_required(p, "wire_width", FeatureUnits(), 1.0, "ports.column.wire_width");

                    parse_voltage_fields(child_optional(p, "voltages"), port);
                }
                numCol = max_idx + 1;
            }
        }

    } // namespace

    void ReadMemCellFromYaml(MemCell& cell, const std::string& inputFile) {
        const YAML::Node root = YAML::LoadFile(inputFile);
        const YAML::Node cellNode = child_required(root, "cell");

        cell.memCellType = read_enum_required<MemCellType>(cellNode, "type", false);
        cell.processNode = (int)std::lround(read_quantity_required(cellNode, "process_node", LengthUnits(), 1e-9, "cell.process_node") / 1e-9);
        cell.area = read_quantity_required(cellNode, "area", FeatureAreaUnits(), 1.0, "cell.area");
        cell.aspectRatio = read_required<double>(cellNode, "aspect_ratio");
        cell.heightInFeatureSize = sqrt(cell.area * cell.aspectRatio);
        cell.widthInFeatureSize = sqrt(cell.area / cell.aspectRatio);

        std::string cellName;
        if (child_optional(cellNode, "name"))
            cellName = read_required<std::string>(cellNode, "name");

        cell.camType = TCAM;
        if (child_optional(cellNode, "cam_type")) {
            cell.camType = read_enum_required<CAMType>(cellNode, "cam_type", false);
        } else {
            std::string probe = to_lower(cellName + " " + inputFile);
            if (probe.find("mcam") != std::string::npos)
                cell.camType = MCAM;
            else if (probe.find("acam") != std::string::npos)
                cell.camType = ACAM;
            else if (probe.find("tcam") != std::string::npos)
                cell.camType = TCAM;
        }

        auto access = child_optional(root, "access_device");
        if (access) {
            cell.accessType = read_enum_required<CellAccessType>(access, "type", false);
            if (child_optional(access, "cmos_width"))
                cell.widthAccessCMOS = read_quantity_required(access, "cmos_width", FeatureUnits(), 1.0, "access_device.cmos_width");
            if (child_optional(access, "voltage_drop"))
                cell.voltageDropAccessDevice = read_quantity_required(access, "voltage_drop", VoltageUnits(), 1.0, "access_device.voltage_drop");
            if (child_optional(access, "leakage_current"))
                cell.leakageCurrentAccessDevice = read_quantity_required(access, "leakage_current", CurrentUnits(), 1.0, "access_device.leakage_current");
        }

        auto res = child_optional(root, "resistance");
        if (res) {
            YAML::Node on = child_optional(res, "on");
            if (!on)
                on = child_optional_bool_key(res, true);
            YAML::Node off = child_optional(res, "off");
            if (!off)
                off = child_optional_bool_key(res, false);
            if (on)
                cell.resistanceOn = parse_quantity_node(on, ResistanceUnits(), 1.0, "resistance.on");
            if (off)
                cell.resistanceOff = parse_quantity_node(off, ResistanceUnits(), 1.0, "resistance.off");

            auto atSet = child_optional(res, "at_set");
            if (atSet) {
                if (child_optional(atSet, "on"))
                    cell.resistanceOnAtSetVoltage = read_quantity_required(atSet, "on", ResistanceUnits(), 1.0, "resistance.at_set.on");
                if (child_optional(atSet, "off"))
                    cell.resistanceOffAtSetVoltage = read_quantity_required(atSet, "off", ResistanceUnits(), 1.0, "resistance.at_set.off");
            }
            auto atReset = child_optional(res, "at_reset");
            if (atReset) {
                if (child_optional(atReset, "on"))
                    cell.resistanceOnAtResetVoltage = read_quantity_required(atReset, "on", ResistanceUnits(), 1.0, "resistance.at_reset.on");
                if (child_optional(atReset, "off"))
                    cell.resistanceOffAtResetVoltage = read_quantity_required(atReset, "off", ResistanceUnits(), 1.0, "resistance.at_reset.off");
            }
            auto atRead = child_optional(res, "at_read");
            if (atRead) {
                if (child_optional(atRead, "on"))
                    cell.resistanceOnAtReadVoltage = read_quantity_required(atRead, "on", ResistanceUnits(), 1.0, "resistance.at_read.on");
                if (child_optional(atRead, "off"))
                    cell.resistanceOffAtReadVoltage = read_quantity_required(atRead, "off", ResistanceUnits(), 1.0, "resistance.at_read.off");
            }
            auto atHalfRead = child_optional(res, "at_half_read");
            if (atHalfRead) {
                if (child_optional(atHalfRead, "on"))
                    cell.resistanceOnAtHalfReadVoltage = read_quantity_required(atHalfRead, "on", ResistanceUnits(), 1.0, "resistance.at_half_read.on");
                if (child_optional(atHalfRead, "off"))
                    cell.resistanceOffAtHalfReadVoltage = read_quantity_required(atHalfRead, "off", ResistanceUnits(), 1.0, "resistance.at_half_read.off");
            }
            auto atHalfReset = child_optional(res, "at_half_reset");
            if (atHalfReset && child_optional(atHalfReset, "on")) {
                cell.resistanceOnAtHalfResetVoltage = read_quantity_required(atHalfReset, "on", ResistanceUnits(), 1.0, "resistance.at_half_reset.on");
            }
        }

        auto cap = child_optional(root, "capacitance");
        if (cap) {
            if (child_optional(cap, "on"))
                cell.capacitanceOn = read_quantity_required(cap, "on", CapacitanceUnits(), 1.0, "capacitance.on");
            if (child_optional(cap, "off"))
                cell.capacitanceOff = read_quantity_required(cap, "off", CapacitanceUnits(), 1.0, "capacitance.off");
        }

        auto device = child_optional(root, "device");
        if (device) {
            cell.gateOxThicknessFactor = read_optional<double>(device, "gate_ox_thickness_factor", cell.gateOxThicknessFactor);
            if (child_optional(device, "soi_width")) {
                cell.widthSOIDevice = read_quantity_required(device, "soi_width", FeatureUnits(), 1.0, "device.soi_width");
            }
        }

        auto read = child_optional(root, "read");
        if (read) {
            const std::string mode = to_lower(read_required<std::string>(read, "mode"));
            if (mode == "voltage")
                cell.readMode = true;
            else if (mode == "current")
                cell.readMode = false;
            else
                throw std::runtime_error("Invalid read.mode: " + mode);

            if (child_optional(read, "voltage"))
                cell.readVoltage = read_quantity_required(read, "voltage", VoltageUnits(), 1.0, "read.voltage");
            if (child_optional(read, "current"))
                cell.readCurrent = read_quantity_required(read, "current", CurrentUnits(), 1.0, "read.current");
            if (child_optional(read, "power"))
                cell.readPower = read_quantity_required(read, "power", PowerUnits(), 1.0, "read.power");
            if (child_optional(read, "energy"))
                cell.readEnergy = read_quantity_required(read, "energy", EnergyUnits(), 1.0, "read.energy");
            if (child_optional(read, "min_sense_voltage"))
                cell.minSenseVoltage = read_quantity_required(read, "min_sense_voltage", VoltageUnits(), 1.0, "read.min_sense_voltage");
            cell.wordlineBoostRatio = read_optional<double>(read, "wordline_boost_ratio", cell.wordlineBoostRatio);
            cell.readFloating = read_optional<bool>(read, "read_floating", cell.readFloating);
        }

        auto write = child_optional(root, "write");
        if (write) {
            auto set = child_optional(write, "set");
            if (set) {
                const std::string mode = to_lower(read_required<std::string>(set, "mode"));
                if (mode == "voltage")
                    cell.setMode = true;
                else if (mode == "current")
                    cell.setMode = false;
                else
                    throw std::runtime_error("Invalid write.set.mode: " + mode);

                if (child_optional(set, "voltage"))
                    cell.setVoltage = read_quantity_required(set, "voltage", VoltageUnits(), 1.0, "write.set.voltage");
                if (child_optional(set, "current"))
                    cell.setCurrent = read_quantity_required(set, "current", CurrentUnits(), 1.0, "write.set.current");
                if (child_optional(set, "pulse"))
                    cell.setPulse = read_quantity_required(set, "pulse", TimeUnits(), 1.0, "write.set.pulse");
                if (child_optional(set, "energy"))
                    cell.setEnergy = read_quantity_required(set, "energy", EnergyUnits(), 1.0, "write.set.energy");
            }

            auto reset = child_optional(write, "reset");
            if (reset) {
                const std::string mode = to_lower(read_required<std::string>(reset, "mode"));
                if (mode == "voltage")
                    cell.resetMode = true;
                else if (mode == "current")
                    cell.resetMode = false;
                else
                    throw std::runtime_error("Invalid write.reset.mode: " + mode);

                if (child_optional(reset, "voltage"))
                    cell.resetVoltage = read_quantity_required(reset, "voltage", VoltageUnits(), 1.0, "write.reset.voltage");
                if (child_optional(reset, "current"))
                    cell.resetCurrent = read_quantity_required(reset, "current", CurrentUnits(), 1.0, "write.reset.current");
                if (child_optional(reset, "pulse"))
                    cell.resetPulse = read_quantity_required(reset, "pulse", TimeUnits(), 1.0, "write.reset.pulse");
                if (child_optional(reset, "energy"))
                    cell.resetEnergy = read_quantity_required(reset, "energy", EnergyUnits(), 1.0, "write.reset.energy");
            }
        }

        auto match = child_optional(root, "match");
        if (match) {
            if (child_optional(match, "cmos_width")) {
                cell.camWidthMatchTran = read_quantity_required(match, "cmos_width", FeatureUnits(), 1.0, "match.cmos_width");
            }
            cell.isNVMdischarge = read_optional<bool>(match, "is_nvm_discharge", cell.isNVMdischarge);
        }

        auto dram = child_optional(root, "dram");
        if (dram && child_optional(dram, "cell_capacitance")) {
            cell.capDRAMCell = read_quantity_required(dram, "cell_capacitance", CapacitanceUnits(), 1.0, "dram.cell_capacitance");
        }

        auto sram = child_optional(root, "sram");
        if (sram) {
            if (child_optional(sram, "nmos_width")) {
                cell.widthSRAMCellNMOS = read_quantity_required(sram, "nmos_width", FeatureUnits(), 1.0, "sram.nmos_width");
            }
            if (child_optional(sram, "pmos_width")) {
                cell.widthSRAMCellPMOS = read_quantity_required(sram, "pmos_width", FeatureUnits(), 1.0, "sram.pmos_width");
            }
        }

        auto flash = child_optional(root, "flash");
        if (flash) {
            if (child_optional(flash, "erase_voltage")) {
                cell.flashEraseVoltage = read_quantity_required(flash, "erase_voltage", VoltageUnits(), 1.0, "flash.erase_voltage");
            }
            if (child_optional(flash, "program_voltage")) {
                cell.flashProgramVoltage = read_quantity_required(flash, "program_voltage", VoltageUnits(), 1.0, "flash.program_voltage");
            }
            if (child_optional(flash, "pass_voltage")) {
                cell.flashPassVoltage = read_quantity_required(flash, "pass_voltage", VoltageUnits(), 1.0, "flash.pass_voltage");
            }
            if (child_optional(flash, "erase_time")) {
                cell.flashEraseTime = read_quantity_required(flash, "erase_time", TimeUnits(), 1.0, "flash.erase_time");
            }
            if (child_optional(flash, "program_time")) {
                cell.flashProgramTime = read_quantity_required(flash, "program_time", TimeUnits(), 1.0, "flash.program_time");
            }
            cell.gateCouplingRatio = read_optional<double>(flash, "gate_coupling_ratio", cell.gateCouplingRatio);
        }

        auto variation = child_optional(root, "variation");
        if (variation) {
            cell.withVariation = read_optional<bool>(variation, "with_variation", cell.withVariation);
            if (child_optional(variation, "resistance_on_variation")) {
                cell.resistanceOnVariation = parse_fraction_or_percent_node(child_optional(variation, "resistance_on_variation"), "variation.resistance_on_variation");
            }
            if (child_optional(variation, "resistance_off_variation")) {
                cell.resistanceOffVariation = parse_fraction_or_percent_node(child_optional(variation, "resistance_off_variation"), "variation.resistance_off_variation");
            }
        }

        auto mcam = child_optional(root, "mcam");
        if (mcam) {
            cell.numResistanceState = read_optional<int>(mcam, "num_resistance_state", cell.numResistanceState);

            auto states = child_optional(mcam, "resistance_state");
            if (states) {
                if (states.IsSequence()) {
                    const int n = std::min<int>(states.size(), 64);
                    for (int i = 0; i < n; i++) {
                        cell.ResistanceState[i] = parse_quantity_node(states[i], ResistanceUnits(), 1.0, "mcam.resistance_state");
                    }
                    if (cell.numResistanceState == 0)
                        cell.numResistanceState = n;
                } else if (states.IsMap()) {
                    for (const auto& it : states) {
                        const int idx = read_scalar_required<int>(it.first, "mcam.resistance_state index");
                        if (idx < 0 || idx >= 64)
                            throw std::runtime_error("mcam.resistance_state index out of range");
                        cell.ResistanceState[idx] = parse_quantity_node(it.second, ResistanceUnits(), 1.0, "mcam.resistance_state");
                    }
                } else {
                    throw std::runtime_error("mcam.resistance_state must be sequence or map");
                }
            }

            auto stateVar = child_optional(mcam, "state_variation");
            if (stateVar) {
                if (stateVar.IsSequence()) {
                    const int n = std::min<int>(stateVar.size(), 64);
                    for (int i = 0; i < n; i++) {
                        cell.resStateVariation[i] = parse_fraction_or_percent_node(stateVar[i], "mcam.state_variation");
                    }
                } else if (stateVar.IsMap()) {
                    for (const auto& it : stateVar) {
                        const int idx = read_scalar_required<int>(it.first, "mcam.state_variation index");
                        if (idx < 0 || idx >= 64)
                            throw std::runtime_error("mcam.state_variation index out of range");
                        cell.resStateVariation[idx] = parse_fraction_or_percent_node(it.second, "mcam.state_variation");
                    }
                } else {
                    throw std::runtime_error("mcam.state_variation must be sequence or map");
                }
            }
        }

        auto ports = child_optional(root, "ports");
        if (ports)
            parse_ports(ports, cell.camPort, cell.camNumRow, cell.camNumCol);
    }

    const std::vector<std::pair<const char*, MemCellType>>& EnumTraits<MemCellType>::mapping() {
        static const std::vector<std::pair<const char*, MemCellType>> k = {
            {"SRAM", SRAM},
            {"DRAM", DRAM},
            {"eDRAM", eDRAM},
            {"MRAM", MRAM},
            {"PCRAM", PCRAM},
            {"ReRAM", memristor},
            {"memristor", memristor},
            {"FBRAM", FBRAM},
            {"SLCNAND", SLCNAND},
            {"MLCNAND", MLCNAND},
            {"FEFETRAM", FEFETRAM},
        };
        return k;
    }

    const std::vector<std::pair<const char*, CellAccessType>>& EnumTraits<CellAccessType>::mapping() {
        static const std::vector<std::pair<const char*, CellAccessType>> k = {
            {"CMOS", CMOS_access},
            {"BJT", BJT_access},
            {"diode", diode_access},
            {"none", none_access},
        };
        return k;
    }

    const std::vector<std::pair<const char*, DeviceRoadmap>>& EnumTraits<DeviceRoadmap>::mapping() {
        static const std::vector<std::pair<const char*, DeviceRoadmap>> k = {
            {"HP", HP},
            {"LSTP", LSTP},
            {"LOP", LOP},
            {"FEFET", FEFET},
            {"LP", LP},
        };
        return k;
    }

    const std::vector<std::pair<const char*, WireType>>& EnumTraits<WireType>::mapping() {
        static const std::vector<std::pair<const char*, WireType>> k = {
            {"LocalAggressive", local_aggressive},
            {"LocalConservative", local_conservative},
            {"SemiAggressive", semi_aggressive},
            {"SemiConservative", semi_conservative},
            {"GlobalAggressive", global_aggressive},
            {"GlobalConservative", global_conservative},
            {"DramWordline", dram_wordline},
        };
        return k;
    }

    const std::vector<std::pair<const char*, WireRepeaterType>>& EnumTraits<WireRepeaterType>::mapping() {
        static const std::vector<std::pair<const char*, WireRepeaterType>> k = {
            {"RepeatedNone", repeated_none},
            {"RepeatedOpt", repeated_opt},
            {"Repeated5%Penalty", repeated_5},
            {"Repeated10%Penalty", repeated_10},
            {"Repeated20%Penalty", repeated_20},
            {"Repeated30%Penalty", repeated_30},
            {"Repeated40%Penalty", repeated_40},
            {"Repeated50%Penalty", repeated_50},
        };
        return k;
    }

    const std::vector<std::pair<const char*, BufferDesignTarget>>& EnumTraits<BufferDesignTarget>::mapping() {
        static const std::vector<std::pair<const char*, BufferDesignTarget>> k = {
            {"latency", latency_first},
            {"balance", latency_area_trade_off},
            {"area", area_first},
        };
        return k;
    }

    const std::vector<std::pair<const char*, MemoryType>>& EnumTraits<MemoryType>::mapping() {
        static const std::vector<std::pair<const char*, MemoryType>> k = {
            {"mem_data", mem_data},
            {"tag", tag},
            {"CAM", CAM},
        };
        return k;
    }

    const std::vector<std::pair<const char*, CAMType>>& EnumTraits<CAMType>::mapping() {
        static const std::vector<std::pair<const char*, CAMType>> k = {
            {"TCAM", TCAM},
            {"MCAM", MCAM},
            {"ACAM", ACAM},
        };
        return k;
    }

    const std::vector<std::pair<const char*, SearchFunction>>& EnumTraits<SearchFunction>::mapping() {
        static const std::vector<std::pair<const char*, SearchFunction>> k = {
            {"EX", EX},
            {"BE", BE},
            {"TH", TH},
        };
        return k;
    }

    const std::vector<std::pair<const char*, RoutingMode>>& EnumTraits<RoutingMode>::mapping() {
        static const std::vector<std::pair<const char*, RoutingMode>> k = {
            {"H-tree", h_tree},
            {"NonH-tree", non_h_tree},
            {"non_h_tree", non_h_tree},
        };
        return k;
    }

    const std::vector<std::pair<const char*, WriteScheme>>& EnumTraits<WriteScheme>::mapping() {
        static const std::vector<std::pair<const char*, WriteScheme>> k = {
            {"SetBeforeReset", set_before_reset},
            {"set_before_reset", set_before_reset},
            {"ResetBeforeSet", reset_before_set},
            {"reset_before_set", reset_before_set},
            {"EraseBeforeSet", erase_before_set},
            {"erase_before_set", erase_before_set},
            {"EraseBeforeReset", erase_before_reset},
            {"erase_before_reset", erase_before_reset},
            {"WriteAndVerify", write_and_verify},
            {"write_and_verify", write_and_verify},
            {"Normal", normal_write},
            {"normal", normal_write},
            {"normal_write", normal_write},
        };
        return k;
    }

    const std::vector<std::pair<const char*, DesignTarget>>& EnumTraits<DesignTarget>::mapping() {
        static const std::vector<std::pair<const char*, DesignTarget>> k = {
            {"cache", cache},
            {"RAM", RAM_chip},
            {"CAM", CAM_chip},
        };
        return k;
    }

    const std::vector<std::pair<const char*, OptimizationTarget>>& EnumTraits<OptimizationTarget>::mapping() {
        static const std::vector<std::pair<const char*, OptimizationTarget>> k = {
            {"ReadLatency", read_latency_optimized},
            {"WriteLatency", write_latency_optimized},
            {"ReadDynamicEnergy", read_energy_optimized},
            {"WriteDynamicEnergy", write_energy_optimized},
            {"ReadEDP", read_edp_optimized},
            {"WriteEDP", write_edp_optimized},
            {"LeakagePower", leakage_optimized},
            {"Area", area_optimized},
            {"SearchLatency", search_latency_optimized},
            {"SearchEnergy", search_energy_optimized},
            {"SearchEDP", search_edp_optimized},
            {"Exploration", full_exploration},
        };
        return k;
    }

    const std::vector<std::pair<const char*, CacheAccessMode>>& EnumTraits<CacheAccessMode>::mapping() {
        static const std::vector<std::pair<const char*, CacheAccessMode>> k = {
            {"normal", normal_access_mode},
            {"sequential", sequential_access_mode},
            {"fast", fast_access_mode},
        };
        return k;
    }

    const std::vector<std::pair<const char*, TypeOfInputEncoder>>& EnumTraits<TypeOfInputEncoder>::mapping() {
        static const std::vector<std::pair<const char*, TypeOfInputEncoder>> k = {
            {"encoding_two_bit", encoding_two_bit},
        };
        return k;
    }

    const std::vector<std::pair<const char*, TypeOfSenseAmp>>& EnumTraits<TypeOfSenseAmp>::mapping() {
        static const std::vector<std::pair<const char*, TypeOfSenseAmp>> k = {
            {"nvsim_vol", nvsim_voltage_sense},
            {"nvsim_cur", nvsim_current_sense},
            {"self_clock", self_clock_sense},
            {"dual_the", dual_threshold_sense},
            {"discharge", discharge},
        };
        return k;
    }

    const std::vector<std::pair<const char*, CAM_PortType>>& EnumTraits<CAM_PortType>::mapping() {
        static const std::vector<std::pair<const char*, CAM_PortType>> k = {
            {"Wordline", Wordline},
            {"Searchline", Searchline},
            {"Bitline", Bitline},
            {"Dataline", Dataline},
            {"Sourceline", Sourceline},
            {"Matchline", Matchline},
            {"Matchline_Bitline", Matchline_Bitline},
            {"Searchline_Bitline", Searchline_Bitline},
        };
        return k;
    }

    const std::vector<std::pair<const char*, CAM_CmosRegion>>& EnumTraits<CAM_CmosRegion>::mapping() {
        static const std::vector<std::pair<const char*, CAM_CmosRegion>> k = {
            {"gate", gate},
            {"source", source},
            {"drain", drain},
            {"diode", diode},
            {"none", none},
        };
        return k;
    }

} // namespace YamlHelpers
