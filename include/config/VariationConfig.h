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
    double mlWireResStdev = 0.0;
    double memoryDeviceResOnStdev = 0.0;
    double memoryDeviceResOffStdev = 0.0;
    double deviceAccessResStdev = 0.0;
    double deviceMatchResStdev = 0.0;
};

#endif /* CONFIG_VARIATIONCONFIG_H_ */
