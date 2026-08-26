#include "config/DerivedValueHelpers.h"

long long DerivedValueHelpers::EffectiveCapacityBits(const InputConfig &input) {
    return static_cast<long long>(input.capacity) * 8;
}

long DerivedValueHelpers::EffectiveBlockSizeBits(const InputConfig &input) {
    return input.wordWidth;
}

bool DerivedValueHelpers::IsFullExploration(const InputConfig &input) {
    return input.optimizationTarget == full_exploration;
}

bool DerivedValueHelpers::ShouldWriteExplorationCsv(const InputConfig &input) {
    return IsFullExploration(input);
}

bool DerivedValueHelpers::HasFixedOuterGeometry(const ResolvedExplorationSpace &resolvedExploration) {
    return resolvedExploration.geometry.numRowMatValues.size() == 1
        && resolvedExploration.geometry.numColumnMatValues.size() == 1
        && resolvedExploration.geometry.numRowSubarrayValues.size() == 1
        && resolvedExploration.geometry.numColumnSubarrayValues.size() == 1;
}
