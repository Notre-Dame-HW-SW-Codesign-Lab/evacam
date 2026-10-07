#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "model/NandCellCurrentModel.h"

NandCellCurrentModel::NandCellCurrentModel(const NandCellCurrentParameters &values)
        : parameters(values) {
    for (double value : {values.beta, values.slopeFactor, values.leakageConductance,
            values.temperature, values.channelDiameter, values.gateLength,
            values.minimumVoltage, values.maximumVoltage,
            values.minimumThreshold, values.maximumThreshold}) {
        if (!std::isfinite(value)) throw std::invalid_argument("NAND current parameters must be finite");
    }
    // These are numerical/experimental guardrails, not a validated physical domain.
    if (values.beta <= 0 || values.beta > 1 || values.slopeFactor < 1 || values.slopeFactor > 10 ||
            values.leakageConductance < 0 || values.leakageConductance > 1 ||
            values.temperature < 100 || values.temperature > 1000 ||
            values.channelDiameter <= 0 || values.gateLength <= 0 ||
            values.minimumVoltage < -100 || values.maximumVoltage > 100 ||
            values.minimumVoltage >= values.maximumVoltage ||
            values.minimumThreshold < -100 || values.maximumThreshold > 100 ||
            values.minimumThreshold > values.maximumThreshold) {
        throw std::invalid_argument("NAND current parameter domain is unsupported");
    }
}

NandCellCurrentResult NandCellCurrentModel::Evaluate(double gate, double source,
        double drain, double threshold) const {
    for (double voltage : {gate, source, drain}) {
        if (!std::isfinite(voltage) || voltage < parameters.minimumVoltage ||
                voltage > parameters.maximumVoltage) {
            throw std::invalid_argument("NAND terminal voltage outside declared domain");
        }
    }
    if (!std::isfinite(threshold) || threshold < parameters.minimumThreshold ||
            threshold > parameters.maximumThreshold) {
        throw std::invalid_argument("NAND threshold outside declared domain");
    }
    const double scale = 2 * parameters.slopeFactor * 8.617333262145e-5 * parameters.temperature;
    const double xs = (gate - source - threshold) / scale;
    const double xd = (gate - drain - threshold) / scale;
    const double ps = std::max(xs, 0.0) + std::log1p(std::exp(-std::abs(xs)));
    const double pd = std::max(xd, 0.0) + std::log1p(std::exp(-std::abs(xd)));
    // Stable difference of softplus values even when Vds is very small.
    const double lo = std::min(xs, xd);
    const double hi = std::max(xs, xd);
    const double separation = std::abs(drain - source) / scale;
    const double sigmoidLo = std::exp(-std::max(-lo, 0.0)) / (1 + std::exp(-std::abs(lo)));
    const double difference = separation < 1 ? std::log1p(sigmoidLo * std::expm1(separation))
            : std::max(hi, 0.0) + std::log1p(std::exp(-std::abs(hi))) -
              (std::max(lo, 0.0) + std::log1p(std::exp(-std::abs(lo))));
    const double signedDifference = drain >= source ? difference : -difference;
    const double gs = parameters.beta * scale * ps *
            std::exp(-std::max(-xs, 0.0)) / (1 + std::exp(-std::abs(xs)));
    const double gd = parameters.beta * scale * pd *
            std::exp(-std::max(-xd, 0.0)) / (1 + std::exp(-std::abs(xd)));
    return {0.5 * parameters.beta * scale * scale * signedDifference * (ps + pd) +
                    parameters.leakageConductance * (drain - source),
            gs - gd, -gs - parameters.leakageConductance, gd + parameters.leakageConductance};
}
