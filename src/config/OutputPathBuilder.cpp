#include "config/OutputPathBuilder.h"

#include <filesystem>
#include <sstream>

#include "MemCell.h"

std::string OutputPathBuilder::DefaultResultsYamlPath(const std::string &inputFile) {
    std::filesystem::path inputPath(inputFile);
    std::string base = inputPath.stem().string();
    const std::string suffix1 = "_config";
    const std::string suffix2 = "-config";

    if (base.size() > suffix1.size()
            && base.compare(base.size() - suffix1.size(), suffix1.size(), suffix1) == 0) {
        base.resize(base.size() - suffix1.size());
    } else if (base.size() > suffix2.size()
            && base.compare(base.size() - suffix2.size(), suffix2.size(), suffix2) == 0) {
        base.resize(base.size() - suffix2.size());
    }

    return (std::filesystem::path("results") / (base + "_results.yaml")).string();
}

std::string OutputPathBuilder::ExplorationCsvPath(const InputConfig &input,
        const TechnologyContext &technology) {
    std::stringstream temp;
    temp << input.outputFilePrefix << "_" << input.capacity / 1024
        << "K_" << input.wordWidth << "_" << input.associativity;

    if (input.internalSensing) temp << "_IN";
    else                       temp << "_EX";

    if (technology.cell->readMode) temp << "_VOL";
    else                           temp << "_CUR";

    temp << ".csv";
    return temp.str();
}
