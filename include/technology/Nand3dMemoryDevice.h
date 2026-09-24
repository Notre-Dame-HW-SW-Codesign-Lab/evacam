#ifndef TECHNOLOGY_NAND3DMEMORYDEVICE_H_
#define TECHNOLOGY_NAND3DMEMORYDEVICE_H_

#include <string>

#include "technology/NandDeviceSpec.h"

// NAND3D parameters are supplied by the ordinary memory_device YAML. Lengths,
// resistance, and integration step use SI units; the solver tolerance is a
// voltage. Storage layers include the reserved validity pair and padding.
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
    double prechargeDriverResistance = 0;
    double solverMaxStep = 0;
    double solverTolerance = 0;
    int solverMaxSteps = 0;
};

#endif  // TECHNOLOGY_NAND3DMEMORYDEVICE_H_
