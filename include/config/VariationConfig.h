#ifndef CONFIG_VARIATIONCONFIG_H_
#define CONFIG_VARIATIONCONFIG_H_

#include <stdint.h>
#include <string>

struct VariationConfig {
    bool enabled = false;
    uint32_t seed = 0;
    std::string mode = "nominal";
    std::string lutFile;
    int samples = 1;
    bool hasUserSeed = false;
    bool hasUserSamples = false;
    double mlWireResStdev = 0.0;
    double memoryDeviceResOnStdev = 0.0;
    double memoryDeviceResOffStdev = 0.0;
    double deviceAccessResStdev = 0.0;
    double deviceMatchResStdev = 0.0;
    double mlWireResMaxVar = 0.0;
    double memoryDeviceResOnMaxVar = 0.0;
    double memoryDeviceResOffMaxVar = 0.0;
    double deviceAccessResMaxVar = 0.0;
    double deviceMatchResMaxVar = 0.0;
};

#endif /* CONFIG_VARIATIONCONFIG_H_ */
