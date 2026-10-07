#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "model/AnalyticalCamTiming.h"
#include "TestSupport.h"

using TestSupport::AssertNear;
using TestSupport::AssertThrows;

void TestClosedFormLimitsAndScaling() {
    for (double tau : {1e-15, 1e-12, 1e-9, 1e-6}) {
        const auto threshold = EvaluateCamDecision(CamDecisionMode::FixedThreshold,
                1, 100 * tau, tau, 0, .025, .5);
        assert(threshold.feasible);
        AssertNear(threshold.time, tau * std::log(2));
        AssertNear(threshold.missVoltage, .5);
        AssertNear(threshold.ramp, .5 / tau);
        for (double margin : {.001, .025, .1, .24}) {
            const auto differential = EvaluateCamDecision(CamDecisionMode::Differential,
                    1, 2 * tau, tau, 0, margin);
            assert(differential.feasible);
            // With tau_match=2*tau_miss, separation becomes x-x^2.
            // The quadratic solution is independent of the production root search.
            const double x = (1 + std::sqrt(1 - 4 * margin)) / 2;
            AssertNear(differential.time, -2 * tau * std::log(x), 0, 1e-10);
            AssertNear(differential.margin, margin);
            const auto scaled = EvaluateCamDecision(CamDecisionMode::Differential,
                    1.2, 2 * tau, tau, 0, 1.2 * margin);
            AssertNear(scaled.time, differential.time);
        }
        assert(!EvaluateCamDecision(CamDecisionMode::Differential, 1, 2*tau, tau, 0, .251).feasible);
        AssertNear(CamDischargeVoltage(1, tau, 0, tau), std::exp(-1.0));
        AssertNear(CamDischargeVoltage(1, tau, 2*tau, tau), std::exp(-.25));
        AssertNear(CamDischargeVoltage(1, tau, 2*tau, 4*tau), std::exp(-3.0));
    }
}

void TestIndependentIntegratedResponse() {
    double maximumRelativeError = 0;
    int cases = 0;
    for (double tau : {1e-12, 1e-10, 1e-8}) {
        for (double ratio : {2.0, 10.0, 1e6}) {
            for (double activation : {0.0, .2*tau, 4*tau}) {
                for (auto mode : {CamDecisionMode::FixedThreshold, CamDecisionMode::Differential}) {
                    const auto result = EvaluateCamDecision(mode, 1.1, ratio*tau, tau, activation, .11, .55);
                    assert(result.feasible);
                    const double dt = result.time / 10000;
                    // Independently integrate C*dV/dt = -g(t)*V. This numerical
                    // reference is test-only; EvaCAM evaluates closed-form curves.
                    const auto advance = [&](double voltage, double time, double timeConstant) {
                        const auto derivative = [&](double v, double t) {
                            const double fraction = activation > 0 ? std::min(1.0, t/activation) : 1.0;
                            return -fraction * v / timeConstant;
                        };
                        const double a = derivative(voltage, time);
                        const double b = derivative(voltage + a*dt/2, time + dt/2);
                        const double c = derivative(voltage + b*dt/2, time + dt/2);
                        const double d = derivative(voltage + c*dt, time + dt);
                        return voltage + dt*(a + 2*b + 2*c + d)/6;
                    };
                    double match = 1.1, miss = 1.1, crossing = 0;
                    for (int step = 0; step < 10020; ++step) {
                        const double nextMatch = advance(match, step*dt, ratio*tau);
                        const double nextMiss = advance(miss, step*dt, tau);
                        const double before = mode == CamDecisionMode::FixedThreshold ? .55-miss : match-miss-.11;
                        const double after = mode == CamDecisionMode::FixedThreshold ? .55-nextMiss : nextMatch-nextMiss-.11;
                        if (crossing == 0 && before < 0 && after >= 0) {
                            crossing = dt*(step - before/(after-before));
                        }
                        match = nextMatch;
                        miss = nextMiss;
                        if (step == 9999) {
                            AssertNear(match, result.matchVoltage, 0, 1e-7);
                            AssertNear(miss, result.missVoltage, 0, 1e-7);
                        }
                    }
                    assert(crossing > 0);
                    AssertNear(result.time, crossing, 0, 1e-7);
                    maximumRelativeError = std::max(maximumRelativeError, std::abs(crossing/result.time - 1));
                    ++cases;
                }
            }
        }
    }
    std::cout << "Analytical decision validation: " << cases
              << " cases; maximum relative crossing error " << maximumRelativeError << '\n';
}

void TestSearchPhases() {
    const auto overlapping = EvaluateCamSearchPhases(2e-9, 3e-9, 4e-9, 6e-9, 1e-9, 2e-9, true);
    AssertNear(overlapping.queryReady, 5e-9);
    AssertNear(overlapping.evaluationStart, 5e-9);
    AssertNear(overlapping.resultReady, 12e-9);
    AssertNear(overlapping.cycleTime, 14e-9);
    const auto serial = EvaluateCamSearchPhases(2e-9, 3e-9, 4e-9, 6e-9, 1e-9, 2e-9, false);
    AssertNear(serial.resultReady, 16e-9);
    AssertNear(EvaluateCamSearchPhases(0, 0, 10e-9, 0, 0, 0, true).resultReady, 10e-9);
}

void TestDomainsAndInfeasibleDecisions() {
    for (double invalid : {-1.0, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()}) {
        AssertThrows<std::invalid_argument>([&] { CamDischargeVoltage(1, 1, invalid, 1); }, "nonnegative");
        AssertThrows<std::invalid_argument>([&] { CamDischargeVoltage(1, 1, 1, invalid); }, "nonnegative");
        AssertThrows<std::invalid_argument>([&] { EvaluateCamSearchPhases(0,0,0,0,invalid,0,true); }, "nonnegative");
    }
    AssertThrows<std::invalid_argument>([] { CamDischargeVoltage(1, 0, 0, 1); }, "positive");
    AssertThrows<std::invalid_argument>([] { CamDischargeVoltage(0, 1, 0, 1); }, "positive");
    AssertThrows<std::invalid_argument>([] { EvaluateCamDecision(CamDecisionMode::LegacyHorowitz,1,2,1,0,.1); }, "decision");
    for (double threshold : {0.0, 1.0, 2.0}) {
        AssertThrows<std::invalid_argument>([&] {
            EvaluateCamDecision(CamDecisionMode::FixedThreshold,1,2,1,0,.1,threshold);
        }, "Threshold");
    }
    assert(!EvaluateCamDecision(CamDecisionMode::Differential,1,1,1,0,.1).feasible);
    assert(!EvaluateCamDecision(CamDecisionMode::Differential,1,2,1,0,1).feasible);
    assert(!EvaluateCamDecision(CamDecisionMode::FixedThreshold,1,2,1,0,.3,.5).feasible);
}


void TestKeeperResponseAndSupplyEnergy() {
    AssertNear(CamInverterTripVoltage(1, .2, 1), .5);
    AssertNear(CamInverterTripVoltage(1, .2, 4), .6);
    AssertThrows<std::invalid_argument>([] { CamInverterTripVoltage(1,.6,1); }, "Supply");
    AssertThrows<std::invalid_argument>([] { CamInverterTripVoltage(1,.2,0); }, "positive");
    const auto decision = EvaluateCamKeeperDecision(1.2, 1.0, .6, 2e-9, 32, .1e-9, .07, .8);
    assert(decision.feasible);
    const auto wired = EvaluateCamKeeperDecision(1.2,1,.6,2e-9,32,.1e-9,.07,.8,1e-10);
    assert(wired.time > decision.time);
    AssertNear(CamKeeperVoltage(1.2,1,.6,2e-9,32,1,.1e-9,wired.time,1e-10),.8);
    AssertNear(CamKeeperVoltage(1.2,1,.6,2e-9,32,0,.1e-9,wired.time,1e-10),wired.matchVoltage);
    AssertNear(CamKeeperVoltage(1.2, 1, .6, 2e-9, 32, 1, .1e-9, decision.time), .8);
    assert(decision.matchVoltage >= 1 && decision.margin >= .2);
    // Independent integration of parallel clamped resistor currents.
    for (int misses : {0, 1, 8, 32}) {
        double voltage = 1.2;
        const double dt = decision.time / 100000;
        for (int step = 0; step < 100000; ++step) {
            const auto slope = [&](double v, double t) {
                return -std::min(1.0, t / .1e-9) * ((32-misses)*std::max(0.0,v-1)
                        + misses*std::max(0.0,v-.6)) / 2e-9;
            };
            const double t = step*dt;
            const double a = slope(voltage,t), b = slope(voltage+a*dt/2,t+dt/2);
            const double c = slope(voltage+b*dt/2,t+dt/2), d = slope(voltage+c*dt,t+dt);
            voltage += dt*(a+2*b+2*c+d)/6;
        }
        AssertNear(CamKeeperVoltage(1.2,1,.6,2e-9,32,misses,.1e-9,decision.time), voltage, 0, 1e-8);
    }
    const auto early = EvaluateCamKeeperDecision(1.2,1,.6,2e-9,32,0,.00001,1.1);
    AssertNear(CamKeeperVoltage(1.2,1,.6,2e-9,32,1,0,early.time),1.1);
    assert(!EvaluateCamKeeperDecision(1.2,1,.6,2e-9,32,0,.5,.8).feasible);
    AssertThrows<std::invalid_argument>([] { CamKeeperVoltage(1,1,.6,1,32,1,0,1); }, "clamp");
    AssertThrows<std::invalid_argument>([] { CamKeeperVoltage(1.2,1,.6,1,32,33,0,1); }, "path");
    AssertThrows<std::invalid_argument>([] { EvaluateCamKeeperDecision(1.2,1,.6,1,32,0,.1,.6); }, "threshold");
    AssertNear(CamSupplyRechargeEnergy(10e-15,1.2,.55,.45),1.2e-15);
    AssertNear(CamSupplyRechargeEnergy(10e-15,1.2,1.2,0),14.4e-15);
    assert(CamSupplyRechargeEnergy(1,1,1,1)==0);
    AssertThrows<std::invalid_argument>([] { CamSupplyRechargeEnergy(1,1,.5,.6); }, "final");
    AssertThrows<std::invalid_argument>([] { CamSupplyRechargeEnergy(1,1,2,0); }, "supply");
}


void TestDiodeKeeperClosedForm() {
    for (int paths : {1, 32, 64}) {
        for (int misses : {0, 1, paths}) {
            const double duration=2e-9, dt=duration/100000;
            double voltage=1.2;
            for (int i=0;i<100000;++i) {
                const auto slope=[&](double v,double t) {
                    const double high=std::max(0.0,v-1.05), low=std::max(0.0,v-.66);
                    return -5e9*std::min(1.0,t/1e-10)*((paths-misses)*high*high+misses*low*low);
                };
                const double t=i*dt;
                const double a=slope(voltage,t), b=slope(voltage+a*dt/2,t+dt/2);
                const double c=slope(voltage+b*dt/2,t+dt/2), d=slope(voltage+c*dt,t+dt);
                voltage+=dt*(a+2*b+2*c+d)/6;
            }
            AssertNear(CamDiodeKeeperVoltage(1.2,1.05,.66,5e9,paths,misses,1e-10,duration),voltage,0,1e-8);
        }
        for (double threshold : {.855,1.1}) {
            const auto decision=EvaluateCamDiodeKeeperDecision(1.2,1.05,.66,5e9,paths,1e-10,.00001,threshold);
            assert(decision.feasible);
            AssertNear(CamDiodeKeeperVoltage(1.2,1.05,.66,5e9,paths,1,1e-10,decision.time),threshold);
            assert(decision.ramp>0);
        }
    }
    AssertThrows<std::invalid_argument>([] { CamDiodeKeeperVoltage(1.2,1,.6,0,32,1,0,1); },"positive");
    AssertThrows<std::invalid_argument>([] { EvaluateCamDiodeKeeperDecision(1.2,1,.6,1,32,0,.1,.6); },"threshold");
    assert(!EvaluateCamDiodeKeeperDecision(1.2,1,.6,1,32,0,.9,.8).feasible);
}

int main() {
    TestDiodeKeeperClosedForm();
    TestKeeperResponseAndSupplyEnergy();
    TestClosedFormLimitsAndScaling();
    TestIndependentIntegratedResponse();
    TestSearchPhases();
    TestDomainsAndInfeasibleDecisions();
}
