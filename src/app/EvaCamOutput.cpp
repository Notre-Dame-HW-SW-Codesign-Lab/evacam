#include "EvaCamOutput.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "CAM_Result.h"
#include "EvaCamConfig.h"
#include "Logger.h"
#include "config/DerivedValueHelpers.h"
#include "config/OutputPathBuilder.h"
#include "output/ResultsYaml.h"
#include "output/VariationSamplesCsv.h"

namespace {

std::vector<std::shared_ptr<Result>> AsResults(
        const std::vector<std::shared_ptr<CAM_Result>> &bestResults) {
    std::vector<std::shared_ptr<Result>> results;
    results.reserve(bestResults.size());
    for (const auto &result : bestResults) {
        results.push_back(result);
    }
    return results;
}

void PrintVariationMetric(const char *name, double scale, const char *unit, const CAMMetricStats &stats) {
    if (!stats.available) {
        return;
    }

    std::cout << " - " << name
              << ": mean=" << stats.mean * scale
              << " " << unit
              << ", stddev=" << stats.stddev * scale
              << " " << unit
              << std::endl;
}

void PrintVariationSampleMetric(const char *name, double scale, const char *unit, const CAMMetricStats &stats) {
    if (!stats.available) {
        return;
    }

    std::cout << " - " << name
              << ": nominal=" << stats.nominal * scale
              << " " << unit
              << ", sample=" << stats.sample * scale
              << " " << unit
              << std::endl;
}

std::string OptimizationTargetName(OptimizationTarget t) {
    switch (t) {
        case read_latency_optimized:
            return "ReadLatency";
        case write_latency_optimized:
            return "WriteLatency";
        case read_energy_optimized:
            return "ReadDynamicEnergy";
        case write_energy_optimized:
            return "WriteDynamicEnergy";
        case read_edp_optimized:
            return "ReadEDP";
        case write_edp_optimized:
            return "WriteEDP";
        case leakage_optimized:
            return "LeakagePower";
        case area_optimized:
            return "Area";
        case search_latency_optimized:
            return "SearchLatency";
        case search_energy_optimized:
            return "SearchEnergy";
        case search_edp_optimized:
            return "SearchEDP";
        case full_exploration:
            return "Exploration";
    }
    return "Unknown";
}

bool HasMonteCarloSamples(const std::shared_ptr<CAM_Result> &result) {
    return result
        && result->bank
        && result->bank->mat
        && result->bank->mat->subarray
        && result->bank->mat->subarray->monteCarloSummary.mode == "monte_carlo"
        && !result->bank->mat->subarray->monteCarloSamples.empty();
}

void WriteVariationSamplesFile(const CAM_Result &result, const std::string &path) {
    std::filesystem::path outPath(path);
    if (!outPath.parent_path().empty()) {
        std::filesystem::create_directories(outPath.parent_path());
    }

    std::ofstream csvOut(path);
    if (!csvOut) {
        throw std::runtime_error("Failed to open variation samples CSV file: " + path);
    }
    WriteVariationSamplesCsv(csvOut, result);
}

std::string ShellQuote(const std::string &value) {
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

bool WriteVariationHistogramFile(const std::string &samplesPath, const std::string &plotPath) {
    const std::filesystem::path scriptPath = "scripts/plot_variation_histograms.py";
    if (!std::filesystem::exists(scriptPath)) {
        std::cerr << "[Output] Warning: variation histogram plotter not found: "
                  << scriptPath.string() << std::endl;
        return false;
    }

    std::filesystem::path outPath(plotPath);
    if (!outPath.parent_path().empty()) {
        std::filesystem::create_directories(outPath.parent_path());
    }

    const std::string command = "python3 "
        + ShellQuote(scriptPath.string())
        + " "
        + ShellQuote(samplesPath)
        + " -o "
        + ShellQuote(plotPath)
        + " >/dev/null 2>&1";
    const int rc = std::system(command.c_str());
    if (rc != 0 || !std::filesystem::exists(plotPath)) {
        std::cerr << "[Output] Warning: failed to generate variation histogram SVG with matplotlib: "
                  << plotPath << std::endl;
        return false;
    }
    return true;
}

}  // namespace

void EvaCamOutput::PrintConsoleSummary(const EvaCamConfig &config,
        long long numSolution,
        const std::vector<std::shared_ptr<CAM_Result>> &bestResults,
        const std::string &explorationOutputFileName) {
    std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());

    if (!DerivedValueHelpers::IsFullExploration(config.input)) {
        if (numSolution > 0) {
            bestResults[config.input.optimizationTarget]->print();
            const CAMMonteCarloSummary &variation =
                    bestResults[config.input.optimizationTarget]->bank->mat->subarray->monteCarloSummary;
            if (variation.enabled) {
                std::cout << std::endl
                          << "========="
                          << std::endl
                          << "VARIATION"
                          << std::endl
                          << "========="
                          << std::endl;
                std::cout << "Mode   : " << variation.mode << std::endl;
                std::cout << "Samples: " << variation.samples << std::endl;
                if (variation.mode == "single_point") {
                    PrintVariationSampleMetric("Matchline Delay", 1e12, "ps", variation.matchlineDelay);
                    PrintVariationSampleMetric("Search Latency", 1e12, "ps", variation.searchLatency);
                    PrintVariationSampleMetric("Search Energy", 1e12, "pJ", variation.searchDynamicEnergy);
                    PrintVariationSampleMetric("Sense Margin", 1e3, "mV", variation.senseMargin);
                } else {
                    PrintVariationMetric("Matchline Delay", 1e12, "ps", variation.matchlineDelay);
                    PrintVariationMetric("Search Latency", 1e12, "ps", variation.searchLatency);
                    PrintVariationMetric("Search Energy", 1e12, "pJ", variation.searchDynamicEnergy);
                    PrintVariationMetric("Sense Margin", 1e3, "mV", variation.senseMargin);
                }
            }
        } else {
            std::cout << "No valid solutions." << std::endl;
        }

        std::cout << std::endl << "Finished!" << std::endl;
        return;
    }

    std::cout << std::endl << explorationOutputFileName << " generated successfully!" << std::endl;
    if (config.constraints.pruningEnabled) {
        std::cout << "The results are pruned" << std::endl;
    }

    for (int i = 0; i < (int)full_exploration; i++) {
        std::cout << "[" << std::left << std::setw(2) << i << "]" << " ";
        std::cout << std::left << std::setw(8) << bestResults[i]->bank->readLatency * 1e12 << "    ";
        std::cout << std::left << std::setw(8) << bestResults[i]->bank->writeLatency * 1e12 << "    ";
        std::cout << std::left << std::setw(5) << bestResults[i]->bank->readDynamicEnergy * 1e12 << "    ";
        std::cout << std::left << std::setw(5) << bestResults[i]->bank->writeDynamicEnergy * 1e12 << "    ";
        std::cout << std::left << std::setw(9) << bestResults[i]->bank->readLatency * bestResults[i]->bank->readDynamicEnergy * 1e24 << "    ";
        std::cout << std::left << std::setw(9) << bestResults[i]->bank->writeLatency * bestResults[i]->bank->writeDynamicEnergy * 1e24 << "    ";
        std::cout << std::left << std::setw(5) << bestResults[i]->bank->leakage * 1e6 << "    ";
        std::cout << std::left << std::setw(8) << bestResults[i]->bank->area * 1e12 << "    ";
        std::cout << std::left << std::setw(8) << bestResults[i]->bank->searchLatency * 1e12 << "    ";
        std::cout << std::left << std::setw(5) << bestResults[i]->bank->searchDynamicEnergy * 1e12 << "    ";
        std::cout << std::left << std::setw(8) << bestResults[i]->bank->searchDynamicEnergy * bestResults[i]->bank->searchLatency * 1e24 << std::endl;
    }
    std::cout << std::left << std::setw(8) << bestResults[7]->bank->numBitSerial << "	"
        << std::left << std::setw(8) << bestResults[8]->bank->numBitSerial << "	"
        << std::left << std::setw(8) << bestResults[9]->bank->numBitSerial << "	"
        << std::left << std::setw(8) << bestResults[3]->bank->numBitSerial << "	"
        << std::left << std::setw(8) << bestResults[6]->bank->numBitSerial << "	"
        << std::endl;
    std::cout << std::left << std::setw(8) << bestResults[7]->bank->mat->areaOptimizationLevel << "	"
        << std::left << std::setw(8) << bestResults[8]->bank->mat->areaOptimizationLevel << "	"
        << std::left << std::setw(8) << bestResults[9]->bank->mat->areaOptimizationLevel << "	"
        << std::left << std::setw(8) << bestResults[3]->bank->mat->areaOptimizationLevel << "	"
        << std::left << std::setw(8) << bestResults[6]->bank->mat->areaOptimizationLevel << "	"
        << std::endl;
    std::cout << std::left << std::setw(8) << bestResults[7]->bank->mat->subarray->DriverOptLevel << "	"
        << std::left << std::setw(8) << bestResults[8]->bank->mat->subarray->DriverOptLevel << "	"
        << std::left << std::setw(8) << bestResults[9]->bank->mat->subarray->DriverOptLevel << "	"
        << std::left << std::setw(8) << bestResults[3]->bank->mat->subarray->DriverOptLevel << "	"
        << std::left << std::setw(8) << bestResults[6]->bank->mat->subarray->DriverOptLevel << "	"
        << std::endl;
    std::cout << std::left << std::setw(8) << bestResults[7]->bank->area * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[8]->bank->area * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[9]->bank->area * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[3]->bank->area * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[6]->bank->area * 1e12 << "	"
        << std::endl;
    std::cout << std::left << std::setw(8) << bestResults[7]->bank->searchLatency * 1e9 << "	"
        << std::left << std::setw(8) << bestResults[8]->bank->searchLatency * 1e9 << "	"
        << std::left << std::setw(8) << bestResults[9]->bank->searchLatency * 1e9 << "	"
        << std::left << std::setw(8) << bestResults[3]->bank->searchLatency * 1e9 << "	"
        << std::left << std::setw(8) << bestResults[6]->bank->searchLatency * 1e9 << "	"
        << std::endl;
    std::cout << std::left << std::setw(8) << bestResults[7]->bank->searchDynamicEnergy * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[8]->bank->searchDynamicEnergy * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[9]->bank->searchDynamicEnergy * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[3]->bank->searchDynamicEnergy * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[6]->bank->searchDynamicEnergy * 1e12 << "	"
        << std::endl;
    std::cout << std::left << std::setw(8) << bestResults[7]->bank->writeDynamicEnergy * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[8]->bank->writeDynamicEnergy * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[9]->bank->writeDynamicEnergy * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[3]->bank->writeDynamicEnergy * 1e12 << "	"
        << std::left << std::setw(8) << bestResults[6]->bank->writeDynamicEnergy * 1e12 << "	"
        << std::endl;
    std::cout << std::left << std::setw(8) << bestResults[7]->bank->leakage * 1e6 << "	"
        << std::left << std::setw(8) << bestResults[8]->bank->leakage * 1e6 << "	"
        << std::left << std::setw(8) << bestResults[9]->bank->leakage * 1e6 << "	"
        << std::left << std::setw(8) << bestResults[3]->bank->leakage * 1e6 << "	"
        << std::left << std::setw(8) << bestResults[6]->bank->leakage * 1e6 << "	"
        << std::endl;
}

void EvaCamOutput::WriteYamlResults(const EvaCamConfig &config,
        const std::string &outputYamlFileName,
        long long numSolution,
        const std::vector<std::shared_ptr<CAM_Result>> &bestResults) {
    std::filesystem::path outPath(outputYamlFileName);
    if (!outPath.parent_path().empty()) {
        std::filesystem::create_directories(outPath.parent_path());
    }

    if (numSolution <= 0) {
        std::ofstream yamlOut(outputYamlFileName);
        if (!yamlOut) {
            throw std::runtime_error("Failed to open YAML output file: " + outputYamlFileName);
        }
        WriteResultsYamlNoSolutions(yamlOut);
    } else if (DerivedValueHelpers::IsFullExploration(config.input)) {
        std::unordered_map<OptimizationTarget, std::string> variationSamplesFiles;
        std::unordered_map<OptimizationTarget, std::string> variationPlotFiles;
        for (const auto &result : bestResults) {
            if (!HasMonteCarloSamples(result)) {
                continue;
            }

            const std::string samplePath = OutputPathBuilder::VariationSamplesCsvPath(
                    outputYamlFileName,
                    OptimizationTargetName(result->optimizationTarget));
            const std::string plotPath = OutputPathBuilder::VariationHistogramsSvgPath(samplePath);
            WriteVariationSamplesFile(*result, samplePath);
            variationSamplesFiles[result->optimizationTarget] = samplePath;
            if (config.variationPlots && WriteVariationHistogramFile(samplePath, plotPath)) {
                variationPlotFiles[result->optimizationTarget] = plotPath;
            }
        }

        std::ofstream yamlOut(outputYamlFileName);
        if (!yamlOut) {
            throw std::runtime_error("Failed to open YAML output file: " + outputYamlFileName);
        }
        WriteResultsYamlMulti(yamlOut, AsResults(bestResults), variationSamplesFiles, variationPlotFiles);
    } else {
        const auto &result = bestResults[config.input.optimizationTarget];
        std::string variationSamplesFile;
        std::string variationPlotFile;
        if (HasMonteCarloSamples(result)) {
            variationSamplesFile = OutputPathBuilder::VariationSamplesCsvPath(outputYamlFileName);
            variationPlotFile = OutputPathBuilder::VariationHistogramsSvgPath(variationSamplesFile);
            WriteVariationSamplesFile(*result, variationSamplesFile);
            if (!config.variationPlots || !WriteVariationHistogramFile(variationSamplesFile, variationPlotFile)) {
                variationPlotFile.clear();
            }
        }

        std::ofstream yamlOut(outputYamlFileName);
        if (!yamlOut) {
            throw std::runtime_error("Failed to open YAML output file: " + outputYamlFileName);
        }
        WriteResultsYaml(yamlOut, *result, variationSamplesFile, variationPlotFile);
    }
}
