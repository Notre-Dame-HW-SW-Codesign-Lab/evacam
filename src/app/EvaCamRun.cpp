#include "EvaCamRun.h"

#include <iostream>
#include <sstream>

#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "EvaCamOutput.h"
#include "EvaCamResultExtractor.h"
#include "config/OutputFileLock.h"
#include "input/CliOptions.h"

namespace {

class ScopedStdoutRedirect {
    public:
        explicit ScopedStdoutRedirect(bool enabled) : enabled_(enabled) {
            if (enabled_) {
                previous_ = std::cout.rdbuf(buffer_.rdbuf());
            }
        }

        ~ScopedStdoutRedirect() {
            if (enabled_) {
                std::cout.rdbuf(previous_);
            }
        }

        ScopedStdoutRedirect(const ScopedStdoutRedirect&) = delete;
        ScopedStdoutRedirect& operator=(const ScopedStdoutRedirect&) = delete;

    private:
        bool enabled_;
        std::ostringstream buffer_;
        std::streambuf *previous_ = nullptr;
};

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
    ScopedStdoutRedirect stdoutRedirect(!options.stdoutOutput);

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
