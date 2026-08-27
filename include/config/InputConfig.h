#ifndef CONFIG_INPUTCONFIG_H_
#define CONFIG_INPUTCONFIG_H_

#include <stdint.h>
#include <string>

#include "constant.h"
#include "typedef.h"

struct InputConfig {
    DesignTarget designTarget = CAM_chip;
    SearchFunction searchFunction = EX;
    OptimizationTarget optimizationTarget = read_latency_optimized;
    int processNode = 90;
    int64_t capacity = 0;
    long wordWidth = 0;
    long vectorDimensions = 0;
    DeviceRoadmap deviceRoadmap = HP;
    std::string fileMemCell;
    std::string fileTechnology;
    bool hasCamWidthMatchTran = false;
    double camWidthMatchTran = 0;
    int temperature = 300;
    double maxDriverCurrent = 0;
    WriteScheme writeScheme = normal_write;
    long pageSize = 0;
    long flashBlockSize = 0;
    RoutingMode routingMode = h_tree;
    bool internalSensing = true;
    double maxNmosSize = MAX_NMOS_SIZE;
    std::string outputYamlFileName;
    std::string outputFilePrefix = "results/output";
};

#endif /* CONFIG_INPUTCONFIG_H_ */
