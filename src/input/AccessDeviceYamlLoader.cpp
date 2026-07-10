#include "input/AccessDeviceYamlLoader.h"

#include "MemCell.h"
#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

namespace YamlHelpers {

AccessDeviceSpec ReadAccessDeviceSpecFromYaml(const std::string& inputFile) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    require_schema(root, "access_device", "access device config");
    YAML::Node access = root;

    AccessDeviceSpec spec;
    spec.type = read_enum_required<CellAccessType>(access, "type", false);
    if (child_optional(access, "width")) {
        spec.width = read_quantity_required(access, "width", FeatureUnits(), 1.0,
                "access_device.width");
    }
    if (child_optional(access, "connected_terminal")) {
        spec.connectedTerminal = read_enum_required<CAM_CmosRegion>(
                access, "connected_terminal", false);
    }
    if (child_optional(access, "is_nmos")) {
        spec.isNmos = read_required<bool>(access, "is_nmos");
    }
    if (child_optional(access, "voltage_drop")) {
        spec.voltageDrop = read_quantity_required(access, "voltage_drop", VoltageUnits(),
                1.0, "access_device.voltage_drop");
    }
    if (child_optional(access, "leakage_current")) {
        spec.leakageCurrent = read_quantity_required(access, "leakage_current",
                CurrentUnits(), 1.0, "access_device.leakage_current");
    }
    return spec;
}

void ReadAccessDeviceFromYaml(MemCell& cell, const std::string& inputFile) {
    const AccessDeviceSpec spec = ReadAccessDeviceSpecFromYaml(inputFile);
    cell.accessType = spec.type;
    cell.widthAccessCMOS = spec.width;
    cell.voltageDropAccessDevice = spec.voltageDrop;
    cell.leakageCurrentAccessDevice = spec.leakageCurrent;
}
}  // namespace YamlHelpers
