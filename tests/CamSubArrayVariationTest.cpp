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
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <map>
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
    const auto configPath = McamTestConfig::WriteMcamConfigVariant(*temporary,
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
    assert(first.squaredEuclideanDistance == 1);
    assert(first.squaredEuclideanDistance == repeated.squaredEuclideanDistance);
    assert(first.matchlineConductance == repeated.matchlineConductance);
    assert(first.matchlineVoltage == repeated.matchlineVoltage);
    assert(std::isfinite(first.searchLatency) && first.searchLatency > 0);
    assert(std::isfinite(first.searchDynamicEnergy) && first.searchDynamicEnergy > 0);
    assert(std::isfinite(first.matchlineConductance)
            && first.matchlineConductance > 0);
    assert(std::isfinite(first.matchlineVoltage)
            && first.matchlineVoltage >= 0);

    variation.seed++;
    const EvaCAMMatchResult differentSeed =
            subarray.EvaluateMcamExactMatch(stored, query);
    assert(!Near(first.matchlineDelay, differentSeed.matchlineDelay)
            || !Near(first.senseMargin, differentSeed.senseMargin));
}

void TestMcamSamplesAndBounds() {
    Fixture fixture = MakeMcamFixture();
    auto &subarray = *fixture.subarray;
    auto &cell = *fixture.config->technology.cell;
    // Tiny geometry permits exhaustive stored/query enumeration without
    // reconstructing electrical fixtures; sensing remains fixed and nominal.
    subarray.CAM_opt.BitSerialWidth = 2;
    fixture.config->variation.enabled = false;
    for (int q0 = 0; q0 < cell.numResistanceState; ++q0) {
        for (int q1 = 0; q1 < cell.numResistanceState; ++q1) {
            const std::vector<int> query{q0, q1};
            const auto bounds = subarray.McamDistanceVoltageBounds(query);
            std::map<double, std::pair<double, double>> exhaustive;
            std::map<double, std::pair<double, double>> exhaustiveLatency;
            for (int a = 0; a < cell.numResistanceState; ++a) {
                for (int b = 0; b < cell.numResistanceState; ++b) {
                    const auto samples = subarray.EvaluateMcamDistanceSamples({a, b}, query);
                    assert(samples.size() == 1);
                    const auto &sample = samples.front();
                    auto entry = exhaustive.emplace(sample.squaredEuclideanDistance,
                            std::make_pair(sample.matchlineConductance, sample.matchlineConductance));
                    entry.first->second.first = std::min(entry.first->second.first, sample.matchlineConductance);
                    entry.first->second.second = std::max(entry.first->second.second, sample.matchlineConductance);
                    auto timing = exhaustiveLatency.emplace(sample.squaredEuclideanDistance,
                            std::make_pair(sample.searchLatency, sample.searchLatency));
                    timing.first->second.first = std::min(timing.first->second.first, sample.searchLatency);
                    timing.first->second.second = std::max(timing.first->second.second, sample.searchLatency);
                }
            }
            assert(bounds.size() == exhaustive.size());
            size_t index = 0;
            for (const auto &entry : exhaustive) {
                const auto &bound = bounds[index++];
                assert(bound.squaredEuclideanDistance == entry.first);
                assert(Near(bound.minimumConductance, entry.second.first));
                assert(Near(bound.maximumConductance, entry.second.second));
                assert(Near(bound.minimumVoltage, subarray.McamSensedVoltage(entry.second.second)));
                assert(Near(bound.maximumVoltage, subarray.McamSensedVoltage(entry.second.first)));
                assert(Near(bound.minimumSearchLatency, exhaustiveLatency.at(entry.first).first));
                assert(Near(bound.maximumSearchLatency, exhaustiveLatency.at(entry.first).second));
                assert(bound.minimumConductanceDeltaCounts.size()
                        == static_cast<size_t>(cell.numResistanceState));
                assert(bound.maximumConductanceDeltaCounts.size()
                        == static_cast<size_t>(cell.numResistanceState));
                for (const auto &counts : {bound.minimumConductanceDeltaCounts,
                                          bound.maximumConductanceDeltaCounts}) {
                    assert(std::accumulate(counts.begin(), counts.end(), 0) == 2);
                    int witnessDistance = 0;
                    for (size_t delta = 0; delta < counts.size(); ++delta) {
                        witnessDistance += counts[delta] * static_cast<int>(delta * delta);
                    }
                    assert(witnessDistance == entry.first);
                }
            }
        }
    }
    ConfigureMonteCarlo(fixture, 11);
    cell.hasMcamStateVariations = true;
    for (int state = 0; state < cell.numResistanceState; ++state) {
        cell.resStateVariation[state] = 0.4; // Exercises the positive resistance floor.
    }
    const std::vector<int> query{1, 2};
    for (const std::string granularity : {"cell", "effective"}) {
        fixture.config->variation.monteCarloGranularity = granularity;
        const auto bounds = subarray.McamDistanceVoltageBounds(query);
        // The recovered compositions independently reproduce both resistance
        // endpoints, including the exact-match boundary timing convention.
        for (const auto &bound : bounds) {
            const auto low = subarray.EvaluateMcamZeroQueryComposition(
                    bound.minimumConductanceDeltaCounts, 3);
            const auto high = subarray.EvaluateMcamZeroQueryComposition(
                    bound.maximumConductanceDeltaCounts, -3);
            assert(Near(low.squaredEuclideanDistance, bound.squaredEuclideanDistance));
            assert(Near(high.squaredEuclideanDistance, bound.squaredEuclideanDistance));
            assert(Near(low.matchlineConductance, bound.minimumConductance));
            assert(Near(high.matchlineConductance, bound.maximumConductance));
            assert(Near(low.matchlineVoltage, bound.maximumVoltage));
            assert(Near(high.matchlineVoltage, bound.minimumVoltage));
            assert(Near(high.searchLatency, bound.minimumSearchLatency));
            assert(Near(low.searchLatency, bound.maximumSearchLatency));
        }
        for (int a = 0; a < cell.numResistanceState; ++a) {
            for (int b = 0; b < cell.numResistanceState; ++b) {
                const auto samples = subarray.EvaluateMcamDistanceSamples({a, b}, query);
                const auto repeated = subarray.EvaluateMcamDistanceSamples({a, b}, query);
                assert(samples.size() == 11);
                double voltage = 0, conductance = 0, latency = 0, energy = 0;
                for (size_t index = 0; index < samples.size(); ++index) {
                    const auto &sample = samples[index];
                    assert(sample.matchlineVoltage == repeated[index].matchlineVoltage);
                    const auto bound = std::find_if(bounds.begin(), bounds.end(), [&](const auto &value) {
                        return value.squaredEuclideanDistance == sample.squaredEuclideanDistance;
                    });
                    assert(bound != bounds.end());
                    assert(sample.matchlineConductance >= bound->minimumConductance);
                    assert(sample.matchlineConductance <= bound->maximumConductance);
                    assert(sample.matchlineVoltage >= bound->minimumVoltage);
                    assert(sample.matchlineVoltage <= bound->maximumVoltage);
                    assert(sample.searchLatency >= bound->minimumSearchLatency);
                    assert(sample.searchLatency <= bound->maximumSearchLatency);
                    voltage += sample.matchlineVoltage;
                    conductance += sample.matchlineConductance;
                    latency += sample.searchLatency;
                    energy += sample.searchDynamicEnergy;
                }
                const auto mean = subarray.EvaluateMcamDistance({a, b}, query);
                assert(mean.matchlineVoltage == voltage / samples.size());
                assert(mean.matchlineConductance == conductance / samples.size());
                assert(mean.searchLatency == latency / samples.size());
                assert(mean.searchDynamicEnergy == energy / samples.size());
            }
        }
    }
    for (int state = 0; state < cell.numResistanceState; ++state) {
        cell.resStateVariation[state] = 0;
    }
    const auto zero = subarray.EvaluateMcamDistanceSamples(query, query);
    assert(zero.size() == 11);
    for (const auto &sample : zero) {
        assert(sample.matchlineVoltage == zero.front().matchlineVoltage);
    }
    fixture.config->variation.mode = "single_point";
    assert(subarray.EvaluateMcamDistanceSamples(query, query).size() == 1);
    fixture.config->variation.mode = "corner";
    TestSupport::AssertThrows<std::invalid_argument>([&] {
        subarray.EvaluateMcamDistanceSamples(query, query);
    }, "does not support");
    TestSupport::AssertThrows<std::invalid_argument>([&] {
        subarray.McamDistanceVoltageBounds(query);
    }, "does not support");
    fixture.config->variation.enabled = false;
    TestSupport::AssertThrows<std::invalid_argument>([&] {
        subarray.EvaluateMcamZeroQueryComposition(
                std::vector<int>(cell.numResistanceState, std::numeric_limits<int>::max()), 0);
    }, "delta counts");
    TestSupport::AssertThrows<std::invalid_argument>([&] {
        subarray.McamDistanceVoltageBounds({-1, 0});
    }, "symbols");
    TestSupport::AssertThrows<std::invalid_argument>([&] {
        subarray.EvaluateMcamDistanceSamples({}, query);
    }, "BitSerialWidth");
}

void TestMcamRawStateSamplingOracleAndCacheInvalidation() {
    Fixture fixture = MakeMcamFixture();
    Fixture alternate = MakeMcamFixture();
    auto &subarray = *fixture.subarray;
    auto &cell = *fixture.config->technology.cell;
    assert(cell.numResistanceState == 8);
    subarray.CAM_opt.BitSerialWidth = 16;
    ConfigureMonteCarlo(fixture, 9);
    cell.hasMcamStateVariations = true;

    std::vector<double> descending(cell.ResistanceState,
            cell.ResistanceState + cell.numResistanceState);
    std::sort(descending.begin(), descending.end(), std::greater<double>());
    const int rawToDistance[] = {3, 0, 6, 1, 7, 2, 5, 4};
    const int distanceToRaw[] = {1, 3, 5, 0, 7, 6, 2, 4};
    for (int raw = 0; raw < 8; ++raw) {
        cell.ResistanceState[raw] = descending[rawToDistance[raw]];
        cell.resStateVariation[raw] = 0.04 + 0.035 * raw;
    }
    const std::vector<int> query(16, 0);
    std::vector<int> stored(16);
    for (int index = 0; index < 16; ++index) {
        stored[index] = index % 8;
    }
    // Independently reproduce the original seed contract, retaining unsigned
    // 32-bit overflow and the raw-state stream offset. The mapping is fixed
    // by nominal resistance even when sampled resistances cross one another.
    const auto verify = [&]() {
        const auto &variation = fixture.config->variation;
        const auto samples = subarray.EvaluateMcamDistanceSamples(stored, query);
        assert(samples.size() == static_cast<size_t>(variation.samples));
        for (size_t sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
            double expected = 0;
            for (size_t index = 0; index < stored.size(); ++index) {
                const int raw = distanceToRaw[stored[index]];
                uint32_t seed = variation.seed;
                seed ^= static_cast<uint32_t>(sampleIndex) + 0x9e3779b9u
                    + (seed << 6) + (seed >> 2);
                seed ^= static_cast<uint32_t>(100 + raw) + 0x85ebca6bu
                    + (seed << 6) + (seed >> 2);
                const uint32_t coordinate = variation.monteCarloGranularity == "cell"
                    ? static_cast<uint32_t>(index) : 0;
                seed ^= coordinate + 0xc2b2ae35u + (seed << 6) + (seed >> 2);
                VariationSampler sampler(seed);
                expected += 1.0 / sampler.SampleResistance(
                        cell.ResistanceState[raw], cell.resStateVariation[raw]);
            }
            // Match the existing effective-resistance round trip exactly.
            assert(samples[sampleIndex].matchlineConductance == 1.0 / (1.0 / expected));
        }
        return samples;
    };
    for (const std::string granularity : {"cell", "effective"}) {
        fixture.config->variation.monteCarloGranularity = granularity;
        const auto original = verify();
        verify(); // Cached reads must match fresh draws.
        ++fixture.config->variation.seed;
        const auto changedSeed = verify();
        assert(original.front().matchlineConductance != changedSeed.front().matchlineConductance);
        cell.resStateVariation[3] += 0.12;
        const auto changedDeviation = verify();
        assert(changedSeed.front().matchlineConductance != changedDeviation.front().matchlineConductance);
        cell.ResistanceState[3] *= 1.01; // Preserve nominal ordering, change distribution.
        verify();

        // Evict this thread's cache with a different subarray/distribution,
        // then return to the first distribution and compare every sample.
        ConfigureMonteCarlo(alternate, 3);
        alternate.config->technology.cell->hasMcamStateVariations = true;
        for (int raw = 0; raw < 8; ++raw) {
            alternate.config->technology.cell->resStateVariation[raw] = 0.02;
        }
        const std::vector<int> alternateQuery(alternate.subarray->CAM_opt.BitSerialWidth, 0);
        alternate.subarray->EvaluateMcamDistanceSamples(alternateQuery, alternateQuery);
        verify();
    }
}

}  // namespace

int main() {
    TestResistanceSamplingIsDeterministicAndStreamSeparated();
    TestMonteCarloCellAndCornerResistanceSamples();
    TestTimingSummaryHandlesDisabledSinglePointAndMonteCarlo();
    TestPowerSummaryAndCellReadEnergy();
    TestMcamExactMatchStateVariationIsDeterministic();
    TestMcamSamplesAndBounds();
    TestMcamRawStateSamplingOracleAndCacheInvalidation();
    std::cout << "CAM_SubArray variation tests passed\n";
    return 0;
}
