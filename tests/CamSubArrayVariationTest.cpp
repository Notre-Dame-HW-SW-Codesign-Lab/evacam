#include "Bank.h"
#include "CAM_SubArray.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "Mat.h"
#include "Result.h"
#include "McamTestConfig.h"
#include "TestSupport.h"
#include "input/CliOptions.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

namespace {

bool Near(double actual, double expected, double relativeTolerance = 1e-10,
        double absoluteTolerance = 1e-20) {
    const double difference = std::fabs(actual - expected);
    return difference <= absoluteTolerance
        || difference <= relativeTolerance * std::max(std::fabs(actual), std::fabs(expected));
}

struct Fixture {
    std::shared_ptr<EvaCamConfig> config;
    std::shared_ptr<Result> result;
    CAM_SubArray *subarray = nullptr;
    std::shared_ptr<TestSupport::TemporaryDirectory> temporary;
};

Fixture MakeFixture() {
    CliOptions options;
    options.inputFileName = "config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml";
    EvaCamContext context = EvaCamContextBuilder::Build(options);
    EvaCamExplorer explorer(context.config, 1);
    EvaCamExplorationResult exploration = explorer.Run();
    std::shared_ptr<Result> result = exploration.bestResults.at(leakage_optimized);
    assert(result && result->bank && result->bank->mat && result->bank->mat->subarray);
    return {context.config, result, result->bank->mat->subarray.get(), nullptr};
}

Fixture MakeMcamFixture() {
    auto temporary = std::make_shared<TestSupport::TemporaryDirectory>("mcam-variation");
    const auto configPath = McamTestConfig::WriteZeroSenseVariant(*temporary,
            "config/2FeFET_MCAM/2FeFET_MCAM.config.yaml");
    CliOptions options;
    options.inputFileName = configPath.string();
    EvaCamContext context = EvaCamContextBuilder::Build(options);
    EvaCamExplorer explorer(context.config, 1);
    EvaCamExplorationResult exploration = explorer.Run();
    std::shared_ptr<Result> result = exploration.bestResults.at(leakage_optimized);
    assert(result && result->bank && result->bank->mat && result->bank->mat->subarray);
    return {context.config, result, result->bank->mat->subarray.get(), temporary};
}

void ConfigureMonteCarlo(Fixture &fixture, int samples = 7) {
    VariationConfig &variation = fixture.config->variation;
    variation.enabled = true;
    variation.mode = "monte_carlo";
    variation.samples = samples;
    variation.seed = 314159u;
    variation.monteCarloGranularity = "effective";
    variation.memoryDeviceResOnStdev = 0.15;
    variation.memoryDeviceResOffStdev = 0.20;
}

void TestResistanceSamplingIsDeterministicAndStreamSeparated() {
    Fixture fixture = MakeFixture();
    CAM_SubArray &subarray = *fixture.subarray;
    ConfigureMonteCarlo(fixture);

    const double first = subarray.SampleVariationResistance(1000.0, 0.2, 2, 3);
    assert(first == subarray.SampleVariationResistance(1000.0, 0.2, 2, 3));
    assert(first != subarray.SampleVariationResistance(1000.0, 0.2, 5, 3));
    assert(first != subarray.SampleVariationResistance(1000.0, 0.2, 2, 4));
    assert(subarray.SampleVariationResistance(1000.0, 0.0, 2, 3) == 1000.0);

    fixture.config->variation.enabled = false;
    assert(subarray.SampleVariationResistance(1000.0, 0.2, 2, 3) == 1000.0);

    ConfigureMonteCarlo(fixture);
    const double cellZero = subarray.SampleCellVariationResistance(1000.0, 0.2, 2, 3, 0);
    assert(cellZero == subarray.SampleCellVariationResistance(1000.0, 0.2, 2, 3, 0));
    assert(cellZero != subarray.SampleCellVariationResistance(1000.0, 0.2, 2, 3, 1));
    assert(subarray.SampleCellVariationResistance(1000.0, 0.0, 2, 3, 0) == 1000.0);
}

void TestMonteCarloCellAndCornerResistanceSamples() {
    Fixture fixture = MakeFixture();
    CAM_SubArray &subarray = *fixture.subarray;
    ConfigureMonteCarlo(fixture);
    fixture.config->variation.monteCarloGranularity = "cell";

    const CAMResistanceSample sample = subarray.BuildVariationResistanceSample(2);
    assert(sample.hasAggregateMatchlineRes);
    assert(sample.cellResOn > 0.0 && sample.cellResOff > 0.0);
    assert(sample.oneMissEffectiveCellRes > 0.0);
    assert(sample.allMatchEffectiveCellRes > 0.0);
    assert(!Near(sample.oneMissEffectiveCellRes, sample.allMatchEffectiveCellRes));

    VariationConfig &variation = fixture.config->variation;
    variation.mode = "corner";
    variation.samples = 4;
    variation.memoryDeviceResOnMaxVar = 0.10;
    variation.memoryDeviceResOffMaxVar = 0.20;
    const double on = subarray.nominalResMatchTran;
    const double off = subarray.nominalResMatchTranOff;
    const CAMResistanceSample lowLow = subarray.BuildVariationResistanceSample(0);
    const CAMResistanceSample highLow = subarray.BuildVariationResistanceSample(1);
    const CAMResistanceSample lowHigh = subarray.BuildVariationResistanceSample(2);
    const CAMResistanceSample highHigh = subarray.BuildVariationResistanceSample(3);
    assert(Near(lowLow.matchRes, on * 0.9));
    assert(Near(highLow.matchRes, on * 1.1));
    assert(Near(lowHigh.matchResOff, off * 1.2));
    assert(Near(highHigh.matchResOff, off * 1.2));
}

void TestTimingSummaryHandlesDisabledSinglePointAndMonteCarlo() {
    Fixture fixture = MakeFixture();
    CAM_SubArray &subarray = *fixture.subarray;
    const double originalMatchlineDelay = subarray.matchlineDelay;
    const double originalReferenceDelay = subarray.referDelay;

    subarray.UpdateVariationTimingSummary();
    assert(!subarray.variationSummary.enabled);
    assert(subarray.variationSamples.empty());
    assert(Near(subarray.matchlineDelay, originalMatchlineDelay));

    VariationConfig &variation = fixture.config->variation;
    variation.enabled = true;
    variation.mode = "single_point";
    variation.samples = 1;
    variation.seed = 11u;
    variation.memoryDeviceResOnStdev = 0.15;
    variation.memoryDeviceResOffStdev = 0.20;
    subarray.UpdateVariationTimingSummary();
    assert(subarray.variationSummary.enabled);
    assert(subarray.variationSummary.mode == "single_point");
    assert(subarray.variationSummary.samples == 1);
    assert(subarray.variationSummary.matchlineDelay.available);
    assert(subarray.variationSummary.matchlineDelay.sample == subarray.matchlineDelay);
    assert(subarray.variationSummary.referenceDelay.available);
    assert(subarray.variationSummary.referenceDelay.nominal == originalReferenceDelay);
    assert(subarray.variationSummary.referenceDelay.sample == subarray.referDelay);
    assert(subarray.variationSamples.empty());

    Fixture empty = MakeFixture();
    ConfigureMonteCarlo(empty, 1);
    empty.subarray->UpdateVariationTimingSummary();
    assert(!empty.subarray->variationSummary.enabled);
    assert(!empty.subarray->variationSummary.matchlineDelay.available);
    assert(empty.subarray->variationSamples.empty());

    Fixture monteCarlo = MakeFixture();
    ConfigureMonteCarlo(monteCarlo, 7);
    CAM_SubArray &mcSubarray = *monteCarlo.subarray;
    const double nominal = mcSubarray.matchlineDelay;
    const double nominalReference = mcSubarray.referDelay;
    mcSubarray.UpdateVariationTimingSummary();
    const CAMMetricStats &stats = mcSubarray.variationSummary.matchlineDelay;
    const CAMMetricStats &referenceStats = mcSubarray.variationSummary.referenceDelay;
    assert(mcSubarray.variationSummary.enabled);
    assert(mcSubarray.variationSamples.size() == 7);
    assert(stats.available && Near(stats.nominal, nominal));
    assert(referenceStats.available && Near(referenceStats.nominal, nominalReference));
    std::vector<double> values;
    for (const CAMVariationSample &item : mcSubarray.variationSamples)
        values.push_back(item.matchlineDelay);
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double squareError = 0.0;
    for (double value : values)
        squareError += (value - mean) * (value - mean);
    assert(Near(stats.mean, mean));
    assert(Near(stats.stddev, std::sqrt(squareError / values.size())));
    assert(Near(stats.min, *std::min_element(values.begin(), values.end())));
    assert(Near(stats.max, *std::max_element(values.begin(), values.end())));
    std::sort(values.begin(), values.end());
    assert(Near(stats.p95, values[static_cast<size_t>(std::ceil(0.95 * values.size())) - 1]));
}

void TestPowerSummaryAndCellReadEnergy() {
    Fixture fixture = MakeFixture();
    CAM_SubArray &subarray = *fixture.subarray;
    ConfigureMonteCarlo(fixture, 7);
    subarray.UpdateVariationTimingSummary();
    subarray.UpdateVariationPowerSummary();
    const CAMMetricStats &energy = subarray.variationSummary.searchDynamicEnergy;
    assert(energy.available);
    assert(energy.min <= energy.mean && energy.mean <= energy.max);
    assert(energy.stddev >= 0.0 && energy.p95 >= energy.mean);
    for (const CAMVariationSample &sample : subarray.variationSamples)
        assert(sample.searchDynamicEnergy > 0.0);

    const CAMResistanceSample nominal = subarray.BuildNominalResistanceSample();
    const double shortRead = subarray.SampleCellReadEnergy(nominal, 0.0);
    const double longRead = subarray.SampleCellReadEnergy(nominal, subarray.senseAmp->readLatency);
    assert(shortRead > 0.0);
    assert(longRead > shortRead);

    const double savedReadEnergy = fixture.config->technology.cell->readEnergy;
    fixture.config->technology.cell->readEnergy = 1e-15;
    assert(Near(subarray.SampleCellReadEnergy(nominal, 0.0),
            subarray.SampleCellReadEnergy(nominal, subarray.senseAmp->readLatency)));
    fixture.config->technology.cell->readEnergy = savedReadEnergy;

    Fixture singlePoint = MakeFixture();
    CAM_SubArray &singleSubarray = *singlePoint.subarray;
    ConfigureMonteCarlo(singlePoint);
    singlePoint.config->variation.mode = "single_point";
    singlePoint.config->variation.samples = 1;
    singleSubarray.UpdateVariationTimingSummary();
    singleSubarray.UpdateVariationPowerSummary();
    assert(singleSubarray.variationSummary.searchDynamicEnergy.available);
    assert(singleSubarray.variationSummary.searchDynamicEnergy.sample > 0.0);
}

void TestMcamExactMatchStateVariationIsDeterministic() {
    Fixture fixture = MakeMcamFixture();
    CAM_SubArray &subarray = *fixture.subarray;
    auto &cell = *fixture.config->technology.cell;
    cell.hasMcamStateVariations = true;
    for (int state = 0; state < cell.numResistanceState; state++) {
        cell.resStateVariation[state] = 0.15;
    }

    VariationConfig &variation = fixture.config->variation;
    variation.enabled = true;
    variation.mode = "monte_carlo";
    variation.samples = 7;
    variation.seed = 9876u;
    variation.monteCarloGranularity = "cell";

    std::vector<int> stored(subarray.CAM_opt.BitSerialWidth);
    for (size_t index = 0; index < stored.size(); index++) {
        stored[index] = static_cast<int>(index % cell.numResistanceState);
    }
    std::vector<int> query = stored;
    query[0] = (query[0] + 1) % cell.numResistanceState;

    const EvaCAMMatchResult first = subarray.EvaluateMcamExactMatch(stored, query);
    const EvaCAMMatchResult repeated = subarray.EvaluateMcamExactMatch(stored, query);
    assert(!first.hit && !repeated.hit);
    assert(first.searchLatency == repeated.searchLatency);
    assert(first.searchDynamicEnergy == repeated.searchDynamicEnergy);
    assert(first.matchlineDelay == repeated.matchlineDelay);
    assert(first.senseMargin == repeated.senseMargin);
    assert(std::isfinite(first.searchLatency) && first.searchLatency > 0);
    assert(std::isfinite(first.searchDynamicEnergy) && first.searchDynamicEnergy > 0);

    variation.seed++;
    const EvaCAMMatchResult differentSeed =
            subarray.EvaluateMcamExactMatch(stored, query);
    assert(!Near(first.matchlineDelay, differentSeed.matchlineDelay)
            || !Near(first.senseMargin, differentSeed.senseMargin));
}

}  // namespace

int main() {
    TestResistanceSamplingIsDeterministicAndStreamSeparated();
    TestMonteCarloCellAndCornerResistanceSamples();
    TestTimingSummaryHandlesDisabledSinglePointAndMonteCarlo();
    TestPowerSummaryAndCellReadEnergy();
    TestMcamExactMatchStateVariationIsDeterministic();
    std::cout << "CAM_SubArray variation tests passed\n";
    return 0;
}
