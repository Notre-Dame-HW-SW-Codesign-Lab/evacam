#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

#include "model/AnalyticalCamTiming.h"

namespace {

void RequireFinitePositive(double value) {
    if (!std::isfinite(value) || value <= 0) {
        throw std::invalid_argument("[Analytical CAM timing] Expected a finite positive value.");
    }
}

void RequireFiniteNonnegative(double value) {
    if (!std::isfinite(value) || value < 0) {
        throw std::invalid_argument("[Analytical CAM timing] Expected a finite nonnegative value.");
    }
}

// Integral of the normalized, linearly activated conductance.
double Exposure(double time, double activation) {
    return activation > 0 && time < activation
            ? time * (time / activation) / 2 : time - activation / 2;
}

double ExposureTime(double exposure, double activation) {
    return exposure < activation / 2
            ? std::sqrt(exposure) * std::sqrt(2 * activation) : exposure + activation / 2;
}

} // namespace

double CamKeeperVoltage(double precharge, double highClamp, double lowClamp,
        double branchTau, int paths, int misses, double activationTime, double time, double wireTau) {
    for (double value : {precharge, branchTau}) RequireFinitePositive(value);
    for (double value : {highClamp, lowClamp, activationTime, time, wireTau}) RequireFiniteNonnegative(value);
    if (precharge <= highClamp || highClamp <= lowClamp || paths <= 0 || misses < 0 || misses > paths)
        throw std::invalid_argument("[CAM keeper] Invalid clamp ordering or path count.");
    const double exposure = Exposure(time, activationTime);
    if (misses == 0) return highClamp + (precharge - highClamp) * std::exp(-exposure / (branchTau / paths + wireTau));
    const double floor = highClamp - (highClamp - lowClamp) * misses / paths;
    const double transition = (branchTau / paths + wireTau) * std::log((precharge - floor) / (highClamp - floor));
    if (exposure <= transition)
        return floor + (precharge - floor) * std::exp(-exposure / (branchTau / paths + wireTau));
    return lowClamp + (highClamp - lowClamp) * std::exp(-(exposure - transition) / (branchTau / misses + wireTau));
}

CamDecisionResult EvaluateCamKeeperDecision(double precharge, double highClamp,
        double lowClamp, double branchTau, int paths, double activationTime,
        double requiredMargin, double threshold, double wireTau) {
    // Validate the response parameters even when the requested margin is infeasible.
    CamKeeperVoltage(precharge, highClamp, lowClamp, branchTau, paths, 1, activationTime, 0, wireTau);
    RequireFinitePositive(requiredMargin);
    if (!std::isfinite(threshold) || threshold <= lowClamp || threshold >= precharge)
        throw std::invalid_argument("[CAM keeper] Decision threshold must lie above low clamp and below precharge.");
    const double floor = highClamp - (highClamp - lowClamp) / paths;
    double exposure;
    double slope;
    if (threshold >= highClamp) {
        exposure = (branchTau / paths + wireTau) * std::log((precharge - floor) / (threshold - floor));
        slope = (threshold - floor) / (branchTau / paths + wireTau);
    } else {
        const double transition = (branchTau / paths + wireTau) * std::log((precharge - floor) / (highClamp - floor));
        exposure = transition + (branchTau + wireTau) * std::log((highClamp - lowClamp) / (threshold - lowClamp));
        slope = (threshold - lowClamp) / (branchTau + wireTau);
    }
    CamDecisionResult result;
    result.time = ExposureTime(exposure, activationTime);
    result.matchVoltage = CamKeeperVoltage(precharge, highClamp, lowClamp,
            branchTau, paths, 0, activationTime, result.time, wireTau);
    result.missVoltage = threshold;
    result.margin = result.matchVoltage - threshold;
    result.ramp = slope / precharge * (activationTime > 0 ? std::min(1.0, result.time / activationTime) : 1);
    result.feasible = result.margin >= requiredMargin;
    return result;
}

double CamSupplyRechargeEnergy(double capacitance, double supply, double precharge, double finalVoltage) {
    RequireFiniteNonnegative(capacitance);
    RequireFinitePositive(supply);
    RequireFinitePositive(precharge);
    RequireFiniteNonnegative(finalVoltage);
    if (precharge > supply || finalVoltage > precharge)
        throw std::invalid_argument("[CAM energy] Require 0 <= final <= precharge <= supply.");
    return capacitance * supply * (precharge - finalVoltage);
}

double CamInverterTripVoltage(double supply, double threshold, double pToNStrength) {
    RequireFinitePositive(supply);
    RequireFiniteNonnegative(threshold);
    RequireFinitePositive(pToNStrength);
    if (2 * threshold >= supply)
        throw std::invalid_argument("[CAM inverter] Supply must exceed twice threshold for the square-law trip approximation.");
    const double ratio = std::sqrt(pToNStrength);
    return (threshold + ratio * (supply - threshold)) / (1 + ratio);
}

double CamDiodeKeeperVoltage(double precharge, double highClamp, double lowClamp,
        double rate, int paths, int misses, double activationTime, double time) {
    // Reuse the domain validation, independently of the RC approximation.
    CamKeeperVoltage(precharge, highClamp, lowClamp, 1, paths, misses, activationTime, time);
    RequireFinitePositive(rate);
    const double exposure = Exposure(time, activationTime);
    if (misses == 0 || misses == paths) {
        const double floor = misses == 0 ? highClamp : lowClamp;
        return floor + 1 / (1 / (precharge - floor) + paths * rate * exposure);
    }
    const double delta = highClamp - lowClamp;
    const double center = lowClamp + delta * (paths - misses) / paths;
    const double scale = delta * std::sqrt(static_cast<double>(misses) * (paths - misses)) / paths;
    const double start = std::atan((precharge - center) / scale);
    const double transition = (start - std::atan((highClamp - center) / scale)) / (paths * rate * scale);
    if (exposure <= transition)
        return center + scale * std::tan(start - paths * rate * scale * exposure);
    return lowClamp + 1 / (1 / delta + misses * rate * (exposure - transition));
}

CamDecisionResult EvaluateCamDiodeKeeperDecision(double precharge, double highClamp,
        double lowClamp, double rate, int paths, double activationTime,
        double requiredMargin, double threshold) {
    CamDiodeKeeperVoltage(precharge, highClamp, lowClamp, rate, paths, 1, activationTime, 0);
    RequireFinitePositive(requiredMargin);
    if (!std::isfinite(threshold) || threshold <= lowClamp || threshold >= precharge)
        throw std::invalid_argument("[CAM diode keeper] Invalid decision threshold.");
    double exposure;
    if (paths == 1) {
        exposure = (1 / (threshold - lowClamp) - 1 / (precharge - lowClamp)) / rate;
    } else {
        const double delta = highClamp - lowClamp;
        const double center = highClamp - delta / paths;
        const double scale = delta * std::sqrt(paths - 1.0) / paths;
        exposure = (std::atan((precharge - center) / scale)
                - std::atan((std::max(threshold, highClamp) - center) / scale)) / (paths * rate * scale);
        if (threshold < highClamp)
            exposure += (1 / (threshold - lowClamp) - 1 / delta) / rate;
    }
    CamDecisionResult result;
    result.time = ExposureTime(exposure, activationTime);
    result.matchVoltage = CamDiodeKeeperVoltage(precharge, highClamp, lowClamp,
            rate, paths, 0, activationTime, result.time);
    result.missVoltage = threshold;
    result.margin = result.matchVoltage - threshold;
    const double highOverdrive = std::max(0.0, threshold - highClamp);
    result.ramp = rate * ((paths - 1) * highOverdrive * highOverdrive
            + (threshold - lowClamp) * (threshold - lowClamp)) / precharge
            * (activationTime > 0 ? std::min(1.0, result.time / activationTime) : 1);
    result.feasible = result.margin >= requiredMargin;
    return result;
}

double CamDischargeVoltage(double precharge, double tau, double activationTime, double time) {
    RequireFinitePositive(precharge);
    RequireFinitePositive(tau);
    RequireFiniteNonnegative(activationTime);
    RequireFiniteNonnegative(time);
    return precharge * std::exp(-Exposure(time, activationTime) / tau);
}

CamDecisionResult EvaluateCamDecision(CamDecisionMode mode, double precharge,
        double matchTau, double missTau, double activationTime,
        double requiredMargin, double threshold) {
    for (double value : {precharge, matchTau, missTau, requiredMargin}) {
        RequireFinitePositive(value);
    }
    RequireFiniteNonnegative(activationTime);
    if (mode != CamDecisionMode::FixedThreshold && mode != CamDecisionMode::Differential) {
        throw std::invalid_argument("[Analytical CAM timing] Expected threshold or differential decision.");
    }
    if (mode == CamDecisionMode::FixedThreshold
            && (!std::isfinite(threshold) || threshold <= 0 || threshold >= precharge)) {
        throw std::invalid_argument("[Analytical CAM timing] Threshold must lie between zero and precharge.");
    }
    CamDecisionResult result;
    if (matchTau <= missTau || requiredMargin >= precharge) {
        return result;
    }
    // Dimensionless exposure z = integral(g/g_on dt) / missTau keeps
    // the root search stable across femtosecond and microsecond scales.
    const double ratio = missTau / matchTau;
    const auto separation = [&](double z) {
        return precharge * std::exp(-ratio * z) * -std::expm1(-(1 - ratio) * z);
    };
    double z;
    if (mode == CamDecisionMode::FixedThreshold) {
        z = std::log(precharge / threshold);
    } else {
        const double peak = -std::log(ratio) / (1 - ratio);
        if (separation(peak) < requiredMargin) {
            return result;
        }
        // First crossing on the increasing branch, not the later crossing
        // after both match and mismatch lines have leaked toward ground.
        double lower = 0, upper = peak;
        for (int iteration = 0; iteration < 96; ++iteration) {
            const double middle = lower + (upper - lower) / 2;
            if (separation(middle) < requiredMargin) lower = middle;
            else upper = middle;
        }
        z = upper;
    }
    result.time = ExposureTime(z * missTau, activationTime);
    RequireFinitePositive(result.time);
    result.matchVoltage = precharge * std::exp(-ratio * z);
    result.missVoltage = precharge * std::exp(-z);
    result.margin = separation(z);
    const double activation = activationTime > 0 ? std::min(1.0, result.time / activationTime) : 1;
    result.ramp = (result.missVoltage / precharge) * activation / missTau;
    RequireFinitePositive(result.ramp);
    result.feasible = result.margin >= requiredMargin;
    return result;
}

CamSearchPhases EvaluateCamSearchPhases(double input, double control, double precharge,
        double evaluation, double output, double recovery, bool overlapPrecharge) {
    for (double value : {input, control, precharge, evaluation, output, recovery}) {
        RequireFiniteNonnegative(value);
    }
    CamSearchPhases phases;
    phases.queryReady = input + control;
    phases.evaluationStart = overlapPrecharge ? std::max(phases.queryReady, precharge)
            : phases.queryReady + precharge;
    phases.resultReady = phases.evaluationStart + evaluation + output;
    phases.cycleTime = phases.resultReady + recovery;
    RequireFiniteNonnegative(phases.cycleTime);
    return phases;
}
