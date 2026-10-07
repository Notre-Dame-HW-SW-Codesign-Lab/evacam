#include <algorithm>
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <vector>

#include "model/NandNonlinearString.h"
#include "TestSupport.h"

namespace {
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

NandCellCurrentParameters Parameters() {
    return {2e-5, 1.5, 1e-12, 300, 47e-9, 50e-9, -2, 8, -1, 5};
}

void CheckKcl(const std::vector<NandNonlinearDevice> &devices, const NandNonlinearDcResult &result) {
    Require(result.voltages.size() == devices.size() + 1, "all nodes returned");
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto &device = devices[i];
        const double current = device.model.Evaluate(device.gateVoltage, result.voltages[i],
                result.voltages[i + 1], device.threshold).current;
        AssertNear(current, result.current, 2e-14, 2e-8);
    }
    Require(result.maximumVoltageCorrection <= 1e-10, "full Newton voltage correction converges");
}

void TestSingleDeviceAndZeroBias() {
    const NandCellCurrentModel model(Parameters());
    for (double source : {0.0, .7}) for (double drain : {0.0, .7}) {
        const auto result = NandNonlinearString::Solve({{model, 2, 1}}, source, drain);
        AssertNear(result.current, model.Evaluate(2, source, drain, 1).current);
        AssertNear(result.voltages.front(), source);
        AssertNear(result.voltages.back(), drain);
        AssertNear(result.maximumKclResidual, 0);
        Require(result.iterations == 1 && result.backtracks == 0, "single device needs no Newton step");
    }
    const auto zero = NandNonlinearString::Solve({{model, 2, 1}, {model, 5, 2}, {model, 6, 1}}, .2, .2);
    AssertNear(zero.current, 0);
    for (double voltage : zero.voltages) AssertNear(voltage, .2);
}

void TestHomogeneousStringsAndResistorLimit() {
    const NandCellCurrentModel model(Parameters());
    for (size_t count : {2u, 8u, 32u, 128u, 512u}) {
        const std::vector<NandNonlinearDevice> devices(count, {model, 2, 1});
        const auto result = NandNonlinearString::Solve(devices, 0, .7);
        // Identical cell potential differences telescope: N*I = F(Vs)-F(Vd).
        AssertNear(result.current, model.Evaluate(2, 0, .7, 1).current / count, 2e-15, 1e-8);
        CheckKcl(devices, result);
        Require(std::is_sorted(result.voltages.begin(), result.voltages.end()), "passive node ordering");
    }
    auto p = Parameters();
    p.beta = 1e-200;
    std::vector<NandNonlinearDevice> resistors;
    double resistance = 0;
    for (double g : {1e-5, 3e-5, 2e-6, 7e-6}) {
        p.leakageConductance = g;
        resistance += 1 / g;
        resistors.push_back({NandCellCurrentModel(p), 2, 1});
    }
    for (double drain : {-.7, .7}) {
        const auto result = NandNonlinearString::Solve(resistors, 0, drain);
        AssertNear(result.current, drain / resistance, 1e-20);
        CheckKcl(resistors, result);
    }
}

void TestSelectedPositionsReverseBiasAndConcurrency() {
    const NandCellCurrentModel model(Parameters());
    for (size_t selected : {0u, 3u, 7u}) {
        std::vector<NandNonlinearDevice> devices(8, {model, 6, 1});
        devices[selected].gateVoltage = 1.1;
        const auto result = NandNonlinearString::Solve(devices, 0, .7);
        CheckKcl(devices, result);
        // Mirror device order and endpoints without changing absolute gate biases.
        // Rebuild since immutable model parameters deliberately disable assignment.
        const std::vector<NandNonlinearDevice> mirrored(devices.rbegin(), devices.rend());
        const auto reverse = NandNonlinearString::Solve(mirrored, .7, 0);
        AssertNear(reverse.current, -result.current, 2e-14, 1e-8);
        for (size_t i = 0; i < result.voltages.size(); ++i) {
            AssertNear(reverse.voltages[i], result.voltages[result.voltages.size() - 1 - i], 1e-9);
        }
        std::vector<std::future<NandNonlinearDcResult>> futures;
        for (int run = 0; run < 4; ++run) futures.push_back(std::async(std::launch::async,
                [&devices] { return NandNonlinearString::Solve(devices, 0, .7); }));
        for (auto &future : futures) AssertNear(future.get().current, result.current);
    }
}

void TestNearOffCurrentAndToleranceRefinement() {
    auto p = Parameters();
    p.leakageConductance = 0;
    const NandCellCurrentModel model(p);
    const std::vector<NandNonlinearDevice> devices{{model, .5, 1}, {model, 6, 1}, {model, 6, 1}};
    const auto result = NandNonlinearString::Solve(devices, 0, .7);
    Require(result.current > 0 && result.current < 1e-10, "near-off current retained");
    CheckKcl(devices, result);
    NandNonlinearDcOptions tight;
    tight.absoluteCurrentTolerance = 1e-18;
    tight.relativeCurrentTolerance = 1e-10;
    tight.voltageTolerance = 1e-12;
    const auto refined = NandNonlinearString::Solve(devices, 0, .7, tight);
    AssertNear(result.current, refined.current, 1e-18, 1e-6);
    Require(refined.maximumKclResidual <= 1e-18 + 1e-10 * std::abs(refined.current), "tight KCL convergence");
}

void TestLineSearchAndBoundedFailure() {
    const NandCellCurrentModel model(Parameters());
    std::vector<NandNonlinearDevice> devices(8, {model, 6, 1});
    devices.back().gateVoltage = 0;
    const auto result = NandNonlinearString::Solve(devices, 0, 4);
    Require(result.backtracks > 0, "stiff selected device requires damping");
    CheckKcl(devices, result);
    NandNonlinearDcOptions noBacktracks;
    noBacktracks.maxBacktracks = 0;
    AssertThrows<std::runtime_error>([&] {
        NandNonlinearString::Solve(devices, 0, 4, noBacktracks);
    }, "line search failed");
}

void TestInvalidInputsAndConvergenceFailures() {
    const NandCellCurrentModel model(Parameters());
    const std::vector<NandNonlinearDevice> devices{{model, 1.1, 1}, {model, 6, 1}, {model, 6, 1}};
    AssertThrows<std::invalid_argument>([&] { NandNonlinearString::Solve({}, 0, .7); }, "inputs");
    AssertThrows<std::invalid_argument>([&] {
        NandNonlinearString::Solve(std::vector<NandNonlinearDevice>(513, {model, 2, 1}), 0, .7);
    }, "inputs");
    for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        AssertThrows<std::invalid_argument>([&] { NandNonlinearString::Solve(devices, bad, .7); }, "inputs");
        AssertThrows<std::invalid_argument>([&] { NandNonlinearString::Solve(devices, 0, bad); }, "inputs");
        for (auto field : {&NandNonlinearDcOptions::absoluteCurrentTolerance,
                &NandNonlinearDcOptions::relativeCurrentTolerance, &NandNonlinearDcOptions::voltageTolerance}) {
            NandNonlinearDcOptions options;
            options.*field = bad;
            AssertThrows<std::invalid_argument>([&] { NandNonlinearString::Solve(devices, 0, .7, options); }, "inputs");
        }
    }
    for (int index = 0; index < 6; ++index) {
        NandNonlinearDcOptions options;
        if (index == 0) options.absoluteCurrentTolerance = 0;
        if (index == 1) options.relativeCurrentTolerance = -1;
        if (index == 2) options.relativeCurrentTolerance = 1;
        if (index == 3) options.voltageTolerance = 0;
        if (index == 4) options.maxIterations = 0;
        if (index == 5) options.maxBacktracks = -1;
        AssertThrows<std::invalid_argument>([&] { NandNonlinearString::Solve(devices, 0, .7, options); }, "inputs");
    }
    AssertThrows<std::invalid_argument>([&] { NandNonlinearString::Solve(devices, 0, 9); }, "terminal");
    AssertThrows<std::invalid_argument>([&] { NandNonlinearString::Solve({{model, 9, 1}}, 0, .7); }, "terminal");
    AssertThrows<std::invalid_argument>([&] { NandNonlinearString::Solve({{model, 2, 6}}, 0, .7); }, "threshold");
    NandNonlinearDcOptions limited;
    limited.maxIterations = 1;
    AssertThrows<std::runtime_error>([&] { NandNonlinearString::Solve(devices, 0, .7, limited); }, "iteration limit");
    auto off = Parameters();
    off.minimumVoltage = off.minimumThreshold = -100;
    off.maximumVoltage = off.maximumThreshold = 100;
    off.leakageConductance = 0;
    const NandCellCurrentModel underflow(off);
    AssertThrows<std::runtime_error>([&] {
        NandNonlinearString::Solve({{underflow, -100, 100}, {underflow, -100, 100}}, 0, .7);
    }, "singular Jacobian");
}
} // namespace

int main() {
    TestSingleDeviceAndZeroBias();
    TestHomogeneousStringsAndResistorLimit();
    TestSelectedPositionsReverseBiasAndConcurrency();
    TestNearOffCurrentAndToleranceRefinement();
    TestLineSearchAndBoundedFailure();
    TestInvalidInputsAndConvergenceFailures();
    std::cout << "NandNonlinearString tests passed\n";
}
