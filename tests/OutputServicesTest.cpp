#include "EvaCamConfig.h"
#include "EvaCamConfigPrinter.h"
#include "EvaCamContextBuilder.h"
#include "EvaCamExplorer.h"
#include "EvaCamOutput.h"
#include "EvaCamResultExtractor.h"
#include "Result.h"
#include "TestSupport.h"
#include "input/CliOptions.h"
#include "output/EvaCamOutputDetail.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace {

EvaCamExplorationResult RunConfig(const std::string &configFile,
        std::shared_ptr<EvaCamConfig> *config) {
    CliOptions options;
    options.inputFileName = configFile;
    EvaCamContext context = EvaCamContextBuilder::Build(options);
    *config = context.config;
    EvaCamExplorationResult exploration = EvaCamExplorer(context.config, 1).Run();
    assert(exploration.numSolution > 0);
    return exploration;
}

void TestExtractorCompletePartialAndNoSolutions() {
    std::shared_ptr<EvaCamConfig> config;
    EvaCamExplorationResult exploration = RunConfig(
            "config/2FeFET_TCAM/2FeFET_TCAM_explicit_subarray.config.yaml", &config);
    const auto valid = exploration.bestResults.at(leakage_optimized);
    assert(valid && valid->bank && valid->bank->initialized);

    auto incomplete = std::make_shared<Result>();
    std::vector<std::shared_ptr<Result>> results = {nullptr, incomplete, valid};
    EvaCamRunResultDto dto = ExtractEvaCamRunResult(3, results, "exploration.csv", "output.yaml");
    assert(dto.numSolutions == 3);
    assert(dto.explorationCsvPath == "exploration.csv");
    assert(dto.outputYamlPath == "output.yaml");
    assert(dto.bestResults.size() == 1);

    const EvaCamDesignResultDto &design = dto.bestResults.at("LeakagePower");
    assert(design.optimizationTarget == "LeakagePower");
    assert(design.summary.at("area.total.area_m2") == valid->bank->area);
    assert(design.summary.at("bandwidth.write_Bps") > 0);
    assert(design.summary.at("timing.exact_match_sense_margin_v")
            == valid->bank->mat->subarray->senseMargin);
    assert(design.summary.at("timing.minimum_required_sense_margin_v")
            == valid->bank->mat->subarray->senseVoltage);
    assert(design.geometry.at("capacity_bits") == valid->bank->capacity);
    assert(design.breakdown.count("search_latency.h_tree_s") == 1);
    assert(!design.variation.enabled);

    EvaCamRunResultDto noSolutions = ExtractEvaCamRunResult(0, results, "a.csv", "b.yaml");
    assert(noSolutions.numSolutions == 0);
    assert(noSolutions.bestResults.empty());
    assert(noSolutions.explorationCsvPath == "a.csv");
}

void TestExtractorVariation() {
    std::shared_ptr<EvaCamConfig> config;
    EvaCamExplorationResult exploration = RunConfig(
            "config/2FeFET_TCAM/2FeFET_TCAM_corner.config.yaml", &config);
    const auto result = exploration.bestResults.at(leakage_optimized);
    EvaCamRunResultDto dto = ExtractEvaCamRunResult(1, {result}, "", "");
    const EvaCamVariationDto &variation = dto.bestResults.at("LeakagePower").variation;
    assert(variation.enabled);
    assert(variation.mode == "corner");
    assert(variation.samples > 0);
    assert(variation.matchlineDelay.available);
    assert(variation.searchLatency.available);
    assert(variation.searchDynamicEnergy.available);
    assert(variation.senseMargin.available);
    assert(variation.referenceDelay.available);
    assert(!variation.sampleData.empty());
    assert(!variation.sampleData.front().cornerLabel.empty());
}

void TestConfigPrinterAndConsoleSummary() {
    std::shared_ptr<EvaCamConfig> config;
    EvaCamExplorationResult exploration = RunConfig(
            "config/2FeFET_TCAM/2FeFET_TCAM_explicit_subarray.config.yaml", &config);

    TestSupport::StreamCapture configCapture(std::cout);
    EvaCamConfigPrinter::Print(*config);
    configCapture.Stop();
    const std::string printedConfig = configCapture.Text();
    assert(printedConfig.find("DESIGN SPECIFICATION") != std::string::npos);
    assert(printedConfig.find("Memory Cell: FEFET RAM") != std::string::npos);
    assert(printedConfig.find("Searching for the best solution") != std::string::npos);

    TestSupport::StreamCapture noSolutionCapture(std::cout);
    EvaCamOutput::PrintConsoleSummary(*config, 0, {}, "unused.csv");
    noSolutionCapture.Stop();
    assert(noSolutionCapture.Text().find("No valid solutions.") != std::string::npos);
    assert(noSolutionCapture.Text().find("Minimum Required Sense Margin") != std::string::npos);
    assert(noSolutionCapture.Text().find("Finished!") != std::string::npos);

    TestSupport::StreamCapture solutionCapture(std::cout);
    EvaCamOutput::PrintConsoleSummary(*config, exploration.numSolution,
            exploration.bestResults, "unused.csv");
    solutionCapture.Stop();
    assert(solutionCapture.Text().find("SUMMARY RESULT") != std::string::npos);
    assert(solutionCapture.Text().find("Exact-Match Sense Margin") != std::string::npos);
    assert(solutionCapture.Text().find("Minimum Required Sense Margin") != std::string::npos);
    assert(solutionCapture.Text().find("Finished!") != std::string::npos);

    config->input.optimizationTarget = full_exploration;
    config->exploration.pruningEnabled = true;
    TestSupport::StreamCapture pruningConfigCapture(std::cout);
    EvaCamConfigPrinter::Print(*config);
    pruningConfigCapture.Stop();
    assert(pruningConfigCapture.Text().find("Pareto frontier filtering enabled")
            != std::string::npos);

    TestSupport::StreamCapture emptyFrontierCapture(std::cout);
    EvaCamOutput::PrintConsoleSummary(*config, 0, {}, "empty.csv");
    emptyFrontierCapture.Stop();
    assert(emptyFrontierCapture.Text().find("No valid solutions.")
            != std::string::npos);
    assert(emptyFrontierCapture.Text().find("Finished!") != std::string::npos);
}

void TestYamlAndVariationSampleWrites() {
    TestSupport::TemporaryDirectory directory("evacam-output-services");
    std::shared_ptr<EvaCamConfig> config;
    EvaCamExplorationResult exploration = RunConfig(
            "config/2FeFET_TCAM/2FeFET_TCAM_corner.config.yaml", &config);
    config->variationPlots = false;
    const std::filesystem::path yamlPath = directory.Path() / "nested output/results.yaml";
    EvaCamOutput::WriteYamlResults(*config, yamlPath.string(), exploration.numSolution,
            exploration.bestResults);
    assert(std::filesystem::exists(yamlPath));
    YAML::Node yaml = YAML::LoadFile(yamlPath.string());
    const YAML::Node variation = yaml["summary"]["timing"]["variation"];
    assert(variation["mode"].as<std::string>() == "corner");
    assert(variation["sample_file"]);
    assert(!variation["plot_file"]);
    const std::filesystem::path samplePath = variation["sample_file"].as<std::string>();
    assert(std::filesystem::exists(samplePath));

    const std::filesystem::path noSolutionsPath = directory.Path() / "none/results.yaml";
    EvaCamOutput::WriteYamlResults(*config, noSolutionsPath.string(), 0, {});
    YAML::Node noSolutions = YAML::LoadFile(noSolutionsPath.string());
    assert(noSolutions["status"].as<std::string>() == "no_valid_solutions");
    assert(noSolutions["summary"]["timing"]["minimum_required_sense_margin"]);
}

void TestHistogramCommandQuotingAndFailureInjection() {
    assert(EvaCamOutputDetail::ShellQuote("a'b c") == "'a'\\''b c'");
    const std::string command = EvaCamOutputDetail::BuildVariationHistogramCommand(
            "samples with 'quote.csv", "plot with 'quote.svg");
    assert(command.find("'samples with '\\''quote.csv'") != std::string::npos);
    assert(command.find("'plot with '\\''quote.svg'") != std::string::npos);

    TestSupport::TemporaryDirectory directory("evacam-histogram-command");
    const std::string plotPath = (directory.Path() / "plot.svg").string();
    std::string capturedCommand;
    TestSupport::StreamCapture errorCapture(std::cerr);
    const bool failed = EvaCamOutputDetail::WriteVariationHistogramFile(
            "samples.csv", plotPath,
            [&](const std::string &value) {
                capturedCommand = value;
                return 7;
            });
    errorCapture.Stop();
    assert(!failed);
    assert(capturedCommand == EvaCamOutputDetail::BuildVariationHistogramCommand(
            "samples.csv", plotPath));
    assert(errorCapture.Text().find("failed to generate") != std::string::npos);

    const bool succeeded = EvaCamOutputDetail::WriteVariationHistogramFile(
            "samples.csv", plotPath,
            [&](const std::string &) {
                std::ofstream output(plotPath);
                output << "<svg/>";
                return 0;
            });
    assert(succeeded);
}

void TestVariationSampleDetection() {
    assert(!EvaCamOutputDetail::HasVariationSamples(nullptr));
    assert(!EvaCamOutputDetail::HasMonteCarloSamples(nullptr));

    std::shared_ptr<EvaCamConfig> config;
    EvaCamExplorationResult exploration = RunConfig(
            "config/2FeFET_TCAM/2FeFET_TCAM_explicit_subarray.config.yaml", &config);
    const std::shared_ptr<Result> result = exploration.bestResults.at(leakage_optimized);
    CAM_SubArray &subarray = *result->bank->mat->subarray;
    assert(!EvaCamOutputDetail::HasVariationSamples(result));

    subarray.variationSummary.enabled = true;
    subarray.variationSummary.mode = "monte_carlo";
    subarray.variationSamples.push_back({});
    assert(EvaCamOutputDetail::HasVariationSamples(result));
    assert(EvaCamOutputDetail::HasMonteCarloSamples(result));

    subarray.variationSummary.mode = "corner";
    assert(EvaCamOutputDetail::HasVariationSamples(result));
    assert(!EvaCamOutputDetail::HasMonteCarloSamples(result));

    subarray.variationSummary.mode = "single_point";
    assert(!EvaCamOutputDetail::HasVariationSamples(result));
}

}  // namespace

int main() {
    TestExtractorCompletePartialAndNoSolutions();
    TestExtractorVariation();
    TestConfigPrinterAndConsoleSummary();
    TestYamlAndVariationSampleWrites();
    TestHistogramCommandQuotingAndFailureInjection();
    TestVariationSampleDetection();
    std::cout << "Output services tests passed" << std::endl;
}
