#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "BankWithoutHtree.h"
#include "EvaCamConfig.h"
#include "ResultsYaml.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"
#include "VariationSamplesCsv.h"

namespace {

using TestModelBuilders::MakeWire;
using TestSupport::Require;

std::shared_ptr<Result> MakeCalculatedResult() {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/2FeFET_TCAM/2FeFET_TCAM.config.yaml");
    config->input.routingMode = non_h_tree;

    const Wire localWire = MakeWire(config);
    const Wire globalWire = MakeWire(config, global_aggressive);
    auto bank = std::make_shared<BankWithoutHtree>();
    const CAM_Opt options = {area_first, area_first, 1};
    bank->Initialize(1, 1, 8192, 64, 1, 1, 1, true, 1, 1, 1, 1, 1, 1,
            area_first, config->technology.cell->camType, EX, config, localWire,
            globalWire, options);
    Require(bank->initialized && !bank->invalid, "serialization fixture bank initializes");
    bank->CalculateRC();
    bank->CalculateLatencyAndPower();

    auto result = std::make_shared<Result>();
    result->config = config;
    result->bank = bank;
    return result;
}

YAML::Node WriteAndParse(const Result &result, const std::string &samples = "",
        const std::string &plot = "") {
    std::ostringstream output;
    WriteResultsYaml(output, result, samples, plot);
    Require(output.str().find("nan") == std::string::npos, "YAML must not contain nan");
    Require(output.str().find("inf") == std::string::npos, "YAML must not contain inf");
    return YAML::Load(output.str());
}

void AssertScalar(const YAML::Node &node, const char *key) {
    Require(node[key] && node[key].IsScalar(), std::string("missing scalar: ") + key);
}

std::string CsvNumber(double value) {
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

void TestSingleResultStructureAssumptionsAndBreakdowns() {
    const auto result = MakeCalculatedResult();
    result->config->peripherals.noPrechargeInc = true;
    result->config->peripherals.includeLeakage = true;
    result->config->peripherals.scaledVoltage = 0.9;
    result->bank->mat->subarray->senseMargin = 0.1234;
    result->bank->mat->subarray->senseVoltage = 0.0456;
    const YAML::Node root = WriteAndParse(*result, "samples.csv", "plot.svg");

    Require(root["assumptions"]["model_identifier"].as<std::string>() == "evacam-cam-v1",
            "model identifier is emitted");
    Require(root["assumptions"]["routing"].as<std::string>() == "non_h_tree",
            "non-H-tree routing is emitted");
    Require(root["assumptions"]["technology"]["roadmap"].as<std::string>() == "HP",
            "roadmap is emitted");
    Require(root["assumptions"]["modeling_options"]["exclude_precharge_latency"].as<bool>(),
            "precharge assumption is emitted");
    Require(root["assumptions"]["modeling_options"]["include_leakage"].as<bool>(),
            "leakage assumption is emitted");
    Require(!root["summary"]["timing"]["variation"],
            "nominal result does not invent variation output");
    AssertScalar(root["summary"]["area"]["total"], "area");
    AssertScalar(root["summary"]["timing"], "search_latency");
    Require(root["summary"]["timing"]["exact_match_sense_margin"].as<std::string>()
                == "0.123V",
            "nominal exact-match sense margin is emitted without variation");
    Require(root["summary"]["timing"]["minimum_required_sense_margin"].as<std::string>()
                == "0.046V",
            "minimum required sense margin is emitted without variation");
    Require(root["summary"]["timing"]["sense_margin_slack"].as<std::string>()
                == "0.078V",
            "sense margin slack is emitted without variation");
    Require(root["summary"]["timing"]["sense_margin_pass"].as<bool>(),
            "sense margin pass status is emitted");
    Require(!root["summary"]["timing"]["sense_margin_enforced"].as<bool>(),
            "diagnostic sense-margin mode is emitted");
    AssertScalar(root["summary"]["timing"]["search_latency_breakdown"], "non_h_tree");
    AssertScalar(root["summary"]["power"], "search_dynamic_energy");
    AssertScalar(root["summary"]["power"]["search_dynamic_energy_breakdown"], "non_h_tree");
    AssertScalar(root["summary"]["power"], "read_dynamic_energy");
    AssertScalar(root["breakdown"]["subarray_area"], "total_cell_area");
    AssertScalar(root["breakdown"]["search_latency"], "matchline");
    AssertScalar(root["breakdown"]["search_dynamic_energy"], "cell_read");
    AssertScalar(root["breakdown"]["write_dynamic_energy"], "cell_set");
    AssertScalar(root["breakdown"]["leakage"], "sense_amplifier");
}

void TestZeroAreaDenominatorsAndFiniteFormatting() {
    const auto result = MakeCalculatedResult();
    result->config->technology.cell->area = 0;
    result->bank->area = 0;
    result->bank->mat->area = 0;
    result->bank->mat->subarray->area = 0;
    const YAML::Node root = WriteAndParse(*result);
    const YAML::Node area = root["summary"]["area"];
    Require(area["mat"]["cell_area_utilization"].as<std::string>() == "0.000%",
            "zero mat-area denominator is safe");
    Require(area["subarray"]["cell_area_utilization"].as<std::string>() == "0.000%",
            "zero subarray-area denominator is safe");
    Require(area["efficiency"].as<std::string>() == "0.000%",
            "zero bank-area denominator is safe");
}

void SetAvailable(CAMMetricStats &stats, double nominal, double sample) {
    stats.available = true;
    stats.nominal = nominal;
    stats.sample = sample;
    stats.mean = sample + 1e-12;
    stats.stddev = 2e-13;
    stats.min = sample - 1e-13;
    stats.max = sample + 2e-13;
    stats.p95 = sample + 1e-13;
}

void TestVariationSummarySinglePointAndMonteCarlo() {
    const auto result = MakeCalculatedResult();
    auto &summary = result->bank->mat->subarray->variationSummary;
    summary.enabled = true;
    summary.mode = "single_point";
    summary.samples = 1;
    SetAvailable(summary.matchlineDelay, 1e-9, 2e-9);
    SetAvailable(summary.searchLatency, 3e-9, 4e-9);
    SetAvailable(summary.searchDynamicEnergy, 5e-15, 6e-15);
    SetAvailable(summary.senseMargin, 0.7, 0.6);
    SetAvailable(summary.referenceDelay, 7e-9, 8e-9);
    YAML::Node root = WriteAndParse(*result, "single.csv", "single.svg");
    const YAML::Node variation = root["summary"]["timing"]["variation"];
    Require(root["summary"]["timing"]["exact_match_sense_margin"].as<std::string>()
                == "0.700V",
            "always-on sense margin remains nominal when variation is enabled");
    Require(variation["mode"].as<std::string>() == "single_point", "single-point mode");
    Require(variation["samples"].as<int>() == 1, "single-point samples");
    Require(variation["sample_file"].as<std::string>() == "single.csv", "sample path");
    Require(variation["plot_file"].as<std::string>() == "single.svg", "plot path");
    AssertScalar(variation["matchline_delay"], "sample");
    Require(!variation["matchline_delay"]["mean"], "single-point output omits stats");

    summary.mode = "monte_carlo";
    summary.samples = 7;
    root = WriteAndParse(*result);
    const YAML::Node monteCarlo = root["summary"]["timing"]["variation"];
    Require(monteCarlo["mode"].as<std::string>() == "monte_carlo", "Monte Carlo mode");
    AssertScalar(monteCarlo["matchline_delay"], "mean");
    AssertScalar(monteCarlo["search_dynamic_energy"], "p95");
    AssertScalar(monteCarlo["exact_match_sense_margin"], "stddev");
    AssertScalar(monteCarlo["reference_delay"], "mean");
}

void TestMultiResultTargetNamesAndRoadmapsAndNoSolutions() {
    const auto fixture = MakeCalculatedResult();
    const OptimizationTarget targets[] = {read_latency_optimized, write_latency_optimized,
        read_energy_optimized, write_energy_optimized, read_edp_optimized,
        write_edp_optimized, leakage_optimized, area_optimized,
        search_latency_optimized, search_energy_optimized, search_edp_optimized,
        full_exploration};
    const char *names[] = {"ReadLatency", "WriteLatency", "ReadDynamicEnergy",
        "WriteDynamicEnergy", "ReadEDP", "WriteEDP", "LeakagePower", "Area",
        "SearchLatency", "SearchEnergy", "SearchEDP", "Exploration"};
    std::vector<std::shared_ptr<Result>> results;
    for (OptimizationTarget target : targets) {
        auto result = std::make_shared<Result>(*fixture);
        result->optimizationTarget = target;
        results.push_back(result);
    }
    std::ostringstream output;
    WriteResultsYamlMulti(output, results, {{area_optimized, "area.csv"}},
            {{area_optimized, "area.svg"}});
    const YAML::Node root = YAML::Load(output.str());
    for (const char *name : names) {
        Require(root[name]["summary"] && root[name]["breakdown"],
                std::string("multi-result section: ") + name);
    }
    Require(!root["Area"]["summary"]["timing"]["variation"],
            "empty variation is absent for multi result");

    for (DeviceRoadmap roadmap : {HP, LSTP, LOP, FEFET, LP}) {
        EvaCamConfig config;
        config.input.processNode = 90;
        config.input.deviceRoadmap = roadmap;
        config.input.routingMode = h_tree;
        config.peripherals.scaledVoltage = 1.0;
        std::ostringstream noSolutions;
        WriteResultsYamlNoSolutions(noSolutions, config);
        const YAML::Node noSolutionRoot = YAML::Load(noSolutions.str());
        Require(noSolutionRoot["status"].as<std::string>() == "no_valid_solutions",
                "no-solution status");
        Require(noSolutionRoot["summary"]["timing"]
                    ["minimum_required_sense_margin"].IsScalar(),
                "no-solution minimum sense margin");
        Require(noSolutionRoot["assumptions"]["technology"]["roadmap"].IsScalar(),
                "every roadmap serializes");
        Require(noSolutionRoot["assumptions"]["routing"].as<std::string>() == "h_tree",
                "H-tree assumption serializes");
    }
}

void TestVariationSamplesCsvHeaderQuotingAndSamples() {
    Result empty;
    std::ostringstream emptyOutput;
    WriteVariationSamplesCsv(emptyOutput, empty);
    const std::string header = "sample,corner_label,memory_device_res_on_corner,"
            "memory_device_res_off_corner,matchline_delay_s,search_latency_s,"
            "search_dynamic_energy_j,exact_match_sense_margin_v,reference_delay_s,"
            "nominal_matchline_delay_s,nominal_search_latency_s,"
            "nominal_search_dynamic_energy_j,nominal_exact_match_sense_margin_v,"
            "nominal_reference_delay_s\n";
    Require(emptyOutput.str() == header, "CSV emits only the stable header without a bank");

    const auto result = MakeCalculatedResult();
    auto &subarray = *result->bank->mat->subarray;
    subarray.variationSummary.matchlineDelay.nominal = 1.25e-9;
    subarray.variationSummary.searchLatency.nominal = 2.5e-9;
    subarray.variationSummary.searchDynamicEnergy.nominal = 3.75e-15;
    subarray.variationSummary.senseMargin.nominal = 0.65;
    subarray.variationSummary.referenceDelay.nominal = 4.5e-9;
    subarray.variationSamples = {
        {0, "nominal", "nominal", "nominal", 1e-9, 2e-9, 3e-15, 0.6, 4e-9},
        {1, "corner, \"slow\"", "on,low", "off\"high", 5e-9, 6e-9, 7e-15, 0.5, 8e-9}
    };
    std::ostringstream output;
    WriteVariationSamplesCsv(output, *result);
    const std::string csv = output.str();
    Require(csv.find("0,\"nominal\",\"nominal\",\"nominal\",1.0000000000000001e-09")
                    != std::string::npos, "nominal row preserves numeric field order");
    Require(csv.find("1,\"corner, \"\"slow\"\"\",\"on,low\",\"off\"\"high\"")
                    != std::string::npos, "corner labels use CSV quoting");
    const std::string nominalColumns = "," + CsvNumber(1.25e-9) + ","
            + CsvNumber(2.5e-9) + "," + CsvNumber(3.75e-15) + ","
            + CsvNumber(0.65) + "," + CsvNumber(4.5e-9) + "\n";
    Require(csv.find(nominalColumns) != std::string::npos,
            "nominal fields follow every sample");
}

}  // namespace

int main() {
    TestSingleResultStructureAssumptionsAndBreakdowns();
    TestZeroAreaDenominatorsAndFiniteFormatting();
    TestVariationSummarySinglePointAndMonteCarlo();
    TestMultiResultTargetNamesAndRoadmapsAndNoSolutions();
    TestVariationSamplesCsvHeaderQuotingAndSamples();
    std::cout << "Results serialization tests passed\n";
    return 0;
}
