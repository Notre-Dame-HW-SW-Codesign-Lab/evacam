#include "model/FefetGateModel.h"

#include <cmath>
#include <initializer_list>
#include <stdexcept>

FefetGateResponse EvaluateFefetGate(double resistanceOn, double resistanceOff,
        double capacitance, double supplyVoltage, double switchingVoltage) {
    for (double value : {resistanceOn, resistanceOff, capacitance, supplyVoltage, switchingVoltage}) {
        if (!std::isfinite(value) || value <= 0) {
            throw std::invalid_argument("[FeFET gate] Parameters must be finite and positive.");
        }
    }
    if (resistanceOff <= resistanceOn) {
        throw std::invalid_argument("[FeFET gate] Off resistance must exceed on resistance.");
    }
    const double ratio = resistanceOn / resistanceOff;
    FefetGateResponse response;
    response.matchVoltage = supplyVoltage * ratio / (1 + ratio);
    response.mismatchVoltage = supplyVoltage / (1 + ratio);
    if (switchingVoltage <= response.matchVoltage || switchingVoltage >= response.mismatchVoltage) {
        throw std::invalid_argument("[FeFET gate] Switching voltage must separate match and mismatch levels.");
    }
    response.timeConstant = resistanceOn / (1 + ratio) * capacitance;
    response.delay = -response.timeConstant * std::log1p(-switchingVoltage / response.mismatchVoltage);
    response.ramp = (response.mismatchVoltage - switchingVoltage)
            / response.timeConstant / supplyVoltage;
    // Supply work during full settling, above the steady divider dissipation.
    response.chargingEnergy = capacitance * response.mismatchVoltage * response.mismatchVoltage;
    response.staticPower = supplyVoltage * supplyVoltage / resistanceOff / (1 + ratio);
    if (!std::isfinite(response.delay) || response.delay <= 0
            || !std::isfinite(response.ramp) || response.ramp <= 0
            || !std::isfinite(response.chargingEnergy) || !std::isfinite(response.staticPower)) {
        throw std::invalid_argument("[FeFET gate] Derived response is outside the finite physical domain.");
    }
    return response;
}
