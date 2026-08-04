#include "TestModelBuilders.h"
#include "TestSupport.h"
#include "formula.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;

Technology MakeFinfetTechnology() {
    TechnologySpec spec = TestModelBuilders::MakeTechnologySpec();
    spec.featureSizeInNano = 22;
    spec.featureSize = 22e-9;
    spec.useUpdatedLib = true;
    spec.heightFin = 20e-9;
    spec.widthFin = 8e-9;
    spec.PitchFin = 30e-9;

    Technology tech;
    tech.InitializeFromSpec(spec);
    return tech;
}

void TestGateCapAndFbramCapacitancesUseReferenceValues() {
    const Technology tech = *TestModelBuilders::MakeTechnology();
    const double transistorWidth = 4e-6;
    const double expectedGate = (tech.capIdealGate() + tech.capOverlap()
            + 3 * tech.capFringe()) * transistorWidth
            + tech.phyGateLength() * tech.capPolywire();
    AssertNear(CalculateGateCap(transistorWidth, tech), expectedGate);

    const double thicknessFactor = 2.0;
    const double expectedFbramGate = (tech.capIdealGate() / thicknessFactor
            + tech.capOverlap() + 3 * tech.capFringe()) * transistorWidth
            + tech.phyGateLength() * tech.capPolywire();
    AssertNear(CalculateFBRAMGateCap(transistorWidth, thicknessFactor, tech), expectedFbramGate);
    AssertNear(CalculateFBRAMDrainCap(transistorWidth, tech),
            (3 * tech.capSidewall() + tech.capDrainToChannel()) * transistorWidth);
    assert(CalculateFBRAMDrainCap(0, tech) == 0);
}

void TestGateAreaAndCapacitanceCoverPlanarFinfetAndZeroWidths() {
    const Technology planar = *TestModelBuilders::MakeTechnology();
    const double regionHeight = MAX_TRANSISTOR_HEIGHT * planar.featureSize();
    double height = 0;
    double width = 0;
    const double inverterArea = CalculateGateArea(
            INV, 1, 2 * planar.featureSize(), 4 * planar.featureSize(),
            regionHeight, planar, &height, &width, false);
    TestSupport::AssertFinitePositive(inverterArea, "planar inverter area");
    AssertNear(inverterArea, height * width);

    double norInput = 0;
    double norOutput = 0;
    double nandInput = 0;
    double nandOutput = 0;
    CalculateGateCapacitance(NOR, 3, 2 * planar.featureSize(), 4 * planar.featureSize(),
            regionHeight, planar, &norInput, &norOutput);
    CalculateGateCapacitance(NAND, 3, 2 * planar.featureSize(), 4 * planar.featureSize(),
            regionHeight, planar, &nandInput, &nandOutput);
    AssertNear(norInput, nandInput);
    TestSupport::AssertFinitePositive(norOutput, "NOR output capacitance");
    TestSupport::AssertFinitePositive(nandOutput, "NAND output capacitance");

    double zeroInput = -1;
    double zeroOutput = -1;
    CalculateGateCapacitance(INV, 1, 0, 0, regionHeight, planar, &zeroInput, &zeroOutput);
    assert(zeroInput == 0);
    assert(zeroOutput == 0);
    CalculateGateCapacitance(INV, 1, 0, 0, regionHeight, planar, nullptr, nullptr);

    const Technology finfet = MakeFinfetTechnology();
    const double finfetHeight = MAX_TRANSISTOR_HEIGHT * finfet.featureSize();
    const double finfetArea = CalculateGateArea(
            NAND, 2, 2 * finfet.featureSize(), 3 * finfet.featureSize(),
            finfetHeight, finfet, &height, &width, true);
    TestSupport::AssertFinitePositive(finfetArea, "FinFET NAND area");
    AssertNear(finfetArea, height * width);

    AssertThrows<std::runtime_error>([&] {
        CalculateGateArea(INV, 1, -planar.featureSize(), planar.featureSize(),
                regionHeight, planar, &height, &width, false);
    }, "width cannot be negative");
    AssertThrows<std::runtime_error>([&] {
        CalculateGateCapacitance(INV, 1, planar.featureSize(), -planar.featureSize(),
                regionHeight, planar, &norInput, &norOutput);
    }, "width cannot be negative");
}

void TestDrainCapacitorCoversNmosAndPmosPaths() {
    const Technology tech = *TestModelBuilders::MakeTechnology();
    const double width = 3 * tech.featureSize();
    const double regionHeight = MAX_TRANSISTOR_HEIGHT * tech.featureSize();
    const double nmos = CalculateDrainCap(width, NMOS, regionHeight, tech);
    const double pmos = CalculateDrainCap(width, PMOS, regionHeight, tech);
    TestSupport::AssertFinitePositive(nmos, "NMOS drain capacitance");
    TestSupport::AssertFinitePositive(pmos, "PMOS drain capacitance");
    double expectedNmos = 0;
    double expectedPmos = 0;
    CalculateGateCapacitance(INV, 1, width, 0, regionHeight, tech, nullptr, &expectedNmos);
    CalculateGateCapacitance(INV, 1, 0, width, regionHeight, tech, nullptr, &expectedPmos);
    AssertNear(nmos, expectedNmos);
    AssertNear(pmos, expectedPmos);
    assert(CalculateDrainCap(0, NMOS, regionHeight, tech) == 0);
}

void TestLeakageAndResistanceCoverTypesAndTemperatureBounds() {
    const Technology tech = *TestModelBuilders::MakeTechnology();
    const double nmosWidth = 2e-6;
    const double pmosWidth = 4e-6;

    AssertNear(CalculateGateLeakage(INV, 1, nmosWidth, pmosWidth, 300, tech),
            std::max(nmosWidth * tech.currentOffNmos()[0], pmosWidth * tech.currentOffPmos()[0]));
    AssertNear(CalculateGateLeakage(NOR, 2, nmosWidth, pmosWidth, 400, tech),
            AVG_RATIO_LEAK_2INPUT_NOR * nmosWidth * tech.currentOffNmos()[100] * 2);
    AssertNear(CalculateGateLeakage(NAND, 3, nmosWidth, pmosWidth, 400, tech),
            AVG_RATIO_LEAK_3INPUT_NAND * pmosWidth * tech.currentOffPmos()[100] * 3);
    assert(CalculateGateLeakage(99, 2, nmosWidth, pmosWidth, 300, tech) == 0);
    AssertThrows<std::runtime_error>([&] {
        CalculateGateLeakage(INV, 1, nmosWidth, pmosWidth, 299, tech);
    }, "Temperature is out of range");
    AssertThrows<std::runtime_error>([&] {
        CalculateGateLeakage(INV, 1, nmosWidth, pmosWidth, 401, tech);
    }, "Temperature is out of range");

    AssertNear(CalculateOnResistance(nmosWidth, NMOS, 300, tech),
            tech.effectiveResistanceMultiplier() * tech.vdd() / (tech.currentOnNmos()[0] * nmosWidth));
    AssertNear(CalculateOnResistance(pmosWidth, PMOS, 400, tech),
            tech.effectiveResistanceMultiplier() * tech.vdd() / (tech.currentOnPmos()[100] * pmosWidth));
    assert(std::isinf(CalculateOnResistance(0, NMOS, 300, tech)));
    AssertThrows<std::runtime_error>([&] {
        CalculateOnResistance(nmosWidth, NMOS, 401, tech);
    }, "Temperature is out of range");
}

void TestTransconductanceCoversLegacyUpdatedAndTypes() {
    const Technology legacy = *TestModelBuilders::MakeTechnology();
    const double width = 2e-6;
    const double legacyNmos = legacy.effectiveElectronMobility() * legacy.capOx() / 2
            * width / legacy.phyGateLength() * std::min(legacy.vdsatNmos(), legacy.vdd() - legacy.vth());
    const double legacyPmos = legacy.effectiveHoleMobility() * legacy.capOx() / 2
            * width / legacy.phyGateLength() * std::min(legacy.vdsatPmos(), legacy.vdd() - legacy.vth());
    AssertNear(CalculateTransconductance(width, NMOS, legacy), legacyNmos);
    AssertNear(CalculateTransconductance(width, PMOS, legacy), legacyPmos);
    assert(CalculateTransconductance(0, NMOS, legacy) == 0);

    const Technology updated = MakeFinfetTechnology();
    AssertNear(CalculateTransconductance(width, NMOS, updated),
            2 * updated.currentGmNmos() * width / (0.7 * updated.vdd() - updated.vth()));
    AssertNear(CalculateTransconductance(width, PMOS, updated),
            2 * updated.currentGmPmos() * width / (0.7 * updated.vdd() - updated.vth()));
}

void TestHorowitzAndWireReferencesAndInvalidDimensions() {
    double rampOutput = 0;
    const double delay = horowitz(2, 0.5, 4, &rampOutput);
    const double expectedDelay = 2 * std::sqrt(std::log(0.5) * std::log(0.5) + 2 * (1.0 / 4 / 2) * 0.5 * 0.5);
    AssertNear(delay, expectedDelay);
    AssertNear(rampOutput, 0.5 / expectedDelay);
    AssertNear(horowitz(0, 0.5, 4, nullptr), 0);
    AssertThrows<std::runtime_error>([] { horowitz(-1, 0.5, 4, nullptr); }, "cannot be negative");
    AssertThrows<std::runtime_error>([] { horowitz(1, 0.5, 0, nullptr); }, "must be positive");

    AssertNear(CalculateWireResistance(2, 10, 8, 1, 2, 1.5), 1.5 * 2 / 5 / 8);
    assert(std::isinf(CalculateWireResistance(2, 2, 8, 1, 2, 1.5)));
    AssertNear(CalculateWireCapacitance(2, 4, 3, 6, 8, 1.5, 2, 3, 0.25),
            2 * 2 * 3 * 4 / 8 + 2 * 2 * 1.5 * 2 * 3 / 6 + 0.25);
    assert(std::isinf(CalculateWireCapacitance(2, 4, 3, 0, 8, 1.5, 2, 3, 0.25)));
}

}  // namespace

int main() {
    TestGateCapAndFbramCapacitancesUseReferenceValues();
    TestGateAreaAndCapacitanceCoverPlanarFinfetAndZeroWidths();
    TestDrainCapacitorCoversNmosAndPmosPaths();
    TestLeakageAndResistanceCoverTypesAndTemperatureBounds();
    TestTransconductanceCoversLegacyUpdatedAndTypes();
    TestHorowitzAndWireReferencesAndInvalidDimensions();

    std::cout << "Formula coverage tests passed" << std::endl;
    return 0;
}
