#ifndef CONFIG_VARIATIONCONFIG_H_
#define CONFIG_VARIATIONCONFIG_H_

#include <stdint.h>
#include <string>

struct VariationConfig {
    bool enabled = false;
    uint32_t seed = 1;
    std::string mode = "nominal";
    int samples = 1;
    std::string distribution = "lognormal";
    double mlWireResSigma = 0.0;
    double cellResOnSigma = 0.0;
    double cellResOffSigma = 0.0;
    double deviceAccessResSigma = 0.0;
    double deviceMatchResSigma = 0.0;
};

#endif /* CONFIG_VARIATIONCONFIG_H_ */
