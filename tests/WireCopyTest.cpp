#include "EvaCamConfig.h"
#include "Result.h"
#include "Wire.h"

#include <cassert>
#include <iostream>
#include <memory>

namespace {

void AssertSameWireState(const Wire &actual, const Wire &expected) {
    assert(actual.initialized == expected.initialized);
    assert(actual.featureSizeInNano == expected.featureSizeInNano);
    assert(actual.featureSize == expected.featureSize);
    assert(actual.wireType == expected.wireType);
    assert(actual.wireRepeaterType == expected.wireRepeaterType);
    assert(actual.temperature == expected.temperature);
    assert(actual.isLowSwing == expected.isLowSwing);
    assert(actual.barrierThickness == expected.barrierThickness);
    assert(actual.horizontalDielectric == expected.horizontalDielectric);
    assert(actual.wirePitch == expected.wirePitch);
    assert(actual.aspectRatio == expected.aspectRatio);
    assert(actual.ildThickness == expected.ildThickness);
    assert(actual.wireWidth == expected.wireWidth);
    assert(actual.wireThickness == expected.wireThickness);
    assert(actual.wireSpacing == expected.wireSpacing);
    assert(actual.repeaterSize == expected.repeaterSize);
    assert(actual.repeaterSpacing == expected.repeaterSpacing);
    assert(actual.repeaterHeight == expected.repeaterHeight);
    assert(actual.repeaterWidth == expected.repeaterWidth);
    assert(actual.repeatedWirePitch == expected.repeatedWirePitch);
    assert(actual.resWirePerUnit == expected.resWirePerUnit);
    assert(actual.capWirePerUnit == expected.capWirePerUnit);
    assert(actual.copper_resistivity == expected.copper_resistivity);
    assert(actual.config == expected.config);
}

void TestCopyConstructInitializedWire() {
    auto config = std::make_shared<EvaCamConfig>();
    Wire original;
    original.Initialize(32, local_aggressive, repeated_none, 350, false, config);

    Wire copy(original);

    AssertSameWireState(copy, original);
    assert(copy.config == config);

    original.Initialize(65, semi_conservative, repeated_none, 300, true, config);
    assert(copy.featureSizeInNano == 32);
    assert(copy.wireType == local_aggressive);
    assert(copy.temperature == 350);
    assert(!copy.isLowSwing);
}

void TestAssignInitializedWire() {
    auto config = std::make_shared<EvaCamConfig>();
    Wire original;
    original.Initialize(45, global_conservative, repeated_none, 325, false, config);

    Wire assigned;
    assigned = original;

    AssertSameWireState(assigned, original);
    assert(assigned.config == config);
}

void TestResultOwnsWireSnapshots() {
    auto config = std::make_shared<EvaCamConfig>();
    Result best;
    best.Initialize(config);
    best.optimizationTarget = read_latency_optimized;

    auto candidate = std::make_shared<Result>();
    candidate->Initialize(config);
    candidate->bank->readLatency = 1.0;
    candidate->bank->writeLatency = 1.0;
    candidate->bank->readDynamicEnergy = 1.0;
    candidate->bank->writeDynamicEnergy = 1.0;
    candidate->bank->area = 1.0;
    candidate->bank->leakage = 1.0;
    candidate->localWire.Initialize(32, local_aggressive, repeated_none, 350, false, config);
    candidate->globalWire.Initialize(32, global_conservative, repeated_none, 350, false, config);

    best.compareAndUpdate(candidate);

    candidate->localWire.Initialize(65, semi_conservative, repeated_none, 300, true, config);
    candidate->globalWire.Initialize(65, semi_aggressive, repeated_none, 300, true, config);

    assert(best.localWire.featureSizeInNano == 32);
    assert(best.localWire.wireType == local_aggressive);
    assert(best.localWire.temperature == 350);
    assert(!best.localWire.isLowSwing);
    assert(best.globalWire.featureSizeInNano == 32);
    assert(best.globalWire.wireType == global_conservative);
    assert(best.globalWire.temperature == 350);
    assert(!best.globalWire.isLowSwing);
}

}  // namespace

int main() {
    TestCopyConstructInitializedWire();
    TestAssignInitializedWire();
    TestResultOwnsWireSnapshots();

    std::cout << "Wire copy tests passed" << std::endl;
    return 0;
}
