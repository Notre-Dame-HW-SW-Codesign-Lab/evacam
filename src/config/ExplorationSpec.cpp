#include "config/ExplorationSpec.h"

#include <algorithm>

namespace {

std::vector<int> BuildPow2ClampedValues(int minValue, int maxValue, int upperBound) {
    const int clampedMin = std::min(minValue, upperBound);
    const int clampedMax = std::min(maxValue, upperBound);
    if (clampedMin > clampedMax) {
        return {};
    }

    return IntValueDomain::PowersOfTwo(clampedMin, clampedMax).Values();
}

}  // namespace

ExplorationSpec ExplorationSpec::Default() {
    return ExplorationSpec();
}

void ExplorationSpec::ApplyDeepExplorationDefaults() {
    deepExploration = true;

    geometry.numRowMat = IntValueDomain::PowersOfTwo(1, 64);
    geometry.numColumnMat = IntValueDomain::PowersOfTwo(1, 64);
    geometry.numRowSubarray = IntValueDomain::PowersOfTwo(1, 16);
    geometry.numColumnSubarray = IntValueDomain::PowersOfTwo(1, 16);
    geometry.muxSenseAmp = IntValueDomain::PowersOfTwo(1, 64);
    geometry.muxOutputLev1 = IntValueDomain::PowersOfTwo(1, 64);
    geometry.muxOutputLev2 = IntValueDomain::PowersOfTwo(1, 64);

    wires.localWireType = IntValueDomain::Sequential(local_aggressive, local_conservative);
    wires.globalWireType = IntValueDomain::Sequential(global_aggressive, global_conservative);
    wires.localWireRepeaterType = IntValueDomain::Sequential(repeated_none, repeated_opt);
    wires.globalWireRepeaterType = IntValueDomain::Sequential(repeated_none, repeated_opt);
}

std::vector<int> ExplorationSpec::ActiveMatPerRowValues(int numColumnMat) const {
    return BuildPow2ClampedValues(geometry.numActiveMatPerRow.Min(),
            geometry.numActiveMatPerRow.Max(), numColumnMat);
}

std::vector<int> ExplorationSpec::ActiveMatPerColumnValues(int numRowMat) const {
    return BuildPow2ClampedValues(geometry.numActiveMatPerColumn.Min(),
            geometry.numActiveMatPerColumn.Max(), numRowMat);
}

std::vector<int> ExplorationSpec::ActiveSubarrayPerRowValues(int numColumnSubarray) const {
    return BuildPow2ClampedValues(geometry.numActiveSubarrayPerRow.Min(),
            geometry.numActiveSubarrayPerRow.Max(), numColumnSubarray);
}

std::vector<int> ExplorationSpec::ActiveSubarrayPerColumnValues(int numRowSubarray) const {
    return BuildPow2ClampedValues(geometry.numActiveSubarrayPerColumn.Min(),
            geometry.numActiveSubarrayPerColumn.Max(), numRowSubarray);
}

bool ExplorationSpec::IsValidPartitioning(long blockSizeBits, int numActiveMatPerRow,
        int numActiveMatPerColumn, int numActiveSubarrayPerRow,
        int numActiveSubarrayPerColumn) const {
    return blockSizeBits / (numActiveMatPerRow * numActiveMatPerColumn
            * numActiveSubarrayPerRow * numActiveSubarrayPerColumn) != 0;
}
