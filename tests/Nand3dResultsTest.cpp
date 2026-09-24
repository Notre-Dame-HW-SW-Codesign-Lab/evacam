#include <algorithm>
#include <cmath>
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
#include "Result.h"
#include "ResultsYaml.h"
#include "TestSupport.h"

namespace {

using TestSupport::Require;

void RequireClose(double actual, double expected, const std::string &message) {
    Require(std::isfinite(actual) && std::isfinite(expected)
            && std::abs(actual - expected) <= std::max(std::abs(expected), 1e-30) * 1e-12,
            message);
}

std::shared_ptr<Result> MakeResult(RoutingMode route, const std::string &placement) {
    auto config = std::make_shared<EvaCamConfig>();
    config->logger.SetOutputEnabled(false);
    config->ReadConfigFromFile("config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml");
    config->input.routingMode = route;
    config->technology.cell->nand3d.peripheralPlacement = placement;
    config->technology.cell->nand3d.electrical.source = "Synthetic 3D: \"fixture\"\nRC verification only";
    EvaCamExplorer explorer(config, 1);
    const auto run = explorer.Run();
    Require(run.numSolution > 0, "3D NAND fixture has a legal design");
    for (const auto &result : run.bestResults) {
        if (result && result->bank && result->bank->initialized && result->bank->mat
                && result->bank->mat->subarray && result->bank->mat->subarray->nandModel) {
            return result;
        }
    }
    throw std::runtime_error("3D NAND run returned no initialized result");
}

void TestNand3dResultContract(RoutingMode route, const std::string &placement = "beside") {
    const auto result = MakeResult(route, placement);
    const auto &bank = *result->bank;
    const auto &sub = *bank.mat->subarray;
    const auto &metrics = sub.nandModel->Metrics();
    const auto &device = result->config->technology.cell->nand3d;
    const auto dto = ExtractEvaCamDesignResult(*result);
    Require(dto.metadata.at("model_identifier") == "evacam-nand3d-tcam-v1", "3D model identifier");
    Require(dto.metadata.at("array_layout") == "vertical_3d", "3D layout is not reported as planar");
    Require(dto.metadata.at("calibration_status") == "synthetic", "solver verification does not imply device calibration");
    Require(dto.metadata.at("device_validation") == "not_performed_by_evacam",
            "numerical tests do not fabricate physical device validation");
    Require(dto.metadata.at("terminal_conductance_model") == "dc_linear_resistor_network",
            "terminal conductance describes the supplied linear circuit");
    Require(dto.metadata.at("model_source") == device.electrical.source, "3D model source is preserved");
    Require(dto.metadata.at("model_backend") == device.electrical.model, "model identifier agrees with selected YAML backend");
    Require(dto.metadata.count("transient_solver") != 0, "transient solver is identified");
    Require(dto.metadata.at("peripheral_placement") == placement, "supported peripheral placement survives factory and output");
    Require(dto.metadata.at("read_metrics") == "unavailable", "conventional NAND read remains unavailable");
    Require(dto.summary.count("timing.slowest_match_time_constant_s") == 0
            && dto.summary.count("timing.fastest_mismatch_time_constant_s") == 0,
            "transient result does not fabricate single-exponential time constants");
    Require(dto.summary.count("timing.read_latency_s") == 0
            && dto.summary.count("energy.read_dynamic_j") == 0,
            "3D NAND omits unsupported read metrics");
    Require(!sub.inputBuf && !sub.senseAmp && !sub.precharger,
            "3D NAND results do not require legacy parallel-CAM peripherals");

    const double entries = static_cast<double>(device.stringRows) * device.stringColumns;
    const double blocks = dto.geometry.at("block_count");
    RequireClose(dto.geometry.at("strings_per_block"), entries, "3D string grid resolves entry count");
    RequireClose(dto.geometry.at("physical_page_bits"), device.stringColumns,
            "one physical page spans one select group");
    RequireClose(dto.geometry.at("physical_cell_count"), entries * device.storageLayers * blocks,
            "physical storage counts vertical storage layers without padding double-counting");
    RequireClose(dto.geometry.at("allocated_capacity_bits"), entries * metrics.keyWidth * blocks,
            "logical capacity excludes complementary, validity, and dummy devices");
    RequireClose(dto.geometry.at("storage_layers"), device.storageLayers, "3D layer count is emitted");
    RequireClose(dto.geometry.at("string_rows"), device.stringRows, "3D group count is emitted");
    RequireClose(dto.geometry.at("string_columns"), device.stringColumns, "3D group width is emitted");
    Require(dto.geometry.at("vertical_stack_height_m") > 0, "vertical stack height is explicit");
    Require(dto.geometry.at("staircase_area_m2") > 0, "staircase area is explicit");
    Require(dto.geometry.at("occupied_footprint_area_m2") > 0, "3D footprint is explicit");
    for (const auto &item : metrics.geometryMetrics) {
        RequireClose(dto.geometry.at(item.first), item.second, "backend geometry survives extraction");
    }
    Require(!metrics.diagnosticMetrics.empty(), "finite-precharge/solver diagnostics are available");
    for (const auto &item : metrics.diagnosticMetrics) {
        RequireClose(dto.summary.at("diagnostics." + item.first), item.second,
                "backend numerical diagnostics survive extraction");
    }
    RequireClose(dto.summary.at("timing.program_page_latency_s"), device.electrical.programPage.latency,
            "page program cost keeps its physical scope");
    RequireClose(dto.summary.at("energy.erase_block_dynamic_j"), device.electrical.eraseBlock.energy,
            "block erase cost keeps its physical scope");
    const std::string routeName = route == h_tree ? "h_tree" : "non_h_tree";
    RequireClose(dto.breakdown.at("search_latency.local_all_rounds_s")
            + dto.breakdown.at("search_latency." + routeName + "_s"), bank.searchLatency,
            "whole-query latency includes local block schedule and routing");

    std::ostringstream serialized;
    WriteResultsYaml(serialized, *result);
    const auto yaml = YAML::Load(serialized.str());
    Require(yaml["assumptions"]["model_identifier"].as<std::string>()
            == "evacam-nand3d-tcam-v1", "YAML assumptions identify 3D backend");
    Require(yaml["assumptions"]["modeling_options"]["strict_sense_margin"].as<bool>(),
            "YAML assumptions identify mandatory NAND3D margin rejection");
    Require(yaml["assumptions"]["nand"]["model_source"].as<std::string>()
            == device.electrical.source, "YAML assumptions use 3D electrical provenance");
    Require(yaml["metadata"]["array_layout"].as<std::string>() == "vertical_3d", "YAML includes 3D metadata");
    RequireClose(yaml["summary"]["timing"]["search_latency_s"].as<double>(),
            dto.summary.at("timing.search_latency_s"), "YAML and structured result share SI search timing");
    RequireClose(yaml["geometry"]["physical_cell_count"].as<double>(), dto.geometry.at("physical_cell_count"),
            "YAML and structured result share physical cell count");
    Require(!yaml["summary"]["timing"]["slowest_match_time_constant_s"], "YAML omits one-pole parameters");
    Require(yaml["summary"]["diagnostics"].IsMap(), "YAML groups numerical diagnostics separately");

    std::ostringstream noSolutions;
    WriteResultsYamlNoSolutions(noSolutions, *result->config);
    const auto failedYaml = YAML::Load(noSolutions.str());
    RequireClose(failedYaml["summary"]["timing"]["minimum_required_sense_margin_v"].as<double>(),
            device.electrical.minSenseMargin, "failed 3D run uses the 3D margin requirement");
    Require(failedYaml["assumptions"]["nand"]["calibration_status"].as<std::string>() == "synthetic",
            "failed 3D run retains provenance");

    TestSupport::StreamCapture captured(std::cout);
    result->print();
    EvaCamConfigPrinter::Print(*result->config);
    result->config->technology.cell->PrintCell();
    EvaCamOutput::PrintConsoleSummary(*result->config, 0, {}, "");
    captured.Stop();
    const auto &console = captured.Text();
    Require(console.find("3D NAND") != std::string::npos, "console names the 3D device");
    Require(console.find("Sampled-match") != std::string::npos, "console does not claim exhaustive pattern bounds");
    Require(console.find("synthetic") != std::string::npos, "console states physical calibration status");
    Require(console.find("Planar Area Per Flash Transistor") == std::string::npos,
            "3D console does not use a planar transistor footprint");
}

}  // namespace

int main() {
    TestNand3dResultContract(h_tree);
    TestNand3dResultContract(non_h_tree);
    TestNand3dResultContract(h_tree, "under_array");
    std::cout << "3D NAND output tests passed\n";
}
