#include <iostream>
#include <iomanip>
#include <sstream>
#include <yaml-cpp/yaml.h>

#include "input/CliOptions.h"
#include "Logger.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "EvaCamOutput.h"
#include "SubarrayDimensionTester.h"
#include "config/OutputFileLock.h"

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

}  // namespace

int main(int argc, char *argv[]) {
    std::setw(10);
    std::cout << std::fixed << std::setprecision(3);

    try {
        const CliOptions cliOptions = CliOptionsParser::Parse(argc, argv);
        if (cliOptions.showHelp) {
            CliOptionsParser::PrintUsage(std::cout);
            return 0;
        }

        if (cliOptions.subarrayDimensionTest) {
            if (!cliOptions.outputYamlFileName.empty()) {
                throw std::invalid_argument(
                        "--output is not supported in subarray dimension test mode; "
                        "set output.directory and output.summary_csv in the tester config.");
            }
            SubarrayDimensionTesterOptions testerOptions;
            testerOptions.configPath = cliOptions.inputFileName;
            testerOptions.jobs = cliOptions.threads;
            testerOptions.stdoutOutput = cliOptions.stdoutOutput;
            testerOptions.verbose = cliOptions.verbose;
            testerOptions.variationPlots = cliOptions.variationPlots;
            const SubarrayDimensionTestSummary summary =
                    SubarrayDimensionTester::Run(testerOptions);
            return summary.failedRuns == 0 ? 0 : 1;
        }

        ScopedStdoutRedirect stdoutRedirect(!cliOptions.stdoutOutput);
        EvaCamContext context = EvaCamContextBuilder::Build(cliOptions);
        auto config = context.config;
        std::string outputYamlFileName = context.outputYamlFileName;
        auto outputYamlLock = OutputFileLock::Acquire(outputYamlFileName);

        EvaCamExplorer explorer(config, cliOptions.threads);
        EvaCamExplorationResult explorationResult = explorer.Run();

        EvaCamOutput::PrintConsoleSummary(*config,
                explorationResult.numSolution,
                explorationResult.bestResults,
                explorationResult.explorationCsvPath);
        EvaCamOutput::WriteYamlResults(*config,
                outputYamlFileName,
                explorationResult.numSolution,
                explorationResult.bestResults);
        return 0;

    } catch (const std::invalid_argument &e) {
        std::lock_guard<std::mutex> lock(Logger::OutputMutex());
        std::cerr << e.what() << std::endl;
        CliOptionsParser::PrintUsage(std::cerr);
        return 1;
    } catch (const YAML::Exception& e) {
        std::lock_guard<std::mutex> lock(Logger::OutputMutex());
        if (e.mark.line != -1) {
            std::cerr << "YAML error at line " 
                << (e.mark.line + 1)
                << ", column "
                << (e.mark.column + 1)
                << ": "
                << e.what()
                << std::endl;
        } else {
            std::cerr << "YAML error: " << e.what() << "\n";
        }
        return 1;

    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(Logger::OutputMutex());
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
