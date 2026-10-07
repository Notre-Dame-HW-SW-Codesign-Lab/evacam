#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#include "model/NandCellCurrentModel.h"
#include "TestSupport.h"

namespace {
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

NandCellCurrentParameters Parameters() {
    return {2e-5, 1.5, 1e-12, 300, 47e-9, 50e-9, -2, 8, -1, 5};
}

void TestCurrentOrientationAndAnalyticLimits() {
    auto parameters = Parameters();
    const NandCellCurrentModel model(parameters);
    parameters.beta = 1; // Model owns a copy, subsequent caller changes have no effect.
    const auto forward = model.Evaluate(5, 0, .7, 1);
    // Strong-inversion triode limit, softplus correction negligible at these biases.
    AssertNear(forward.current, 2e-5 * (4 * .7 - .7 * .7 / 2) + 1e-12 * .7, 1e-18);
    AssertNear(model.Evaluate(3, 0, 4, 1).current, .5 * 2e-5 * 4 + 4e-12, 1e-16);
    Require(model.Evaluate(2, 0, .7, 2).current < model.Evaluate(2, 0, .7, 1).current,
            "explicit higher threshold suppresses current");
    const auto reverse = model.Evaluate(5, .7, 0, 1);
    AssertNear(reverse.current, -forward.current);
    AssertNear(reverse.gateDerivative, -forward.gateDerivative);
    AssertNear(reverse.sourceDerivative, -forward.drainDerivative);
    AssertNear(reverse.drainDerivative, -forward.sourceDerivative);
    const auto zero = model.Evaluate(2, .3, .3, 1);
    AssertNear(zero.current, 0);
    AssertNear(zero.gateDerivative, 0);
    Require(zero.drainDerivative > 0, "finite conductance at zero Vds");
    for (double vds : {1e-3, 1e-9, 1e-16, -1e-16}) {
        const auto tiny = model.Evaluate(2, 0, vds, 1);
        AssertNear(tiny.current / vds, model.Evaluate(2, 0, 0, 1).drainDerivative, 2e-8);
    }
    const auto shifted = model.Evaluate(5.5, .5, 1.2, 1);
    AssertNear(shifted.current, forward.current);
    AssertNear(model.Evaluate(5.5, 0, .7, 1.5).current, forward.current);

    auto noLeakage = Parameters();
    noLeakage.leakageConductance = 0;
    const NandCellCurrentModel subthreshold(noLeakage);
    const double scale = 2 * 1.5 * 8.617333262145e-5 * 300;
    const double expected = .5 * noLeakage.beta * scale * scale *
            std::exp(-2 / scale) * (1 - std::exp(-1.4 / scale));
    AssertNear(subthreshold.Evaluate(0, 0, .7, 1).current, expected, 0, 5e-6);
    Require(subthreshold.Evaluate(.1, 0, .7, 1).current > expected, "subthreshold current is smooth");
    auto resistor = Parameters();
    resistor.beta = 1e-200;
    resistor.leakageConductance = 1e-5;
    AssertNear(NandCellCurrentModel(resistor).Evaluate(2, .1, .7, 1).current, 6e-6);
}

void TestJacobianAcrossBiasAndThresholdStates() {
    const NandCellCurrentModel model(Parameters());
    for (double gate : {-.5, .9, 1.0, 1.1, 2.0, 6.0}) {
        for (double source : {0.0, .3, .7}) {
            for (double drain : {0.0, .3, .7}) {
                for (double threshold : {1.0, 2.0}) {
                    const auto result = model.Evaluate(gate, source, drain, threshold);
                    const std::array<double, 3> analytic{result.gateDerivative,
                            result.sourceDerivative, result.drainDerivative};
                    for (size_t terminal = 0; terminal < 3; ++terminal) {
                        std::array<double, 3> plus{gate, source, drain}, minus = plus;
                        const double h = 1e-5;
                        plus[terminal] += h;
                        minus[terminal] -= h;
                        const double finiteDifference = (model.Evaluate(plus[0], plus[1], plus[2], threshold).current -
                                model.Evaluate(minus[0], minus[1], minus[2], threshold).current) / (2 * h);
                        AssertNear(analytic[terminal], finiteDifference, 1e-17, 5e-8);
                    }
                    AssertNear(result.gateDerivative + result.sourceDerivative + result.drainDerivative, 0, 3e-20);
                    Require(result.drainDerivative > 0 && result.sourceDerivative < 0,
                            "passive terminal monotonicity");
                    Require(result.current * (drain - source) >= 0, "nonnegative dissipation");
                }
            }
        }
    }
}

void TestParameterAndBiasValidation() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const std::vector<double NandCellCurrentParameters::*> fields{
        &NandCellCurrentParameters::beta, &NandCellCurrentParameters::slopeFactor,
        &NandCellCurrentParameters::leakageConductance, &NandCellCurrentParameters::temperature,
        &NandCellCurrentParameters::channelDiameter, &NandCellCurrentParameters::gateLength,
        &NandCellCurrentParameters::minimumVoltage, &NandCellCurrentParameters::maximumVoltage,
        &NandCellCurrentParameters::minimumThreshold, &NandCellCurrentParameters::maximumThreshold};
    for (auto field : fields) for (double bad : {nan, infinity, -infinity}) {
        auto p = Parameters();
        p.*field = bad;
        AssertThrows<std::invalid_argument>([&] { NandCellCurrentModel model(p); }, "finite");
    }
    const std::vector<std::pair<double NandCellCurrentParameters::*, double>> invalid{
        {fields[0], 0}, {fields[0], 2}, {fields[1], .5}, {fields[1], 11},
        {fields[2], -1}, {fields[2], 2}, {fields[3], 99}, {fields[3], 1001},
        {fields[4], 0}, {fields[5], -1}, {fields[6], -101}, {fields[6], 8},
        {fields[7], 101}, {fields[8], -101}, {fields[8], 6}, {fields[9], 101}};
    for (const auto &entry : invalid) {
        auto p = Parameters();
        p.*entry.first = entry.second;
        AssertThrows<std::invalid_argument>([&] { NandCellCurrentModel model(p); }, "domain");
    }
    const NandCellCurrentModel model(Parameters());
    for (double bad : {nan, infinity, -3.0, 9.0}) {
        AssertThrows<std::invalid_argument>([&] { model.Evaluate(bad, 0, .7, 1); }, "terminal");
        AssertThrows<std::invalid_argument>([&] { model.Evaluate(2, bad, .7, 1); }, "terminal");
        AssertThrows<std::invalid_argument>([&] { model.Evaluate(2, 0, bad, 1); }, "terminal");
        AssertThrows<std::invalid_argument>([&] { model.Evaluate(2, 0, .7, bad); }, "threshold");
    }
    auto wide = Parameters();
    wide.minimumVoltage = wide.minimumThreshold = -100;
    wide.maximumVoltage = wide.maximumThreshold = 100;
    const auto extreme = NandCellCurrentModel(wide).Evaluate(100, -100, 100, -100);
    Require(std::isfinite(extreme.current) && extreme.current > 0, "large biases do not overflow softplus");
}
} // namespace

int main() {
    TestCurrentOrientationAndAnalyticLimits();
    TestJacobianAcrossBiasAndThresholdStates();
    TestParameterAndBiasValidation();
    std::cout << "NandCellCurrentModel tests passed\n";
}
