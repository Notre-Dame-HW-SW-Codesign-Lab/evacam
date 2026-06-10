#include "formula.h"
#include "Technology.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool Near(double actual, double expected, double tolerance = 1e-12) {
    const double scale = std::max(1.0, std::fabs(expected));
    return std::fabs(actual - expected) <= tolerance * scale;
}

Technology MakeTechnology(int processNode = 90, bool useUpdatedLib = false) {
    Technology tech;
    tech.Initialize(processNode, HP, useUpdatedLib);
    return tech;
}

struct ExpectedLayout {
    double unitWidthRegionP = 0;
    double unitWidthRegionN = 0;
    double heightRegionP = 0;
    double heightRegionN = 0;
    double unitWidthDrainP = 0;
    double unitWidthDrainN = 0;
    double heightDrainP = 0;
    double heightDrainN = 0;
    int numFoldedPMOS = 1;
    int numFoldedNMOS = 1;
};

template <typename Func>
void AssertThrowsRuntimeError(Func func) {
    bool threw = false;
    try {
        func();
    } catch (const std::runtime_error &) {
        threw = true;
    }
    assert(threw);
}

ExpectedLayout CalculateExpectedLegacyLayout(
        double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech,
        bool useUpdatedRegionLayout) {
    const double ratio = widthPMOS / (widthPMOS + widthNMOS);
    double maxWidthPMOS, maxWidthNMOS;
    if (ratio == 0) {
        maxWidthPMOS = 0;
        maxWidthNMOS = heightTransistorRegion;
    } else if (ratio == 1) {
        maxWidthPMOS = heightTransistorRegion;
        maxWidthNMOS = 0;
    } else {
        maxWidthPMOS = ratio * (heightTransistorRegion - MIN_GAP_BET_P_AND_N_DIFFS * tech.featureSize());
        maxWidthNMOS = maxWidthPMOS / ratio * (1 - ratio);
    }

    ExpectedLayout layout;
    if (widthPMOS > 0) {
        if (widthPMOS < maxWidthPMOS) {
            layout.unitWidthRegionP = tech.featureSize();
            layout.heightRegionP = widthPMOS;
            layout.unitWidthDrainP = 0;
            layout.heightDrainP = widthPMOS;
        } else {
            layout.numFoldedPMOS = (int)(std::ceil(widthPMOS / (maxWidthPMOS - 3 * tech.featureSize())));
            layout.unitWidthRegionP = layout.numFoldedPMOS * tech.featureSize()
                + (layout.numFoldedPMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout.heightRegionP = maxWidthPMOS;
            layout.unitWidthDrainP = (layout.numFoldedPMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout.heightDrainP = maxWidthPMOS;
        }
    }
    if (widthNMOS > 0) {
        if (widthNMOS < maxWidthNMOS) {
            layout.unitWidthRegionN = tech.featureSize();
            layout.heightRegionN = widthNMOS;
            layout.unitWidthDrainN = 0;
            layout.heightDrainN = widthNMOS;
        } else {
            layout.numFoldedNMOS = (int)(std::ceil(widthNMOS / (maxWidthNMOS - 3 * tech.featureSize())));
            layout.unitWidthRegionN = layout.numFoldedNMOS * tech.featureSize()
                + (layout.numFoldedNMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout.heightRegionN = maxWidthNMOS;
            layout.unitWidthDrainN = (layout.numFoldedNMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout.heightDrainN = maxWidthNMOS;
        }
    }

    if (useUpdatedRegionLayout && tech.featureSize() <= 22 * 1e-9) {
        int maxNumPFin, maxNumNFin;
        if (ratio == 0) {
            maxNumPFin = 0;
            maxNumNFin = (int)(std::floor(heightTransistorRegion / tech.pitchFin()));
        } else if (ratio == 1) {
            maxNumPFin = (int)(std::floor(heightTransistorRegion / tech.pitchFin()));
            maxNumNFin = 0;
        } else {
            maxNumPFin = (int)(std::floor(
                    ratio * (heightTransistorRegion - MIN_GAP_BET_P_AND_N_DIFFS * tech.featureSize())
                    / tech.pitchFin()));
            maxNumNFin = (int)(std::floor(maxNumPFin / ratio * (1 - ratio)));
        }

        int NumPFin = (int)(std::ceil(widthPMOS / (2 * tech.heightFin() + tech.widthFin())));
        if (NumPFin > 0) {
            if (NumPFin < maxNumPFin) {
                layout.unitWidthRegionP = tech.featureSize();
                layout.heightRegionP = (NumPFin - 1) * tech.pitchFin() + 2 * tech.widthFin();
            } else {
                int numFoldedPFin = (int)(std::ceil(NumPFin / maxNumPFin));
                layout.unitWidthRegionP = numFoldedPFin * tech.featureSize()
                    + (numFoldedPFin - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
                layout.heightRegionP = (maxNumPFin - 1) * tech.pitchFin() + 2 * tech.widthFin();
            }
        } else {
            layout.unitWidthRegionP = 0;
            layout.heightRegionP = 0;
        }

        int NumNFin = (int)(std::ceil(widthNMOS / (2 * tech.heightFin() + tech.widthFin())));
        if (NumNFin > 0) {
            if (NumNFin < maxNumNFin) {
                layout.unitWidthRegionN = tech.featureSize();
                layout.heightRegionN = (NumNFin - 1) * tech.pitchFin() + 2 * tech.widthFin();
            } else {
                int numFoldedNMOS = (int)(std::ceil(NumNFin / maxNumNFin));
                layout.unitWidthRegionN = numFoldedNMOS * tech.featureSize()
                    + (numFoldedNMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
                layout.heightRegionN = (maxNumNFin - 1) * tech.pitchFin() + 2 * tech.widthFin();
            }
        } else {
            layout.unitWidthRegionN = 0;
            layout.heightRegionN = 0;
        }
    }

    return layout;
}

double ExpectedGateArea(
        int gateType, int numInput,
        double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech,
        bool useUpdatedRegionLayout) {
    const ExpectedLayout layout = CalculateExpectedLegacyLayout(
            widthNMOS, widthPMOS, heightTransistorRegion, tech, useUpdatedRegionLayout);
    double widthRegionP, widthRegionN;
    switch (gateType) {
        case INV:
            widthRegionP = 2 * tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2) + layout.unitWidthRegionP;
            widthRegionN = 2 * tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2) + layout.unitWidthRegionN;
            break;
        case NOR:
            widthRegionP = 2 * tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                + layout.unitWidthRegionP * numInput + (numInput - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            widthRegionN = 2 * tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                + layout.unitWidthRegionN * numInput
                + (numInput - 1) * tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2);
            break;
        case NAND:
            widthRegionN = 2 * tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                + layout.unitWidthRegionN * numInput + (numInput - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            widthRegionP = 2 * tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                + layout.unitWidthRegionP * numInput
                + (numInput - 1) * tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2);
            break;
        default:
            widthRegionN = widthRegionP = 0;
    }

    const double width = std::max(widthRegionN, widthRegionP);
    double height;
    if (widthPMOS > 0 && widthNMOS > 0) {
        height = layout.heightRegionN + layout.heightRegionP + tech.featureSize() * MIN_GAP_BET_P_AND_N_DIFFS
            + 2 * tech.featureSize() * MIN_WIDTH_POWER_RAIL;
    } else {
        height = layout.heightRegionN + layout.heightRegionP;
    }
    return width * height;
}

void ExpectedGateCapacitance(
        int gateType, int numInput,
        double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech,
        double *capInput, double *capOutput) {
    const ExpectedLayout layout = CalculateExpectedLegacyLayout(
            widthNMOS, widthPMOS, heightTransistorRegion, tech, false);
    double widthDrainP = 0;
    double widthDrainN = 0;
    switch (gateType) {
        case INV:
            if (widthPMOS > 0) {
                widthDrainP = tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2) + layout.unitWidthDrainP;
            }
            if (widthNMOS > 0) {
                widthDrainN = tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2) + layout.unitWidthDrainN;
            }
            break;
        case NOR:
            if (widthPMOS > 0) {
                widthDrainP = tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                    + layout.unitWidthDrainP * numInput + (numInput - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            }
            if (widthNMOS > 0) {
                widthDrainN = (tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                        + layout.unitWidthDrainN) * numInput;
            }
            break;
        case NAND:
            if (widthNMOS > 0) {
                widthDrainN = tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                    + layout.unitWidthDrainN * numInput + (numInput - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            }
            if (widthPMOS > 0) {
                widthDrainP = (tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                        + layout.unitWidthDrainP) * numInput;
            }
            break;
        default:
            widthDrainN = widthDrainP = 0;
    }

    double capDrainSidewallN, capDrainSidewallP;
    if (layout.numFoldedNMOS % 2 == 0) {
        capDrainSidewallN = 2 * widthDrainN * tech.capSidewall();
    } else {
        capDrainSidewallN = (2 * widthDrainN + layout.heightDrainN) * tech.capSidewall();
    }
    if (layout.numFoldedPMOS % 2 == 0) {
        capDrainSidewallP = 2 * widthDrainP * tech.capSidewall();
    } else {
        capDrainSidewallP = (2 * widthDrainP + layout.heightDrainP) * tech.capSidewall();
    }

    *capOutput = widthDrainN * layout.heightDrainN * tech.capJunction()
        + widthDrainP * layout.heightDrainP * tech.capJunction()
        + capDrainSidewallN + capDrainSidewallP
        + layout.numFoldedNMOS * layout.heightDrainN * tech.capDrainToChannel()
        + layout.numFoldedPMOS * layout.heightDrainP * tech.capDrainToChannel();
    *capInput = CalculateGateCap(widthNMOS, tech) + CalculateGateCap(widthPMOS, tech);
}

void TestIntegerLogHelpers() {
    assert(!isPow2(0));
    assert(!isPow2(-4));
    assert(!isPow2(3));
    assert(isPow2(1));
    assert(isPow2(16));

    assert(intLog2(1) == 0);
    assert(intLog2(16) == 4);
    AssertThrowsRuntimeError([] {
        intLog2(12);
    });
}

void TestHorowitzGuardBehavior() {
    double rampOutput = 0;
    const double delay = horowitz(0, 0.5, 2.0, &rampOutput);
    assert(delay == 0);
    assert(rampOutput == 2.0);

    AssertThrowsRuntimeError([&] {
        horowitz(-1.0, 0.5, 2.0, &rampOutput);
    });
    AssertThrowsRuntimeError([&] {
        horowitz(1.0, 0.5, 0.0, &rampOutput);
    });
    AssertThrowsRuntimeError([&] {
        horowitz(1.0, 0.5, -1.0, &rampOutput);
    });

    const double tr = 2.0;
    const double beta = 0.5;
    const double rampInput = 4.0;
    const double vs = 0.5;
    const double alpha = 1 / rampInput / tr;
    const double expectedDelay = tr * std::sqrt(std::log(vs) * std::log(vs)
        + 2 * alpha * beta * (1 - vs));
    const double expectedRamp = (1 - vs) / expectedDelay;

    rampOutput = 0;
    assert(Near(horowitz(tr, beta, rampInput, &rampOutput), expectedDelay));
    assert(Near(rampOutput, expectedRamp));
}

void TestGateAreaWidthGuards() {
    const Technology tech = MakeTechnology();
    double height = -1;
    double width = -1;

    assert(CalculateGateArea(INV, 1, 0, 0, 1e-6, tech, &height, &width, false) == 0);
    assert(height == 0);
    assert(width == 0);

    AssertThrowsRuntimeError([&] {
        CalculateGateArea(INV, 1, -1e-9, 1e-9, 1e-6, tech, &height, &width, false);
    });
    AssertThrowsRuntimeError([&] {
        CalculateGateArea(INV, 1, 1e-9, -1e-9, 1e-6, tech, &height, &width, false);
    });
}

void TestGateCapacitanceWidthGuards() {
    const Technology tech = MakeTechnology();
    double capInput = -1;
    double capOutput = -1;

    CalculateGateCapacitance(INV, 1, 0, 0, 1e-6, tech, &capInput, &capOutput);
    assert(capInput == 0);
    assert(capOutput == 0);

    AssertThrowsRuntimeError([&] {
        CalculateGateCapacitance(INV, 1, -1e-9, 1e-9, 1e-6, tech, &capInput, &capOutput);
    });
    AssertThrowsRuntimeError([&] {
        CalculateGateCapacitance(INV, 1, 1e-9, -1e-9, 1e-6, tech, &capInput, &capOutput);
    });
}

void TestFinfetUnavailableFinsGuard() {
    const Technology tech = MakeTechnology(22, true);
    double height = 0;
    double width = 0;

    AssertThrowsRuntimeError([&] {
        CalculateGateArea(INV, 1, 1e-9, 1e-9, 1e-12, tech, &height, &width, true);
    });
}

void TestNominalGateCalculations() {
    const Technology tech = MakeTechnology();
    const double widthNMOS = MIN_NMOS_SIZE * tech.featureSize();
    const double widthPMOS = widthNMOS * tech.pnSizeRatio();
    const double heightTransistorRegion = MAX_TRANSISTOR_HEIGHT * tech.featureSize();
    double height = 0;
    double width = 0;
    double capInput = 0;
    double capOutput = 0;

    const double area = CalculateGateArea(
            INV, 1, widthNMOS, widthPMOS, heightTransistorRegion,
            tech, &height, &width, false);
    CalculateGateCapacitance(
            INV, 1, widthNMOS, widthPMOS, heightTransistorRegion,
            tech, &capInput, &capOutput);

    assert(area > 0);
    assert(height > 0);
    assert(width > 0);
    assert(capInput > 0);
    assert(capOutput > 0);
    assert(Near(area, height * width));
}

void TestDetailedLegacyGateGeometry() {
    const Technology planarTech = MakeTechnology();
    const double planarHeight = MAX_TRANSISTOR_HEIGHT * planarTech.featureSize();
    const double planarNmos = 40 * planarTech.featureSize();
    const double planarPmos = 80 * planarTech.featureSize();

    for (int gateType : {INV, NOR, NAND}) {
        double height = 0;
        double width = 0;
        double capInput = 0;
        double capOutput = 0;
        double expectedCapInput = 0;
        double expectedCapOutput = 0;

        const double area = CalculateGateArea(
                gateType, 3, planarNmos, planarPmos, planarHeight,
                planarTech, &height, &width, false);
        CalculateGateCapacitance(
                gateType, 3, planarNmos, planarPmos, planarHeight,
                planarTech, &capInput, &capOutput);
        ExpectedGateCapacitance(
                gateType, 3, planarNmos, planarPmos, planarHeight,
                planarTech, &expectedCapInput, &expectedCapOutput);

        assert(Near(area, ExpectedGateArea(
                gateType, 3, planarNmos, planarPmos, planarHeight,
                planarTech, false)));
        assert(Near(area, height * width));
        assert(Near(capInput, expectedCapInput));
        assert(Near(capOutput, expectedCapOutput));
    }

    const Technology finfetTech = MakeTechnology(22, true);
    const double finfetHeight = MAX_TRANSISTOR_HEIGHT * finfetTech.featureSize();
    const double finfetNmos = 10 * finfetTech.featureSize();
    const double finfetPmos = 20 * finfetTech.featureSize();
    double height = 0;
    double width = 0;
    const double area = CalculateGateArea(
            NAND, 2, finfetNmos, finfetPmos, finfetHeight,
            finfetTech, &height, &width, true);

    assert(Near(area, ExpectedGateArea(
            NAND, 2, finfetNmos, finfetPmos, finfetHeight,
            finfetTech, true)));
    assert(Near(area, height * width));
}

}  // namespace

int main() {
    TestIntegerLogHelpers();
    TestHorowitzGuardBehavior();
    TestGateAreaWidthGuards();
    TestGateCapacitanceWidthGuards();
    TestFinfetUnavailableFinsGuard();
    TestNominalGateCalculations();
    TestDetailedLegacyGateGeometry();

    std::cout << "Formula tests passed" << std::endl;
    return 0;
}
