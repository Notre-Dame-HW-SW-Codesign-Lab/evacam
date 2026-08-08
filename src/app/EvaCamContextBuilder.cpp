#include "EvaCamContextBuilder.h"

#include <filesystem>
#include <fstream>
#include <mutex>

#include "EvaCamConfig.h"
#include "config/OutputPathBuilder.h"
#include "input/YamlNodeHelpers.h"

namespace {

void ValidateInputFile(const std::string &inputFileName) {
    if (!std::filesystem::exists(inputFileName)) {
        throw std::runtime_error("Config file: " + inputFileName + " does not exist.");
    }
    if (!std::filesystem::is_regular_file(inputFileName)) {
        throw std::runtime_error("Config file: " + inputFileName + " is not a regular file.");
    }
    std::ifstream input(inputFileName);
    if (!input) {
        throw std::runtime_error("Config file: " + inputFileName + " is not readable.");
    }
}

}  // namespace

EvaCamContext EvaCamContextBuilder::Build(const CliOptions &options) {
    ValidateInputFile(options.inputFileName);

    // yaml-cpp performs mutable process-global work while decoding some
    // scalars. Keep configuration parsing exclusive while allowing the much
    // more expensive exploration phase of independent runs to overlap.
    std::lock_guard<std::recursive_mutex> parserLock(YamlHelpers::ParserMutex());

    auto config = std::make_shared<EvaCamConfig>();
    config->logger.SetOutputEnabled(options.stdoutOutput);
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
