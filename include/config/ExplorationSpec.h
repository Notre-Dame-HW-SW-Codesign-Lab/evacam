#ifndef EVACAM_CONFIG_EXPLORATIONSPEC_H_
#define EVACAM_CONFIG_EXPLORATIONSPEC_H_

#include <vector>

#include "IntValueDomain.h"
#include "typedef.h"

struct GeometryExplorationSpec {
    IntValueDomain numRowMat = IntValueDomain::PowersOfTwo(1, 16);
    IntValueDomain numColumnMat = IntValueDomain::PowersOfTwo(1, 16);
    IntValueDomain numActiveMatPerRow = IntValueDomain::PowersOfTwo(1, 16);
    IntValueDomain numActiveMatPerColumn = IntValueDomain::PowersOfTwo(1, 16);
    IntValueDomain numRowSubarray = IntValueDomain::PowersOfTwo(1, 16);
    IntValueDomain numColumnSubarray = IntValueDomain::PowersOfTwo(1, 16);
    IntValueDomain numActiveSubarrayPerRow = IntValueDomain::PowersOfTwo(1, 16);
    IntValueDomain numActiveSubarrayPerColumn = IntValueDomain::PowersOfTwo(1, 16);
    IntValueDomain numRow = IntValueDomain::PowersOfTwo(16, 512);
    IntValueDomain numColumn = IntValueDomain::PowersOfTwo(16, 512);
    IntValueDomain muxSenseAmp = IntValueDomain::PowersOfTwo(1, 32);
    IntValueDomain muxOutputLev1 = IntValueDomain::PowersOfTwo(1, 32);
    IntValueDomain muxOutputLev2 = IntValueDomain::PowersOfTwo(1, 32);
    IntValueDomain numRowPerSet = IntValueDomain::PowersOfTwo(1, 32);
};

struct WireExplorationSpec {
    IntValueDomain localWireType = IntValueDomain::Sequential(local_aggressive, local_conservative);
    IntValueDomain globalWireType = IntValueDomain::Sequential(global_aggressive, global_conservative);
    IntValueDomain localWireRepeaterType = IntValueDomain::Sequential(repeated_none, repeated_50);
    IntValueDomain globalWireRepeaterType = IntValueDomain::Sequential(repeated_none, repeated_50);
    IntValueDomain isLocalWireLowSwing = IntValueDomain::Sequential(false, true);
    IntValueDomain isGlobalWireLowSwing = IntValueDomain::Sequential(false, true);
};

struct CamOptimizationExplorationSpec {
    IntValueDomain areaOptimizationLevel = IntValueDomain::Sequential(latency_first, area_first);
    IntValueDomain rowDriverOptLevel = IntValueDomain::Sequential(latency_first, area_first);
    IntValueDomain priorityOptLevel = IntValueDomain::Sequential(latency_first, area_first);
    IntValueDomain bitSerialWidth = IntValueDomain::PowersOfTwo(8, 1024);
};

struct ExplorationSpec {
    GeometryExplorationSpec geometry;
    WireExplorationSpec wires;
    CamOptimizationExplorationSpec cam;
    bool useCactiAssumption = false;
    bool deepExploration = false;

    static ExplorationSpec Default();
    void ApplyDeepExplorationDefaults(int associativity);

    std::vector<int> ActiveMatPerRowValues(int numColumnMat) const;
    std::vector<int> ActiveMatPerColumnValues(int numRowMat) const;
    std::vector<int> ActiveSubarrayPerRowValues(int numColumnSubarray) const;
    std::vector<int> ActiveSubarrayPerColumnValues(int numRowSubarray) const;

    bool IsValidPartitioning(long blockSizeBits,
            int numActiveMatPerRow,
            int numActiveMatPerColumn,
            int numActiveSubarrayPerRow,
            int numActiveSubarrayPerColumn) const;
};

#endif  // EVACAM_CONFIG_EXPLORATIONSPEC_H_
