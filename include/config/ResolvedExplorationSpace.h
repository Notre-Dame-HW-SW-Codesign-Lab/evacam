#ifndef EVACAM_CONFIG_RESOLVEDEXPLORATIONSPACE_H_
#define EVACAM_CONFIG_RESOLVEDEXPLORATIONSPACE_H_

#include <vector>

struct ResolvedGeometryExplorationSpace {
    std::vector<int> numRowMatValues;
    std::vector<int> numColumnMatValues;
    std::vector<int> numRowSubarrayValues;
    std::vector<int> numColumnSubarrayValues;
    std::vector<int> muxSenseAmpValues;
    std::vector<int> muxOutputLev1Values;
    std::vector<int> muxOutputLev2Values;
};

struct ResolvedWireExplorationSpace {
    std::vector<int> localWireTypeValues;
    std::vector<int> globalWireTypeValues;
    std::vector<int> localWireRepeaterTypeValues;
    std::vector<int> globalWireRepeaterTypeValues;
    std::vector<int> isLocalWireLowSwingValues;
    std::vector<int> isGlobalWireLowSwingValues;
};

struct ResolvedCamOptimizationExplorationSpace {
    std::vector<int> areaOptimizationLevelValues;
    std::vector<int> rowDriverOptLevelValues;
    std::vector<int> priorityOptLevelValues;
    std::vector<int> bitSerialWidthValues;
};

struct ResolvedExplorationSpace {
    ResolvedGeometryExplorationSpace geometry;
    ResolvedWireExplorationSpace wires;
    ResolvedCamOptimizationExplorationSpace cam;
};

#endif  // EVACAM_CONFIG_RESOLVEDEXPLORATIONSPACE_H_
