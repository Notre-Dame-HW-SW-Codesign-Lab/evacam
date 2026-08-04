#include "EvaCamConfig.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"
#include "Wire.h"
#include "factories/WireFactory.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

namespace {

using TestModelBuilders::MakeEvaCamConfig;
using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::StreamCapture;

std::shared_ptr<EvaCamConfig> MakeWireConfig() {
    auto config = MakeEvaCamConfig();
    config->technology.cell->minSenseVoltage = 0.07;
    config->input.maxNmosSize = 100;
    return config;
}

void AssertInitializedGeometry(const Wire &wire, const std::shared_ptr<EvaCamConfig> &config) {
    assert(wire.initialized);
    assert(wire.config == config);
    assert(wire.config->technology.tech == config->technology.tech);
    AssertFinitePositive(wire.featureSize, "feature size");
    AssertFinitePositive(wire.wirePitch, "wire pitch");
    AssertFinitePositive(wire.wireWidth, "wire width");
    AssertFinitePositive(wire.wireThickness, "wire thickness");
    AssertFinitePositive(wire.wireSpacing, "wire spacing");
    AssertFinitePositive(wire.resWirePerUnit, "wire resistance");
    AssertFinitePositive(wire.capWirePerUnit, "wire capacitance");
}

void TestCalculateLatencyAndPowerRequiresInitialization() {
    Wire wire;
    double delay = 0;
    double energy = 0;
    double leakage = 0;
    AssertThrows<std::runtime_error>([&]() {
        wire.CalculateLatencyAndPower(1e-3, &delay, &energy, &leakage);
    }, "Require initialization first");
}

void TestInitializeAllConfiguredWireTypes() {
    const WireType types[] = {
        local_aggressive, local_conservative, semi_aggressive,
        semi_conservative, global_aggressive, global_conservative,
    };
    auto config = MakeWireConfig();
    for (WireType type : types) {
        Wire wire;
        wire.Initialize(90, type, repeated_none, 310, false, config);
        AssertInitializedGeometry(wire, config);
        assert(wire.featureSizeInNano == 90);
        assert(wire.wireType == type);
        assert(wire.wireRepeaterType == repeated_none);
        assert(wire.temperature == 310);
        assert(!wire.isLowSwing);
    }
}

void TestInitializeRejectsUnsupportedProcessAndLowSwingRepeaters() {
    auto config = MakeWireConfig();
    Wire unsupported;
    AssertThrows<std::runtime_error>([&]() {
        unsupported.Initialize(201, local_aggressive, repeated_none, 300, false, config);
    }, "unsupported wire process");
    assert(!unsupported.initialized);

    Wire invalidMode;
    AssertThrows<std::runtime_error>([&]() {
        invalidMode.Initialize(90, local_aggressive, repeated_opt, 300, true, config);
    }, "low swing is not supported");
    assert(!invalidMode.initialized);
}

void TestPassiveWireReferenceValuesAndOptionalOutputs() {
    auto config = MakeWireConfig();
    Wire wire;
    wire.Initialize(90, local_aggressive, repeated_none, 300, false, config);

    const double length = 2.5e-3;
    double delay = -1;
    double energy = -1;
    double leakage = -1;
    wire.CalculateLatencyAndPower(length, &delay, &energy, &leakage);
    AssertNear(delay, 2.3 * wire.resWirePerUnit * wire.capWirePerUnit * length * length / 2);
    AssertNear(energy, wire.capWirePerUnit * length * config->technology.tech->vdd()
            * config->technology.tech->vdd());
    assert(leakage == 0);

    wire.CalculateLatencyAndPower(length, nullptr, nullptr, nullptr);
    delay = energy = leakage = 123;
    wire.CalculateLatencyAndPower(length, &delay, &energy, &leakage);
    assert(delay != 123 && energy != 123 && leakage != 123);
}

void TestLowSwingWireCalculationAndResetBehavior() {
    auto config = MakeWireConfig();
    Wire wire;
    wire.Initialize(90, semi_conservative, repeated_none, 300, true, config);
    AssertInitializedGeometry(wire, config);

    double delay = -1;
    double energy = -1;
    double leakage = -1;
    wire.CalculateLatencyAndPower(1e-3, &delay, &energy, &leakage);
    AssertFiniteNonNegative(delay, "low swing delay");
    AssertFiniteNonNegative(energy, "low swing energy");
    AssertFiniteNonNegative(leakage, "low swing leakage");

    const double firstDelay = delay;
    const double firstEnergy = energy;
    const double firstLeakage = leakage;
    delay = energy = leakage = 987;
    wire.CalculateLatencyAndPower(1e-3, &delay, &energy, &leakage);
    AssertNear(delay, firstDelay);
    AssertNear(energy, firstEnergy);
    AssertNear(leakage, firstLeakage);
}

void TestOptimalAndPenalizedRepeaters() {
    auto config = MakeWireConfig();
    Wire optimal;
    optimal.Initialize(90, global_aggressive, repeated_opt, 300, false, config);
    AssertFinitePositive(optimal.repeaterSize, "optimal repeater size");
    AssertFinitePositive(optimal.repeaterSpacing, "optimal repeater spacing");
    AssertFinitePositive(optimal.repeaterHeight, "optimal repeater height");
    AssertFinitePositive(optimal.repeaterWidth, "optimal repeater width");
    AssertFinitePositive(optimal.repeatedWirePitch, "repeated wire pitch");

    const double optimalDelay = optimal.getRepeatedWireUnitDelay();
    const double optimalEnergy = optimal.getRepeatedWireUnitDynamicEnergy();
    const double optimalLeakage = optimal.getRepeatedWireUnitLeakage();
    AssertFinitePositive(optimalDelay, "optimal unit delay");
    AssertFinitePositive(optimalEnergy, "optimal unit energy");
    AssertFinitePositive(optimalLeakage, "optimal unit leakage");

    const WireRepeaterType penalizedTypes[] = {
        repeated_5, repeated_10, repeated_20, repeated_30, repeated_40, repeated_50,
    };
    const double penalties[] = {0.05, 0.10, 0.20, 0.30, 0.40, 0.50};
    for (unsigned int index = 0; index < sizeof(penalizedTypes) / sizeof(*penalizedTypes); index++) {
        Wire wire;
        wire.Initialize(90, global_aggressive, penalizedTypes[index], 300, false, config);
        AssertFinitePositive(wire.repeaterSize, "penalized repeater size");
        AssertFinitePositive(wire.repeaterSpacing, "penalized repeater spacing");
        assert(wire.getRepeatedWireUnitDelay() <= optimalDelay * (1 + penalties[index] + 1e-10));
    }
}

void TestRepeatedWireCalculationUsesUnitValuesAndResetsOutputs() {
    auto config = MakeWireConfig();
    Wire wire;
    wire.Initialize(90, global_conservative, repeated_opt, 300, false, config);
    const double length = 1.25e-3;
    double delay = -1;
    double energy = -1;
    double leakage = -1;
    wire.CalculateLatencyAndPower(length, &delay, &energy, &leakage);
    AssertNear(delay, wire.getRepeatedWireUnitDelay() * length);
    AssertNear(energy, wire.getRepeatedWireUnitDynamicEnergy() * length);
    AssertNear(leakage, wire.getRepeatedWireUnitLeakage() * length);

    const double firstDelay = delay;
    delay = energy = leakage = 111;
    wire.CalculateLatencyAndPower(length, &delay, &energy, &leakage);
    AssertNear(delay, firstDelay);
    wire.CalculateLatencyAndPower(length, nullptr, nullptr, nullptr);
}

void TestRepeaterPublicAdjustmentAndCopySemantics() {
    auto config = MakeWireConfig();
    Wire wire;
    wire.Initialize(90, global_aggressive, repeated_opt, 300, false, config);
    const double optimalDelay = wire.getRepeatedWireUnitDelay();
    wire.findPenalizedRepeater(0.2);
    assert(wire.getRepeatedWireUnitDelay() <= optimalDelay * 1.2 * (1 + 1e-10));

    Wire copy(wire);
    assert(copy.initialized == wire.initialized);
    assert(copy.config == config);
    AssertNear(copy.repeaterSize, wire.repeaterSize);
    AssertNear(copy.repeaterSpacing, wire.repeaterSpacing);
    wire.Initialize(90, local_aggressive, repeated_none, 300, false, config);
    assert(copy.wireType == global_aggressive);
    assert(copy.wireRepeaterType == repeated_opt);

    Wire assigned;
    assigned = copy;
    assert(assigned.config == config);
    AssertNear(assigned.getRepeatedWireUnitDelay(), copy.getRepeatedWireUnitDelay());
}

void TestPrintPropertyForPassiveAndRepeatedWires() {
    auto config = MakeWireConfig();
    Wire passive;
    passive.Initialize(90, local_aggressive, repeated_none, 300, true, config);
    StreamCapture passiveCapture(std::cout);
    passive.PrintProperty();
    passiveCapture.Stop();
    assert(passiveCapture.Text().find("passive") != std::string::npos);
    assert(passiveCapture.Text().find("Low Swing") != std::string::npos);

    Wire repeated;
    repeated.Initialize(90, global_aggressive, repeated_opt, 300, false, config);
    StreamCapture repeatedCapture(std::cout);
    repeated.PrintProperty();
    repeatedCapture.Stop();
    assert(repeatedCapture.Text().find("active") != std::string::npos);
    assert(repeatedCapture.Text().find("Repeater Size") != std::string::npos);
}

void TestWireFactoryDefaultsAndFixedConfiguration() {
    auto config = MakeWireConfig();
    Wire localDefault = WireFactory::CreateDefaultLocalWire(config);
    Wire globalDefault = WireFactory::CreateDefaultGlobalWire(config);
    assert(localDefault.wireType == local_aggressive);
    assert(globalDefault.wireType == global_aggressive);
    assert(localDefault.wireRepeaterType == repeated_none);
    assert(globalDefault.wireRepeaterType == repeated_none);
    assert(!localDefault.isLowSwing && !globalDefault.isLowSwing);
    assert(localDefault.config == config && globalDefault.config == config);

    config->exploration.wires.localWireType = IntValueDomain::Sequential(local_conservative, local_conservative);
    config->exploration.wires.localWireRepeaterType = IntValueDomain::Sequential(repeated_20, repeated_20);
    config->exploration.wires.isLocalWireLowSwing = IntValueDomain::Sequential(false, false);
    config->exploration.wires.globalWireType = IntValueDomain::Sequential(global_conservative, global_conservative);
    config->exploration.wires.globalWireRepeaterType = IntValueDomain::Sequential(repeated_none, repeated_none);
    config->exploration.wires.isGlobalWireLowSwing = IntValueDomain::Sequential(true, true);
    Wire localConfigured = WireFactory::CreateDefaultLocalWire(config);
    Wire globalConfigured = WireFactory::CreateDefaultGlobalWire(config);
    assert(localConfigured.wireType == local_conservative);
    assert(localConfigured.wireRepeaterType == repeated_20);
    assert(!localConfigured.isLowSwing);
    assert(globalConfigured.wireType == global_conservative);
    assert(globalConfigured.wireRepeaterType == repeated_none);
    assert(globalConfigured.isLowSwing);
    assert(localConfigured.config->technology.tech == config->technology.tech);
    assert(globalConfigured.config->technology.cell == config->technology.cell);
}

}  // namespace

int main() {
    TestCalculateLatencyAndPowerRequiresInitialization();
    TestInitializeAllConfiguredWireTypes();
    TestInitializeRejectsUnsupportedProcessAndLowSwingRepeaters();
    TestPassiveWireReferenceValuesAndOptionalOutputs();
    TestLowSwingWireCalculationAndResetBehavior();
    TestOptimalAndPenalizedRepeaters();
    TestRepeatedWireCalculationUsesUnitValuesAndResetsOutputs();
    TestRepeaterPublicAdjustmentAndCopySemantics();
    TestPrintPropertyForPassiveAndRepeatedWires();
    TestWireFactoryDefaultsAndFixedConfiguration();
    std::cout << "Wire and factory tests passed" << std::endl;
    return 0;
}
