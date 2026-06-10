#include "EvaCamContextBuilder.h"

#include <filesystem>

#include "EvaCamConfig.h"
#include "config/OutputPathBuilder.h"

namespace {

void ValidateInputFile(const std::string &inputFileName) {
    if (!std::filesystem::exists(inputFileName)) {
        throw std::runtime_error("Config file: " + inputFileName + " does not exist.");
    }
}

}  // namespace

EvaCamContext EvaCamContextBuilder::Build(const CliOptions &options) {
    ValidateInputFile(options.inputFileName);

    auto config = std::make_shared<EvaCamConfig>();
    if (options.verbose) {
        config->logger.SetVerbose(true);
        config->logger.Verbose() << "Verbose output enabled";
    }
    config->variationPlots = options.variationPlots;

    config->logger.Verbose() << "User-defined configuration file (" << options.inputFileName << ") is loaded";
    config->logger.Verbose();

    config->ReadConfigFromFile(options.inputFileName);

    EvaCamContext context;
    context.config = config;
    context.inputFileName = options.inputFileName;
    context.outputYamlFileName = options.outputYamlFileName;
    if (context.outputYamlFileName.empty()) {
        context.outputYamlFileName = config->input.outputYamlFileName;
    }
    if (context.outputYamlFileName.empty()) {
        context.outputYamlFileName = OutputPathBuilder::DefaultResultsYamlPath(context.inputFileName);
    }

    return context;
}
