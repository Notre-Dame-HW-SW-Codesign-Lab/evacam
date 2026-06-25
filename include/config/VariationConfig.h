#ifndef CONFIG_VARIATIONCONFIG_H_
#define CONFIG_VARIATIONCONFIG_H_

#include <stdint.h>
#include <string>

struct VariationConfig {
    bool enabled = false;
    uint32_t seed = 0;
    std::string mode = "nominal";
    std::string monteCarloGranularity = "cell";
    std::string lutFile;
    int samples = 1;
    bool hasUserSeed = false;
    bool hasUserSamples = false;
    double memoryDeviceResOnStdev = 0.0;
    double memoryDeviceResOffStdev = 0.0;
    double memoryDeviceResOnMaxVar = 0.0;
    double memoryDeviceResOffMaxVar = 0.0;
};

#endif /* CONFIG_VARIATIONCONFIG_H_ */
