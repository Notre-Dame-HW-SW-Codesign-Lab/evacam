#ifndef EVACAM_MCAM_ANALYSIS_H_
#define EVACAM_MCAM_ANALYSIS_H_

#include <cstdint>
#include <vector>

#include "EvaCAMMatchResult.h"

struct EvaCAMDistanceVoltageBounds {
    double squaredEuclideanDistance = 0;
    double minimumConductance = 0;
    double maximumConductance = 0;
    double minimumVoltage = 0;
    double maximumVoltage = 0;
    double minimumSearchLatency = 0;
    double maximumSearchLatency = 0;
    std::vector<int> minimumConductanceDeltaCounts;
    std::vector<int> maximumConductanceDeltaCounts;
    std::vector<int> minimumConductanceStored;
    std::vector<int> minimumConductanceQuery;
    std::vector<int> maximumConductanceStored;
    std::vector<int> maximumConductanceQuery;
    double sensingTime = 0;
};

struct EvaCAMMcamCompositionResult {
    std::vector<int> deltaCounts;
    std::uint64_t coordinatePermutations = 0;
    EvaCAMMatchResult result;
};

#endif
