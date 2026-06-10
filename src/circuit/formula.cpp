#include "formula.h"
#include <stdexcept>

namespace {

struct GateLayout {
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

void CheckTransistorWidths(double widthNMOS, double widthPMOS) {
    if (widthNMOS < 0 || widthPMOS < 0) {
        throw std::runtime_error("Error: transistor width cannot be negative.");
    }
}

double CalculatePmosRatio(double widthNMOS, double widthPMOS) {
    return widthPMOS / (widthPMOS + widthNMOS);
}

void CalculateMaxPlanarWidths(
        double ratio, double heightTransistorRegion, const Technology &tech,
        double *maxWidthPMOS, double *maxWidthNMOS) {
    if (ratio == 0) { // no PMOS
        *maxWidthPMOS = 0;
        *maxWidthNMOS = heightTransistorRegion;
    } else if (ratio == 1) { // no NMOS
        *maxWidthPMOS = heightTransistorRegion;
        *maxWidthNMOS = 0;
    } else {
        *maxWidthPMOS = ratio * (heightTransistorRegion - MIN_GAP_BET_P_AND_N_DIFFS * tech.featureSize());
        *maxWidthNMOS = *maxWidthPMOS / ratio * (1 - ratio);
    }
}

void ApplyPlanarPmosLayout(double widthPMOS, double maxWidthPMOS, const Technology &tech, GateLayout *layout) {
    if (widthPMOS > 0) {
        if (widthPMOS < maxWidthPMOS) { // No folding
            layout->unitWidthRegionP = tech.featureSize();
            layout->heightRegionP = widthPMOS;
            layout->unitWidthDrainP = 0;
            layout->heightDrainP = widthPMOS;
        } else { // Folding
            layout->numFoldedPMOS = (int)(ceil(widthPMOS / (maxWidthPMOS - 3 * tech.featureSize())));
            layout->unitWidthRegionP = layout->numFoldedPMOS * tech.featureSize()
                + (layout->numFoldedPMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout->heightRegionP = maxWidthPMOS;
            layout->unitWidthDrainP = (layout->numFoldedPMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout->heightDrainP = maxWidthPMOS;
        }
    } else {
        layout->unitWidthRegionP = 0;
        layout->heightRegionP = 0;
        layout->unitWidthDrainP = 0;
        layout->heightDrainP = 0;
    }
}

void ApplyPlanarNmosLayout(double widthNMOS, double maxWidthNMOS, const Technology &tech, GateLayout *layout) {
    if (widthNMOS > 0) {
        if (widthNMOS < maxWidthNMOS) { // No folding
            layout->unitWidthRegionN = tech.featureSize();
            layout->heightRegionN = widthNMOS;
            layout->unitWidthDrainN = 0;
            layout->heightDrainN = widthNMOS;
        } else { // Folding
            layout->numFoldedNMOS = (int)(ceil(widthNMOS / (maxWidthNMOS - 3 * tech.featureSize())));
            layout->unitWidthRegionN = layout->numFoldedNMOS * tech.featureSize()
                + (layout->numFoldedNMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout->heightRegionN = maxWidthNMOS;
            layout->unitWidthDrainN = (layout->numFoldedNMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout->heightDrainN = maxWidthNMOS;
        }
    } else {
        layout->unitWidthRegionN = 0;
        layout->heightRegionN = 0;
        layout->unitWidthDrainN = 0;
        layout->heightDrainN = 0;
    }
}

void ApplyFinfetRegionLayout(
        double ratio, double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech, GateLayout *layout) {
    // Technology node is changed from 120-22nm to 45-7nm, and FinFET is
    // introduced when technology node is beyond 32nm. Modified according
    // to "Scaling 2-Layer RRAM Cross-Point Array towards 10 nm Node:
    // a Device-Circuit Co-Design, ISCAS 2015".
    int maxNumPFin, maxNumNFin;
    if (ratio == 0) { // no PFinFET
        maxNumPFin = 0;
        maxNumNFin = (int)(floor(heightTransistorRegion / tech.pitchFin()));
    } else if (ratio == 1) { // no NFinFET
        maxNumPFin = (int)(floor(heightTransistorRegion / tech.pitchFin()));
        maxNumNFin = 0;
    } else {
        maxNumPFin = (int)(floor(
                ratio * (heightTransistorRegion - MIN_GAP_BET_P_AND_N_DIFFS * tech.featureSize())
                / tech.pitchFin()));
        maxNumNFin = (int)(floor(maxNumPFin / ratio * (1 - ratio)));
    }

    int NumPFin = (int)(ceil(widthPMOS / (2 * tech.heightFin() + tech.widthFin())));
    if (NumPFin > 0) {
        if (maxNumPFin <= 0) {
            throw std::runtime_error("Error: PMOS FinFET folding has no available fins.");
        }
        if (NumPFin < maxNumPFin) { // No folding
            layout->unitWidthRegionP = tech.featureSize();
            layout->heightRegionP = (NumPFin - 1) * tech.pitchFin() + 2 * tech.widthFin();
        } else { // Folding
            int numFoldedPFin = (int)(ceil(NumPFin / maxNumPFin));
            layout->unitWidthRegionP = numFoldedPFin * tech.featureSize()
                + (numFoldedPFin - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout->heightRegionP = (maxNumPFin - 1) * tech.pitchFin() + 2 * tech.widthFin();
        }
    } else {
        layout->unitWidthRegionP = 0;
        layout->heightRegionP = 0;
    }

    int NumNFin = (int)(ceil(widthNMOS / (2 * tech.heightFin() + tech.widthFin())));
    if (NumNFin > 0) {
        if (maxNumNFin <= 0) {
            throw std::runtime_error("Error: NMOS FinFET folding has no available fins.");
        }
        if (NumNFin < maxNumNFin) { // No folding
            layout->unitWidthRegionN = tech.featureSize();
            layout->heightRegionN = (NumNFin - 1) * tech.pitchFin() + 2 * tech.widthFin();
        } else { // Folding
            int numFoldedNMOS = (int)(ceil(NumNFin / maxNumNFin));
            layout->unitWidthRegionN = numFoldedNMOS * tech.featureSize()
                + (numFoldedNMOS - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            layout->heightRegionN = (maxNumNFin - 1) * tech.pitchFin() + 2 * tech.widthFin();
        }
    } else {
        layout->unitWidthRegionN = 0;
        layout->heightRegionN = 0;
    }
}

GateLayout CalculateGateLayout(
        double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech,
        bool useUpdatedRegionLayout) {
    const double ratio = CalculatePmosRatio(widthNMOS, widthPMOS);
    double maxWidthPMOS, maxWidthNMOS;
    CalculateMaxPlanarWidths(ratio, heightTransistorRegion, tech, &maxWidthPMOS, &maxWidthNMOS);

    GateLayout layout;
    ApplyPlanarPmosLayout(widthPMOS, maxWidthPMOS, tech, &layout);
    ApplyPlanarNmosLayout(widthNMOS, maxWidthNMOS, tech, &layout);

    if (useUpdatedRegionLayout && tech.featureSize() <= 22 * 1e-9) {
        ApplyFinfetRegionLayout(ratio, widthNMOS, widthPMOS, heightTransistorRegion, tech, &layout);
    }

    return layout;
}

}  // namespace

bool isPow2(int n) {
    if (n < 1) {
        return false;
    }
    return !(n & (n - 1));
}

int intLog2(int n) {
    if (!isPow2(n)) {
        throw std::runtime_error("[ERROR] Must be a power of 2.");
    }

    return __builtin_ctz(n);
}

double CalculateGateCap(double width, const Technology &tech) {
    return (tech.capIdealGate() + tech.capOverlap() + 3 * tech.capFringe()) * width
        + tech.phyGateLength() * tech.capPolywire();
}

double CalculateFBRAMGateCap(double width, double thicknessFactor, const Technology &tech) {
    return (tech.capIdealGate() / thicknessFactor + tech.capOverlap() + 3 * tech.capFringe()) * width
        + tech.phyGateLength() * tech.capPolywire();
}

double CalculateFBRAMDrainCap(double width, const Technology &tech) {
    return (3 * tech.capSidewall() + tech.capDrainToChannel()) * width;
}

double CalculateGateArea(
        int gateType, int numInput,
        double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech,
        double *height, double *width, bool UseUpdatedLib) {
    CheckTransistorWidths(widthNMOS, widthPMOS);
    if (widthNMOS == 0 && widthPMOS == 0) {
        *height = 0;
        *width = 0;
        return 0;
    }

    const GateLayout layout = CalculateGateLayout(
            widthNMOS, widthPMOS, heightTransistorRegion, tech, UseUpdatedLib);
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

    *width = MAX(widthRegionN, widthRegionP);
    if (widthPMOS > 0 && widthNMOS > 0) { // it is a gate
        *height = layout.heightRegionN + layout.heightRegionP + tech.featureSize() * MIN_GAP_BET_P_AND_N_DIFFS
            + 2 * tech.featureSize() * MIN_WIDTH_POWER_RAIL;
    } else { // it is a transistor
        // One of them is zero, and no power rail is added.
        *height = layout.heightRegionN + layout.heightRegionP;
    }

    return (*width) * (*height);
}

void CalculateGateCapacitance(
        int gateType, int numInput,
        double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech,
        double *capInput, double *capOutput) {
    CheckTransistorWidths(widthNMOS, widthPMOS);
    if (widthNMOS == 0 && widthPMOS == 0) {
        if (capInput) {
            *capInput = 0;
        }
        if (capOutput) {
            *capOutput = 0;
        }
        return;
    }

    const GateLayout layout = CalculateGateLayout(
            widthNMOS, widthPMOS, heightTransistorRegion, tech, false);
    double widthDrainP = 0, widthDrainN = 0;

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
            // PMOS is in series, worst case capacitance is below.
            if (widthPMOS > 0) {
                widthDrainP = tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                    + layout.unitWidthDrainP * numInput + (numInput - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            }
            // NMOS is parallel, capacitance is multiplied as below.
            if (widthNMOS > 0) {
                widthDrainN = (tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                        + layout.unitWidthDrainN) * numInput;
            }
            break;
        case NAND:
            // NMOS is in series, worst case capacitance is below.
            if (widthNMOS > 0) {
                widthDrainN = tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                    + layout.unitWidthDrainN * numInput + (numInput - 1) * tech.featureSize() * MIN_GAP_BET_POLY;
            }
            // PMOS is parallel, capacitance is multiplied as below.
            if (widthPMOS > 0) {
                widthDrainP = (tech.featureSize() * (CONTACT_SIZE + MIN_GAP_BET_CONTACT_POLY * 2)
                        + layout.unitWidthDrainP) * numInput;
            }
            break;
        default:
            widthDrainN = widthDrainP = 0;
    }

    // Junction capacitance.
    double capDrainBottomN = widthDrainN * layout.heightDrainN * tech.capJunction();
    double capDrainBottomP = widthDrainP * layout.heightDrainP * tech.capJunction();

    // Sidewall capacitance.
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

    // Drain to channel capacitance.
    double capDrainToChannelN = layout.numFoldedNMOS * layout.heightDrainN * tech.capDrainToChannel();
    double capDrainToChannelP = layout.numFoldedPMOS * layout.heightDrainP * tech.capDrainToChannel();

    if (capOutput) {
        *(capOutput) = capDrainBottomN + capDrainBottomP + capDrainSidewallN + capDrainSidewallP + capDrainToChannelN + capDrainToChannelP;
    }
    if (capInput) {
        *(capInput) = CalculateGateCap(widthNMOS, tech) + CalculateGateCap(widthPMOS, tech);
    }
}

double CalculateDrainCap(
        double width, int type,
        double heightTransistorRegion, const Technology &tech) {
    double drainCap = 0;
    if (type == NMOS) {
        CalculateGateCapacitance(INV, 1, width, 0, heightTransistorRegion, tech, NULL, &drainCap);
    } else {
        CalculateGateCapacitance(INV, 1, 0, width, heightTransistorRegion, tech, NULL, &drainCap);
    }
    return drainCap;
}

double CalculateGateLeakage(
        int gateType, int numInput,
        double widthNMOS, double widthPMOS,
        double temperature, const Technology &tech) {
    int tempIndex = (int)temperature - 300;
    if ((tempIndex > 100) || (tempIndex < 0)) {
        throw std::runtime_error("Error: Temperature is out of range");
    }
    const double *leakN = tech.currentOffNmos().data();
    const double *leakP = tech.currentOffPmos().data();
    double leakageN, leakageP;
    switch (gateType) {
        case INV:
            leakageN = widthNMOS * leakN[tempIndex];
            leakageP = widthPMOS * leakP[tempIndex];
            return MAX(leakageN, leakageP);
        case NOR:
            leakageN = widthNMOS * leakN[tempIndex] * numInput;
            if (numInput == 2) {
                return AVG_RATIO_LEAK_2INPUT_NOR * leakageN;
            } else {
                return AVG_RATIO_LEAK_3INPUT_NOR * leakageN;
            }
        case NAND:
            leakageP = widthPMOS * leakP[tempIndex] * numInput;
            if (numInput == 2) {
                return AVG_RATIO_LEAK_2INPUT_NAND * leakageP;
            } else {
                return AVG_RATIO_LEAK_3INPUT_NAND * leakageP;
            }
        default:
            return 0.0;
    }
}

double CalculateOnResistance(double width, int type, double temperature, const Technology &tech) {
    double r;
    int tempIndex = (int)temperature - 300;
    if ((tempIndex > 100) || (tempIndex < 0)) {
        throw std::runtime_error("Error: Temperature is out of range");
    }
    if (type == NMOS) {
        r = tech.effectiveResistanceMultiplier() * tech.vdd() / (tech.currentOnNmos()[tempIndex] * width);
    } else {
        r = tech.effectiveResistanceMultiplier() * tech.vdd() / (tech.currentOnPmos()[tempIndex] * width);
    }
    return r;
}

double CalculateTransconductance(double width, int type, const Technology &tech) {
    double gm;

    if (tech.useUpdatedLib()) {
        if (type == NMOS) {
            gm = (2 * tech.currentGmNmos()) * width / (0.7 * tech.vdd() - tech.vth());
        } else { // type == PMOS
            gm = (2 * tech.currentGmPmos()) * width / (0.7 * tech.vdd() - tech.vth());
        }
    } else {
        double vsat;
        if (type == NMOS) {
            vsat = MIN(tech.vdsatNmos(), tech.vdd() - tech.vth());
            gm = (tech.effectiveElectronMobility() * tech.capOx()) / 2 * width / tech.phyGateLength() * vsat;
        } else {
            vsat = MIN(tech.vdsatPmos(), tech.vdd() - tech.vth());
            gm = (tech.effectiveHoleMobility() * tech.capOx()) / 2 * width / tech.phyGateLength() * vsat;
        }
    }
    return gm;
}

double horowitz(double tr, double beta, double rampInput, double *rampOutput) {
    if (tr < 0) {
        throw std::runtime_error("Error: horowitz time constant cannot be negative.");
    }
    if (rampInput <= 0) {
        throw std::runtime_error("Error: horowitz input ramp must be positive.");
    }
    if (tr == 0) {
        if (rampOutput) {
            *rampOutput = rampInput;
        }
        return 0;
    }

    double alpha;
    alpha = 1 / rampInput / tr;
    double vs = 0.5; // Normalized switching voltage
    double result = tr * sqrt(log(vs) * log(vs) + 2 * alpha * beta * (1 - vs));
    if (rampOutput) {
        *rampOutput = (1 - vs) / result;
    }
    return result;
}

double CalculateWireResistance(
        double resistivity, double wireWidth, double wireThickness,
        double barrierThickness, double dishingThickness, double alphaScatter) {
    return alphaScatter * resistivity / (wireThickness - barrierThickness - dishingThickness)
        / (wireWidth - 2 * barrierThickness);
}

double CalculateWireCapacitance(
        double permittivity, double wireWidth, double wireThickness, double wireSpacing,
        double ildThickness, double millerValue, double horizontalDielectric,
        double verticalDielectric, double fringeCap) {
    double verticalCap, sidewallCap;
    verticalCap = 2 * permittivity * verticalDielectric * wireWidth / ildThickness;
    sidewallCap = 2 * permittivity * millerValue * horizontalDielectric * wireThickness / wireSpacing;
    return (verticalCap + sidewallCap + fringeCap);
}
