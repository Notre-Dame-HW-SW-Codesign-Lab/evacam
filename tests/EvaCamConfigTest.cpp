#include "EvaCamConfig.h"
#include "Result.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

class TestBank : public Bank {
    public:
        void Initialize(int, int, long long, long, int, int, int, bool, int, int,
                int, int, int, int, BufferDesignTarget, CAMType, SearchFunction,
                std::shared_ptr<EvaCamConfig>, const Wire &, const Wire &, const CAM_Opt &) override {}
        void CalculateArea() override {}
        void CalculateRC() override {}
        void CalculateLatencyAndPower() override {}
};

bool DomainsEqual(const IntValueDomain &actual, const IntValueDomain &expected) {
    return actual.Kind() == expected.Kind() && actual.Values() == expected.Values();
}

std::shared_ptr<Result> MakeResult(double readLatency, double writeLatency,
        double readEnergy, double writeEnergy, double area, double leakage) {
    auto result = std::make_shared<Result>();
    result->bank = std::make_shared<TestBank>();
    result->bank->readLatency = readLatency;
    result->bank->writeLatency = writeLatency;
    result->bank->readDynamicEnergy = readEnergy;
    result->bank->writeDynamicEnergy = writeEnergy;
    result->bank->area = area;
    result->bank->leakage = leakage;
    return result;
}

std::vector<std::shared_ptr<Result>> MakeOptimizationResults() {
    std::vector<std::shared_ptr<Result>> results(full_exploration);
    for (int index = read_latency_optimized; index < full_exploration; index++) {
        results[index] = MakeResult(2.0 + index, 3.0 + index, 5.0 + index,
                7.0 + index, 11.0 + index, 13.0 + index);
    }
    return results;
}

void TestConstructionDefaults() {
    EvaCamConfig config;

    assert(config.input.processNode == 90);
    assert(config.input.temperature == 300);
    assert(config.input.capacity == 0);
    assert(config.input.wordWidth == 0);
    assert(config.input.vectorDimensions == 0);
    assert(!config.useCactiAssumption);
    assert(!config.requestDeepExploration);
    assert(config.variationPlots);
    assert(!config.variation.enabled);
    assert(config.variation.mode == "nominal");
    assert(!config.technology.tech);
    assert(!config.technology.cell);
    assert(!config.exploration.deepExploration);
    assert(DomainsEqual(config.exploration.geometry.numRowMat,
            IntValueDomain::PowersOfTwo(1, 16)));
    assert(config.resolvedExploration.geometry.numRowMatValues
            == std::vector<int>({1, 2, 4, 8, 16}));
}

void TestSetDeepExplorationEnabledResetsAndExpandsDomains() {
    EvaCamConfig config;
    config.exploration.geometry.numRowMat = IntValueDomain::FixedSet({3});

    config.SetDeepExploration(true);

    assert(config.exploration.deepExploration);
    assert(DomainsEqual(config.exploration.geometry.numRowMat,
            IntValueDomain::PowersOfTwo(1, 64)));
    assert(DomainsEqual(config.exploration.geometry.numRowSubarray,
            IntValueDomain::PowersOfTwo(1, 16)));
    assert(config.resolvedExploration.geometry.numRowMatValues.back() == 64);
    assert(config.resolvedExploration.geometry.numRowSubarrayValues.back() == 16);
}

void TestSetDeepExplorationDisabledUsesRestrictedDomains() {
    EvaCamConfig config;
    config.SetDeepExploration(true);

    config.SetDeepExploration(false);

    assert(!config.exploration.deepExploration);
    assert(DomainsEqual(config.exploration.geometry.numRowMat,
            IntValueDomain::PowersOfTwo(1, 4)));
    assert(DomainsEqual(config.exploration.geometry.numRowSubarray,
            IntValueDomain::PowersOfTwo(1, 8)));
    assert(DomainsEqual(config.exploration.wires.localWireType,
            IntValueDomain::Sequential(local_aggressive, semi_conservative)));
    assert(DomainsEqual(config.exploration.wires.globalWireType,
            IntValueDomain::Sequential(semi_aggressive, global_conservative)));
    assert(config.resolvedExploration.geometry.numRowMatValues
            == std::vector<int>({1, 2, 4}));
}

void TestBuildResultLimitsUsesEachOptimizationMetric() {
    auto config = TestModelBuilders::MakeEvaCamConfig();
    config->constraints.readLatency = 0.5;
    config->constraints.writeLatency = 1.0;
    config->constraints.readDynamicEnergy = 1.5;
    config->constraints.writeDynamicEnergy = 2.0;
    config->constraints.readEdp = 2.5;
    config->constraints.writeEdp = 3.0;
    config->constraints.area = 3.5;
    config->constraints.leakage = 4.0;
    const auto results = MakeOptimizationResults();

    const ResultLimits limits = config->BuildResultLimits(results);

    TestSupport::AssertNear(limits.readLatency,
            results[read_latency_optimized]->bank->readLatency * 1.5);
    TestSupport::AssertNear(limits.writeLatency,
            results[write_latency_optimized]->bank->writeLatency * 2.0);
    TestSupport::AssertNear(limits.readDynamicEnergy,
            results[read_energy_optimized]->bank->readDynamicEnergy * 2.5);
    TestSupport::AssertNear(limits.writeDynamicEnergy,
            results[write_energy_optimized]->bank->writeDynamicEnergy * 3.0);
    TestSupport::AssertNear(limits.readEdp,
            results[read_edp_optimized]->bank->readLatency
                    * results[read_edp_optimized]->bank->readDynamicEnergy * 3.5);
    TestSupport::AssertNear(limits.writeEdp,
            results[write_edp_optimized]->bank->writeLatency
                    * results[write_edp_optimized]->bank->writeDynamicEnergy * 4.0);
    TestSupport::AssertNear(limits.area,
            results[area_optimized]->bank->area * 4.5);
    TestSupport::AssertNear(limits.leakage,
            results[leakage_optimized]->bank->leakage * 5.0);
}

void TestBuildResultLimitsRejectsMissingNullAndBanklessResults() {
    EvaCamConfig config;
    TestSupport::AssertThrows<std::invalid_argument>(
            [&config]() { (void)config.BuildResultLimits({}); },
            "populated result and bank");

    auto results = MakeOptimizationResults();
    results[read_energy_optimized].reset();
    TestSupport::AssertThrows<std::invalid_argument>(
            [&config, &results]() { (void)config.BuildResultLimits(results); },
            "populated result and bank");

    results = MakeOptimizationResults();
    results[area_optimized]->bank.reset();
    TestSupport::AssertThrows<std::invalid_argument>(
            [&config, &results]() { (void)config.BuildResultLimits(results); },
            "populated result and bank");
}

void TestApplyResultLimitsResetsAndUpdatesMultipleResults() {
    EvaCamConfig config;
    const ResultLimits limits{1, 2, 3, 4, 5, 6, 7, 8};
    auto first = MakeResult(10, 11, 12, 13, 14, 15);
    auto second = MakeResult(20, 21, 22, 23, 24, 25);

    config.ApplyResultLimits(limits, {first, second});

    for (const auto &result : {first, second}) {
        TestSupport::AssertNear(result->bank->readLatency, 1e41);
        TestSupport::AssertNear(result->bank->writeLatency, 1e41);
        TestSupport::AssertNear(result->bank->readDynamicEnergy, 1e41);
        TestSupport::AssertNear(result->bank->writeDynamicEnergy, 1e41);
        TestSupport::AssertNear(result->bank->area, 1e41);
        TestSupport::AssertNear(result->bank->leakage, 1e41);
        TestSupport::AssertNear(result->limitReadLatency, limits.readLatency);
        TestSupport::AssertNear(result->limitWriteLatency, limits.writeLatency);
        TestSupport::AssertNear(result->limitReadDynamicEnergy, limits.readDynamicEnergy);
        TestSupport::AssertNear(result->limitWriteDynamicEnergy, limits.writeDynamicEnergy);
        TestSupport::AssertNear(result->limitReadEdp, limits.readEdp);
        TestSupport::AssertNear(result->limitWriteEdp, limits.writeEdp);
        TestSupport::AssertNear(result->limitArea, limits.area);
        TestSupport::AssertNear(result->limitLeakage, limits.leakage);
    }
}

void TestApplyResultLimitsAcceptsEmptyResults() {
    EvaCamConfig config;
    const ResultLimits limits{};

    config.ApplyResultLimits(limits, {});
}

void TestApplyResultLimitsRejectsNullAndBanklessResultsBeforeMutation() {
    EvaCamConfig config;
    const ResultLimits limits{1, 2, 3, 4, 5, 6, 7, 8};
    auto valid = MakeResult(10, 11, 12, 13, 14, 15);
    std::shared_ptr<Result> missing;
    TestSupport::AssertThrows<std::invalid_argument>(
            [&config, &limits, &valid, &missing]() {
                config.ApplyResultLimits(limits, {valid, missing});
            },
            "every result to contain a bank");
    TestSupport::AssertNear(valid->bank->readLatency, 10);

    auto bankless = std::make_shared<Result>();
    TestSupport::AssertThrows<std::invalid_argument>(
            [&config, &limits, &bankless]() {
                config.ApplyResultLimits(limits, {bankless});
            },
            "every result to contain a bank");
}

void TestReadConfigFromFileLoadsAndInitializesTechnology() {
    TestSupport::TemporaryDirectory directory("evacam-config");
    const std::filesystem::path root = std::filesystem::current_path();
    const std::filesystem::path configPath = directory.WriteFile("minimal.config.yaml",
            "schema: config\n"
            "name: EvaCamConfigTest\n"
            "architecture: " + (root / "config/2FeFET_TCAM/2FeFET_TCAM.architecture.yaml").string() + "\n"
            "cell: " + (root / "config/2FeFET_TCAM/2FeFET_TCAM.cell.yaml").string() + "\n"
            "technology: " + (root / "config/lib/technology/cmos.legacy.yaml").string() + "\n"
            "optimization:\n"
            "  target: LeakagePower\n"
            "  buffer_design: latency\n"
            "  row_driver: latency\n"
            "  priority_encoder: latency\n");
    EvaCamConfig config;

    config.ReadConfigFromFile(configPath.string());

    assert(config.input.capacity == 512);
    assert(config.input.wordWidth == 64);
    assert(config.input.temperature == 350);
    assert(config.input.optimizationTarget == leakage_optimized);
    assert(config.technology.tech);
    assert(config.technology.tech->initialized());
    assert(config.technology.cell);
    assert(config.technology.cell->camType == TCAM);
}

void TestReadConfigFromFileRejectsMissingFile() {
    EvaCamConfig config;

    TestSupport::AssertThrows<YAML::BadFile>(
            [&config]() { config.ReadConfigFromFile("tests/no-such-config.yaml"); }, "");
}

}  // namespace

int main() {
    TestConstructionDefaults();
    TestSetDeepExplorationEnabledResetsAndExpandsDomains();
    TestSetDeepExplorationDisabledUsesRestrictedDomains();
    TestBuildResultLimitsUsesEachOptimizationMetric();
    TestBuildResultLimitsRejectsMissingNullAndBanklessResults();
    TestApplyResultLimitsResetsAndUpdatesMultipleResults();
    TestApplyResultLimitsAcceptsEmptyResults();
    TestApplyResultLimitsRejectsNullAndBanklessResultsBeforeMutation();
    TestReadConfigFromFileLoadsAndInitializesTechnology();
    TestReadConfigFromFileRejectsMissingFile();

    std::cout << "EvaCamConfig tests passed" << std::endl;
    return 0;
}
