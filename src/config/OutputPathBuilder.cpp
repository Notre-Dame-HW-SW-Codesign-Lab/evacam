#include <filesystem>
#include <sstream>
#include <vector>

#include "config/OutputPathBuilder.h"
#include "MemCell.h"

std::string OutputPathBuilder::DefaultResultsYamlPath(const std::string &inputFile) {
    std::filesystem::path inputPath(inputFile);
    std::string base = inputPath.stem().string();
    const std::vector<std::string> suffixes = {
        "_tool_config",
        "-tool-config",
        "_system_config",
        "-system-config",
        "_config",
        "-config",
        ".config",
    };

    for (const std::string &suffix : suffixes) {
        if (base.size() > suffix.size()
                && base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0) {
            base.resize(base.size() - suffix.size());
            break;
        }
    }

    return (std::filesystem::path("results") / (base + "_results.yaml")).string();
}

std::string OutputPathBuilder::VariationSamplesCsvPath(const std::string &outputYamlFile) {
    return VariationSamplesCsvPath(outputYamlFile, "");
}

std::string OutputPathBuilder::VariationSamplesCsvPath(const std::string &outputYamlFile,
        const std::string &label) {
    std::filesystem::path yamlPath(outputYamlFile);
    std::string stem = yamlPath.stem().string();
    const std::string suffix = "_results";

    if (stem.size() > suffix.size()
            && stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0) {
        stem.resize(stem.size() - suffix.size());
    }

    if (!label.empty()) {
        stem += "_" + label;
    }
    stem += "_variation_samples.csv";

    return (yamlPath.parent_path() / stem).string();
}

std::string OutputPathBuilder::VariationHistogramsSvgPath(const std::string &samplesCsvFile) {
    std::filesystem::path csvPath(samplesCsvFile);
    std::string stem = csvPath.stem().string();
    const std::string suffix = "_variation_samples";
    if (stem.size() > suffix.size()
            && stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0) {
        stem.resize(stem.size() - suffix.size());
        stem += "_variation";
    }
    return (csvPath.parent_path() / (stem + "_histograms.svg")).string();
}

std::string OutputPathBuilder::ExplorationCsvPath(const InputConfig &input,
        const TechnologyContext &technology) {
    std::stringstream temp;
    const long comparisonWidth = technology.cell->camType == MCAM
        ? input.vectorDimensions : input.wordWidth;
    temp << input.outputFilePrefix << "_" << input.capacity / 1024
        << "K_" << comparisonWidth;

    if (input.internalSensing) temp << "_IN";
    else                       temp << "_EX";

    if (technology.cell->readMode) temp << "_VOL";
    else                           temp << "_CUR";

    temp << ".csv";
    return temp.str();
}
