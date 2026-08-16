#include "EvaCamConfig.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "Result.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"
#include "UnitFormatter.h"
#include "input/CliOptions.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

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

std::shared_ptr<Result> MakeResult(double metric = 10.0) {
    auto result = std::make_shared<Result>();
    result->config = TestModelBuilders::MakeEvaCamConfig();
    result->bank = std::make_shared<TestBank>();
    result->bank->readLatency = metric;
    result->bank->writeLatency = metric + 1;
    result->bank->readDynamicEnergy = metric + 2;
    result->bank->writeDynamicEnergy = metric + 3;
    result->bank->searchLatency = metric + 4;
    result->bank->searchDynamicEnergy = metric + 5;
    result->bank->area = metric + 6;
    result->bank->leakage = metric + 7;
    result->localWire = TestModelBuilders::MakeWire(result->config, local_aggressive);
    result->globalWire = TestModelBuilders::MakeWire(result->config, global_conservative);
    result->limitReadLatency = result->limitWriteLatency = 1e41;
    result->limitReadDynamicEnergy = result->limitWriteDynamicEnergy = 1e41;
    result->limitReadEdp = result->limitWriteEdp = 1e41;
    result->limitArea = result->limitLeakage = 1e41;
    return result;
}

void TestConstructorInitializeAndReset() {
    Result result;
    assert(!result.initialized);
    auto config = TestModelBuilders::MakeEvaCamConfig();
    result.Initialize(config);
    assert(result.initialized);
    assert(result.config == config);
    assert(result.bank);
    assert(result.optimizationTarget == read_latency_optimized);
    assert(result.bank->readLatency == 1e41);
    assert(result.limitArea == 1e41);
    result.bank->readLatency = 1;
    result.bank->searchLatency = 1;
    result.reset();
    assert(result.bank->readLatency == 1e41);
    assert(result.bank->searchLatency == 1e41);
}

void TestCopySharesBankButCopiesWireSnapshots() {
    auto original = MakeResult();
    Result copy(*original);
    assert(copy.bank == original->bank);
    assert(copy.localWire.featureSizeInNano == original->localWire.featureSizeInNano);
    original->localWire.Initialize(65, semi_aggressive, repeated_none, 300, true, original->config);
    assert(copy.localWire.featureSizeInNano == 90);
    original->bank->readLatency = 99;
    assert(copy.bank->readLatency == 99);
}

double TargetMetric(const Result &result, OptimizationTarget target) {
    const Bank &bank = *result.bank;
    switch (target) {
        case read_latency_optimized: return bank.readLatency;
        case write_latency_optimized: return bank.writeLatency;
        case read_energy_optimized: return bank.readDynamicEnergy;
        case write_energy_optimized: return bank.writeDynamicEnergy;
        case read_edp_optimized: return bank.readLatency * bank.readDynamicEnergy;
        case write_edp_optimized: return bank.writeLatency * bank.writeDynamicEnergy;
        case area_optimized: return bank.area;
        case leakage_optimized: return bank.leakage;
        case search_latency_optimized: return bank.searchLatency;
        case search_energy_optimized: return bank.searchDynamicEnergy;
        case search_edp_optimized: return bank.searchLatency * bank.searchDynamicEnergy;
        default: return 0;
    }
}

void TestEveryOptimizationTargetAndTies() {
    const OptimizationTarget targets[] = {read_latency_optimized, write_latency_optimized,
        read_energy_optimized, write_energy_optimized, read_edp_optimized,
        write_edp_optimized, area_optimized, leakage_optimized,
        search_latency_optimized, search_energy_optimized, search_edp_optimized};
    for (OptimizationTarget target : targets) {
        auto best = MakeResult(10);
        auto candidate = MakeResult(9);
        best->optimizationTarget = target;
        best->compareAndUpdate(candidate);
        assert(best->bank == candidate->bank);

        auto tied = MakeResult(3);
        auto current = MakeResult(3);
        current->optimizationTarget = target;
        tied->bank->readLatency = current->bank->readLatency;
        tied->bank->writeLatency = current->bank->writeLatency;
        tied->bank->readDynamicEnergy = current->bank->readDynamicEnergy;
        tied->bank->writeDynamicEnergy = current->bank->writeDynamicEnergy;
        tied->bank->searchLatency = current->bank->searchLatency;
        tied->bank->searchDynamicEnergy = current->bank->searchDynamicEnergy;
        tied->bank->area = current->bank->area;
        tied->bank->leakage = current->bank->leakage;
        current->compareAndUpdate(tied);
        assert(current->bank != tied->bank);
        assert(TargetMetric(*current, target) == TargetMetric(*tied, target));
    }
}

void TestConstraintsRejectCandidatesAndUpdatesWireSnapshots() {
    auto best = MakeResult(10);
    auto candidate = MakeResult(1);
    best->optimizationTarget = read_latency_optimized;
    best->limitArea = 1;
    best->compareAndUpdate(candidate);
    assert(best->bank != candidate->bank);
    best->limitArea = 1e41;
    candidate->localWire.Initialize(32, semi_aggressive, repeated_none, 350, true, candidate->config);
    candidate->globalWire.Initialize(45, global_aggressive, repeated_opt, 300, false, candidate->config);
    best->compareAndUpdate(candidate);
    assert(best->bank == candidate->bank);
    candidate->localWire.Initialize(65, local_conservative, repeated_none, 300, false, candidate->config);
    assert(best->localWire.featureSizeInNano == 32);
    assert(best->globalWire.featureSizeInNano == 45);
}

void TestCsvLabels() {
    auto result = MakeResult(1);
    auto &bank = *result->bank;
    bank.numRowMat = 2; bank.numColumnMat = 3; bank.numActiveMatPerColumn = 1; bank.numActiveMatPerRow = 2;
    bank.numRowSubarray = 4; bank.numColumnSubarray = 5; bank.numActiveSubarrayPerColumn = 3; bank.numActiveSubarrayPerRow = 2;
    bank.muxSenseAmp = 1; bank.muxOutputLev1 = 2; bank.muxOutputLev2 = 3; bank.areaOptimizationLevel = latency_first;
    bank.mat = std::make_unique<Mat>();
    bank.mat->subarray = std::make_unique<CAM_SubArray>();
    bank.mat->subarray->numRow = 16; bank.mat->subarray->numColumn = 32;
    bank.height = 1e-6; bank.width = 2e-6; bank.mat->height = 3e-6; bank.mat->width = 4e-6;
    bank.mat->subarray->height = 5e-6; bank.mat->subarray->width = 6e-6;
    const WireType wireTypes[] = {local_aggressive, local_conservative, semi_aggressive,
        semi_conservative, global_aggressive, global_conservative, dram_wordline};
    const char *wireLabels[] = {"Local Aggressive", "Local Conservative",
        "Semi-Global Aggressive", "Semi-Global Conservative", "Global Aggressive",
        "Global Conservative", "DRAM Wire"};
    const WireRepeaterType repeaterTypes[] = {repeated_none, repeated_opt, repeated_5,
        repeated_10, repeated_20, repeated_30, repeated_40, repeated_50};
    const char *repeaterLabels[] = {"No Repeaters", "Fully-Optimized Repeaters",
        "Repeaters with 5% Overhead", "Repeaters with 10% Overhead",
        "Repeaters with 20% Overhead", "Repeaters with 30% Overhead",
        "Repeaters with 40% Overhead", "Repeaters with 50% Overhead"};
    for (int index = 0; index < 7; index++) {
        result->localWire.wireType = wireTypes[index];
        result->globalWire.wireType = wireTypes[index];
        std::ostringstream output;
        result->printToCsvFile(output);
        assert(output.str().find(wireLabels[index]) != std::string::npos);
    }
    for (int index = 0; index < 8; index++) {
        result->localWire.wireRepeaterType = repeaterTypes[index];
        result->globalWire.wireRepeaterType = repeaterTypes[index];
        std::ostringstream output;
        result->printToCsvFile(output);
        assert(output.str().find(repeaterLabels[index]) != std::string::npos);
    }
    const BufferDesignTarget designs[] = {latency_first, area_first, latency_area_trade_off};
    const char *designLabels[] = {"Latency-Optimized", "Area-Optimized", "Balanced"};
    for (int index = 0; index < 3; index++) {
        bank.areaOptimizationLevel = designs[index];
        std::ostringstream output;
        result->printToCsvFile(output);
        assert(output.str().find(designLabels[index]) != std::string::npos);
    }
}

void TestConsolePrintForCalculatedResult() {
    CliOptions options;
    options.inputFileName =
            "config/2FeFET_TCAM/2FeFET_TCAM_explicit_subarray.config.yaml";
    TestSupport::StreamCapture capture(std::cout);
    EvaCamContext context = EvaCamContextBuilder::Build(options);
    EvaCamExplorationResult exploration = EvaCamExplorer(context.config, 1).Run();
    assert(exploration.numSolution > 0);
    const auto &result = exploration.bestResults.at(leakage_optimized);
    assert(result);
    result->print();
    capture.Stop();
    const std::string output = capture.Text();
    assert(output.find("CONFIGURATION") != std::string::npos);
    assert(output.find("SUMMARY RESULT") != std::string::npos);
    assert(output.find("Subarray Dimensions") != std::string::npos);
    std::ostringstream expectedSearchLatency;
    expectedSearchLatency << "Search Latency = "
                          << ToSecond(result->bank->searchLatency);
    assert(output.find(expectedSearchLatency.str()) != std::string::npos);
    const size_t leakageStart = output.find("Subarray Leakage Power");
    assert(leakageStart != std::string::npos);
    const size_t leakageEnd = output.find('\n', leakageStart);
    assert(output.substr(leakageStart, leakageEnd - leakageStart).find('J')
            == std::string::npos);
}

}  // namespace

int main() {
    TestConstructorInitializeAndReset();
    TestCopySharesBankButCopiesWireSnapshots();
    TestEveryOptimizationTargetAndTies();
    TestConstraintsRejectCandidatesAndUpdatesWireSnapshots();
    TestCsvLabels();
    TestConsolePrintForCalculatedResult();
    std::cout << "Result model tests passed" << std::endl;
}
