#ifndef EVACAM_CONFIG_RESOLVEDWORDGEOMETRY_H_
#define EVACAM_CONFIG_RESOLVEDWORDGEOMETRY_H_

#include <stdint.h>

struct ResolvedWordGeometry {
    int64_t logicalCapacityBits = 0;
    // Capacity actually allocated as complete logical words. This differs
    // from logicalCapacityBits only when memory.physical_capacity is supplied.
    int64_t allocatedCapacityBits = 0;
    long logicalWordBits = 0;
    int64_t entryCount = 0;
    int bitsPerCell = 1;
    long physicalColumnsPerWord = 0;
    int paddingBits = 0;
};

#endif  // EVACAM_CONFIG_RESOLVEDWORDGEOMETRY_H_
