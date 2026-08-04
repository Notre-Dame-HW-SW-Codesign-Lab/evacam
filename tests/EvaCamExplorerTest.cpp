#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "Result.h"
#include "TestSupport.h"
#include "config/ExplorationSpaceResolver.h"
#include "input/CliOptions.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

std::shared_ptr<EvaCamConfig> LoadReducedConfig() {
    CliOptions options;
    options.inputFileName = "config/PCM-2T2R-JSSC11/PCM-2T2R-JSSC11.config.yaml";
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
}

void TestInitializationAndDeterministicThreading() {
    const EvaCamExplorationResult oneThread = RunReduced(1);
    const EvaCamExplorationResult manyThreads = RunReduced(4);
    assert(oneThread.numSolution == 1);
    assert(manyThreads.numSolution == oneThread.numSolution);
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
        ++rows;
    }
    assert(rows == static_cast<std::size_t>(result.numSolution));
}

void TestUnconstrainedAndConstrainedPasses() {
    auto config = LoadReducedConfig();
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
    // The first pass establishes all eight limits; the second pass retains the
    // single candidate at the inclusive limit for every observable constraint.
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
    TestInitializationAndDeterministicThreading();
    TestWireCandidateFilteringAndCsvBuffering();
    TestUnconstrainedAndConstrainedPasses();
    TestConstrainedPassCanReturnZeroSolutions();
    TestInvalidWireDomainIsRejectedBeforeSearch();
    std::cout << "EvaCam explorer tests passed" << std::endl;
    return 0;
}
