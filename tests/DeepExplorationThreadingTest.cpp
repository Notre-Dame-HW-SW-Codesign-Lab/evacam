#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "CandidateSpec.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "Result.h"
#include "TestSupport.h"
#include "input/CliOptions.h"

namespace {

// Active-mat counts remain capped at 16 even when total mats expand to 64:
// (1+2+3+4+5+5+5)^2 * (1+2+3+4+5)^2 * 7^3.
constexpr long long kExpectedRawCandidates = 48234375;
constexpr long long kExpectedModeledCandidates = 9161187;
constexpr std::array<int, 5> kThreadCounts = {1, 2, 3, 7, 16};

EvaCamExplorerTestHooks MakeYieldHooks() {
    EvaCamExplorerTestHooks hooks;
    hooks.schedulerPoint = []() {
        std::this_thread::yield();
    };
    hooks.beforeGeometry = [](std::size_t outerIndex) {
        for (std::size_t iteration = 0; iteration <= outerIndex % 3; ++iteration) {
            std::this_thread::yield();
        }
    };
    return hooks;
}

std::string AbsolutePath(const std::string &path) {
    return std::filesystem::absolute(path).lexically_normal().string();
}

std::filesystem::path WriteDeepExplorationConfig(
        const TestSupport::TemporaryDirectory &directory) {
    std::ostringstream architecture;
    architecture
        << "schema: architecture\n"
        << "design:\n"
        << "  target: CAM\n"
        << "  search_function: BE\n"
        << "  system_process_node: 45nm\n"
        << "  device_roadmap: HP\n"
        << "  temperature: 350K\n"
        << "memory:\n"
        << "  capacity: 16B\n"
        << "  word_width: 16bits\n"
        << "routing:\n"
        << "  type: H-tree\n"
        << "peripherals:\n"
        << "  input:\n"
        << "    buffer: false\n"
        << "    encoder: false\n"
        << "    custom_encoder: false\n"
        << "  output:\n"
        << "    buffer: false\n"
        << "    priority_encoder: false\n"
        << "    accumulator: false\n"
        << "  write_driver: true\n"
        << "wires:\n"
        << "  local:\n"
        << "    type: LocalConservative\n"
        << "    repeater: RepeatedNone\n"
        << "    low_swing: false\n"
        << "  global:\n"
        << "    type: GlobalAggressive\n"
        << "    repeater: RepeatedNone\n"
        << "    low_swing: false\n"
        << "sensing: " << AbsolutePath(
                "config/2FeFET_TCAM/2FeFET_TCAM.sensing.yaml") << "\n"
        << "matchline:\n"
        << "  additional_cap: 6fF\n"
        << "  match_transistor:\n"
        << "    cmos_width: 3F\n";
    const std::filesystem::path architecturePath = directory.WriteFile(
            "deep-threading.architecture.yaml", architecture.str());

    std::ostringstream config;
    config
        << "schema: config\n"
        << "name: deep-exploration-threading\n"
        << "architecture: " << architecturePath.string() << "\n"
        << "cell: " << AbsolutePath(
                "config/2FeFET_TCAM/2FeFET_TCAM.cell.yaml") << "\n"
        << "technology: " << AbsolutePath(
                "config/lib/technology/cmos.legacy.yaml") << "\n"
        << "optimization:\n"
        << "  target: WriteLatency\n"
        << "  deep_exploration: true\n"
        << "  buffer_design: latency\n"
        << "  row_driver: latency\n"
        << "  priority_encoder: latency\n";
    return directory.WriteFile("deep-threading.config.yaml", config.str());
}

std::shared_ptr<EvaCamConfig> LoadConfig(const std::filesystem::path &path) {
    CliOptions options;
    options.inputFileName = path.string();
    options.variationPlots = false;
    return EvaCamContextBuilder::Build(options).config;
}

void AssertDomain(const std::vector<int> &actual,
        const std::vector<int> &expected) {
    assert(actual == expected);
}

void AssertDeepDomains(const EvaCamConfig &config) {
    const std::vector<int> deepMatAndMuxDomain = {1, 2, 4, 8, 16, 32, 64};
    const std::vector<int> subarrayDomain = {1, 2, 4, 8, 16};
    assert(config.requestDeepExploration);
    assert(config.exploration.deepExploration);
    AssertDomain(config.resolvedExploration.geometry.numRowMatValues,
            deepMatAndMuxDomain);
    AssertDomain(config.resolvedExploration.geometry.numColumnMatValues,
            deepMatAndMuxDomain);
    AssertDomain(config.resolvedExploration.geometry.numRowSubarrayValues,
            subarrayDomain);
    AssertDomain(config.resolvedExploration.geometry.numColumnSubarrayValues,
            subarrayDomain);
    AssertDomain(config.resolvedExploration.geometry.muxSenseAmpValues,
            deepMatAndMuxDomain);
    AssertDomain(config.resolvedExploration.geometry.muxOutputLev1Values,
            deepMatAndMuxDomain);
    AssertDomain(config.resolvedExploration.geometry.muxOutputLev2Values,
            deepMatAndMuxDomain);
    AssertDomain(config.exploration.ActiveMatPerRowValues(64), subarrayDomain);
    AssertDomain(config.exploration.ActiveMatPerColumnValues(64), subarrayDomain);
}

void AssertSameAccounting(const CandidateAccounting &actual,
        const CandidateAccounting &expected) {
    assert(actual.rawCandidates == expected.rawCandidates);
    assert(actual.structurallyRejected == expected.structurallyRejected);
    assert(actual.duplicateCandidates == expected.duplicateCandidates);
    assert(actual.modeledCandidates == expected.modeledCandidates);
    assert(actual.invalidCandidates == expected.invalidCandidates);
    assert(actual.validCandidates == expected.validCandidates);
    assert(actual.constraintRejected == expected.constraintRejected);
    assert(actual.constraintPassingCandidates == expected.constraintPassingCandidates);
    assert(actual.pruningRejectedCandidates == expected.pruningRejectedCandidates);
    assert(actual.retainedCandidates == expected.retainedCandidates);
    assert(actual.reconstructionEvaluations == expected.reconstructionEvaluations);
}

void AssertExpectedAccounting(const EvaCamExplorationResult &result) {
    const CandidateAccounting &accounting = result.candidateAccounting;
    assert(accounting.rawCandidates == kExpectedRawCandidates);
    assert(accounting.structurallyRejected
            == kExpectedRawCandidates - kExpectedModeledCandidates);
    assert(accounting.duplicateCandidates == 0);
    assert(accounting.modeledCandidates == kExpectedModeledCandidates);
    assert(accounting.invalidCandidates + accounting.validCandidates
            == accounting.modeledCandidates);
    assert(accounting.validCandidates == result.numSolution);
    assert(accounting.constraintRejected == 0);
    assert(accounting.constraintPassingCandidates == accounting.validCandidates);
    assert(accounting.pruningRejectedCandidates == 0);
    assert(accounting.retainedCandidates == accounting.validCandidates);
    assert(accounting.reconstructionEvaluations == 0);
    assert(accounting.HasConsistentEnumerationCounts());
    assert(accounting.HasConsistentModelCounts());
    assert(accounting.HasConsistentFilteringCounts());
}

void AssertSameDesign(const Result &actual, const Result &expected) {
    assert(actual.optimizationTarget == expected.optimizationTarget);
    assert(actual.bank && expected.bank);
    assert(CandidateSpec::FromBank(*actual.bank)
            == CandidateSpec::FromBank(*expected.bank));
    const CandidateMetrics actualMetrics = CandidateMetrics::FromBank(*actual.bank);
    const CandidateMetrics expectedMetrics = CandidateMetrics::FromBank(*expected.bank);
    assert(actualMetrics.objectiveValues == expectedMetrics.objectiveValues);
    assert(actual.localWire.wireType == expected.localWire.wireType);
    assert(actual.localWire.wireRepeaterType
            == expected.localWire.wireRepeaterType);
    assert(actual.localWire.isLowSwing == expected.localWire.isLowSwing);
    assert(actual.globalWire.wireType == expected.globalWire.wireType);
    assert(actual.globalWire.wireRepeaterType
            == expected.globalWire.wireRepeaterType);
    assert(actual.globalWire.isLowSwing == expected.globalWire.isLowSwing);
}

void AssertEquivalentResults(const EvaCamExplorationResult &actual,
        const EvaCamExplorationResult &expected) {
    assert(actual.numSolution == expected.numSolution);
    AssertSameAccounting(actual.candidateAccounting,
            expected.candidateAccounting);
    assert(actual.bestResults.size() == expected.bestResults.size());
    for (std::size_t index = 0; index < expected.bestResults.size(); ++index) {
        assert(actual.bestResults[index]);
        assert(expected.bestResults[index]);
        AssertSameDesign(*actual.bestResults[index],
                *expected.bestResults[index]);
    }
}

void TestDeepExplorationIsDeterministicAcrossThreadCounts() {
    TestSupport::TemporaryDirectory directory("evacam-deep-threading-test");
    const std::filesystem::path configPath =
        WriteDeepExplorationConfig(directory);
    std::optional<EvaCamExplorationResult> baseline;
    std::vector<std::pair<int, double>> timings;

    for (int threadCount : kThreadCounts) {
        const auto config = LoadConfig(configPath);
        AssertDeepDomains(*config);
        const auto start = std::chrono::steady_clock::now();
        const EvaCamExplorationResult result =
            EvaCamExplorer(config, threadCount, MakeYieldHooks()).Run();
        const auto stop = std::chrono::steady_clock::now();
        const double elapsedSeconds =
            std::chrono::duration<double>(stop - start).count();
        timings.emplace_back(threadCount, elapsedSeconds);

        assert(result.numSolution > 0);
        AssertExpectedAccounting(result);
        assert(result.bestResults.size()
                == static_cast<std::size_t>(full_exploration));
        if (!baseline) {
            baseline = result;
        } else {
            AssertEquivalentResults(result, *baseline);
        }
    }

    std::cout << "Deep exploration scaling (informational):";
    for (const auto &[threadCount, elapsedSeconds] : timings) {
        std::cout << " " << threadCount << "t=" << std::fixed
                  << std::setprecision(3) << elapsedSeconds << "s";
    }
    std::cout << std::endl;
}

}  // namespace

int main() {
    TestDeepExplorationIsDeterministicAcrossThreadCounts();
    std::cout << "Deep exploration threading test passed" << std::endl;
    return 0;
}
