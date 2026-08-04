#include <cmath>
#include <iostream>
#include <string>

#include "Technology.h"

#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestModelBuilders::MakeTechnologySpec;
using TestSupport::AssertNear;
using TestSupport::Require;
using TestSupport::StreamCapture;

Technology MakeTechnology(const TechnologySpec &spec) {
    Technology technology;
    technology.InitializeFromSpec(spec);
    return technology;
}

TechnologySpec MakeInterpolationSpec() {
    TechnologySpec spec = MakeTechnologySpec();
    spec.featureSizeInNano = 65;
    spec.featureSize = 65e-9;
    spec.roadmap = LOP;
    spec.useUpdatedLib = true;
    spec.vdd = 0.8;
    spec.vth = 0.1;
    spec.vdsatNmos = 0.3;
    spec.vdsatPmos = 0.4;
    spec.phyGateLength = 20e-9;
    spec.capIdealGate = 4e-10;
    spec.capFringe = 1e-10;
    spec.capJunction = 2e-3;
    spec.capOverlap = 9e-10;
    spec.capSidewall = 8e-10;
    spec.capDrainToChannel = 7e-10;
    spec.capOx = 0.01;
    spec.buildInPotential = 0.7;
    spec.effectiveElectronMobility = 0.02;
    spec.effectiveHoleMobility = 0.004;
    spec.pnSizeRatio = 2.0;
    spec.effectiveResistanceMultiplier = 1.2;
    spec.currentGmNmos = 800;
    spec.currentGmPmos = 400;
    spec.heightFin = 30e-9;
    spec.widthFin = 8e-9;
    spec.PitchFin = 40e-9;
    spec.capPolywire = 3e-10;
    for (std::size_t index = 0; index < spec.currentOnNmos.size(); index++) {
        spec.currentOnNmos[index] = 800.0 + 10.0 * index;
        spec.currentOnPmos[index] = 500.0 + 5.0 * index;
        spec.currentOffNmos[index] = 0.2 + 0.01 * index;
        spec.currentOffPmos[index] = 0.08 + 0.002 * index;
    }
    return spec;
}

double Interpolate(double lhs, double rhs, double alpha) {
    return (1 - alpha) * lhs + alpha * rhs;
}

void AssertDerivedValues(const Technology &technology) {
    const double scale = 1 + technology.vdd() / 0.9;
    AssertNear(technology.buildInPotential(), 0.9);
    AssertNear(technology.capJunction(), 1e-3 / std::pow(scale, 0.5));
    AssertNear(technology.capSidewall(), 2.5e-10 / std::pow(scale, 0.33));
    AssertNear(technology.capDrainToChannel(), 0.5e-10 / std::pow(scale, 0.33));
    AssertNear(
            technology.vdsatNmos(),
            technology.phyGateLength() * 1e5 / technology.effectiveElectronMobility());
    AssertNear(
            technology.vdsatPmos(),
            technology.phyGateLength() * 1e5 / technology.effectiveHoleMobility());
}

void TestDefaultConstructionIsUninitialized() {
    Technology technology;
    Require(!technology.initialized(), "default construction must remain uninitialized");
}

void TestInitializeFromSpecCopiesEveryGetter() {
    const TechnologySpec spec = MakeTechnologySpec();
    const Technology technology = MakeTechnology(spec);

    Require(technology.initialized(), "initialization flag");
    Require(technology.featureSizeInNano() == spec.featureSizeInNano, "feature node");
    AssertNear(technology.featureSize(), spec.featureSize);
    Require(technology.deviceRoadmap() == spec.roadmap, "device roadmap");
    Require(technology.useUpdatedLib() == spec.useUpdatedLib, "library selection");
    AssertNear(technology.vdd(), spec.vdd);
    AssertNear(technology.vth(), spec.vth);
    AssertNear(technology.vdsatNmos(), spec.vdsatNmos);
    AssertNear(technology.vdsatPmos(), spec.vdsatPmos);
    AssertNear(technology.phyGateLength(), spec.phyGateLength);
    AssertNear(technology.capIdealGate(), spec.capIdealGate);
    AssertNear(technology.capFringe(), spec.capFringe);
    AssertNear(technology.capJunction(), spec.capJunction);
    AssertNear(technology.capOverlap(), spec.capOverlap);
    AssertNear(technology.capSidewall(), spec.capSidewall);
    AssertNear(technology.capDrainToChannel(), spec.capDrainToChannel);
    AssertNear(technology.capOx(), spec.capOx);
    AssertNear(technology.buildInPotential(), spec.buildInPotential);
    AssertNear(technology.effectiveElectronMobility(), spec.effectiveElectronMobility);
    AssertNear(technology.effectiveHoleMobility(), spec.effectiveHoleMobility);
    AssertNear(technology.pnSizeRatio(), spec.pnSizeRatio);
    AssertNear(
            technology.effectiveResistanceMultiplier(),
            spec.effectiveResistanceMultiplier);
    AssertNear(technology.currentGmNmos(), spec.currentGmNmos);
    AssertNear(technology.currentGmPmos(), spec.currentGmPmos);
    AssertNear(technology.heightFin(), spec.heightFin);
    AssertNear(technology.widthFin(), spec.widthFin);
    AssertNear(technology.pitchFin(), spec.PitchFin);
    AssertNear(technology.capPolywire(), spec.capPolywire);
}

void TestTemperatureTablesPreserveEndpointsAndInterpolateInteriorValues() {
    const TechnologySpec spec = MakeTechnologySpec();
    const Technology technology = MakeTechnology(spec);

    for (std::size_t anchor = 0; anchor < spec.currentOnNmos.size(); anchor++) {
        const std::size_t index = anchor * 10;
        AssertNear(technology.currentOnNmos()[index], spec.currentOnNmos[anchor]);
        AssertNear(technology.currentOnPmos()[index], spec.currentOnPmos[anchor]);
        AssertNear(technology.currentOffNmos()[index], spec.currentOffNmos[anchor]);
        AssertNear(technology.currentOffPmos()[index], spec.currentOffPmos[anchor]);
    }

    AssertNear(technology.currentOnNmos()[5], 1043.5);
    AssertNear(technology.currentOnPmos()[5], 634.0);
    AssertNear(technology.currentOffNmos()[5], 0.06);
    AssertNear(technology.currentOffPmos()[5], 0.0521);
    AssertNear(technology.currentOnNmos()[37], 1001.9);
    AssertNear(technology.currentOnPmos()[37], 602.0);
    AssertNear(technology.currentOffNmos()[37], 0.124);
    AssertNear(technology.currentOffPmos()[37], 0.05274);
}

void TestInterpolateWithUsesLinearAndGeometricRules() {
    const TechnologySpec lhsSpec = MakeTechnologySpec();
    const TechnologySpec rhsSpec = MakeInterpolationSpec();
    Technology technology = MakeTechnology(lhsSpec);
    const Technology rhs = MakeTechnology(rhsSpec);
    constexpr double alpha = 0.25;

    technology.InterpolateWith(rhs, alpha);

    AssertNear(technology.vdd(), 1.1);
    AssertNear(technology.vth(), 0.175);
    AssertNear(technology.phyGateLength(), 32.75e-9);
    AssertNear(technology.capIdealGate(), 5.785e-10);
    AssertNear(technology.capFringe(), 2.125e-10);
    AssertNear(technology.capOx(), 0.015475);
    AssertNear(technology.effectiveElectronMobility(), 0.02325725);
    AssertNear(technology.effectiveHoleMobility(), 0.005374);
    AssertNear(technology.pnSizeRatio(), 2.3375);
    AssertNear(technology.effectiveResistanceMultiplier(), 1.455);
    AssertNear(technology.currentOnNmos()[30], Interpolate(1011, 830, alpha));
    AssertNear(technology.currentOnPmos()[35], Interpolate(604, 517.5, alpha));
    AssertNear(
            technology.currentOffNmos()[20],
            std::pow(0.09, 0.75) * std::pow(0.22, 0.25));
    AssertNear(
            technology.currentOffPmos()[70],
            std::pow(0.0534, 0.75) * std::pow(0.094, 0.25));
    AssertDerivedValues(technology);

    Require(technology.featureSizeInNano() == lhsSpec.featureSizeInNano,
            "interpolation preserves the base feature node");
    AssertNear(technology.featureSize(), lhsSpec.featureSize);
    Require(technology.deviceRoadmap() == lhsSpec.roadmap,
            "interpolation preserves the base roadmap");
    Require(technology.useUpdatedLib() == lhsSpec.useUpdatedLib,
            "interpolation preserves the base library selection");
    AssertNear(technology.capOverlap(), lhsSpec.capOverlap);
    AssertNear(technology.currentGmNmos(), lhsSpec.currentGmNmos);
    AssertNear(technology.currentGmPmos(), lhsSpec.currentGmPmos);
    AssertNear(technology.heightFin(), lhsSpec.heightFin);
    AssertNear(technology.widthFin(), lhsSpec.widthFin);
    AssertNear(technology.pitchFin(), lhsSpec.PitchFin);
    AssertNear(technology.capPolywire(), lhsSpec.capPolywire);
}

void TestInterpolateEndpointsAndSameNodeNoOp() {
    const TechnologySpec lhsSpec = MakeTechnologySpec();
    const TechnologySpec rhsSpec = MakeInterpolationSpec();
    const Technology rhs = MakeTechnology(rhsSpec);

    Technology atZero = MakeTechnology(lhsSpec);
    atZero.InterpolateWith(rhs, 0);
    AssertNear(atZero.vdd(), lhsSpec.vdd);
    AssertNear(atZero.currentOnNmos()[30], 1011);
    AssertNear(atZero.currentOffNmos()[20], 0.09);
    AssertDerivedValues(atZero);

    Technology atOne = MakeTechnology(lhsSpec);
    atOne.InterpolateWith(rhs, 1);
    AssertNear(atOne.vdd(), rhsSpec.vdd);
    AssertNear(atOne.vth(), rhsSpec.vth);
    AssertNear(atOne.currentOnNmos()[30], 830);
    AssertNear(atOne.currentOffNmos()[20], 0.22);
    Require(atOne.featureSizeInNano() == lhsSpec.featureSizeInNano,
            "alpha one preserves base identity");
    AssertDerivedValues(atOne);

    TechnologySpec sameNodeSpec = MakeInterpolationSpec();
    sameNodeSpec.featureSizeInNano = lhsSpec.featureSizeInNano;
    sameNodeSpec.vdd = 0.4;
    Technology noOp = MakeTechnology(lhsSpec);
    noOp.InterpolateWith(MakeTechnology(sameNodeSpec), 0.5);
    AssertNear(noOp.vdd(), lhsSpec.vdd);
    AssertNear(noOp.capJunction(), lhsSpec.capJunction);
    AssertNear(noOp.currentOnNmos()[30], 1011);
}

void TestInterpolationCurrentlyAcceptsOutOfRangeAlpha() {
    const TechnologySpec lhsSpec = MakeTechnologySpec();
    const TechnologySpec rhsSpec = MakeInterpolationSpec();
    Technology technology = MakeTechnology(lhsSpec);
    const Technology rhs = MakeTechnology(rhsSpec);

    technology.InterpolateWith(rhs, 1.25);

    AssertNear(technology.vdd(), 0.7);
    AssertNear(technology.currentOnNmos()[0], 737.5);
    AssertNear(
            technology.currentOffNmos()[0],
            std::pow(0.05, -0.25) * std::pow(0.2, 1.25));
    Require(std::isfinite(technology.vdsatNmos()),
            "out-of-range alpha currently produces a finite extrapolation");
    Require(technology.featureSizeInNano() == lhsSpec.featureSizeInNano,
            "extrapolation preserves base identity");
}

void TestInitializationCurrentlyAcceptsUncheckedSpec() {
    TechnologySpec spec = MakeTechnologySpec();
    spec.vdd = -1;
    spec.effectiveElectronMobility = 0;
    spec.currentOnNmos[0] = -5;

    const Technology technology = MakeTechnology(spec);

    Require(technology.initialized(), "unchecked spec is currently accepted");
    AssertNear(technology.vdd(), -1);
    AssertNear(technology.effectiveElectronMobility(), 0);
    AssertNear(technology.currentOnNmos()[0], -5);
}

void TestPrintPropertyIncludesIdentityAndRepresentativeValues() {
    Technology technology = MakeTechnology(MakeTechnologySpec());
    StreamCapture capture(std::cout);

    technology.PrintProperty();
    capture.Stop();
    const std::string output = capture.Text();

    for (const char *expected : {
            "Fabrication Process Technology Node:",
            " - Node = 90nm",
            " - Feature Size = 9e-08m",
            " - Device Roadmap = HP",
            " - Using Updated Library = false",
            " - Vdd = 1.2V",
            " - Vth = 0.2V",
            " - Physical Gate Length = 3.7e-08m",
            " - PN Size Ratio = 2.45",
            " - NMOS Current @300K = 1050A/m",
            " - PMOS Current @300K = 639A/m"}) {
        Require(output.find(expected) != std::string::npos,
                "printed technology properties missing: " + std::string(expected));
    }
}

}  // namespace

int main() {
    TestDefaultConstructionIsUninitialized();
    TestInitializeFromSpecCopiesEveryGetter();
    TestTemperatureTablesPreserveEndpointsAndInterpolateInteriorValues();
    TestInterpolateWithUsesLinearAndGeometricRules();
    TestInterpolateEndpointsAndSameNodeNoOp();
    TestInterpolationCurrentlyAcceptsOutOfRangeAlpha();
    TestInitializationCurrentlyAcceptsUncheckedSpec();
    TestPrintPropertyIncludesIdentityAndRepresentativeValues();
    std::cout << "Technology tests passed\n";
    return 0;
}
