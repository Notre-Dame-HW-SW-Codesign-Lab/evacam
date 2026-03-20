#include "TechnologyTables.h"

#include <array>
#include <cmath>
#include <limits>

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kBuildInPotential = 0.9;
constexpr double kCapJunctionBottom = 1e-3;
constexpr double kCapJunctionSidewall = 2.5e-10;
constexpr double kCapDrainToChannelEdge = 0.5e-10;
constexpr double kCapJunctionGrade = 0.5;
constexpr double kCapSidewallGrade = 0.33;
constexpr double kCapDrainToChannelGrade = 0.33;
constexpr double kSiliconSaturationVelocity = 1e5;

double ComputeJunctionCap(double vdd) {
    return kCapJunctionBottom / std::pow(1 + vdd / kBuildInPotential, kCapJunctionGrade);
}

double ComputeSidewallCap(double vdd) {
    return kCapJunctionSidewall / std::pow(1 + vdd / kBuildInPotential, kCapSidewallGrade);
}

double ComputeDrainToChannelCap(double vdd) {
    return kCapDrainToChannelEdge / std::pow(1 + vdd / kBuildInPotential, kCapDrainToChannelGrade);
}

double ComputeVdsat(double phyGateLength, double mobility) {
    if (mobility == 0.0) {
        return kInf;
    }
    return phyGateLength * kSiliconSaturationVelocity / mobility;
}

struct TechnologyInputs {
    struct OperatingPoint {
        double vdd;
        double vth;
        double phyGateLength;
    };

    struct CapacitanceInputs {
        double capIdealGate;
        double capFringe;
        double capOx;
    };

    struct MobilityInputs {
        double effectiveElectronMobility;
        double effectiveHoleMobility;
    };

    struct SizingInputs {
        double pnSizeRatio;
        double effectiveResistanceMultiplier;
    };

    struct GmInputs {
        double currentGmNmos;
        double currentGmPmos;
    };

    struct FinInputs {
        double heightFin;
        double widthFin;
        double pitchFin;
        double capPolywire;
    };

    struct CurrentTables {
        std::array<double, 11> currentOnNmos;
        std::array<double, 11> currentOnPmos;
        std::array<double, 11> currentOffNmos;
        std::array<double, 11> currentOffPmos;
    };

    int featureSizeInNano;
    DeviceRoadmap roadmap;
    bool useUpdatedLib;

    OperatingPoint operatingPoint;
    CapacitanceInputs capacitance;
    MobilityInputs mobility;
    SizingInputs sizing;
    GmInputs gm;
    FinInputs fin;
    CurrentTables currents;
};

using OperatingPoint = TechnologyInputs::OperatingPoint;
using CapacitanceInputs = TechnologyInputs::CapacitanceInputs;
using MobilityInputs = TechnologyInputs::MobilityInputs;
using SizingInputs = TechnologyInputs::SizingInputs;
using GmInputs = TechnologyInputs::GmInputs;
using FinInputs = TechnologyInputs::FinInputs;
using CurrentTables = TechnologyInputs::CurrentTables;

struct VddValue { double value; };
struct VthValue { double value; };
struct GateLengthValue { double value; };
struct IdealGateCapValue { double value; };
struct FringeCapValue { double value; };
struct OxCapValue { double value; };
struct ElectronMobilityValue { double value; };
struct HoleMobilityValue { double value; };
struct PnSizeRatioValue { double value; };
struct ResistanceMultiplierValue { double value; };
struct GmNmosValue { double value; };
struct GmPmosValue { double value; };
struct FinHeightValue { double value; };
struct FinWidthValue { double value; };
struct FinPitchValue { double value; };
struct PolywireCapValue { double value; };
struct OnNmosTable { std::array<double, 11> value; };
struct OnPmosTable { std::array<double, 11> value; };
struct OffNmosTable { std::array<double, 11> value; };
struct OffPmosTable { std::array<double, 11> value; };

VddValue Vdd(double value) { return {value}; }
VthValue Vth(double value) { return {value}; }
GateLengthValue GateLength(double value) { return {value}; }
IdealGateCapValue IdealGateCap(double value) { return {value}; }
FringeCapValue FringeCap(double value) { return {value}; }
OxCapValue OxCap(double value) { return {value}; }
ElectronMobilityValue ElectronMobility(double value) { return {value}; }
HoleMobilityValue HoleMobility(double value) { return {value}; }
PnSizeRatioValue PnSizeRatio(double value) { return {value}; }
ResistanceMultiplierValue ResistanceMultiplier(double value) { return {value}; }
GmNmosValue GmNmos(double value) { return {value}; }
GmPmosValue GmPmos(double value) { return {value}; }
FinHeightValue FinHeight(double value) { return {value}; }
FinWidthValue FinWidth(double value) { return {value}; }
FinPitchValue FinPitch(double value) { return {value}; }
PolywireCapValue PolywireCap(double value) { return {value}; }
OnNmosTable OnNmos(std::array<double, 11> value) { return {value}; }
OnPmosTable OnPmos(std::array<double, 11> value) { return {value}; }
OffNmosTable OffNmos(std::array<double, 11> value) { return {value}; }
OffPmosTable OffPmos(std::array<double, 11> value) { return {value}; }

OperatingPoint Op(VddValue vdd, VthValue vth, GateLengthValue phyGateLength) {
    return {vdd.value, vth.value, phyGateLength.value};
}

CapacitanceInputs Caps(IdealGateCapValue capIdealGate, FringeCapValue capFringe, OxCapValue capOx) {
    return {capIdealGate.value, capFringe.value, capOx.value};
}

MobilityInputs Mobility(ElectronMobilityValue effectiveElectronMobility, HoleMobilityValue effectiveHoleMobility) {
    return {effectiveElectronMobility.value, effectiveHoleMobility.value};
}

SizingInputs Sizing(PnSizeRatioValue pnSizeRatio, ResistanceMultiplierValue effectiveResistanceMultiplier) {
    return {pnSizeRatio.value, effectiveResistanceMultiplier.value};
}

GmInputs Gm(GmNmosValue currentGmNmos, GmPmosValue currentGmPmos) {
    return {currentGmNmos.value, currentGmPmos.value};
}

FinInputs Fin(FinHeightValue heightFin, FinWidthValue widthFin, FinPitchValue pitchFin, PolywireCapValue capPolywire) {
    return {heightFin.value, widthFin.value, pitchFin.value, capPolywire.value};
}

CurrentTables Tables(
        OnNmosTable currentOnNmos,
        OnPmosTable currentOnPmos,
        OffNmosTable currentOffNmos,
        OffPmosTable currentOffPmos) {
    return {currentOnNmos.value, currentOnPmos.value, currentOffNmos.value, currentOffPmos.value};
}

TechnologySpec BuildTechnologySpecFromInputs(const TechnologyInputs &inputs) {
    TechnologySpec spec{};
    spec.featureSizeInNano = inputs.featureSizeInNano;
    spec.featureSize = inputs.featureSizeInNano * 1e-9;
    spec.roadmap = inputs.roadmap;
    spec.useUpdatedLib = inputs.useUpdatedLib;
    spec.vdd = inputs.operatingPoint.vdd;
    spec.vth = inputs.operatingPoint.vth;
    spec.vdsatNmos = ComputeVdsat(inputs.operatingPoint.phyGateLength, inputs.mobility.effectiveElectronMobility);
    spec.vdsatPmos = ComputeVdsat(inputs.operatingPoint.phyGateLength, inputs.mobility.effectiveHoleMobility);
    spec.phyGateLength = inputs.operatingPoint.phyGateLength;
    spec.capIdealGate = inputs.capacitance.capIdealGate;
    spec.capFringe = inputs.capacitance.capFringe;
    spec.capJunction = ComputeJunctionCap(inputs.operatingPoint.vdd);
    spec.capOverlap = inputs.capacitance.capIdealGate * 0.2;
    spec.capSidewall = ComputeSidewallCap(inputs.operatingPoint.vdd);
    spec.capDrainToChannel = ComputeDrainToChannelCap(inputs.operatingPoint.vdd);
    spec.capOx = inputs.capacitance.capOx;
    spec.buildInPotential = kBuildInPotential;
    spec.effectiveElectronMobility = inputs.mobility.effectiveElectronMobility;
    spec.effectiveHoleMobility = inputs.mobility.effectiveHoleMobility;
    spec.pnSizeRatio = inputs.sizing.pnSizeRatio;
    spec.effectiveResistanceMultiplier = inputs.sizing.effectiveResistanceMultiplier;
    spec.currentGmNmos = inputs.gm.currentGmNmos;
    spec.currentGmPmos = inputs.gm.currentGmPmos;
    spec.heightFin = inputs.fin.heightFin;
    spec.widthFin = inputs.fin.widthFin;
    spec.PitchFin = inputs.fin.pitchFin;
    spec.capPolywire = inputs.fin.capPolywire;
    spec.currentOnNmos = inputs.currents.currentOnNmos;
    spec.currentOnPmos = inputs.currents.currentOnPmos;
    spec.currentOffNmos = inputs.currents.currentOffNmos;
    spec.currentOffPmos = inputs.currents.currentOffPmos;
    return spec;
}

TechnologySpec Spec(
        int featureSizeInNano,
        DeviceRoadmap roadmap,
        bool useUpdatedLib,
        OperatingPoint operatingPoint,
        CapacitanceInputs capacitance,
        MobilityInputs mobility,
        SizingInputs sizing,
        GmInputs gm,
        FinInputs fin,
        CurrentTables currents) {
    return BuildTechnologySpecFromInputs({
            featureSizeInNano,
            roadmap,
            useUpdatedLib,
            operatingPoint,
            capacitance,
            mobility,
            sizing,
            gm,
            fin,
            currents});
}

int BucketNode(int featureSizeInNano) {
    if (featureSizeInNano >= 200) {
        return 200;
    }
    if (featureSizeInNano >= 120) {
        return 120;
    }
    if (featureSizeInNano >= 90) {
        return 90;
    }
    if (featureSizeInNano >= 65) {
        return 65;
    }
    if (featureSizeInNano >= 45) {
        return 45;
    }
    if (featureSizeInNano >= 32) {
        return 32;
    }
    if (featureSizeInNano >= 22) {
        return 22;
    }
    return -1;
}

const TechnologySpec *FindExactTechnologySpec(
        int featureSizeInNano,
        DeviceRoadmap roadmap,
        bool useUpdatedLib) {
    // These are authored technology inputs. Derived fields such as vdsat,
    // junction capacitances, overlap capacitance, and featureSize are
    // computed inside BuildTechnologySpecFromInputs().
    static const std::array<TechnologySpec, 8> kUpdatedLibrarySpecs{{
        #include "TechnologyTableUpdated.inc"
    }};

    static const std::array<TechnologySpec, 2> kLegacyFefetSpecs{{
        #include "TechnologyTableLegacyFefet.inc"
    }};

    static const std::array<TechnologySpec, 14> kLegacyPlanarSpecs{{
        #include "TechnologyTableLegacyPlanar.inc"
    }};

    const auto findIn = [&](const auto &specs) -> const TechnologySpec * {
        for (const auto &spec : specs) {
            if (spec.featureSizeInNano == featureSizeInNano
                    && spec.roadmap == roadmap
                    && spec.useUpdatedLib == useUpdatedLib) {
                return &spec;
            }
        }
        return nullptr;
    };

    if (const TechnologySpec *spec = findIn(kUpdatedLibrarySpecs)) {
        return spec;
    }
    if (const TechnologySpec *spec = findIn(kLegacyFefetSpecs)) {
        return spec;
    }
    if (const TechnologySpec *spec = findIn(kLegacyPlanarSpecs)) {
        return spec;
    }

    return nullptr;
}

}  // namespace

const TechnologySpec *FindTechnologySpec(
        int featureSizeInNano,
        DeviceRoadmap roadmap,
        bool useUpdatedLib) {
    if (const TechnologySpec *spec = FindExactTechnologySpec(featureSizeInNano, roadmap, useUpdatedLib)) {
        return spec;
    }

    if (useUpdatedLib) {
        return nullptr;
    }

    const int bucketNode = BucketNode(featureSizeInNano);
    if (bucketNode == -1) {
        return nullptr;
    }

    return FindExactTechnologySpec(bucketNode, roadmap, useUpdatedLib);
}
