#ifndef TECHNOLOGYSPEC_H_
#define TECHNOLOGYSPEC_H_

#include <array>

#include "typedef.h"

struct TechnologySpec {
    int featureSizeInNano;
    double featureSize;
    DeviceRoadmap roadmap;
    bool useUpdatedLib;

    double vdd;
    double vth;
    double vdsatNmos;
    double vdsatPmos;
    double phyGateLength;
    double capIdealGate;
    double capFringe;
    double capJunction;
    double capOverlap;
    double capSidewall;
    double capDrainToChannel;
    double capOx;
    double buildInPotential;
    double effectiveElectronMobility;
    double effectiveHoleMobility;
    double pnSizeRatio;
    double effectiveResistanceMultiplier;
    double currentGmNmos;
    double currentGmPmos;
    double heightFin;
    double widthFin;
    double PitchFin;
    double capPolywire;

    // Values at 300K, 310K, ..., 400K.
    std::array<double, 11> currentOnNmos;
    std::array<double, 11> currentOnPmos;
    std::array<double, 11> currentOffNmos;
    std::array<double, 11> currentOffPmos;
};

#endif /* TECHNOLOGYSPEC_H_ */
