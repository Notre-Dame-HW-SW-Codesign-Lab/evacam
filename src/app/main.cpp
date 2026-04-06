#include <iostream>
#include <iomanip>
#include <yaml-cpp/yaml.h>

#include "input/CliOptions.h"
#include "Logger.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "EvaCamOutput.h"
#include "config/OutputFileLock.h"

int main(int argc, char *argv[]) {
    std::setw(10);
    std::cout << std::fixed << std::setprecision(3);

    try {
        const CliOptions cliOptions = CliOptionsParser::Parse(argc, argv);
        if (cliOptions.showHelp) {
            CliOptionsParser::PrintUsage(std::cout);
            return 0;
        }

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
