#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>

#include "EvaCamConfig.h"
#include "EvaCamConfigPrinter.h"
#include "EvaCamExplorer.h"
#include "EvaCamOutput.h"
#include "EvaCamResultExtractor.h"
#include "NandCamModel.h"
#include "Result.h"
#include "ResultsYaml.h"
#include "SubarrayDimensionTester.h"
#include "TestSupport.h"

namespace {

using TestSupport::Require;

std::shared_ptr<Result> MakeResult(RoutingMode route) {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/NAND_TCAM/NAND_TCAM.config.yaml");
    config->input.routingMode = route;
    config->technology.cell->nand.source = "Synthetic: \"fixture\"\nSecond line # provenance";
    EvaCamExplorer explorer(config, 1);
    auto run = explorer.Run();
    Require(run.numSolution > 0, "NAND output fixture finds a legal design");
    for (const auto &result : run.bestResults) {
        if (result && result->bank && result->bank->initialized
                && result->bank->mat && result->bank->mat->subarray
                && result->bank->mat->subarray->nandModel) {
            return result;
        }
    }
    throw std::runtime_error("NAND output fixture has no initialized result");
}

void RequireClose(double actual, double expected, const std::string &message) {
    Require(std::abs(actual - expected) <= std::max(std::abs(expected), 1e-30) * 1e-12,
            message);
}

void TestNandOutputContract(RoutingMode route) {
    const auto result = MakeResult(route);
    const auto &bank = *result->bank;
    const auto &sub = *bank.mat->subarray;
    const auto &metrics = sub.nandModel->Metrics();
    Require(!sub.inputBuf && !sub.senseAmp && !sub.precharger,
            "NAND serializers do not need legacy CAM peripherals");
    const auto dto = ExtractEvaCamDesignResult(*result);
    Require(dto.metadata.at("model_identifier") == "evacam-nand-tcam-v1",
            "NAND model identifier");
    Require(dto.metadata.at("calibration_status") == "synthetic",
            "synthetic model cannot claim device calibration");
    Require(dto.metadata.at("read_metrics") == "unavailable", "read availability is explicit");
    Require(dto.summary.count("timing.read_latency_s") == 0
            && dto.summary.count("energy.read_dynamic_j") == 0,
            "unmodeled reads are not fabricated as zero-valued metrics");
    RequireClose(dto.summary.at("timing.program_page_latency_s"), metrics.programPageLatency,
            "page latency retains operation granularity");
    RequireClose(dto.summary.at("energy.erase_block_dynamic_j"), metrics.eraseBlockEnergy,
            "erase energy retains operation granularity");
    RequireClose(dto.summary.at("timing.exact_match_sense_margin_v"), metrics.senseMargin,
            "available reference margin is signed");
    const std::string routeName = route == h_tree ? "h_tree" : "non_h_tree";
    RequireClose(dto.breakdown.at("search_latency.local_all_rounds_s")
            + dto.breakdown.at("search_latency." + routeName + "_s"),
            bank.searchLatency, "bank latency includes local rounds and routing");
    RequireClose(dto.breakdown.at("search_dynamic_energy.local_all_blocks_j")
            + dto.breakdown.at("search_dynamic_energy." + routeName + "_j"),
            bank.searchDynamicEnergy, "bank search energy includes every block and routing");
    RequireClose(dto.geometry.at("physical_cell_count"),
            metrics.physicalCells * dto.geometry.at("block_count"),
            "physical capacity includes encoding, validity, and padding");

    std::ostringstream output;
    WriteResultsYaml(output, *result);
    const auto yaml = YAML::Load(output.str());
    Require(yaml["metadata"]["model_source"].as<std::string>() == metrics.modelSource,
            "source provenance is serialized as a quoted YAML string");
    Require(yaml["assumptions"]["model_identifier"].as<std::string>()
            == dto.metadata.at("model_identifier"), "YAML assumptions identify NAND");
    RequireClose(yaml["summary"]["timing"]["search_latency_s"].as<double>(),
            dto.summary.at("timing.search_latency_s"), "YAML/DTO share exact SI search timing");
    RequireClose(yaml["summary"]["energy"]["program_page_dynamic_j"].as<double>(),
            metrics.programPageEnergy, "YAML names the physical page operation");
    Require(!yaml["summary"]["timing"]["read_latency_s"], "YAML omits unavailable read timing");
    Require(!yaml["summary"]["timing"]["variation"], "NAND does not invent variation statistics");
    Require(yaml["geometry"]["sense_rounds_per_block"].as<int>() == metrics.searchRounds,
            "sense rounds are distinct from whole-query time");

    std::ostringstream multi;
    WriteResultsYamlMulti(multi, {result});
    const auto multiYaml = YAML::Load(multi.str());
    Require(multiYaml[dto.optimizationTarget]["metadata"]["topology"].as<std::string>()
            == "nand_string", "multi-result writer shares the safe NAND branch");

    TestSupport::StreamCapture console(std::cout);
    result->print();
    console.Stop();
    Require(console.Text().find("Whole-query search latency") != std::string::npos,
            "console names whole-query timing");
    Require(console.Text().find("one physical page") != std::string::npos,
            "console names program granularity");
    Require(console.Text().find("synthetic") != std::string::npos,
            "console states model calibration status");

    bool rejectedCsv = false;
    try {
        std::ostringstream csv;
        result->printToCsvFile(csv);
    } catch (const std::runtime_error &error) {
        rejectedCsv = std::string(error.what()).find("legacy exploration CSV") != std::string::npos;
    }
    Require(rejectedCsv, "legacy read-oriented CSV schema is rejected for NAND");

    const auto runDto = ExtractEvaCamRunResult(1, {result}, "", "");
    Require(runDto.bestResults.at(dto.optimizationTarget).metadata == dto.metadata,
            "full-run extraction preserves NAND capability metadata");

    std::ostringstream noSolutions;
    WriteResultsYamlNoSolutions(noSolutions, *result->config);
    const auto noSolutionsYaml = YAML::Load(noSolutions.str());
    RequireClose(noSolutionsYaml["summary"]["timing"]
            ["minimum_required_sense_margin_v"].as<double>(), metrics.requiredSenseMargin,
            "no-solution output uses the NAND margin requirement");
    Require(noSolutionsYaml["assumptions"]["nand"]["model_source"].as<std::string>()
            == metrics.modelSource, "failed-candidate output retains provenance");

    TestSupport::StreamCapture failedConsole(std::cout);
    EvaCamOutput::PrintConsoleSummary(*result->config, 0, {}, "");
    failedConsole.Stop();
    Require(failedConsole.Text().find("Minimum Required Sense Margin: 100 mV")
            != std::string::npos, "no-solution console uses NAND minimum margin");
    Require(failedConsole.Text().find("synthetic") != std::string::npos,
            "no-solution console retains model provenance");

    TestSupport::StreamCapture specification(std::cout);
    result->config->technology.cell->PrintCell();
    EvaCamConfigPrinter::Print(*result->config);
    specification.Stop();
    Require(specification.Text().find("Query Read / Pass Bias: 0.5 / 5 V") != std::string::npos,
            "cell description prints the actual NAND model biases");
    Require(specification.Text().find("Programming Voltage") == std::string::npos,
            "cell description does not fabricate obsolete flash inputs");
    Require(specification.Text().find("Optimization: search latency") != std::string::npos,
            "configuration description identifies the actual search objective");
}

void TestDimensionTesterRejectsNandExplicitly() {
    TestSupport::TemporaryDirectory directory("nand-result-dimension-tester");
    const std::filesystem::path example = std::filesystem::absolute("config/NAND_TCAM");
    auto runConfig = YAML::LoadFile((example / "NAND_TCAM.config.yaml").string());
    runConfig["architecture"] = (example / "NAND_TCAM.architecture.yaml").string();
    runConfig["cell"] = (example / "NAND_TCAM.cell.yaml").string();
    runConfig["technology"] = std::filesystem::absolute(
            "config/lib/technology/cmos.legacy.yaml").string();
    directory.WriteFile("case_64x32.config.yaml", YAML::Dump(runConfig));
    const auto tester = directory.WriteFile("tester.yaml",
            "schema: subarray_dimension_test\n"
            "name: nand_unsupported_tester\n"
            "config_pattern: case_{rows}x{columns}.config.yaml\n"
            "rows: [64]\ncolumns: [32]\n"
            "output: {directory: results, summary_csv: summary.csv}\n");
    SubarrayDimensionTesterOptions options;
    options.configPath = tester.string();
    options.stdoutOutput = false;
    TestSupport::StreamCapture errors(std::cerr);
    const auto result = SubarrayDimensionTester::Run(options);
    errors.Stop();
    Require(result.failedRuns == 1 && result.completedRuns == 0,
            "generic dimension tester rejects NAND geometry");
    std::ifstream input(result.summaryCsvPath);
    const std::string csv(std::istreambuf_iterator<char>(input), {});
    Require(csv.find("NAND TCAM is not supported by the generic subarray dimension tester")
            != std::string::npos, "NAND tester failure is an explicit capability diagnostic");
}

void TestInfeasibleNandRunHasNoFabricatedMetrics() {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/NAND_TCAM/NAND_TCAM.config.yaml");
    config->technology.cell->nand.minSenseMargin = 0.79;
    EvaCamExplorer explorer(config, 1);
    const auto exploration = explorer.Run();
    Require(exploration.numSolution == 0,
            "impossible per-class reference margin rejects every NAND candidate");
    const auto dto = ExtractEvaCamRunResult(exploration.numSolution,
            exploration.bestResults, "", "");
    Require(dto.bestResults.empty(), "infeasible run has no design metrics");
    std::ostringstream output;
    WriteResultsYamlNoSolutions(output, *config);
    const auto yaml = YAML::Load(output.str());
    Require(yaml["status"].as<std::string>() == "no_valid_solutions", "explicit NAND failure status");
    RequireClose(yaml["summary"]["timing"]["minimum_required_sense_margin_v"].as<double>(),
            0.79, "failed run preserves the actual acceptance threshold");
    Require(!yaml["summary"]["timing"]["search_latency_s"],
            "failed run does not fabricate a search result");
}

}  // namespace

int main() {
    TestNandOutputContract(h_tree);
    TestNandOutputContract(non_h_tree);
    TestDimensionTesterRejectsNandExplicitly();
    TestInfeasibleNandRunHasNoFabricatedMetrics();
    std::cout << "NAND output tests passed\n";
}
