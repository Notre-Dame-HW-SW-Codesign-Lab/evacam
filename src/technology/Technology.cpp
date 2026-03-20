#include "Technology.h"
#include "TechnologyTables.h"

#include <iostream>
#include <math.h>
#include <stdexcept>

namespace {

const char *ToString(DeviceRoadmap roadmap) {
    switch (roadmap) {
        case HP: return "HP";
        case LSTP: return "LSTP";
        case LOP: return "LOP";
        case FEFET: return "FEFET";
        case LP: return "LP";
        default: return "Unknown";
    }
}

}  // namespace

void Technology::Initialize(int _featureSizeInNano, DeviceRoadmap _deviceRoadmap, bool _UseUpdatedLib) {
    featureSizeInNano_ = _featureSizeInNano;
    featureSize_ = _featureSizeInNano * 1e-9;
    deviceRoadmap_ = _deviceRoadmap;
    useUpdatedLib_ = _UseUpdatedLib;

    if (const TechnologySpec *spec = FindTechnologySpec(_featureSizeInNano, _deviceRoadmap, _UseUpdatedLib)) {
        ApplySpec(*spec);
        ExpandTemperatureTables(*spec);
        initialized_ = true;
        return;
    }

    throw std::runtime_error("[Technology] Unsupported technology configuration.");
}

void Technology::ApplySpec(const TechnologySpec &spec) {
    vdd_ = spec.vdd;
    vth_ = spec.vth;
    vdsatNmos_ = spec.vdsatNmos;
    vdsatPmos_ = spec.vdsatPmos;
    phyGateLength_ = spec.phyGateLength;
    capIdealGate_ = spec.capIdealGate;
    capFringe_ = spec.capFringe;
    capJunction_ = spec.capJunction;
    capOverlap_ = spec.capOverlap;
    capSidewall_ = spec.capSidewall;
    capDrainToChannel_ = spec.capDrainToChannel;
    capOx_ = spec.capOx;
    buildInPotential_ = spec.buildInPotential;
    effectiveElectronMobility_ = spec.effectiveElectronMobility;
    effectiveHoleMobility_ = spec.effectiveHoleMobility;
    pnSizeRatio_ = spec.pnSizeRatio;
    effectiveResistanceMultiplier_ = spec.effectiveResistanceMultiplier;
    currentGmNmos_ = spec.currentGmNmos;
    currentGmPmos_ = spec.currentGmPmos;
    heightFin_ = spec.heightFin;
    widthFin_ = spec.widthFin;
    pitchFin_ = spec.PitchFin;
    capPolywire_ = spec.capPolywire;
}

std::array<double, 11> Technology::CollectTemperatureAnchors(const std::array<double, 101> &src) {
    std::array<double, 11> anchors{};
    for (int i = 0; i <= 10; i++) {
        anchors[i] = src[i * 10];
    }
    return anchors;
}

void Technology::ExpandCurrentTable(const std::array<double, 11> &src, std::array<double, 101> &dst) {
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
    ExpandCurrentTable(spec.currentOnNmos, currentOnNmos_);
    ExpandCurrentTable(spec.currentOnPmos, currentOnPmos_);
    ExpandCurrentTable(spec.currentOffNmos, currentOffNmos_);
    ExpandCurrentTable(spec.currentOffPmos, currentOffPmos_);
}

void Technology::PrintProperty() {
    std::cout << "Fabrication Process Technology Node:" << std::endl;
    std::cout << " - Node = " << featureSizeInNano_ << "nm" << std::endl;
    std::cout << " - Feature Size = " << featureSize_ << "m" << std::endl;
    std::cout << " - Device Roadmap = " << ToString(deviceRoadmap_) << std::endl;
    std::cout << " - Using Updated Library = " << (useUpdatedLib_ ? "true" : "false") << std::endl;
    std::cout << " - Vdd = " << vdd_ << "V" << std::endl;
    std::cout << " - Vth = " << vth_ << "V" << std::endl;
    std::cout << " - Physical Gate Length = " << phyGateLength_ << "m" << std::endl;
    std::cout << " - PN Size Ratio = " << pnSizeRatio_ << std::endl;
    std::cout << " - NMOS Current @300K = " << currentOnNmos_[0] << "A/m" << std::endl;
    std::cout << " - PMOS Current @300K = " << currentOnPmos_[0] << "A/m" << std::endl;
}

void Technology::InterpolateWith(const std::shared_ptr<Technology>& rhs, double _alpha) {
    if (featureSizeInNano_ != rhs->featureSizeInNano_) {
        vdd_ = (1 - _alpha) * vdd_ + _alpha * rhs->vdd_;
        vth_ = (1 - _alpha) * vth_ + _alpha * rhs->vth_;
        phyGateLength_ = (1 - _alpha) * phyGateLength_ + _alpha * rhs->phyGateLength_;
        capIdealGate_ = (1 - _alpha) * capIdealGate_ + _alpha * rhs->capIdealGate_;
        capFringe_ = (1 - _alpha) * capFringe_ + _alpha * rhs->capFringe_;
        capJunction_ = (1 - _alpha) * capJunction_ + _alpha * rhs->capJunction_;
        capOx_ = (1 - _alpha) * capOx_ + _alpha * rhs->capOx_;
        effectiveElectronMobility_ = (1 - _alpha) * effectiveElectronMobility_ + _alpha * rhs->effectiveElectronMobility_;
        effectiveHoleMobility_ = (1 - _alpha) * effectiveHoleMobility_ + _alpha * rhs->effectiveHoleMobility_;
        pnSizeRatio_ = (1 - _alpha) * pnSizeRatio_ + _alpha * rhs->pnSizeRatio_;
        effectiveResistanceMultiplier_ = (1 - _alpha) * effectiveResistanceMultiplier_ + _alpha * rhs->effectiveResistanceMultiplier_;
        for (int i = 0; i <= 100; i++){
            currentOnNmos_[i] = (1 - _alpha) * currentOnNmos_[i] + _alpha * rhs->currentOnNmos_[i];
            currentOnPmos_[i] = (1 - _alpha) * currentOnPmos_[i] + _alpha * rhs->currentOnPmos_[i];
            currentOffNmos_[i] = pow(currentOffNmos_[i], 1 - _alpha) * pow(rhs->currentOffNmos_[i], _alpha);
            currentOffPmos_[i] = pow(currentOffPmos_[i], 1 - _alpha) * pow(rhs->currentOffPmos_[i], _alpha);
        }
        //capSidewall = 2.5e-10;	/* Unit: F/m, this value is from CACTI, PTM model shows the value is 5e-10 */
        double cjd = 1e-3;             /* Bottom junction capacitance, Unit: F/m^2*/
        double cjswd = 2.5e-10;           /* Isolation-edge sidewall junction capacitance, Unit: F/m */
        double cjswgd = 0.5e-10;          /* Gate-edge sidewall junction capacitance, Unit: F/m */
        double mjd = 0.5;             /* Bottom junction capacitance grating coefficient */
        double mjswd = 0.33;           /* Isolation-edge sidewall junction capacitance grading coefficient */
        double mjswgd = 0.33;          /* Gate-edge sidewall junction capacitance grading coefficient */
        buildInPotential_ = 0.9;			/* This value is from BSIM4 */
        capJunction_ = cjd / pow(1 + vdd_ / buildInPotential_, mjd);
        capSidewall_ = cjswd / pow(1 + vdd_ / buildInPotential_, mjswd);
        capDrainToChannel_ = cjswgd / pow(1 + vdd_ / buildInPotential_, mjswgd);

        vdsatNmos_ = phyGateLength_ * 1e5 /* Silicon saturatio velocity, Unit: m/s */ / effectiveElectronMobility_;
        vdsatPmos_ = phyGateLength_ * 1e5 /* Silicon saturatio velocity, Unit: m/s */ / effectiveHoleMobility_;
    }
}
