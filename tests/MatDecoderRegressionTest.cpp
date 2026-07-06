#include <cassert>
#include <cmath>
#include <iostream>

#include "Bank.h"
#include "CAM_SubArray.h"
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
        "config/2FeFET_TCAM/2FeFET_TCAM_explicit_subarray_tool_config.yaml";

    EvaCamContext context = EvaCamContextBuilder::Build(options);
    EvaCamExplorer explorer(context.config, 1);
    EvaCamExplorationResult exploration = explorer.Run();

    assert(exploration.numSolution == 1);
    const auto &result = exploration.bestResults.at(leakage_optimized);
    assert(result && result->bank && result->bank->mat && result->bank->mat->subarray);

    const CAM_SubArray &subarray = *result->bank->mat->subarray;
    assert(subarray.numRow == 64);
    assert(subarray.numColumn == 64);

    assert(subarray.RowDecMergeNand->numNandInput == 2);
    assert(subarray.RowDriver.at(0)->numNandInput == 2);
    assert(subarray.ColDecMergeNand->numNandInput == 0);
    assert(subarray.senseAmpMuxLev1Nand->numNandInput == 0);
    assert(subarray.senseAmpMuxLev2Nand->numNandInput == 0);

    assert(Near(subarray.RowDecMergeNand->area, 50.682e-12, 1e-15));
    const double decoderLatency = subarray.RowDecMergeNand->readLatency
        + subarray.senseAmpMuxLev1Nand->readLatency
        + subarray.senseAmpMuxLev2Nand->readLatency;
    assert(Near(decoderLatency, 138.099e-12, 1e-15));
    assert(Near(result->bank->mat->predecoderLatency, 62.486e-12, 1e-15));

    std::cout << "Mat/decoder regression tests passed\n";
    return 0;
}
