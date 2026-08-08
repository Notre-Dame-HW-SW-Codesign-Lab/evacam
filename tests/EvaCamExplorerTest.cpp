#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>

#include "CandidateSpec.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "Result.h"
#include "TestSupport.h"
#include "config/ExplorationSpaceResolver.h"
#include "input/CliOptions.h"

namespace {

constexpr std::array<int, 4> kParallelThreadCounts = {2, 3, 7, 16};

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

std::shared_ptr<EvaCamConfig> LoadReducedConfig() {
    CliOptions options;
    options.inputFileName = "config/PCM-2T2R-JSSC11/PCM-2T2R-JSSC11.config.yaml";
    options.stdoutOutput = false;
    EvaCamContext context = EvaCamContextBuilder::Build(options);
    auto config = context.config;
    auto &exploration = config->exploration;
    exploration.cam.areaOptimizationLevel = IntValueDomain::FixedSet({latency_first});
    exploration.cam.rowDriverOptLevel = IntValueDomain::FixedSet({latency_first});
    exploration.cam.priorityOptLevel = IntValueDomain::FixedSet({latency_first});
    exploration.cam.bitSerialWidth = IntValueDomain::FixedSet({64});
    exploration.wires.localWireType = IntValueDomain::FixedSet({local_aggressive});
    exploration.wires.globalWireType = IntValueDomain::FixedSet({global_aggressive});
    exploration.wires.localWireRepeaterType = IntValueDomain::FixedSet({repeated_none});
    exploration.wires.globalWireRepeaterType = IntValueDomain::FixedSet({repeated_none});
    exploration.wires.isLocalWireLowSwing = IntValueDomain::FixedSet({false});
    exploration.wires.isGlobalWireLowSwing = IntValueDomain::FixedSet({false});
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(exploration);
    return config;
}

EvaCamExplorationResult RunReduced(int threads) {
    return EvaCamExplorer(LoadReducedConfig(), threads).Run();
}

void AssertSameDesign(const Result &actual, const Result &expected) {
    assert(actual.optimizationTarget == expected.optimizationTarget);
    assert(actual.bank && expected.bank);
    assert(actual.bank->readLatency == expected.bank->readLatency);
    assert(actual.bank->writeLatency == expected.bank->writeLatency);
    assert(actual.bank->readDynamicEnergy == expected.bank->readDynamicEnergy);
    assert(actual.bank->writeDynamicEnergy == expected.bank->writeDynamicEnergy);
    assert(actual.bank->leakage == expected.bank->leakage);
    assert(actual.bank->area == expected.bank->area);
    assert(actual.localWire.wireType == expected.localWire.wireType);
    assert(actual.globalWire.wireType == expected.globalWire.wireType);
    assert(CandidateSpec::FromBank(*actual.bank)
            == CandidateSpec::FromBank(*expected.bank));
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
    assert(actual.retainedCandidates == expected.retainedCandidates);
    assert(actual.reconstructionEvaluations == expected.reconstructionEvaluations);
}

void TestCandidateIdentityIsExactAndStable() {
    CandidateSpec first;
    first.numRowMat = 1;
    first.numColumnMat = 2;
    first.bitSerialWidth = 64;
    first.localWire = {local_aggressive, repeated_none, false};
    first.globalWire = {global_aggressive, repeated_none, false};
    CandidateSpec same = first;
    CandidateSpec different = first;
    different.bitSerialWidth = 128;

    assert(first == same);
    assert(!(first == different));
    assert(first.StableId() == same.StableId());
    assert(first.StableId() != different.StableId());
    std::unordered_set<CandidateSpec, CandidateSpecHash> candidates;
    candidates.insert(first);
    candidates.insert(same);
    candidates.insert(different);
    assert(candidates.size() == 2);
}

void TestInitializationAndDeterministicThreading() {
    const EvaCamExplorationResult oneThread = RunReduced(1);
    const EvaCamExplorationResult manyThreads = RunReduced(4);
    assert(oneThread.numSolution == 1);
    assert(manyThreads.numSolution == oneThread.numSolution);
    AssertSameAccounting(manyThreads.candidateAccounting,
            oneThread.candidateAccounting);
    assert(oneThread.candidateAccounting.rawCandidates == 1);
    assert(oneThread.candidateAccounting.modeledCandidates == 1);
    assert(oneThread.candidateAccounting.validCandidates == 1);
    assert(oneThread.candidateAccounting.retainedCandidates == 1);
    assert(oneThread.candidateAccounting.HasConsistentEnumerationCounts());
    assert(oneThread.candidateAccounting.HasConsistentModelCounts());
    assert(oneThread.bestResults.size() == static_cast<std::size_t>(full_exploration));
    assert(manyThreads.bestResults.size() == oneThread.bestResults.size());
    for (std::size_t i = 0; i < oneThread.bestResults.size(); ++i) {
        assert(oneThread.bestResults[i]);
        assert(oneThread.bestResults[i]->bank);
        AssertSameDesign(*manyThreads.bestResults[i], *oneThread.bestResults[i]);
    }
}

void TestWireCandidateFilteringAndCsvBuffering() {
    auto config = LoadReducedConfig();
    config->input.optimizationTarget = full_exploration;
    auto &wires = config->exploration.wires;
    wires.localWireRepeaterType = IntValueDomain::FixedSet({repeated_none, repeated_opt});
    wires.globalWireRepeaterType = IntValueDomain::FixedSet({repeated_none, repeated_opt});
    wires.isLocalWireLowSwing = IntValueDomain::FixedSet({false, true});
    wires.isGlobalWireLowSwing = IntValueDomain::FixedSet({false, true});
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);

    TestSupport::TemporaryDirectory directory("evacam-explorer-test");
    const std::filesystem::path path = directory.Path() / "exploration";
    config->input.outputFilePrefix = path.string();
    const EvaCamExplorationResult result = EvaCamExplorer(config, 2).Run();
    // For each wire, repeated low-swing combinations are discarded: 2*2 - 1 = 3.
    assert(result.numSolution == 9);
    assert(!result.explorationCsvPath.empty());
    assert(std::filesystem::exists(result.explorationCsvPath));
    std::ifstream csv(result.explorationCsvPath);
    std::size_t rows = 0;
    for (std::string line; std::getline(csv, line);) {
        assert(!line.empty());
        assert(line.find(",v1.") != std::string::npos);
        assert(line.rfind(",0,0,64,") != std::string::npos);
        ++rows;
    }
    assert(rows == static_cast<std::size_t>(result.numSolution));
}

void TestDisabledPriorityOptimizationIsCanonicalizedBeforeModeling() {
    auto oneThreadConfig = LoadReducedConfig();
    oneThreadConfig->exploration.cam.priorityOptLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    oneThreadConfig->resolvedExploration =
        ExplorationSpaceResolver::Resolve(oneThreadConfig->exploration);
    auto manyThreadConfig = LoadReducedConfig();
    manyThreadConfig->exploration.cam.priorityOptLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    manyThreadConfig->resolvedExploration =
        ExplorationSpaceResolver::Resolve(manyThreadConfig->exploration);

    const EvaCamExplorationResult oneThread = EvaCamExplorer(oneThreadConfig, 1).Run();
    const EvaCamExplorationResult manyThreads = EvaCamExplorer(manyThreadConfig, 4).Run();
    assert(oneThread.numSolution == 1);
    assert(manyThreads.numSolution == 1);
    AssertSameAccounting(manyThreads.candidateAccounting,
            oneThread.candidateAccounting);
    const CandidateAccounting &accounting = oneThread.candidateAccounting;
    assert(accounting.rawCandidates == 3);
    assert(accounting.structurallyRejected == 0);
    assert(accounting.duplicateCandidates == 2);
    assert(accounting.modeledCandidates == 1);
    assert(accounting.invalidCandidates == 0);
    assert(accounting.validCandidates == 1);
    assert(accounting.retainedCandidates == 1);
    assert(accounting.reconstructionEvaluations == 0);
    assert(accounting.HasConsistentEnumerationCounts());
    assert(accounting.HasConsistentModelCounts());
}

std::string ReadFile(const std::string &path) {
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void AssertUniqueCandidateIds(const std::string &path, std::size_t expectedCount) {
    std::ifstream csv(path);
    std::unordered_set<std::string> candidateIds;
    std::size_t rowCount = 0;
    for (std::string line; std::getline(csv, line);) {
        const std::size_t idStart = line.find(",v1.");
        assert(idStart != std::string::npos);
        const std::size_t idEnd = line.find(',', idStart + 1);
        assert(idEnd != std::string::npos);
        assert(candidateIds.insert(line.substr(idStart + 1, idEnd - idStart - 1)).second);
        ++rowCount;
    }
    assert(rowCount == expectedCount);
    assert(candidateIds.size() == expectedCount);
}

void AssertEquivalentExplorationResults(const EvaCamExplorationResult &actual,
        const EvaCamExplorationResult &expected, bool compareCsv) {
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
    if (compareCsv) {
        assert(ReadFile(actual.explorationCsvPath)
                == ReadFile(expected.explorationCsvPath));
    }
}

std::shared_ptr<EvaCamConfig> LoadParallelConfig(const std::string &outputPrefix) {
    auto config = LoadReducedConfig();
    config->input.optimizationTarget = full_exploration;
    config->input.outputFilePrefix = outputPrefix;
    const auto &resolvedGeometry = config->resolvedExploration.geometry;
    const int numRowMat = resolvedGeometry.numRowMatValues.front();
    const int numColumnMat = resolvedGeometry.numColumnMatValues.front();
    const int numRowSubarray = resolvedGeometry.numRowSubarrayValues.front();
    config->exploration.geometry.numRowMat =
        IntValueDomain::FixedSet(
                {numRowMat, numRowMat * 2, numRowMat * 4, numRowMat * 8});
    config->exploration.geometry.numColumnMat =
        IntValueDomain::FixedSet({numColumnMat, numColumnMat * 2});
    config->exploration.geometry.numRowSubarray =
        IntValueDomain::FixedSet({numRowSubarray, numRowSubarray * 2});
    config->exploration.cam.areaOptimizationLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    config->exploration.cam.rowDriverOptLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    config->exploration.cam.priorityOptLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    config->exploration.wires.localWireRepeaterType = IntValueDomain::FixedSet(
            {repeated_none, repeated_opt});
    config->exploration.wires.globalWireRepeaterType = IntValueDomain::FixedSet(
            {repeated_none, repeated_opt});
    config->exploration.wires.isLocalWireLowSwing = IntValueDomain::FixedSet({false, true});
    config->exploration.wires.isGlobalWireLowSwing = IntValueDomain::FixedSet({false, true});
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);
    return config;
}

void TestParallelEnumerationIsDeterministic() {
    TestSupport::TemporaryDirectory directory("evacam-parallel-explorer-test");
    const auto oneThreadConfig = LoadParallelConfig(
            (directory.Path() / "one-thread").string());

    const EvaCamExplorationResult oneThread = EvaCamExplorer(oneThreadConfig, 1).Run();
    assert(oneThread.numSolution > 1);
    assert(oneThread.candidateAccounting.duplicateCandidates > 0);
    assert(oneThread.candidateAccounting.HasConsistentEnumerationCounts());
    assert(oneThread.candidateAccounting.HasConsistentModelCounts());
    AssertUniqueCandidateIds(oneThread.explorationCsvPath,
            static_cast<std::size_t>(oneThread.numSolution));

    for (int threadCount : kParallelThreadCounts) {
        const auto config = LoadParallelConfig(
                (directory.Path() / ("threads-" + std::to_string(threadCount))).string());
        const EvaCamExplorationResult result =
            EvaCamExplorer(config, threadCount, MakeYieldHooks()).Run();
        AssertEquivalentExplorationResults(result, oneThread, true);
        AssertUniqueCandidateIds(result.explorationCsvPath,
                static_cast<std::size_t>(result.numSolution));
    }

    for (int iteration = 0; iteration < 3; ++iteration) {
        const auto repeatConfig = LoadParallelConfig(
                (directory.Path() / ("repeat-" + std::to_string(iteration))).string());
        const EvaCamExplorationResult repeated =
            EvaCamExplorer(repeatConfig, 16, MakeYieldHooks()).Run();
        AssertEquivalentExplorationResults(repeated, oneThread, true);
    }
}

void EnablePermissiveConstraints(const std::shared_ptr<EvaCamConfig> &config) {
    config->input.optimizationTarget = read_latency_optimized;
    config->constraints.enabled = true;
    config->constraints.readLatency = 1e12;
    config->constraints.writeLatency = 1e12;
    config->constraints.readDynamicEnergy = 1e12;
    config->constraints.writeDynamicEnergy = 1e12;
    config->constraints.readEdp = 1e12;
    config->constraints.writeEdp = 1e12;
    config->constraints.area = 1e12;
    config->constraints.leakage = 1e12;
}

void TestParallelConstrainedEnumerationIsDeterministic() {
    TestSupport::TemporaryDirectory directory("evacam-parallel-constrained-test");
    auto oneThreadConfig = LoadParallelConfig(
            (directory.Path() / "one-thread").string());
    EnablePermissiveConstraints(oneThreadConfig);

    const EvaCamExplorationResult oneThread =
        EvaCamExplorer(oneThreadConfig, 1).Run();
    assert(oneThread.numSolution > 1);
    assert(oneThread.candidateAccounting.duplicateCandidates > 0);
    assert(oneThread.candidateAccounting.HasConsistentEnumerationCounts());
    assert(oneThread.candidateAccounting.HasConsistentModelCounts());
    assert(oneThread.candidateAccounting.retainedCandidates
            == oneThread.numSolution);
    assert(oneThread.candidateAccounting.reconstructionEvaluations > 0);
    assert(oneThread.candidateAccounting.reconstructionEvaluations
            <= static_cast<long long>(full_exploration));

    for (int threadCount : kParallelThreadCounts) {
        auto config = LoadParallelConfig(
                (directory.Path() / ("threads-" + std::to_string(threadCount))).string());
        EnablePermissiveConstraints(config);
        const EvaCamExplorationResult result =
            EvaCamExplorer(config, threadCount, MakeYieldHooks()).Run();
        AssertEquivalentExplorationResults(result, oneThread, false);
    }
}

void TestWorkerExceptionJoinsEveryThreadAndRethrows() {
    TestSupport::TemporaryDirectory directory("evacam-worker-exception-test");
    const auto config = LoadParallelConfig(
            (directory.Path() / "failure").string());
    std::atomic<int> workersStarted = 0;
    std::atomic<int> workersFinished = 0;
    EvaCamExplorerTestHooks hooks = MakeYieldHooks();
    hooks.workerStarted = [&workersStarted](std::size_t) {
        workersStarted.fetch_add(1, std::memory_order_relaxed);
    };
    hooks.beforeGeometry = [](std::size_t outerIndex) {
        std::this_thread::yield();
        if (outerIndex == 3) {
            throw std::runtime_error("injected worker geometry failure");
        }
    };
    hooks.workerFinished = [&workersFinished](std::size_t) {
        workersFinished.fetch_add(1, std::memory_order_relaxed);
    };

    bool threwExpectedException = false;
    try {
        (void)EvaCamExplorer(config, 16, hooks).Run();
    } catch (const std::runtime_error &error) {
        threwExpectedException =
            std::string(error.what()) == "injected worker geometry failure";
    }
    assert(threwExpectedException);
    assert(workersStarted.load(std::memory_order_relaxed) == 16);
    assert(workersFinished.load(std::memory_order_relaxed) == 16);

    const auto recoveryConfig = LoadParallelConfig(
            (directory.Path() / "recovery").string());
    const EvaCamExplorationResult recovery =
        EvaCamExplorer(recoveryConfig, 16, MakeYieldHooks()).Run();
    assert(recovery.numSolution > 1);
    assert(recovery.candidateAccounting.HasConsistentEnumerationCounts());
    assert(recovery.candidateAccounting.HasConsistentModelCounts());
}

void TestExplorerRunIsThreadSafeAndOneShot() {
    auto sequentialConfig = LoadReducedConfig();
    sequentialConfig->logger.SetOutputEnabled(false);
    EvaCamExplorer sequentialExplorer(sequentialConfig, 1);
    assert(sequentialExplorer.Run().numSolution > 0);
    TestSupport::AssertThrows<std::logic_error>([&sequentialExplorer]() {
        (void)sequentialExplorer.Run();
    }, "may only be called once");

    auto concurrentConfig = LoadReducedConfig();
    concurrentConfig->logger.SetOutputEnabled(false);
    EvaCamExplorer concurrentExplorer(concurrentConfig, 1);
    std::atomic<int> ready = 0;
    std::atomic<bool> start = false;
    std::array<int, 2> outcomes = {0, 0};
    std::array<std::thread, 2> callers;
    for (std::size_t index = 0; index < callers.size(); ++index) {
        callers[index] = std::thread([&concurrentExplorer, &ready, &start,
                &outcomes, index]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            try {
                outcomes[index] = concurrentExplorer.Run().numSolution > 0 ? 1 : 4;
            } catch (const std::logic_error &error) {
                outcomes[index] = std::string(error.what()).find(
                        "may only be called once") != std::string::npos ? 2 : 4;
            } catch (...) {
                outcomes[index] = 4;
            }
        });
    }
    while (ready.load(std::memory_order_acquire)
            != static_cast<int>(callers.size())) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (std::thread &caller : callers) {
        caller.join();
    }
    assert(static_cast<int>(std::count(outcomes.begin(), outcomes.end(), 1)) == 1);
    assert(static_cast<int>(std::count(outcomes.begin(), outcomes.end(), 2)) == 1);

    auto failingConfig = LoadReducedConfig();
    failingConfig->logger.SetOutputEnabled(false);
    failingConfig->exploration.wires.localWireRepeaterType =
        IntValueDomain::FixedSet({repeated_opt});
    failingConfig->exploration.wires.isLocalWireLowSwing =
        IntValueDomain::FixedSet({true});
    failingConfig->resolvedExploration =
        ExplorationSpaceResolver::Resolve(failingConfig->exploration);
    EvaCamExplorer failingExplorer(failingConfig, 1);
    TestSupport::AssertThrows<std::runtime_error>([&failingExplorer]() {
        (void)failingExplorer.Run();
    }, "low swing is not supported");
    TestSupport::AssertThrows<std::logic_error>([&failingExplorer]() {
        (void)failingExplorer.Run();
    }, "may only be called once");
}

void TestUnconstrainedAndConstrainedPasses() {
    auto config = LoadReducedConfig();
    config->exploration.cam.priorityOptLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);
    config->constraints.enabled = true;
    config->constraints.readLatency = 1e-12;
    config->constraints.writeLatency = 1e-12;
    config->constraints.readDynamicEnergy = 1e-12;
    config->constraints.writeDynamicEnergy = 1e-12;
    config->constraints.readEdp = 1e-12;
    config->constraints.writeEdp = 1e-12;
    config->constraints.area = 1e-12;
    config->constraints.leakage = 1e-12;
    const EvaCamExplorationResult result = EvaCamExplorer(config, 1).Run();
    // The exhaustive pass establishes all eight limits; cached metrics retain
    // the single candidate at the inclusive limit without a second full sweep.
    assert(result.numSolution == 1);
    for (const auto &best : result.bestResults) {
        assert(best && best->bank);
        assert(best->bank->readLatency <= best->limitReadLatency);
        assert(best->bank->writeLatency <= best->limitWriteLatency);
        assert(best->bank->readDynamicEnergy <= best->limitReadDynamicEnergy);
        assert(best->bank->writeDynamicEnergy <= best->limitWriteDynamicEnergy);
        assert(best->bank->readLatency * best->bank->readDynamicEnergy <= best->limitReadEdp);
        assert(best->bank->writeLatency * best->bank->writeDynamicEnergy <= best->limitWriteEdp);
        assert(best->bank->area <= best->limitArea);
        assert(best->bank->leakage <= best->limitLeakage);
    }
    assert(result.candidateAccounting.rawCandidates == 3);
    assert(result.candidateAccounting.duplicateCandidates == 2);
    assert(result.candidateAccounting.modeledCandidates == 1);
    assert(result.candidateAccounting.validCandidates == 1);
    assert(result.candidateAccounting.constraintRejected == 0);
    assert(result.candidateAccounting.retainedCandidates == 1);
    // The one winning organization serves every objective and is rebuilt once,
    // instead of rerunning the entire exploration space.
    assert(result.candidateAccounting.reconstructionEvaluations == 1);
}

void TestConstrainedPassCanReturnZeroSolutions() {
    auto config = LoadReducedConfig();
    auto &exploration = config->exploration;
    exploration.cam.areaOptimizationLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    exploration.cam.rowDriverOptLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    exploration.cam.priorityOptLevel = IntValueDomain::FixedSet(
            {latency_first, latency_area_trade_off, area_first});
    exploration.wires.localWireType = IntValueDomain::FixedSet(
            {local_aggressive, local_conservative});
    exploration.wires.globalWireType = IntValueDomain::FixedSet(
            {global_aggressive, global_conservative});
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(exploration);
    config->constraints.enabled = true;
    config->constraints.readLatency = config->constraints.writeLatency = 1e-12;
    config->constraints.readDynamicEnergy = config->constraints.writeDynamicEnergy = 1e-12;
    config->constraints.readEdp = config->constraints.writeEdp = 1e-12;
    config->constraints.area = config->constraints.leakage = 1e-12;
    const EvaCamExplorationResult result = EvaCamExplorer(config, 2).Run();
    assert(result.numSolution == 0);
    for (const auto &best : result.bestResults) {
        assert(best && best->bank);
        assert(best->bank->readLatency == 1e41);
    }
}

void TestInvalidWireDomainIsRejectedBeforeSearch() {
    auto config = LoadReducedConfig();
    config->exploration.wires.localWireRepeaterType = IntValueDomain::FixedSet({repeated_opt});
    config->exploration.wires.isLocalWireLowSwing = IntValueDomain::FixedSet({true});
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);
    bool threw = false;
    try {
        (void)EvaCamExplorer(config, 1).Run();
    } catch (const std::runtime_error &) {
        threw = true;
    }
    assert(threw);
}

}  // namespace

int main() {
    TestCandidateIdentityIsExactAndStable();
    TestInitializationAndDeterministicThreading();
    TestWireCandidateFilteringAndCsvBuffering();
    TestDisabledPriorityOptimizationIsCanonicalizedBeforeModeling();
    TestParallelEnumerationIsDeterministic();
    TestParallelConstrainedEnumerationIsDeterministic();
    TestWorkerExceptionJoinsEveryThreadAndRethrows();
    TestExplorerRunIsThreadSafeAndOneShot();
    TestUnconstrainedAndConstrainedPasses();
    TestConstrainedPassCanReturnZeroSolutions();
    TestInvalidWireDomainIsRejectedBeforeSearch();
    std::cout << "EvaCam explorer tests passed" << std::endl;
    return 0;
}
