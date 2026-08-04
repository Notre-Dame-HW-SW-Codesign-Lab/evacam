#include "config/DerivedValueHelpers.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

namespace {

InputConfig MakeInputConfig(int64_t capacity, long wordWidth, OptimizationTarget target) {
    InputConfig input;
    input.capacity = capacity;
    input.wordWidth = wordWidth;
    input.optimizationTarget = target;
    return input;
}

ResolvedExplorationSpace MakeFixedOuterGeometry() {
    ResolvedExplorationSpace resolved;
    resolved.geometry.numRowMatValues = {2};
    resolved.geometry.numColumnMatValues = {4};
    resolved.geometry.numRowSubarrayValues = {8};
    resolved.geometry.numColumnSubarrayValues = {16};
    return resolved;
}

void TestEffectiveCapacityBitsConvertsBytesAndPreservesLargeValues() {
    const InputConfig smallInput = MakeInputConfig(1024, 64, read_latency_optimized);
    assert(DerivedValueHelpers::EffectiveCapacityBits(smallInput) == 8192);

    const InputConfig largeInput = MakeInputConfig(
            static_cast<int64_t>(std::numeric_limits<int>::max()) + 1,
            64,
            read_latency_optimized);
    assert(DerivedValueHelpers::EffectiveCapacityBits(largeInput)
            == (static_cast<long long>(std::numeric_limits<int>::max()) + 1) * 8);
}

void TestEffectiveBlockSizeBitsUsesIntegerDivisionAndZeroCapacity() {
    const InputConfig exactInput = MakeInputConfig(1024, 64, read_latency_optimized);
    assert(DerivedValueHelpers::EffectiveBlockSizeBits(exactInput) == 128);

    const InputConfig truncatedInput = MakeInputConfig(1025, 64, read_latency_optimized);
    assert(DerivedValueHelpers::EffectiveBlockSizeBits(truncatedInput) == 128);

    const InputConfig emptyInput = MakeInputConfig(0, 64, read_latency_optimized);
    assert(DerivedValueHelpers::EffectiveBlockSizeBits(emptyInput) == 0);
}

void TestIsFullExplorationRecognizesDseTargetOnly() {
    const InputConfig explorationInput = MakeInputConfig(1024, 64, full_exploration);
    assert(DerivedValueHelpers::IsFullExploration(explorationInput));

    const InputConfig optimizationInput = MakeInputConfig(1024, 64, area_optimized);
    assert(!DerivedValueHelpers::IsFullExploration(optimizationInput));
}

void TestShouldWriteExplorationCsvMatchesDseTarget() {
    const InputConfig explorationInput = MakeInputConfig(1024, 64, full_exploration);
    assert(DerivedValueHelpers::ShouldWriteExplorationCsv(explorationInput));

    const InputConfig optimizationInput = MakeInputConfig(1024, 64, search_energy_optimized);
    assert(!DerivedValueHelpers::ShouldWriteExplorationCsv(optimizationInput));
}

void TestHasFixedOuterGeometryRequiresExactlyOneValueForEveryOuterDimension() {
    const ResolvedExplorationSpace fixed = MakeFixedOuterGeometry();
    assert(DerivedValueHelpers::HasFixedOuterGeometry(fixed));

    ResolvedExplorationSpace multipleRowMats = fixed;
    multipleRowMats.geometry.numRowMatValues = {1, 2};
    assert(!DerivedValueHelpers::HasFixedOuterGeometry(multipleRowMats));

    ResolvedExplorationSpace multipleColumnMats = fixed;
    multipleColumnMats.geometry.numColumnMatValues = {1, 2};
    assert(!DerivedValueHelpers::HasFixedOuterGeometry(multipleColumnMats));

    ResolvedExplorationSpace multipleRowSubarrays = fixed;
    multipleRowSubarrays.geometry.numRowSubarrayValues = {1, 2};
    assert(!DerivedValueHelpers::HasFixedOuterGeometry(multipleRowSubarrays));

    ResolvedExplorationSpace multipleColumnSubarrays = fixed;
    multipleColumnSubarrays.geometry.numColumnSubarrayValues = {1, 2};
    assert(!DerivedValueHelpers::HasFixedOuterGeometry(multipleColumnSubarrays));

    ResolvedExplorationSpace missingRowMat = fixed;
    missingRowMat.geometry.numRowMatValues.clear();
    assert(!DerivedValueHelpers::HasFixedOuterGeometry(missingRowMat));
}

}  // namespace

int main() {
    TestEffectiveCapacityBitsConvertsBytesAndPreservesLargeValues();
    TestEffectiveBlockSizeBitsUsesIntegerDivisionAndZeroCapacity();
    TestIsFullExplorationRecognizesDseTargetOnly();
    TestShouldWriteExplorationCsvMatchesDseTarget();
    TestHasFixedOuterGeometryRequiresExactlyOneValueForEveryOuterDimension();

    std::cout << "Derived value helper tests passed" << std::endl;
    return 0;
}
