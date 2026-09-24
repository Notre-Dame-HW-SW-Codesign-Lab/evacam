#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "model/NandRcLadder.h"

namespace {

void Require(bool condition, const std::string &message) {
    if (!condition) throw std::invalid_argument("NAND RC ladder: " + message);
}

void ValidateBoundary(const NandRcBoundary &boundary) {
    if (!boundary.connected) return;
    Require(std::isfinite(boundary.resistance) && boundary.resistance > 0,
            "connected boundary resistance must be finite and positive");
    Require(std::isfinite(1 / boundary.resistance),
            "boundary conductance must be representable and finite");
    Require(std::isfinite(boundary.voltage), "boundary voltage must be finite");
}

struct Step {
    std::vector<double> voltage;
    double leftCharge = 0;
    double rightCharge = 0;
};

void RequireFiniteArithmetic(double value, const char *quantity) {
    if (!std::isfinite(value)) {
        throw std::runtime_error(std::string("NAND RC ladder: nonfinite ") + quantity);
    }
}

void ValidateStep(const Step &step) {
    for (double value : step.voltage) RequireFiniteArithmetic(value, "voltage");
    RequireFiniteArithmetic(step.leftCharge, "left source charge");
    RequireFiniteArithmetic(step.rightCharge, "right source charge");
}

Step BackwardEuler(const std::vector<double> &capacitance,
        const std::vector<double> &conductance, const std::vector<double> &initial,
        double step, const NandRcBoundary &left, const NandRcBoundary &right) {
    const std::size_t count = capacitance.size();
    std::vector<double> diagonal(count), upper(count > 1 ? count - 1 : 0);
    Step result;
    result.voltage.resize(count);
    for (std::size_t index = 0; index < count; index++) {
        const double temporal = capacitance[index] / step;
        if (!(std::isfinite(temporal) && temporal > 0)) {
            throw std::runtime_error("NAND RC ladder: temporal conductance must be finite and positive");
        }
        diagonal[index] = temporal;
        result.voltage[index] = temporal * initial[index];
        if (index > 0) diagonal[index] += conductance[index - 1];
        if (index + 1 < count) {
            diagonal[index] += conductance[index];
            upper[index] = -conductance[index];
        }
    }
    if (left.connected) {
        diagonal.front() += 1 / left.resistance;
        result.voltage.front() += left.voltage / left.resistance;
    }
    if (right.connected) {
        diagonal.back() += 1 / right.resistance;
        result.voltage.back() += right.voltage / right.resistance;
    }
    // Thomas elimination of the positive-definite tridiagonal nodal system.
    for (std::size_t index = 1; index < count; index++) {
        const double multiplier = -conductance[index - 1] / diagonal[index - 1];
        diagonal[index] -= multiplier * upper[index - 1];
        result.voltage[index] -= multiplier * result.voltage[index - 1];
    }
    for (std::size_t offset = count; offset > 0; offset--) {
        const std::size_t index = offset - 1;
        if (!(std::isfinite(diagonal[index]) && diagonal[index] > 0)) {
            throw std::runtime_error("NAND RC ladder: singular or nonfinite nodal system");
        }
        if (index + 1 < count) result.voltage[index] -= upper[index] * result.voltage[index + 1];
        result.voltage[index] /= diagonal[index];
    }
    if (left.connected) result.leftCharge = step * (left.voltage - result.voltage.front()) / left.resistance;
    if (right.connected) result.rightCharge = step * (right.voltage - result.voltage.back()) / right.resistance;
    ValidateStep(result);
    return result;
}

Step RichardsonStep(const std::vector<double> &capacitance,
        const std::vector<double> &conductance, const std::vector<double> &initial,
        double step, const NandRcBoundary &left, const NandRcBoundary &right) {
    const auto coarse = BackwardEuler(capacitance, conductance, initial, step, left, right);
    const auto first = BackwardEuler(capacitance, conductance, initial, step / 2, left, right);
    auto fine = BackwardEuler(capacitance, conductance, first.voltage, step / 2, left, right);
    for (std::size_t index = 0; index < fine.voltage.size(); index++) {
        fine.voltage[index] = 2 * fine.voltage[index] - coarse.voltage[index];
    }
    fine.leftCharge = 2 * (fine.leftCharge + first.leftCharge) - coarse.leftCharge;
    fine.rightCharge = 2 * (fine.rightCharge + first.rightCharge) - coarse.rightCharge;
    ValidateStep(fine);
    return fine;
}

}  // namespace

NandRcResult NandRcLadder::Solve(const std::vector<double> &capacitances,
        const std::vector<double> &seriesResistances,
        const std::vector<double> &initialVoltages, double duration,
        const NandRcBoundary &left, const NandRcBoundary &right,
        const NandRcOptions &options) {
    Require(!capacitances.empty() && capacitances.size() <= 4104,
            "requires between 1 and 4104 dynamic nodes");
    Require(seriesResistances.size() + 1 == capacitances.size()
                    && initialVoltages.size() == capacitances.size(), "inconsistent array lengths");
    for (double value : capacitances) Require(std::isfinite(value) && value > 0,
            "capacitances must be finite and positive (eliminate zero-capacitance nodes first)");
    std::vector<double> conductance;
    for (double resistance : seriesResistances) {
        Require(std::isfinite(resistance) && resistance > 0,
                "series resistances must be finite and positive");
        Require(std::isfinite(1 / resistance),
                "series conductances must be representable and finite");
        conductance.push_back(1 / resistance);
    }
    for (double value : initialVoltages) Require(std::isfinite(value), "initial voltages must be finite");
    Require(std::isfinite(duration) && duration >= 0, "duration must be finite and nonnegative");
    Require(std::isfinite(options.maxStep) && options.maxStep > 0, "maxStep must be finite and positive");
    Require(std::isfinite(options.tolerance) && options.tolerance > 0,
            "tolerance must be finite and positive");
    Require(options.maxSteps > 0, "maxSteps must be positive");
    ValidateBoundary(left);
    ValidateBoundary(right);
    NandRcResult result;
    result.voltages = initialVoltages;
    double minimum = *std::min_element(initialVoltages.begin(), initialVoltages.end());
    double maximum = *std::max_element(initialVoltages.begin(), initialVoltages.end());
    for (const auto &boundary : {left, right}) {
        if (boundary.connected) {
            minimum = std::min(minimum, boundary.voltage);
            maximum = std::max(maximum, boundary.voltage);
        }
    }
    double step = std::min(options.maxStep, duration);
    while (result.elapsed < duration) {
        if (result.acceptedSteps + result.rejectedSteps >= options.maxSteps) {
            throw std::runtime_error("NAND RC ladder: maxSteps exceeded before requested duration");
        }
        step = std::min(step, duration - result.elapsed);
        if (!(step > 0) || result.elapsed + step == result.elapsed) {
            throw std::runtime_error("NAND RC ladder: adaptive time step underflow");
        }
        // Compare second-order Richardson steps, rather than using the much
        // larger first-order BE defect to control a higher-order endpoint.
        const auto coarse = RichardsonStep(capacitances, conductance, result.voltages, step, left, right);
        const auto half = RichardsonStep(capacitances, conductance, result.voltages, step / 2, left, right);
        auto fine = RichardsonStep(capacitances, conductance, half.voltage, step / 2, left, right);
        fine.leftCharge += half.leftCharge;
        fine.rightCharge += half.rightCharge;
        double error = 0;
        bool extrapolationInEnvelope = true;
        Step extrapolated;
        extrapolated.voltage.resize(capacitances.size());
        for (std::size_t index = 0; index < capacitances.size(); index++) {
            const double localError = std::abs(fine.voltage[index] - coarse.voltage[index]) / 3;
            RequireFiniteArithmetic(localError, "local error estimate");
            error = std::max(error, localError);
            const double value = (4 * fine.voltage[index] - coarse.voltage[index]) / 3;
            extrapolated.voltage[index] = value;
            if (!std::isfinite(value)) throw std::runtime_error("NAND RC ladder: nonfinite voltage");
            extrapolationInEnvelope = extrapolationInEnvelope
                    && value >= minimum - options.tolerance && value <= maximum + options.tolerance;
        }
        const double factor = error > 0 ? std::clamp(0.9 * std::cbrt(options.tolerance / error), 0.2, 2.0) : 2.0;
        if (error > options.tolerance || !extrapolationInEnvelope) {
            result.rejectedSteps++;
            step *= extrapolationInEnvelope ? factor : std::min(0.5, factor);
            continue;
        }
        // A second extrapolation supplies a third-order endpoint. The
        // embedded difference controls local error; tiny endpoint excursions
        // (at most tolerance) are projected onto the passive voltage envelope.
        // No projection is used to accept a larger instability.
        for (double &value : extrapolated.voltage) value = std::clamp(value, minimum, maximum);
        extrapolated.leftCharge = (4 * fine.leftCharge - coarse.leftCharge) / 3;
        extrapolated.rightCharge = (4 * fine.rightCharge - coarse.rightCharge) / 3;
        ValidateStep(extrapolated);
        result.voltages = std::move(extrapolated.voltage);
        result.leftSourceCharge += extrapolated.leftCharge;
        result.rightSourceCharge += extrapolated.rightCharge;
        RequireFiniteArithmetic(result.leftSourceCharge, "accumulated left source charge");
        RequireFiniteArithmetic(result.rightSourceCharge, "accumulated right source charge");
        result.maximumEstimatedLocalError = std::max(result.maximumEstimatedLocalError, error);
        result.acceptedSteps++;
        result.elapsed += step;
        step = std::min(options.maxStep, step * factor);
    }
    for (std::size_t index = 0; index < capacitances.size(); index++) {
        result.capacitorChargeChange += capacitances[index] * (result.voltages[index] - initialVoltages[index]);
        result.initialStoredEnergy += 0.5 * capacitances[index] * initialVoltages[index] * initialVoltages[index];
        result.finalStoredEnergy += 0.5 * capacitances[index] * result.voltages[index] * result.voltages[index];
        RequireFiniteArithmetic(result.capacitorChargeChange, "capacitor charge change");
        RequireFiniteArithmetic(result.initialStoredEnergy, "initial stored energy");
        RequireFiniteArithmetic(result.finalStoredEnergy, "final stored energy");
    }
    return result;
}
