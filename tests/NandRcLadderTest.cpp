#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#include "model/NandRcLadder.h"
#include "TestSupport.h"

namespace {
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

void TestSingleRcAndChargingEnergy() {
    const NandRcOptions options{20e-12, 1e-8, 100000};
    const NandRcBoundary ground{true, 10000, 0};
    const NandRcBoundary open{};
    const double tau = 10000 * 20e-15;
    for (double multiple : {0.0, 0.1, 1.0, 5.0}) {
        const auto result = NandRcLadder::Solve({20e-15}, {}, {0.8}, tau * multiple,
                ground, open, options);
        AssertNear(result.voltages[0], 0.8 * std::exp(-multiple), 1e-7);
        AssertNear(result.elapsed, tau * multiple);
        AssertNear(result.capacitorChargeChange, result.leftSourceCharge, 1e-25);
        Require(result.finalStoredEnergy <= result.initialStoredEnergy, "passive discharge loses stored energy");
    }
    const auto charged = NandRcLadder::Solve({20e-15}, {}, {0}, tau,
            open, {true, 10000, 0.8}, options);
    AssertNear(charged.voltages[0], 0.8 * (1 - std::exp(-1.0)), 1e-7);
    AssertNear(charged.capacitorChargeChange, charged.rightSourceCharge, 1e-25);
    AssertNear(charged.capacitorChargeChange * 0.8,
            20e-15 * 0.8 * charged.voltages[0], 1e-25);
    Require(charged.capacitorChargeChange * 0.8 > charged.finalStoredEnergy,
            "supply energy includes resistor dissipation, not only capacitor stored energy");
}

void TestKnownTwoPoleCircuit() {
    const double resistance = 2000, capacitance = 1e-15;
    const double root5 = std::sqrt(5.0);
    const double slow = (3 - root5) / 2, fast = (3 + root5) / 2;
    for (double time : {0.01, 0.5, 1.0, 3.0, 10.0}) {
        const double expected = (fast * std::exp(-slow * time) - slow * std::exp(-fast * time)) / root5;
        const auto result = NandRcLadder::Solve({capacitance, capacitance}, {resistance}, {1, 1},
                time * resistance * capacitance, {true, resistance, 0}, {},
                {0.2 * resistance * capacitance, 1e-8, 100000});
        AssertNear(result.voltages.back(), expected, 1e-7);
    }
}

void TestStiff512WordlineCounterexample() {
    std::vector<double> capacitance(514, 0.05e-15);
    capacitance.front() = 1e-15;
    capacitance.back() = 20e-15;
    std::vector<double> resistance(513, 5000);
    for (int index = 0; index < 512; index += 2) resistance[index] = 10000;
    resistance.back() = 1000;
    const NandRcOptions options{1e-9, 1e-8, 100000};
    const auto matching = NandRcLadder::Solve(capacitance, resistance,
            std::vector<double>(514, 0.8), 50e-9, {true, 1000, 0}, {}, options);
    // Independent symmetric-modal Python solution, itself checked against
    // analytic RC circuits and a separately assembled dense matrix exponential.
    AssertNear(matching.voltages.back(), 0.582867925882892, 2e-6);
    Require(0.6681125434317468 - matching.voltages.back() - 0.01 < 0.1,
            "full nodal model must preserve the failed original-reference margin");
    for (int index = 1; index < 512; index++) resistance[index] = 5000;
    resistance[511] = 1e9;
    const auto blocked = NandRcLadder::Solve(capacitance, resistance,
            std::vector<double>(514, 0.8), 50e-9, {true, 1000, 0}, {}, options);
    AssertNear(blocked.voltages.back(), 0.7991109363704981, 2e-6);
    Require(blocked.voltages.back() > matching.voltages.back(), "one blocking device preserves voltage");
}

void TestPrechargeAndStateCarry() {
    std::vector<double> capacitance(70, 0.05e-15);
    capacitance.front() = 1e-15;
    capacitance.back() = 20e-15;
    std::vector<double> resistance(69, 5000);
    resistance.back() = 1000;
    const NandRcOptions options{0.2e-9, 1e-8, 100000};
    const auto charged = NandRcLadder::Solve(capacitance, resistance, std::vector<double>(70, 0),
            0.1e-9, {}, {true, 10000, 0.8}, options);
    Require(charged.voltages.front() < charged.voltages.back(), "finite-driver precharge is nonuniform");
    for (int index = 0; index < 68; index += 2) resistance[index] = 10000;
    const auto evaluated = NandRcLadder::Solve(capacitance, resistance, charged.voltages,
            1e-9, {true, 1000, 0}, {}, options);
    const auto assumedUniform = NandRcLadder::Solve(capacitance, resistance,
            std::vector<double>(70, 0.8), 1e-9, {true, 1000, 0}, {}, options);
    Require(evaluated.voltages.back() < assumedUniform.voltages.back(),
            "evaluation must carry actual incomplete precharge state");
    for (int index = 0; index < 68; index++) resistance[index] = 5000;
    const auto recovered = NandRcLadder::Solve(capacitance, resistance, evaluated.voltages,
            20e-9, {true, 1000, 0}, {true, 10000, 0}, options);
    for (double value : recovered.voltages) Require(value < options.tolerance, "long recovery resets every node");
    const auto isolated = NandRcLadder::Solve({1e-15, 2e-15}, {5000}, {0.8, 0.2},
            1e-9, {}, {}, options);
    AssertNear(isolated.capacitorChargeChange, 0, 1e-24);
    AssertNear(isolated.voltages[0], 0.4, 1e-7);
    AssertNear(isolated.voltages[1], 0.4, 1e-7);
}

void TestValidationAndStepLimit() {
    const NandRcOptions options{1, 1e-8, 1000};
    AssertThrows<std::invalid_argument>([&] { NandRcLadder::Solve({}, {}, {}, 1, {}, {}, options); }, "nodes");
    AssertThrows<std::invalid_argument>([&] { NandRcLadder::Solve({1, 1}, {}, {0, 0}, 1, {}, {}, options); }, "lengths");
    AssertThrows<std::invalid_argument>([&] { NandRcLadder::Solve({0}, {}, {0}, 1, {}, {}, options); }, "positive");
    AssertThrows<std::invalid_argument>([&] { NandRcLadder::Solve({1}, {}, {0}, -1, {}, {}, options); }, "duration");
    AssertThrows<std::invalid_argument>([&] { NandRcLadder::Solve({1}, {}, {0}, 1, {true, 0, 1}, {}, options); }, "boundary");
    AssertThrows<std::invalid_argument>([&] { NandRcLadder::Solve({1}, {}, {0}, 1, {}, {}, {0, 1e-8, 100}); }, "maxStep");
    AssertThrows<std::runtime_error>([&] { NandRcLadder::Solve({1}, {}, {1}, 1, {true, 1, 0}, {}, {1, 1e-12, 1}); }, "maxSteps");
}

void TestNonfiniteDerivedArithmeticIsRejected() {
    const NandRcOptions options{1, 1e-8, 1000};
    const double tiny = std::numeric_limits<double>::denorm_min();
    const double huge = std::numeric_limits<double>::max();
    AssertThrows<std::invalid_argument>([&] {
        NandRcLadder::Solve({1, 1}, {tiny}, {0, 0}, 0, {}, {}, options);
    }, "series conductances");
    AssertThrows<std::invalid_argument>([&] {
        NandRcLadder::Solve({1}, {}, {0}, 0, {true, tiny, 1}, {}, options);
    }, "boundary conductance");
    AssertThrows<std::runtime_error>([&] {
        NandRcLadder::Solve({huge}, {}, {0}, 1, {}, {}, options);
    }, "temporal conductance");
    AssertThrows<std::runtime_error>([&] {
        NandRcLadder::Solve({tiny}, {}, {0}, 4, {}, {}, {4, 1e-8, 1000});
    }, "temporal conductance");
    AssertThrows<std::runtime_error>([&] {
        NandRcLadder::Solve({1}, {}, {huge}, 0, {}, {}, options);
    }, "initial stored energy");
    // Each voltage stays at exactly zero because the two drives cancel. The
    // individual integrated rail charges still overflow and must not be
    // returned as infinities behind an apparently valid voltage solution.
    AssertThrows<std::runtime_error>([&] {
        NandRcLadder::Solve({1}, {}, {0}, 100, {true, 10, 1e308},
                {true, 10, -1e308}, {0.1, 1e-8, 10000});
    }, "source charge");
    // An open terminal ignores its unused fields, preserving the public
    // boundary contract while connected sources remain strictly validated.
    const auto isolated = NandRcLadder::Solve({1}, {}, {0.25}, 0,
            {false, tiny, std::numeric_limits<double>::quiet_NaN()}, {}, options);
    AssertNear(isolated.voltages.front(), 0.25);
}
}  // namespace

int main() {
    TestSingleRcAndChargingEnergy();
    TestKnownTwoPoleCircuit();
    TestStiff512WordlineCounterexample();
    TestPrechargeAndStateCarry();
    TestValidationAndStepLimit();
    TestNonfiniteDerivedArithmeticIsRejected();
    std::cout << "NAND nodal RC ladder tests passed\n";
}
