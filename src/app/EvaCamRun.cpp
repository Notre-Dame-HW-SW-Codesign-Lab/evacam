#include "EvaCamRun.h"

#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "EvaCamOutput.h"
#include "EvaCamResultExtractor.h"
#include "config/OutputFileLock.h"
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

}  // namespace

EvaCamRunResultDto RunEvaCam(const EvaCamRunOptions &options) {
    CliOptions cliOptions = BuildCliOptions(options);
    EvaCamContext context = EvaCamContextBuilder::Build(cliOptions);
    auto config = context.config;
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
