#ifndef TECHNOLOGY_NAND3DMEMORYDEVICE_H_
#define TECHNOLOGY_NAND3DMEMORYDEVICE_H_

#include <string>

#include "technology/NandDeviceSpec.h"

// NAND3D parameters are supplied by the ordinary memory_device YAML. Lengths
// and resistance use SI units. Storage layers include the reserved validity
// pair and padding. Electrical estimates use the analytical first-moment model.
struct Nand3dMemoryDevice {
    bool configured = false;
    std::string storageMode;
    NandDeviceSpec electrical;
    int storageLayers = 0;
    int dummyLayers = 0;
    int stringRows = 0;
    int stringColumns = 0;
    double holePitchX = 0;
    double holePitchY = 0;
    double layerPitch = 0;
    double staircaseStepWidth = 0;
    double staircaseContactLength = 0;
    double isolationWidth = 0;
    std::string peripheralPlacement;
};

#endif  // TECHNOLOGY_NAND3DMEMORYDEVICE_H_
