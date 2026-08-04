#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Bank.h"
#include "CAM_SubArray.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "Mat.h"
#include "Result.h"
#include "input/CliOptions.h"

namespace {

CAM_SubArray *RunConfig(const std::string &path, EvaCamExplorationResult *exploration) {
    CliOptions options;
    options.inputFileName = path;
    EvaCamContext context = EvaCamContextBuilder::Build(options);
    *exploration = EvaCamExplorer(context.config, 1).Run();
    assert(exploration->numSolution > 0);

    const auto &result = exploration->bestResults.at(leakage_optimized);
    assert(result && result->bank && result->bank->mat && result->bank->mat->subarray);
    return result->bank->mat->subarray.get();
}

void AssertFinitePositive(double value) {
    assert(std::isfinite(value));
    assert(value > 0);
}

void TestPreInitializationAndCalculationOrderGuards() {
    CAM_SubArray subarray;
    assert(!subarray.initialized);
    assert(!subarray.invalid);

    bool threw = false;
    try {
        subarray.CalculateArea();
    } catch (const std::runtime_error &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        subarray.CalculateLatency(0);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        subarray.CalculatePower();
    } catch (const std::runtime_error &) {
        threw = true;
    }
    assert(threw);
}

void TestSramTopology() {
    EvaCamExplorationResult exploration;
    const CAM_SubArray &subarray = *RunConfig(
            "config/8T-BCAM_65nm/8T-BCAM_65nm.config.yaml", &exploration);

    assert(subarray.initialized && !subarray.invalid);
    // BCAM is represented internally by the TCAM electrical model.
    assert(subarray.camType == TCAM);
    assert(subarray.numRow >= 16 && subarray.numColumn >= 8);
    assert(subarray.RowDecMergeNand && subarray.precharger && subarray.senseAmp);
    assert(!subarray.RowDriver.empty() && subarray.RowDriver.at(0));
    assert(!subarray.ColMux.empty() && subarray.ColMux.at(0));
    AssertFinitePositive(subarray.area);
    AssertFinitePositive(subarray.width);
    AssertFinitePositive(subarray.height);
}

void TestResistiveTcamAggregateCalculations() {
    EvaCamExplorationResult exploration;
    const CAM_SubArray &constSubarray = *RunConfig(
            "config/2FeFET_TCAM/2FeFET_TCAM_explicit_subarray.config.yaml", &exploration);
    CAM_SubArray &subarray = const_cast<CAM_SubArray &>(constSubarray);

    assert(subarray.initialized && !subarray.invalid);
    assert(subarray.camType == TCAM);
    assert(subarray.numRow == 64 && subarray.numColumn == 64);
    assert(subarray.internalSenseAmp && subarray.senseAmp);
    assert(subarray.withWriteDriver);
    assert(subarray.WriteDriver.at(0));
    AssertFinitePositive(subarray.area);
    AssertFinitePositive(subarray.searchLatency);
    AssertFinitePositive(subarray.searchDynamicEnergy);
    AssertFinitePositive(subarray.readDynamicEnergy);
    AssertFinitePositive(subarray.leakage);

    subarray.CalculateArea();
    subarray.CalculateLatency(1e-12);
    subarray.CalculatePower();
    const double area = subarray.area;
    const double energy = subarray.searchDynamicEnergy;
    subarray.CalculateArea();
    subarray.CalculateLatency(1e-12);
    subarray.CalculatePower();
    assert(std::fabs(subarray.area - area) <= area * 1e-12);
    assert(std::fabs(subarray.searchDynamicEnergy - energy) <= energy * 1e-12);
    subarray.PrintProperty();
}

void TestMcamTopologyAndAggregateCalculations() {
    EvaCamExplorationResult exploration;
    const CAM_SubArray &constSubarray = *RunConfig(
            "config/2FeFET_MCAM/2FeFET_MCAM_64x32.config.yaml", &exploration);
    CAM_SubArray &subarray = const_cast<CAM_SubArray &>(constSubarray);

    assert(subarray.initialized && !subarray.invalid);
    assert(subarray.camType == MCAM);
    assert(subarray.numRow == 64 && subarray.numColumn == 32);
    assert(subarray.precharger && subarray.senseAmp && subarray.ColMux.at(0));
    AssertFinitePositive(subarray.area);
    AssertFinitePositive(subarray.matchlineDelay);
    AssertFinitePositive(subarray.searchLatency);
    AssertFinitePositive(subarray.searchDynamicEnergy);

    subarray.CalculateLatency(1e-12);
    subarray.CalculatePower();
    const double latency = subarray.searchLatency;
    subarray.CalculateLatency(1e-12);
    subarray.CalculatePower();
    assert(std::fabs(subarray.searchLatency - latency) <= latency * 1e-12);
}

}  // namespace

int main() {
    TestPreInitializationAndCalculationOrderGuards();
    TestSramTopology();
    TestResistiveTcamAggregateCalculations();
    TestMcamTopologyAndAggregateCalculations();
    std::cout << "CAM subarray topology tests passed\n";
    return 0;
}
