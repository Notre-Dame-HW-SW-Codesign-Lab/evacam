#include <iostream>
#include <iomanip>
#include <yaml-cpp/yaml.h>

#include "CliOptions.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "EvaCamOutput.h"

int main(int argc, char *argv[]) {
    std::setw(10);
    std::cout << std::fixed << std::setprecision(3);

    CliOptions cliOptions;
    try {
        cliOptions = CliOptionsParser::Parse(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        CliOptionsParser::PrintUsage(std::cerr);
        return 1;
    }

    if (cliOptions.showHelp) {
        CliOptionsParser::PrintUsage(std::cout);
        return 0;
    }

    EvaCamContext context;
    try {
        context = EvaCamContextBuilder::Build(cliOptions);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    auto config = context.config;
    std::string outputYamlFileName = context.outputYamlFileName;
    EvaCamExplorationResult explorationResult;

    try {
        EvaCamExplorer explorer(config);
        explorationResult = explorer.Run();

        EvaCamOutput::PrintConsoleSummary(*config,
                explorationResult.numSolution,
                explorationResult.bestResults,
                explorationResult.explorationCsvPath);
        EvaCamOutput::WriteYamlResults(*config,
                outputYamlFileName,
                explorationResult.numSolution,
                explorationResult.bestResults);
        return 0;

    } catch (const YAML::Exception& e) {
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
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
