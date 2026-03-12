#include "../include/Technology.h"
#include "../include/TechnologyTables.h"

#include <iostream>
#include <math.h>
#include <stdexcept>

void Technology::Initialize(int _featureSizeInNano, DeviceRoadmap _deviceRoadmap, bool _UseUpdatedLib) {
    featureSizeInNano = _featureSizeInNano;
    featureSize = _featureSizeInNano * 1e-9;
    deviceRoadmap = _deviceRoadmap;
    UseUpdatedLib = _UseUpdatedLib;

    if (const TechnologySpec *spec = FindTechnologySpec(_featureSizeInNano, _deviceRoadmap, _UseUpdatedLib)) {
        ApplySpec(*spec);
        ExpandTemperatureTables(*spec);
        initialized = true;
        return;
    }

    throw std::runtime_error("[Technology] Unsupported technology configuration.");
}

void Technology::ApplySpec(const TechnologySpec &spec) {
    vdd = spec.vdd;
    vth = spec.vth;
    vdsatNmos = spec.vdsatNmos;
    vdsatPmos = spec.vdsatPmos;
    phyGateLength = spec.phyGateLength;
    capIdealGate = spec.capIdealGate;
    capFringe = spec.capFringe;
    capJunction = spec.capJunction;
    capOverlap = spec.capOverlap;
    capSidewall = spec.capSidewall;
    capDrainToChannel = spec.capDrainToChannel;
    capOx = spec.capOx;
    buildInPotential = spec.buildInPotential;
    effectiveElectronMobility = spec.effectiveElectronMobility;
    effectiveHoleMobility = spec.effectiveHoleMobility;
    pnSizeRatio = spec.pnSizeRatio;
    effectiveResistanceMultiplier = spec.effectiveResistanceMultiplier;
    currentGmNmos = spec.currentGmNmos;
    currentGmPmos = spec.currentGmPmos;
    heightFin = spec.heightFin;
    widthFin = spec.widthFin;
    PitchFin = spec.PitchFin;
    capPolywire = spec.capPolywire;
}

std::array<double, 11> Technology::CollectTemperatureAnchors(const double (&src)[101]) {
    std::array<double, 11> anchors{};
    for (int i = 0; i <= 10; i++) {
        anchors[i] = src[i * 10];
    }
    return anchors;
}

void Technology::ExpandCurrentTable(const std::array<double, 11> &src, double (&dst)[101]) {
    for (int i = 0; i <= 10; i++) {
        dst[i * 10] = src[i];
    }

    for (int i = 1; i < 100; i++) {
        if (i % 10) {
            const double a = dst[i / 10 * 10];
            const double b = dst[i / 10 * 10 + 10];
            dst[i] = a + (b - a) * (i % 10) / 10.0;
        }
    }
}

void Technology::ExpandTemperatureTables(const TechnologySpec &spec) {
    ExpandCurrentTable(spec.currentOnNmos, currentOnNmos);
    ExpandCurrentTable(spec.currentOnPmos, currentOnPmos);
    ExpandCurrentTable(spec.currentOffNmos, currentOffNmos);
    ExpandCurrentTable(spec.currentOffPmos, currentOffPmos);
}

void Technology::PrintProperty() {
    std::cout << "Fabrication Process Technology Node:" << std::endl;
    std::cout << " - Node = " << featureSizeInNano << "nm" << std::endl;
    std::cout << " - Feature Size = " << featureSize << "m" << std::endl;
    std::cout << " - Device Roadmap = "
              << (deviceRoadmap == HP ? "HP" : (deviceRoadmap == LP ? "LP" : "LSTP"))
              << std::endl;
    std::cout << " - Using Updated Library = " << (UseUpdatedLib ? "true" : "false") << std::endl;
    std::cout << " - Vdd = " << vdd << "V" << std::endl;
    std::cout << " - Vth = " << vth << "V" << std::endl;
    std::cout << " - Physical Gate Length = " << phyGateLength << "m" << std::endl;
    std::cout << " - PN Size Ratio = " << pnSizeRatio << std::endl;
    std::cout << " - NMOS Current @300K = " << currentOnNmos[0] << "A/m" << std::endl;
    std::cout << " - PMOS Current @300K = " << currentOnPmos[0] << "A/m" << std::endl;
}

void Technology::InterpolateWith(const std::shared_ptr<Technology>& rhs, double _alpha) {
    if (featureSizeInNano != rhs->featureSizeInNano) {
        vdd = (1 - _alpha) * vdd + _alpha * rhs->vdd;
        vth = (1 - _alpha) * vth + _alpha * rhs->vth;
        phyGateLength = (1 - _alpha) * phyGateLength + _alpha * rhs->phyGateLength;
        capIdealGate = (1 - _alpha) * capIdealGate + _alpha * rhs->capIdealGate;
        capFringe = (1 - _alpha) * capFringe + _alpha * rhs->capFringe;
        capJunction = (1 - _alpha) * capJunction + _alpha * rhs->capJunction;
        capOx = (1 - _alpha) * capOx + _alpha * rhs->capOx;
        effectiveElectronMobility = (1 - _alpha) * effectiveElectronMobility + _alpha * rhs->effectiveElectronMobility;
        effectiveHoleMobility = (1 - _alpha) * effectiveHoleMobility + _alpha * rhs->effectiveHoleMobility;
        pnSizeRatio = (1 - _alpha) * pnSizeRatio + _alpha * rhs->pnSizeRatio;
        effectiveResistanceMultiplier = (1 - _alpha) * effectiveResistanceMultiplier + _alpha * rhs->effectiveResistanceMultiplier;
        for (int i = 0; i <= 100; i++){
            currentOnNmos[i] = (1 - _alpha) * currentOnNmos[i] + _alpha * rhs->currentOnNmos[i];
            currentOnPmos[i] = (1 - _alpha) * currentOnPmos[i] + _alpha * rhs->currentOnPmos[i];
            currentOffNmos[i] = pow(currentOffNmos[i], 1 - _alpha) * pow(rhs->currentOffNmos[i], _alpha);
            currentOffPmos[i] = pow(currentOffPmos[i], 1 - _alpha) * pow(rhs->currentOffPmos[i], _alpha);
        }
        //capSidewall = 2.5e-10;	/* Unit: F/m, this value is from CACTI, PTM model shows the value is 5e-10 */
        double cjd = 1e-3;             /* Bottom junction capacitance, Unit: F/m^2*/
        double cjswd = 2.5e-10;           /* Isolation-edge sidewall junction capacitance, Unit: F/m */
        double cjswgd = 0.5e-10;          /* Gate-edge sidewall junction capacitance, Unit: F/m */
        double mjd = 0.5;             /* Bottom junction capacitance grating coefficient */
        double mjswd = 0.33;           /* Isolation-edge sidewall junction capacitance grading coefficient */
        double mjswgd = 0.33;          /* Gate-edge sidewall junction capacitance grading coefficient */
        buildInPotential = 0.9;			/* This value is from BSIM4 */
        capJunction = cjd / pow(1 + vdd / buildInPotential, mjd);
        capSidewall = cjswd / pow(1 + vdd / buildInPotential, mjswd);
        capDrainToChannel = cjswgd / pow(1 + vdd / buildInPotential, mjswgd);

        vdsatNmos = phyGateLength * 1e5 /* Silicon saturatio velocity, Unit: m/s */ / effectiveElectronMobility;
        vdsatPmos = phyGateLength * 1e5 /* Silicon saturatio velocity, Unit: m/s */ / effectiveHoleMobility;
    }
}
