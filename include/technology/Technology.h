#ifndef TECHNOLOGY_H_
#define TECHNOLOGY_H_

#include <array>
#include <memory>
#include "TechnologySpec.h"
#include "typedef.h"


class Technology {
    public:
        Technology() {
            initialized_ = false;
        }
        Technology(const Technology&) = default;
        virtual ~Technology() {}

        /* Functions */
        void PrintProperty();
        void InitializeFromSpec(const TechnologySpec &spec);
        void InterpolateWith(const Technology &rhs, double _alpha);

        bool initialized() const { return initialized_; }
        int featureSizeInNano() const { return featureSizeInNano_; }
        double featureSize() const { return featureSize_; }
        DeviceRoadmap deviceRoadmap() const { return deviceRoadmap_; }
        double vdd() const { return vdd_; }
        double vth() const { return vth_; }
        double vdsatNmos() const { return vdsatNmos_; }
        double vdsatPmos() const { return vdsatPmos_; }
        double phyGateLength() const { return phyGateLength_; }
        double capIdealGate() const { return capIdealGate_; }
        double capFringe() const { return capFringe_; }
        double capJunction() const { return capJunction_; }
        double capOverlap() const { return capOverlap_; }
        double capSidewall() const { return capSidewall_; }
        double capDrainToChannel() const { return capDrainToChannel_; }
        double capOx() const { return capOx_; }
        double buildInPotential() const { return buildInPotential_; }
        double effectiveElectronMobility() const { return effectiveElectronMobility_; }
        double effectiveHoleMobility() const { return effectiveHoleMobility_; }
        double pnSizeRatio() const { return pnSizeRatio_; }
        double effectiveResistanceMultiplier() const { return effectiveResistanceMultiplier_; }
        const std::array<double, 101> &currentOnNmos() const { return currentOnNmos_; }
        const std::array<double, 101> &currentOnPmos() const { return currentOnPmos_; }
        const std::array<double, 101> &currentOffNmos() const { return currentOffNmos_; }
        const std::array<double, 101> &currentOffPmos() const { return currentOffPmos_; }
        double capPolywire() const { return capPolywire_; }
        double currentGmNmos() const { return currentGmNmos_; }
        double currentGmPmos() const { return currentGmPmos_; }
        double heightFin() const { return heightFin_; }
        double widthFin() const { return widthFin_; }
        double pitchFin() const { return pitchFin_; }
        bool useUpdatedLib() const { return useUpdatedLib_; }

    private:
        void ApplySpec(const TechnologySpec &spec);
        std::array<double, 11> CollectTemperatureAnchors(const std::array<double, 101> &src);
        void ExpandCurrentTable(const std::array<double, 11> &src, std::array<double, 101> &dst);
        void ExpandTemperatureTables(const TechnologySpec &spec);

        bool initialized_;                       /* Initialization flag */
        int featureSizeInNano_;                  /*Process feature size, Unit: nm */
        double featureSize_;                     /* Process feature size, Unit: m */
        DeviceRoadmap deviceRoadmap_;            /* HP, LSTP, or LOP */
        double vdd_;                             /* Supply voltage, Unit: V */
        double vth_;                             /* Threshold voltage, Unit: V */
        double vdsatNmos_;                       /* Velocity saturation voltage, Unit: V */
        double vdsatPmos_;                       /* Velocity saturation voltage, Unit: V */
        double phyGateLength_;                   /* Physical gate length, Unit: m */
        double capIdealGate_;                    /* Ideal gate capacitance, Unit: F/m */
        double capFringe_;                       /* Fringe capacitance, Unit: F/m */
        double capJunction_;                     /* Junction bottom capacitance, Cj0, Unit: F/m^2 */
        double capOverlap_;                      /* Overlap capacitance, Cover in MASTAR, Unit: F/m */
        double capSidewall_;                     /* Junction sidewall capacitance, Cjsw, Unit: F/m */
        double capDrainToChannel_;               /* Junction drain to channel capacitance, Cjswg, Unit: F/m */
        double capOx_;                           /* Cox_elec in MASTAR, Unit: F/m^2 */
        double buildInPotential_;                /* Bottom junction built-in potential(PB in BSIM4 model), Unit: V */
        double effectiveElectronMobility_;       /* ueff for NMOS in MASTAR, Unit: m^2/V/s */
        double effectiveHoleMobility_;           /* ueff for PMOS in MASTAR, Unit: m^2/V/s */
        double pnSizeRatio_;                     /* PMOS to NMOS size ratio */
        double effectiveResistanceMultiplier_;   /* Extra resistance due to vdsat */

        std::array<double, 101> currentOnNmos_;  /* NMOS saturation current, Unit: A/m */
        std::array<double, 101> currentOnPmos_;  /* PMOS saturation current, Unit: A/m */
        std::array<double, 101> currentOffNmos_; /* NMOS off current (from 300K to 400K), Unit: A/m */
        std::array<double, 101> currentOffPmos_; /* PMOS off current (from 300K to 400K), Unit: A/m */
        double capPolywire_;                     /* Poly wire capacitance, Unit: F/m */

        double currentGmNmos_;
        double currentGmPmos_;
        double heightFin_;
        double widthFin_;
        double pitchFin_;
        bool useUpdatedLib_;
};

#endif /* TECHNOLOGY_H_ */
