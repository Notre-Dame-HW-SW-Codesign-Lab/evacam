#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "BankWithHtree.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "Mat.h"
#include "Result.h"
#include "input/CliOptions.h"

namespace {

bool Near(double actual, double expected, double tolerance) {
    return std::fabs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    CliOptions options;
    options.inputFileName =
        "config/PCM-2T2R-JSSC11/PCM-2T2R-JSSC11.config.yaml";

    EvaCamContext context = EvaCamContextBuilder::Build(options);
    EvaCamExplorer explorer(context.config, 1);
    EvaCamExplorationResult exploration = explorer.Run();

    assert(exploration.numSolution == 1);
    const auto &result = exploration.bestResults.at(read_latency_optimized);
    assert(result && result->bank);

    const auto bank = std::dynamic_pointer_cast<BankWithHtree>(result->bank);
    assert(bank && bank->mat && bank->mat->subarray);

    assert(bank->numAddressBit == 6);
    assert(bank->numDataBit == 16384);
    assert(bank->levelHorizontal == 1);
    assert(bank->levelVertical == 2);

    const auto &horizontal = bank->horizontalLevels.at(0);
    assert(horizontal.addressBits == 6);
    assert(horizontal.dataBits == 16384);
    assert(horizontal.wireGroups == 1);
    assert(horizontal.totalWireGroups == 1);
    assert(horizontal.activeWireGroups == 1);

    const auto &vertical0 = bank->verticalLevels.at(0);
    assert(vertical0.addressBits == 6);
    assert(vertical0.dataBits == 8192);
    assert(vertical0.wireGroups == 1);
    assert(vertical0.totalWireGroups == 2);
    assert(vertical0.activeWireGroups == 2);

    const auto &vertical1 = bank->verticalLevels.at(1);
    assert(vertical1.addressBits == 6);
    assert(vertical1.dataBits == 4096);
    assert(vertical1.wireGroups == 1);
    assert(vertical1.totalWireGroups == 4);
    assert(vertical1.activeWireGroups == 4);

    assert(bank->mat->numAddressBit == 6);
    assert(bank->mat->numDataBit == 2048);
    assert(bank->mat->subarray->numRow == 64);
    assert(bank->mat->subarray->numColumn == 512);
    assert(Near(bank->height, 781.523e-6, 1e-9));
    assert(Near(bank->width, 1.7871e-3, 1e-6));

    std::cout << "H-tree routing regression tests passed\n";
    return 0;
}
