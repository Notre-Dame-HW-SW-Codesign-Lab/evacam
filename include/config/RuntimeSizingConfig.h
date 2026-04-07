#ifndef EVACAM_CONFIG_RUNTIMESIZINGCONFIG_H_
#define EVACAM_CONFIG_RUNTIMESIZINGCONFIG_H_

#include <stdint.h>

struct RuntimeSizingConfig {
    int64_t realCapacity = 0;
    bool hasExplicitCapacity = false;
    bool capacityIsAuto = false;
    bool hasFixedSubarrayDimensions = false;
    int fixedSubarrayRows = 0;
    int fixedSubarrayColumns = 0;
};

#endif  // EVACAM_CONFIG_RUNTIMESIZINGCONFIG_H_
