#include <cassert>
#include <iostream>

#include "Bank.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "Result.h"
#include "config/ExplorationSpaceResolver.h"
#include "input/CliOptions.h"

namespace {

EvaCamExplorationResult RunSearch(bool enableConstraints) {
    CliOptions options;
    options.inputFileName =
        "config/PCM-2T2R-JSSC11/PCM-2T2R-JSSC11.config.yaml";

    EvaCamContext context = EvaCamContextBuilder::Build(options);
    auto &exploration = context.config->exploration;
    exploration.cam.areaOptimizationLevel =
        IntValueDomain::FixedSet({latency_first, latency_area_trade_off, area_first});
    exploration.cam.rowDriverOptLevel =
        IntValueDomain::FixedSet({latency_first, latency_area_trade_off, area_first});
    exploration.cam.priorityOptLevel =
        IntValueDomain::FixedSet({latency_first, latency_area_trade_off, area_first});
    exploration.wires.localWireType =
        IntValueDomain::FixedSet({local_aggressive, local_conservative});
    exploration.wires.globalWireType =
        IntValueDomain::FixedSet({global_aggressive, global_conservative});
    exploration.wires.localWireRepeaterType =
        IntValueDomain::FixedSet({repeated_none});
    exploration.wires.globalWireRepeaterType =
        IntValueDomain::FixedSet({repeated_none});
    context.config->resolvedExploration =
        ExplorationSpaceResolver::Resolve(exploration);
    context.config->constraints.enabled = enableConstraints;

    EvaCamExplorer explorer(context.config, 1);
    return explorer.Run();
}

void AssertSameResult(const Result &actual, const Result &expected) {
    assert(actual.bank->readLatency == expected.bank->readLatency);
    assert(actual.bank->writeLatency == expected.bank->writeLatency);
    assert(actual.bank->readDynamicEnergy == expected.bank->readDynamicEnergy);
    assert(actual.bank->writeDynamicEnergy == expected.bank->writeDynamicEnergy);
    assert(actual.bank->searchLatency == expected.bank->searchLatency);
    assert(actual.bank->searchDynamicEnergy == expected.bank->searchDynamicEnergy);
    assert(actual.bank->leakage == expected.bank->leakage);
    assert(actual.bank->area == expected.bank->area);
    assert(actual.localWire.wireType == expected.localWire.wireType);
    assert(actual.globalWire.wireType == expected.globalWire.wireType);
}

}  // namespace

int main() {
    const EvaCamExplorationResult unconstrained = RunSearch(false);
    const EvaCamExplorationResult permissivelyConstrained = RunSearch(true);

    // 3 buffer x 3 row-driver x 3 priority x 2 local-wire x 2 global-wire.
    assert(unconstrained.numSolution == 108);
    assert(permissivelyConstrained.numSolution == unconstrained.numSolution);
    assert(permissivelyConstrained.bestResults.size()
            == unconstrained.bestResults.size());

    for (std::size_t i = 0; i < unconstrained.bestResults.size(); i++) {
        AssertSameResult(*permissivelyConstrained.bestResults[i],
                *unconstrained.bestResults[i]);
    }

    std::cout << "Exhaustive search regression tests passed\n";
    return 0;
}
