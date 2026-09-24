#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "EvaCamConfig.h"
#include "EvaCamConfigValidator.h"
#include "NandCamModel.h"
#include "NandCamFactory.h"
#include "Wire.h"

namespace {

void AddPattern(YAML::Node &patterns, const NandCamBackend &model,
        const std::string &name, const std::vector<int> &stored,
        const std::vector<int> &query, bool valid, double prechargeVoltage) {
    const auto result = model.Evaluate(stored, query, valid);
    YAML::Node pattern(YAML::NodeType::Map);
    pattern["name"] = name;
    pattern["stored"] = stored;
    pattern["query"] = query;
    pattern["valid"] = valid;
    pattern["ideal_hit"] = result.hit;
    pattern["model_conductance_s"] = result.matchlineConductance;
    pattern["model_voltage_v"] = result.matchlineVoltage;
    pattern["model_margin_v"] = result.senseMargin;
    pattern["model_margin_pass"] = result.senseMarginPass;
    pattern["model_margin_slack_v"] = result.senseMarginSlack;
    pattern["model_required_margin_v"] = result.requiredSenseMargin;
    pattern["model_search_latency_s"] = result.searchLatency;
    pattern["model_search_energy_j"] = result.searchDynamicEnergy;

    // This is an inversion of the production model's exponential voltage,
    // not an independent circuit calculation or numerical ladder solution.
    // An underflowed voltage or rounded unchanged precharge cannot yield tau.
    const double ratio = result.matchlineVoltage / prechargeVoltage;
    const bool canInfer = model.Metrics().modelBackend == "analytical_rc"
        && std::isfinite(ratio) && ratio > 0 && ratio < 1;
    pattern["time_constant_inference_available"] = canInfer;
    if (canInfer) {
        pattern["inferred_time_constant_s"] = model.Metrics().decisionTime / -std::log(ratio);
    } else {
        pattern["inferred_time_constant_s"] = YAML::Node(YAML::NodeType::Null);
    }
    patterns.push_back(pattern);
}

YAML::Node Probe(const std::string &configPath) {
    auto config = std::make_shared<EvaCamConfig>();
    config->logger.SetOutputEnabled(false);
    config->ReadConfigFromFile(configPath);
    EvaCamConfigValidator::Validate(*config);
    if (!config->technology.cell->nandString) {
        throw std::invalid_argument("NandValidationProbe requires a NAND-string configuration");
    }
    const auto &muxValues = config->resolvedExploration.geometry.muxSenseAmpValues;
    if (muxValues.empty()) {
        throw std::invalid_argument("NandValidationProbe requires a resolved sense-amplifier mux");
    }

    // Intentionally isolate the flash RC ladder from spatial wire geometry.
    // NandCamModel consumes these three Wire fields during initialization.
    Wire wire;
    wire.initialized = true;
    wire.resWirePerUnit = 0;
    wire.capWirePerUnit = 0;
    const auto backend = CreateNandCamBackend(config->technology.cell->memCellType);
    auto &model = *backend;
    model.Initialize(config, config->runtimeSizing.fixedSubarrayRows,
            config->runtimeSizing.fixedSubarrayColumns, muxValues.front(), wire);
    const auto &metrics = model.Metrics();
    const bool vertical = config->technology.cell->memCellType == NAND3D;
    const auto &device3d = config->technology.cell->nand3d;
    const auto &device = vertical ? device3d.electrical : config->technology.cell->nand;

    YAML::Node root(YAML::NodeType::Map);
    root["schema"] = "nand_validation_probe";
    root["schema_version"] = 1;
    root["config_path"] = configPath;
    auto metadata = root["metadata"];
    metadata["model_backend"] = metrics.modelBackend;
    metadata["calibration_status"] = metrics.calibrationStatus;
    metadata["model_source"] = metrics.modelSource;
    metadata["zero_wire"] = true;
    metadata["bitline_wire_omitted"] = true;
    metadata["wordline_wire_omitted"] = true;
    metadata["bank_routing_omitted"] = true;
    metadata["scope"] = "one physical block with zero wire resistance and capacitance";
    metadata["purpose"] = "isolate the production exponential approximation from an independent RC ladder reference";
    metadata["initial_condition"] = "all dynamic ladder nodes uniformly precharged";
    metadata["ladder_order"] = "ground, source-select resistor, source capacitor, repeated [flash resistor, internal capacitor], drain-select resistor, bitline capacitor";
    metadata["wordline_order"] = "source to drain: validity pair, complementary key pairs, pass-biased padding";
    metadata["time_constant_source"] = "inferred from production voltage as decision_time / -log(voltage / precharge), not an independent reference";
    metadata["invalid_marker"] = "programmed H,L validity pair queried 0; erased L,L is not an invalid marker";
    if (vertical) {
        for (const auto &item : metrics.metadata) metadata[item.first] = item.second;
        metadata["purpose"] = "independently verify encoded finite-precharge linear RC phases";
        metadata["initial_condition"] = "reset zero, finite-driver precharge, phase state carried between mux rounds";
        metadata["wordline_order"] = "source dummy layers, validity pair, key pairs, pass padding, drain dummy layers";
        metadata["time_constant_source"] = "unavailable for transient model";
    }

    auto geometry = root["geometry"];
    geometry["entries"] = metrics.entries;
    geometry["key_width"] = metrics.keyWidth;
    geometry["data_wordlines"] = metrics.dataWordlines;
    geometry["padding_wordlines"] = metrics.paddingWordlines;
    geometry["physical_cells"] = metrics.physicalCells;
    geometry["physical_page_bits"] = metrics.physicalPageBits;
    geometry["physical_block_bits"] = metrics.physicalBlockBits;
    geometry["sense_amplifiers"] = metrics.senseAmplifiers;
    geometry["sense_rounds"] = metrics.searchRounds;
    geometry["selected_mux_sense_amp"] = muxValues.front();
    if (vertical) {
        for (const auto &item : metrics.geometryMetrics) geometry[item.first] = item.second;
    }

    auto inputs = root["device"];
    inputs["resistance_read_on_ohm"] = device.resistanceReadOn;
    inputs["resistance_pass_ohm"] = device.resistancePass;
    inputs["resistance_off_ohm"] = device.resistanceOff;
    inputs["resistance_select_ohm"] = device.resistanceSelect;
    inputs["capacitance_source_f"] = device.capacitanceSource;
    inputs["capacitance_internal_f"] = device.capacitanceInternal;
    inputs["capacitance_bitline_f"] = device.capacitanceBitline;
    inputs["capacitance_gate_f"] = device.capacitanceGate;
    inputs["capacitance_select_f"] = device.capacitanceSelect;
    inputs["voltage_precharge_v"] = device.voltagePrecharge;
    inputs["voltage_read_v"] = device.voltageRead;
    inputs["voltage_pass_v"] = device.voltagePass;
    inputs["threshold_low_v"] = device.thresholdLow;
    inputs["threshold_high_v"] = device.thresholdHigh;
    inputs["decision_time_s"] = device.decisionTime;
    inputs["precharge_latency_s"] = device.precharge.latency;
    inputs["sense_offset_v"] = device.senseOffset;
    inputs["min_sense_margin_v"] = device.minSenseMargin;
    inputs["configured_reference_voltage_v"] = device.referenceVoltage;
    inputs["supply_efficiency"] = device.supplyEfficiency;
    if (vertical) {
        inputs["dummy_layers"] = device3d.dummyLayers;
        inputs["precharge_driver_resistance_ohm"] = device3d.prechargeDriverResistance;
        inputs["recovery_latency_s"] = device.recovery.latency;
        inputs["solver_tolerance_v"] = device3d.solverTolerance;
    }

    auto predictions = root["model"];
    predictions["search_latency_s"] = metrics.searchLatency;
    predictions["search_energy_j"] = metrics.searchEnergy;
    predictions["match_voltage_v"] = metrics.matchVoltage;
    predictions["mismatch_voltage_v"] = metrics.mismatchVoltage;
    predictions["reference_voltage_v"] = metrics.referenceVoltage;
    predictions["sense_margin_v"] = metrics.senseMargin;
    predictions["required_sense_margin_v"] = metrics.requiredSenseMargin;
    predictions["sense_margin_pass"] = metrics.senseMarginPass;
    if (!vertical) {
        predictions["slowest_match_time_constant_s"] = metrics.slowestMatchTimeConstant;
        predictions["fastest_mismatch_time_constant_s"] = metrics.fastestMismatchTimeConstant;
    }
    predictions["search_latency_breakdown_s"] = metrics.latencyBreakdown;
    predictions["search_energy_breakdown_j"] = metrics.searchEnergyBreakdown;

    const std::size_t width = static_cast<std::size_t>(metrics.keyWidth);
    const std::vector<int> zeros(width, 0);
    const std::vector<int> ones(width, 1);
    const std::vector<int> wildcards(width, -1);
    YAML::Node patterns(YAML::NodeType::Sequence);
    AddPattern(patterns, model, "match0", zeros, zeros, true, device.voltagePrecharge);
    AddPattern(patterns, model, "match1", ones, ones, true, device.voltagePrecharge);
    AddPattern(patterns, model, "allwildcard", wildcards, wildcards, true, device.voltagePrecharge);

    auto stored = wildcards;
    auto query = wildcards;
    stored.front() = 1;
    query.front() = 0;
    AddPattern(patterns, model, "source_mismatch", stored, query, true, device.voltagePrecharge);
    stored = query = wildcards;
    stored.back() = 0;
    query.back() = 1;
    AddPattern(patterns, model, "drain_mismatch", stored, query, true, device.voltagePrecharge);
    AddPattern(patterns, model, "invalid_marker", wildcards, wildcards, false, device.voltagePrecharge);
    root["patterns"] = patterns;
    return root;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: NandValidationProbe <config.yaml>\n";
        return 2;
    }
    try {
        const auto result = Probe(argv[1]);
        YAML::Emitter output;
        output.SetDoublePrecision(17);
        output << result;
        if (!output.good()) {
            throw std::runtime_error("could not serialize NAND validation probe YAML");
        }
        std::cout << output.c_str() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "NandValidationProbe: " << error.what() << '\n';
        return 1;
    }
}
