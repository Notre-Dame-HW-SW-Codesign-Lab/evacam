#include "EvaCamRun.h"

#include <memory>
#include <stdexcept>

#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "EvaCamOutput.h"
#include "EvaCamResultExtractor.h"
#include "config/ExplorationSpaceResolver.h"
#include "config/OutputFileLock.h"
#include "config/IntValueDomain.h"
#include "input/CliOptions.h"

namespace {

CliOptions BuildCliOptions(const EvaCamRunOptions &options) {
    CliOptions cliOptions;
    cliOptions.inputFileName = options.configPath;
    cliOptions.outputYamlFileName = options.outputYamlPath;
    cliOptions.threads = options.threads > 0 ? options.threads : 1;
    cliOptions.verbose = options.verbose;
    cliOptions.stdoutOutput = options.stdoutOutput;
    cliOptions.variationPlots = options.variationPlots;
    return cliOptions;
}

void ApplySubarrayDimensions(
        const EvaCamRunOptions &options,
        const std::shared_ptr<EvaCamConfig> &config) {
    if (options.subarrayRows == 0 && options.subarrayColumns == 0) {
        return;
    }
    if (options.subarrayRows <= 0 || options.subarrayColumns <= 0) {
        throw std::runtime_error(
                "subarray row and column overrides must both be positive");
    }

    const int bitsPerCell = config->wordGeometry.bitsPerCell;
    const long logicalWordBits = static_cast<long>(options.subarrayColumns)
            * bitsPerCell;
    const long long capacityBits = static_cast<long long>(options.subarrayRows)
            * logicalWordBits;
    if (capacityBits % 8 != 0) {
        throw std::runtime_error("overridden subarray capacity must be byte-addressable");
    }

    config->input.capacity = capacityBits / 8;
    config->input.wordWidth = logicalWordBits;
    config->runtimeSizing.hasExplicitCapacity = true;
    config->runtimeSizing.capacityIsAuto = false;
    config->runtimeSizing.realCapacity = 0;
    config->runtimeSizing.hasFixedSubarrayDimensions = true;
    config->runtimeSizing.fixedSubarrayRows = options.subarrayRows;
    config->runtimeSizing.fixedSubarrayColumns = options.subarrayColumns;
    config->ResolveWordGeometry(bitsPerCell);
    config->exploration.geometry.numRow = IntValueDomain::FixedSet(
            {options.subarrayRows});
    config->exploration.geometry.numColumn = IntValueDomain::FixedSet(
            {options.subarrayColumns});
    config->exploration.cam.bitSerialWidth = IntValueDomain::FixedSet(
            {options.subarrayColumns});
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);
}

}  // namespace

EvaCamRunResultDto RunEvaCam(const EvaCamRunOptions &options) {
    CliOptions cliOptions = BuildCliOptions(options);
    EvaCamContext context = EvaCamContextBuilder::Build(cliOptions);
    auto config = context.config;
    ApplySubarrayDimensions(options, config);
    const std::string outputYamlPath = context.outputYamlFileName;

    EvaCamExplorer explorer(config, cliOptions.threads);
    EvaCamExplorationResult explorationResult = explorer.Run();

    if (options.writeYaml) {
        auto outputYamlLock = OutputFileLock::Acquire(outputYamlPath);
        EvaCamOutput::WriteYamlResults(*config,
                outputYamlPath,
                explorationResult.numSolution,
                explorationResult.bestResults);
    }

    return ExtractEvaCamRunResult(explorationResult.numSolution,
            explorationResult.bestResults,
            explorationResult.explorationCsvPath,
            options.writeYaml ? outputYamlPath : "");
}
