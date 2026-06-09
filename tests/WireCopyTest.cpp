#include "EvaCamConfig.h"
#include "formula.h"
#include "Result.h"
#include "Technology.h"
#include "Wire.h"
#include "WireProcessTable.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

namespace {

bool Near(double actual, double expected, double tolerance = 1e-12) {
    const double scale = std::max(1.0, std::fabs(expected));
    return std::fabs(actual - expected) <= tolerance * scale;
}

std::shared_ptr<EvaCamConfig> MakeConfigWithTechnology(int processNode = 90) {
    auto config = std::make_shared<EvaCamConfig>();
    config->technology.tech = std::make_shared<Technology>();
    config->technology.tech->Initialize(
            processNode, config->input.deviceRoadmap, config->peripherals.useUpdatedLib);
    return config;
}

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

void TestWireProcessSpecLookup() {
    WireProcessSpec spec;

    assert(FindWireProcessSpec(21, local_aggressive, &spec));
    assert(Near(spec.featureSize, 22e-9));
    assert(Near(spec.barrierThickness, 0.00e-6));
    assert(Near(spec.horizontalDielectric, 2.55));
    assert(Near(spec.wirePitch, 2 * 22e-9));
    assert(Near(spec.aspectRatio, 1.9));
    assert(Near(spec.ildThickness, 1.9 * 22e-9));
    assert(Near(spec.copperResistivity, 6.0e-8));

    assert(FindWireProcessSpec(32, global_conservative, &spec));
    assert(Near(spec.featureSize, 32e-9));
    assert(Near(spec.barrierThickness, 0.0078e-6));
    assert(Near(spec.horizontalDielectric, 3.16));
    assert(Near(spec.wirePitch, 8 * 32e-9));
    assert(Near(spec.aspectRatio, 2.34));
    assert(Near(spec.ildThickness, 0.385e-6));
    assert(Near(spec.copperResistivity, 2.5e-8));

    assert(FindWireProcessSpec(120, semi_conservative, &spec));
    assert(Near(spec.featureSize, 120e-9));
    assert(Near(spec.barrierThickness, 0.01e-6));
    assert(Near(spec.horizontalDielectric, 3.6));
    assert(Near(spec.wirePitch, 320e-9));
    assert(Near(spec.aspectRatio, 1.5));
    assert(Near(spec.ildThickness, 0.48e-6));
    assert(Near(spec.copperResistivity, COPPER_RESISTIVITY));

    assert(FindWireProcessSpec(200, global_conservative, &spec));
    assert(Near(spec.featureSize, 200e-9));
    assert(Near(spec.barrierThickness, 0.016e-6 * 0.8));
    assert(Near(spec.horizontalDielectric, 3.75 * 3.038 / 2.709));
    assert(Near(spec.wirePitch, 0.945e-6));
    assert(Near(spec.aspectRatio, 2.1 * 2.2 / 2.7));
    assert(Near(spec.ildThickness, 2.2e-6));
    assert(Near(spec.copperResistivity, COPPER_RESISTIVITY));

    assert(!FindWireProcessSpec(201, local_aggressive, &spec));
}

void TestRepeatedWireUnitCalculations() {
    auto config = MakeConfigWithTechnology(90);

    Wire wire;
    wire.Initialize(90, global_aggressive, repeated_opt, 300, false, config);

    const Technology &tech = *config->technology.tech;
    const double nmosSize = MIN_NMOS_SIZE * tech.featureSize() * wire.repeaterSize;
    const double pmosSize = nmosSize * tech.pnSizeRatio();
    const double inputCap = CalculateGateCap(nmosSize, tech)
        + CalculateGateCap(pmosSize, tech);
    const double outputCap = CalculateDrainCap(nmosSize, NMOS, 1 /*no limit*/, tech)
        + CalculateDrainCap(pmosSize, PMOS, 1 /*no limit*/, tech);
    const double outputRes = CalculateOnResistance(nmosSize, NMOS, 300, tech)
        + CalculateOnResistance(pmosSize, PMOS, 300, tech);
    const double wireCap = wire.capWirePerUnit * wire.repeaterSpacing;
    const double wireRes = wire.resWirePerUnit * wire.repeaterSpacing;

    const double tau = outputRes * (inputCap + outputCap)
        + outputRes * wireCap + wireRes * outputCap
        + 0.5 * wireRes * wireCap;
    const double expectedUnitDelay = 0.693 * tau / wire.repeaterSpacing;
    const double expectedUnitDynamicEnergy =
        (inputCap + outputCap + wireCap) * tech.vdd() * tech.vdd()
        / wire.repeaterSpacing;
    const double expectedUnitLeakage =
        CalculateGateLeakage(INV, 1, nmosSize, pmosSize, 300, tech)
        * tech.vdd() / wire.repeaterSpacing;

    assert(std::isfinite(wire.repeaterSize));
    assert(std::isfinite(wire.repeaterSpacing));
    assert(wire.repeaterSize > 0);
    assert(wire.repeaterSpacing > 0);
    assert(Near(wire.getRepeatedWireUnitDelay(), expectedUnitDelay));
    assert(Near(wire.getRepeatedWireUnitDynamicEnergy(), expectedUnitDynamicEnergy));
    assert(Near(wire.getRepeatedWireUnitLeakage(), expectedUnitLeakage));

    const double wireLength = 1.25e-3;
    double delay = 0;
    double dynamicEnergy = 0;
    double leakagePower = 0;
    wire.CalculateLatencyAndPower(wireLength, &delay, &dynamicEnergy, &leakagePower);

    assert(Near(delay, expectedUnitDelay * wireLength));
    assert(Near(dynamicEnergy, expectedUnitDynamicEnergy * wireLength));
    assert(Near(leakagePower, expectedUnitLeakage * wireLength));
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
    TestWireProcessSpecLookup();
    TestRepeatedWireUnitCalculations();
    TestCopyConstructInitializedWire();
    TestAssignInitializedWire();
    TestResultOwnsWireSnapshots();

    std::cout << "Wire copy tests passed" << std::endl;
    return 0;
}
