#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Bank.h"
#include "CAM_SubArray.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "EvaCamRun.h"
#include "Mat.h"
#include "Result.h"
#include "McamTestConfig.h"
#include "TestSupport.h"
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

EvaCamDesignResultDto RunDimensionConfig(const std::string &configPath, int rows, int columns) {
    EvaCamRunOptions options;
    options.configPath = configPath;
    options.subarrayRows = rows;
    options.subarrayColumns = columns;
    options.stdoutOutput = false;

    const EvaCamRunResultDto result = RunEvaCam(options);
    assert(result.numSolutions > 0);
    return result.bestResults.at("LeakagePower");
}

void AssertFinitePositive(double value) {
    assert(std::isfinite(value));
    assert(value > 0);
}

void TestPreInitializationAndCalculationOrderGuards() {
    CAM_SubArray subarray;
    assert(!subarray.initialized);
    assert(!subarray.invalid);
    assert(subarray.ConfiguredRows() == 0);
    assert(subarray.ConfiguredColumns() == 0);

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
    TestSupport::TemporaryDirectory temporary("mcam-topology");
    const auto config = McamTestConfig::WriteMcamConfigVariant(temporary,
            "config/2FeFET_MCAM/2FeFET_MCAM_64x32.config.yaml");
    EvaCamExplorationResult exploration;
    const CAM_SubArray &constSubarray = *RunConfig(config.string(), &exploration);
    CAM_SubArray &subarray = const_cast<CAM_SubArray &>(constSubarray);

    assert(subarray.initialized && !subarray.invalid);
    assert(subarray.camType == MCAM);
    assert(subarray.ConfiguredRows() == 64);
    assert(subarray.ConfiguredColumns() == 32);
    assert(subarray.numRow == 32 && subarray.numColumn == 64);
    assert(subarray.CAM_opt.BitSerialWidth == 32);
    assert(subarray.precharger && subarray.senseAmp && subarray.ColMux.at(0));
    assert(subarray.precharger->numColumn == 64);
    assert(subarray.senseAmp->numColumn == 64);
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

void TestCanonicalMcamReportsSenseMarginDiagnostic() {
    CliOptions options;
    options.inputFileName = "config/2FeFET_MCAM/2FeFET_MCAM.config.yaml";
    options.stdoutOutput = false;
    EvaCamContext context = EvaCamContextBuilder::Build(options);
    const EvaCamExplorationResult exploration = EvaCamExplorer(context.config, 1).Run();
    assert(exploration.numSolution > 0);
    const auto result = exploration.bestResults.at(leakage_optimized);
    assert(result && result->bank && result->bank->mat
            && result->bank->mat->subarray);
    const CAM_SubArray &subarray = *result->bank->mat->subarray;
    assert(!subarray.invalid);
    assert(std::fabs(subarray.senseVoltage - 0.070) < 1e-12);
    assert(subarray.senseMargin > 0);
    assert(subarray.senseMargin < subarray.senseVoltage);
}

void TestMcamAsymmetricDimensionsFollowWordColumns() {
    TestSupport::TemporaryDirectory temporary("mcam-dimensions");
    const auto config = McamTestConfig::WriteMcamConfigVariant(temporary,
            "config/2FeFET_MCAM/2FeFET_MCAM.config.yaml");
    const EvaCamDesignResultDto wide = RunDimensionConfig(config.string(), 16, 64);
    const EvaCamDesignResultDto deep = RunDimensionConfig(config.string(), 64, 16);

    assert(wide.geometry.at("subarray_rows") == 16);
    assert(wide.geometry.at("subarray_columns") == 64);
    assert(deep.geometry.at("subarray_rows") == 64);
    assert(deep.geometry.at("subarray_columns") == 16);
    assert(wide.geometry.at("bit_serial_width") == 64);
    assert(deep.geometry.at("bit_serial_width") == 16);
    assert(wide.summary.at("timing.exact_match_sense_margin_v")
            < deep.summary.at("timing.exact_match_sense_margin_v"));
}

}  // namespace

int main() {
    TestPreInitializationAndCalculationOrderGuards();
    TestSramTopology();
    TestResistiveTcamAggregateCalculations();
    TestMcamTopologyAndAggregateCalculations();
    TestCanonicalMcamReportsSenseMarginDiagnostic();
    TestMcamAsymmetricDimensionsFollowWordColumns();
    std::cout << "CAM subarray topology tests passed\n";
    return 0;
}
