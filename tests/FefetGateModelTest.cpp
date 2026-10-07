#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "model/FefetGateModel.h"
#include "TestSupport.h"

using TestSupport::AssertNear;
using TestSupport::AssertThrows;

void TestGateAgainstIntegratedKcl() {
    // Independent numerical integration of the two-branch circuit. Compare
    // threshold crossing and supply work, not a second copy of the closed form.
    for (double resistance : {1e3, 1e4, 1e5}) {
        for (double ratio : {64.0, 1.6e5}) {
            for (double capacitance : {1e-16, 1e-15, 1e-14}) {
                for (double supply : {0.8, 1.0, 1.2}) {
                    const double off = resistance * ratio;
                    const double threshold = supply * 0.5;
                    const auto result = EvaluateFefetGate(resistance, off, capacitance, supply, threshold);
                    const double dt = resistance * capacitance / 2000;
                    const auto slope = [&](double v) {
                        return ((supply - v) / resistance - v / off) / capacitance;
                    };
                    double voltage = 0, energy = 0, crossing = 0;
                    for (int step = 0; step < 40000; ++step) {
                        const double a = slope(voltage);
                        const double b = slope(voltage + dt * a / 2);
                        const double c = slope(voltage + dt * b / 2);
                        const double d = slope(voltage + dt * c);
                        const double next = voltage + dt / 6 * (a + 2*b + 2*c + d);
                        // Trapezoid supply-current integration (independent of
                        // the model's charging-energy expression).
                        energy += dt * supply * (supply - (voltage + next) / 2) / resistance;
                        if (voltage < threshold && next >= threshold) {
                            crossing = dt * (step + (threshold - voltage) / (next - voltage));
                        }
                        voltage = next;
                    }
                    AssertNear(result.delay, crossing, 0, 1e-7);
                    AssertNear(result.mismatchVoltage, voltage, 0, 1e-8);
                    AssertNear(result.staticPower, supply * (supply - voltage) / resistance, 0, 4e-4);
                    AssertNear(result.chargingEnergy, energy - result.staticPower * dt * 40000, 0, 1e-6);
                    AssertNear(result.ramp, slope(threshold) / supply);
                    AssertNear(result.matchVoltage / resistance,
                            (supply - result.matchVoltage) / off);
                }
            }
        }
    }
}

void TestGateDomain() {
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()}) {
        AssertThrows<std::invalid_argument>([&] { EvaluateFefetGate(invalid, 1e9, 1e-15, 1, .5); }, "finite and positive");
        AssertThrows<std::invalid_argument>([&] { EvaluateFefetGate(1e4, invalid, 1e-15, 1, .5); }, "finite and positive");
        AssertThrows<std::invalid_argument>([&] { EvaluateFefetGate(1e4, 1e9, invalid, 1, .5); }, "finite and positive");
        AssertThrows<std::invalid_argument>([&] { EvaluateFefetGate(1e4, 1e9, 1e-15, invalid, .5); }, "finite and positive");
        AssertThrows<std::invalid_argument>([&] { EvaluateFefetGate(1e4, 1e9, 1e-15, 1, invalid); }, "finite and positive");
    }
    AssertThrows<std::invalid_argument>([] { EvaluateFefetGate(1e4, 1e4, 1e-15, 1, .5); }, "exceed");
    for (double threshold : {.01, .99}) {
        AssertThrows<std::invalid_argument>([&] { EvaluateFefetGate(1e4, 1e5, 1e-15, 1, threshold); }, "separate");
    }
}

int main() {
    TestGateAgainstIntegratedKcl();
    TestGateDomain();
}
