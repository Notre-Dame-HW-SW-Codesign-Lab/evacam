#ifndef INPUT_ACCESSDEVICEYAMLLOADER_H_
#define INPUT_ACCESSDEVICEYAMLLOADER_H_

#include <string>

#include "typedef.h"

class MemCell;

namespace YamlHelpers {

struct AccessDeviceSpec {
    CellAccessType type = none_access;
    double width = 0;
    CAM_CmosRegion connectedTerminal = none;
    bool isNmos = true;
    double voltageDrop = 0;
    double leakageCurrent = 0;
};

AccessDeviceSpec ReadAccessDeviceSpecFromYaml(const std::string& inputFile);
void ReadAccessDeviceFromYaml(MemCell& cell, const std::string& inputFile);

}  // namespace YamlHelpers

#endif  // INPUT_ACCESSDEVICEYAMLLOADER_H_
