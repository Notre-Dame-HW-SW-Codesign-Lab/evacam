#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "config/EvaCamConfig.h"
#include "circuit/Wire.h"
#include "model/Nand3dCamModel.h"

namespace {

void Require(bool condition, const std::string &message) {
    if (!condition) throw std::invalid_argument("NAND3D CAM: " + message);
}

void Positive(double value, const std::string &field) {
    Require(std::isfinite(value) && value > 0, field + " must be finite and positive");
}

void Nonnegative(double value, const std::string &field) {
    Require(std::isfinite(value) && value >= 0, field + " must be finite and nonnegative");
}

void Peripheral(const NandPeripheralSpec &peripheral, const std::string &name) {
    Positive(peripheral.area, name + ".area");
    Nonnegative(peripheral.latency, name + ".latency");
    Nonnegative(peripheral.energy, name + ".energy");
    Nonnegative(peripheral.leakage, name + ".leakage");
}

void Operation(const NandOperationSpec &operation, const std::string &name, bool positiveTime) {
    if (positiveTime) Positive(operation.latency, name + ".latency");
    else Nonnegative(operation.latency, name + ".latency");
    Nonnegative(operation.energy, name + ".energy");
}

double Sum(const std::map<std::string, double> &values) {
    double total = 0;
    for (const auto &item : values) {
        Nonnegative(item.second, item.first);
        total += item.second;
    }
    Positive(total, "operation/component total");
    return total;
}

}  // namespace

void Nand3dCamModel::Initialize(std::shared_ptr<EvaCamConfig> config, long long entries,
        long keyWidth, int muxSenseAmp, const Wire &wire) {
    initialized = false;
    metrics = {};
    Require(config && config->technology.cell, "configuration must include a memory device");
    const auto &cell = *config->technology.cell;
    Require(cell.memCellType == NAND3D && cell.nandString && cell.camType == TCAM,
            "requires NAND3D with nand_string topology and TCAM");
    Require(config->input.designTarget == CAM_chip && config->input.searchFunction == EX
                    && config->input.internalSensing && !cell.withVariation,
            "only exact internally sensed CAM without variation is supported");
    device = cell.nand3d;
    const auto &electrical = device.electrical;
    Require(device.configured && device.storageMode == "SLC"
                    && electrical.model == "transient_rc", "requires configured SLC transient_rc NAND3D");
    Require(!electrical.source.empty(), "model source is required");
    Require(electrical.calibrationStatus == "synthetic" || electrical.calibrationStatus == "uncalibrated"
                    || electrical.calibrationStatus == "calibrated", "invalid calibration status");
    Positive(electrical.resistancePass, "resistance.pass");
    Positive(electrical.resistanceReadOn, "resistance.read_on");
    Positive(electrical.resistanceOff, "resistance.off");
    Positive(electrical.resistanceSelect, "resistance.select");
    Require(electrical.resistanceOff > electrical.resistanceReadOn
                    && electrical.resistanceReadOn >= electrical.resistancePass,
            "requires off > read_on >= pass resistance");
    Positive(electrical.capacitanceSource, "capacitance.source");
    Positive(electrical.capacitanceInternal, "capacitance.internal");
    Positive(electrical.capacitanceBitline, "capacitance.bitline");
    Positive(electrical.capacitanceGate, "capacitance.gate");
    Nonnegative(electrical.capacitanceSelect, "capacitance.select");
    Require(std::isfinite(electrical.thresholdLow) && std::isfinite(electrical.thresholdHigh),
            "thresholds must be finite");
    Nonnegative(electrical.voltageRead, "bias.read");
    Positive(electrical.voltagePass, "bias.pass");
    Positive(electrical.voltagePrecharge, "bias.precharge");
    Require(electrical.thresholdLow < electrical.voltageRead
                    && electrical.voltageRead < electrical.thresholdHigh
                    && electrical.thresholdHigh < electrical.voltagePass, "inconsistent threshold/bias ordering");
    Positive(electrical.decisionTime, "sensing.decision_time");
    Positive(electrical.minSenseMargin, "sensing.min_margin");
    Nonnegative(electrical.senseOffset, "sensing.offset");
    Nonnegative(electrical.referenceVoltage, "sensing.reference_voltage");
    Require(electrical.referenceVoltage < electrical.voltagePrecharge,
            "sense reference must be below precharge rail");
    Positive(electrical.supplyEfficiency, "supply_efficiency");
    Require(electrical.supplyEfficiency <= 1, "supply efficiency must not exceed one");
    Peripheral(electrical.wordlineDriver, "wordline_driver");
    Peripheral(electrical.sense, "sense");
    Peripheral(electrical.pageBuffer, "page_buffer");
    Operation(electrical.query, "query", false);
    Operation(electrical.setup, "setup", false);
    Operation(electrical.precharge, "precharge", true);
    Operation(electrical.recovery, "recovery", true);
    Operation(electrical.programPage, "program_page", true);
    Operation(electrical.eraseBlock, "erase_block", true);
    Positive(electrical.programPage.energy, "program_page.energy");
    Positive(electrical.eraseBlock.energy, "erase_block.energy");
    Require(device.storageLayers >= 4 && device.storageLayers <= 4096
                    && device.dummyLayers >= 0 && device.dummyLayers <= 4096
                    && device.storageLayers + device.dummyLayers <= 4096,
            "requires 4..4096 storage layers and at most 4096 total storage/dummy layers");
    Require(device.stringRows > 0 && device.stringColumns >= 8 && device.stringColumns % 8 == 0
                    && static_cast<long long>(device.stringRows) * device.stringColumns <= 1048576,
            "byte-aligned string columns and positive rows must form at most 1048576 strings");
    Require(entries == static_cast<long long>(device.stringRows) * device.stringColumns,
            "entries must equal string_rows * string_columns");
    Require(keyWidth > 0 && keyWidth <= (device.storageLayers - 2) / 2,
            "keyWidth must fit storage layers including the reserved validity pair");
    Require(config->input.pageSize == device.stringColumns,
            "physical flash.page_size must equal string_columns bits");
    Require(config->input.flashBlockSize == entries * device.storageLayers,
            "physical flash.block_size must equal rows * columns * storage_layers bits");
    Require(muxSenseAmp > 0 && muxSenseAmp <= 256 && device.stringColumns % muxSenseAmp == 0,
            "sense mux must divide string_columns and be at most 256 for transient simulation");
    Positive(device.holePitchX, "layout.hole_pitch_x");
    Positive(device.holePitchY, "layout.hole_pitch_y");
    Positive(device.layerPitch, "layout.layer_pitch");
    Positive(device.staircaseStepWidth, "layout.staircase_step_width");
    Positive(device.staircaseContactLength, "layout.staircase_contact_length");
    Nonnegative(device.isolationWidth, "layout.isolation_width");
    Require(device.peripheralPlacement == "beside" || device.peripheralPlacement == "under_array",
            "peripheral placement must be beside or under_array");
    Positive(device.prechargeDriverResistance, "precharge_driver_resistance");
    Positive(device.solverMaxStep, "solver.max_step");
    Positive(device.solverTolerance, "solver.tolerance");
    Require(device.solverTolerance <= 1e-3 && device.solverMaxSteps >= 100,
            "solver tolerance must be at most 1mV and max_steps at least 100");
    Require(wire.initialized, "local wire must be initialized");
    Nonnegative(wire.resWirePerUnit, "wire resistance");
    Nonnegative(wire.capWirePerUnit, "wire capacitance");
    Nonnegative(config->peripherals.addCapOnML, "additional bitline capacitance");

    mux = muxSenseAmp;
    sourceDummyLayers = device.dummyLayers / 2;
    totalGateLayers = device.storageLayers + device.dummyLayers;
    options = {device.solverMaxStep, device.solverTolerance, device.solverMaxSteps};
    metrics.modelBackend = electrical.model;
    metrics.modelSource = electrical.source;
    metrics.calibrationStatus = electrical.calibrationStatus;
    metrics.entries = entries;
    metrics.keyWidth = keyWidth;
    metrics.dataWordlines = device.storageLayers;
    metrics.paddingWordlines = device.storageLayers - 2 * (keyWidth + 1);
    metrics.physicalCells = entries * device.storageLayers;
    metrics.physicalPageBits = device.stringColumns;
    metrics.physicalBlockBits = metrics.physicalCells;
    metrics.senseAmplifiers = device.stringColumns / mux;
    metrics.searchRounds = device.stringRows * mux;
    metrics.queryBitCount = 2 * (keyWidth + 1);
    metrics.decisionTime = electrical.decisionTime;
    metrics.requiredSenseMargin = electrical.minSenseMargin;
    metrics.metadata = {
        {"model_identifier", "evacam-nand3d-tcam-v1"}, {"array_layout", "vertical_3d"},
        {"topology", "nand_string"}, {"encoding", "complementary_pair_with_validity_pair"},
        {"sense_polarity", "match_discharges_bitline"}, {"sensing_bound", "sampled_patterns"},
        {"sense_margin_definition", "min(reference-match,mismatch-reference)-offset"},
        {"transient_solver", "adaptive_backward_euler_richardson_step_doubling"},
        {"numerical_validation", "linear_rc_solver_reference_tests"},
        {"device_validation", "not_performed_by_evacam"},
        {"terminal_conductance_model", "dc_linear_resistor_network"},
        {"precharge_initial_condition", "reset_zero_then_carried_between_phases_and_mux_rounds"},
        {"recovery_condition", "all_pass_both_terminals_grounded_final_residual_checked"},
        {"peripheral_placement", device.peripheralPlacement},
        {"internal_capacitance_model", "uniform_shunt_only_no_gate_or_neighbor_coupling"},
        {"wordline_wire_model", "lateral_manhattan_span_plus_all_string_gate_loads"},
        {"bitline_wire_model", "lateral_pi_section_no_vertical_stack_length"},
        {"voltage_sampling", "worst_sense_mux_round_for_each_pattern"}
    };

    const double coreWidth = device.stringColumns * device.holePitchX;
    const double coreHeight = device.stringRows * device.holePitchY;
    const double staircaseWidth = (totalGateLayers + 2) * device.staircaseStepWidth;
    const double staircaseArea = staircaseWidth * device.staircaseContactLength;
    const double insideWidth = coreWidth + staircaseWidth;
    const double insideHeight = std::max(coreHeight, device.staircaseContactLength);
    const double arrayWidth = insideWidth + 2 * device.isolationWidth;
    const double arrayHeight = insideHeight + 2 * device.isolationWidth;
    const double arrayArea = arrayWidth * arrayHeight;
    const double driverCount = totalGateLayers + 2 * device.stringRows;
    const double driverArea = driverCount * electrical.wordlineDriver.area;
    const double senseArea = metrics.senseAmplifiers * electrical.sense.area;
    const double bufferArea = device.stringColumns * electrical.pageBuffer.area;
    const double peripheralArea = driverArea + senseArea + bufferArea;
    const double peripheralExtension = device.peripheralPlacement == "under_array"
            ? std::max(0.0, peripheralArea - arrayArea) : peripheralArea;
    metrics.areaBreakdown = {{"core_array", coreWidth * coreHeight}, {"staircase", staircaseArea},
        {"isolation", arrayArea - insideWidth * insideHeight},
        {"placement_whitespace", std::max(0.0, insideWidth * insideHeight - coreWidth * coreHeight - staircaseArea)},
        {"peripheral_extension", peripheralExtension}};
    metrics.area = Sum(metrics.areaBreakdown);
    metrics.height = arrayHeight;
    metrics.width = metrics.area / metrics.height;
    Positive(metrics.width, "footprint width");
    Positive(metrics.height, "footprint height");
    const double bitlineLength = coreHeight;
    const double wireResistance = bitlineLength * wire.resWirePerUnit;
    const double wireCapacitance = bitlineLength * wire.capWirePerUnit;
    Nonnegative(wireResistance, "lateral bitline wire resistance");
    Nonnegative(wireCapacitance, "lateral bitline wire capacitance");
    gateCapacitancePerWordline = entries * electrical.capacitanceGate
            + (coreWidth + coreHeight) * wire.capWirePerUnit;
    Positive(gateCapacitancePerWordline, "wordline capacitance");
    metrics.geometryMetrics = {
        {"storage_layers", device.storageLayers}, {"dummy_layers", device.dummyLayers},
        {"string_rows", device.stringRows}, {"string_columns", device.stringColumns},
        {"select_group_count", device.stringRows}, {"strings_per_group", device.stringColumns},
        {"dummy_devices_per_block", entries * static_cast<double>(device.dummyLayers)},
        {"select_devices_per_block", entries * 2.0},
        {"vertical_stack_height_m", (totalGateLayers + 2) * device.layerPitch},
        {"block_core_width_m", coreWidth}, {"block_core_height_m", coreHeight},
        {"staircase_area_m2", staircaseArea}, {"isolation_area_m2", metrics.areaBreakdown.at("isolation")},
        {"lateral_bitline_length_m", bitlineLength}, {"peripheral_area_m2", peripheralArea},
        {"wordline_driver_area_m2", driverArea}, {"sense_amplifier_area_m2", senseArea},
        {"page_buffer_area_m2", bufferArea}, {"occupied_footprint_area_m2", metrics.area}
    };
    // Source node, one internal node after every storage/dummy transistor,
    // then drain select and a lateral BL pi-section when it has both R and C.
    capacitances.assign(totalGateLayers + 1, electrical.capacitanceInternal);
    capacitances.front() = electrical.capacitanceSource;
    allPassResistances.assign(totalGateLayers, electrical.resistancePass);
    if (wireResistance > 0 && wireCapacitance > 0) {
        capacitances.push_back(wireCapacitance / 2);
        allPassResistances.push_back(electrical.resistanceSelect);
        capacitances.push_back(electrical.capacitanceBitline + config->peripherals.addCapOnML + wireCapacitance / 2);
        allPassResistances.push_back(wireResistance);
    } else {
        capacitances.push_back(electrical.capacitanceBitline + config->peripherals.addCapOnML + wireCapacitance);
        allPassResistances.push_back(electrical.resistanceSelect + wireResistance);
    }
    initialPrecharge = NandRcLadder::Solve(capacitances, allPassResistances,
            std::vector<double>(capacitances.size(), 0), electrical.precharge.latency,
            {}, {true, device.prechargeDriverResistance, electrical.voltagePrecharge}, options);
    metrics.diagnosticMetrics["precharge_min_voltage_v"] = *std::min_element(initialPrecharge.voltages.begin(), initialPrecharge.voltages.end());
    metrics.diagnosticMetrics["precharge_max_voltage_v"] = *std::max_element(initialPrecharge.voltages.begin(), initialPrecharge.voltages.end());
    metrics.diagnosticMetrics["precharge_source_energy_j_per_string"] = electrical.voltagePrecharge * initialPrecharge.capacitorChargeChange;
    metrics.diagnosticMetrics["transient_absolute_tolerance_v"] = options.tolerance;
    metrics.diagnosticMetrics["transient_max_step_s"] = options.maxStep;
    metrics.diagnosticMetrics["transient_max_steps_per_phase"] = options.maxSteps;
    metrics.matchVoltage = 0;
    metrics.mismatchVoltage = electrical.voltagePrecharge;
    double maximumPrechargeEnergy = 0;
    long long work = 0;
    int samples = 0;
    const auto sample = [&](const std::vector<int> &stored, const std::vector<int> &query, bool valid, bool match) {
        const auto response = Simulate(Encode(stored, query, valid));
        if (match) metrics.matchVoltage = std::max(metrics.matchVoltage, response.maximumVoltage);
        else metrics.mismatchVoltage = std::min(metrics.mismatchVoltage, response.minimumVoltage);
        maximumPrechargeEnergy = std::max(maximumPrechargeEnergy, response.prechargeEnergy);
        metrics.diagnosticMetrics["maximum_estimated_local_error_v"] = std::max(
                metrics.diagnosticMetrics["maximum_estimated_local_error_v"], response.maximumLocalError);
        metrics.diagnosticMetrics["maximum_reset_residual_v"] = std::max(
                metrics.diagnosticMetrics["maximum_reset_residual_v"], response.finalResetVoltage);
        metrics.diagnosticMetrics["maximum_solver_steps_per_pattern"] = std::max(
                metrics.diagnosticMetrics["maximum_solver_steps_per_pattern"], static_cast<double>(response.steps));
        work += response.steps * static_cast<long long>(capacitances.size());
        if (work > 200000000) throw std::runtime_error("NAND3D CAM: transient sampling work limit exceeded; reduce geometry/mux or adjust solver settings");
        samples++;
    };
    const std::vector<int> zeros(keyWidth, 0), ones(keyWidth, 1), masks(keyWidth, -1);
    sample(zeros, zeros, true, true);
    sample(ones, ones, true, true);
    sample(masks, masks, true, true);
    for (int polarity : {0, 1}) {
        std::vector<int> alternating(keyWidth);
        for (long bit = 0; bit < keyWidth; bit++) alternating[bit] = (bit + polarity) % 2;
        sample(alternating, alternating, true, true);
        for (long bit = 0; bit < keyWidth; bit++) {
            for (bool masked : {false, true}) {
                std::vector<int> query(keyWidth, masked ? -1 : polarity);
                query[bit] = polarity;
                auto stored = query;
                stored[bit] = 1 - polarity;
                sample(stored, query, true, false);
            }
        }
    }
    sample(masks, masks, false, false);
    sample(zeros, zeros, false, false);
    sample(ones, ones, false, false);
    metrics.diagnosticMetrics["sampled_patterns"] = samples;
    metrics.diagnosticMetrics["sampling_node_steps"] = static_cast<double>(work);
    metrics.referenceVoltage = electrical.referenceVoltage > 0 ? electrical.referenceVoltage
            : (metrics.matchVoltage + metrics.mismatchVoltage) / 2;
    metrics.senseMargin = std::min(metrics.referenceVoltage - metrics.matchVoltage,
            metrics.mismatchVoltage - metrics.referenceVoltage) - electrical.senseOffset;
    metrics.senseMarginPass = metrics.senseMargin >= metrics.requiredSenseMargin;
    const double rounds = metrics.searchRounds;
    metrics.latencyBreakdown = {{"query", electrical.query.latency}, {"setup", electrical.setup.latency},
        {"initial_and_final_wordline_bias", 2 * electrical.wordlineDriver.latency},
        {"query_and_recovery_wordline_bias", 2 * rounds * electrical.wordlineDriver.latency},
        {"select_gate_drivers", 2 * rounds * electrical.wordlineDriver.latency},
        {"precharge", rounds * electrical.precharge.latency}, {"evaluation", rounds * electrical.decisionTime},
        {"sensing", rounds * electrical.sense.latency}, {"page_buffers", rounds * electrical.pageBuffer.latency},
        {"recovery", rounds * electrical.recovery.latency}};
    metrics.searchLatency = Sum(metrics.latencyBreakdown);
    const double selectCapPerGroup = 2 * (device.stringColumns * electrical.capacitanceSelect
            + coreWidth * wire.capWirePerUnit);
    metrics.searchEnergyBreakdown = {{"query", electrical.query.energy}, {"setup", electrical.setup.energy},
        {"wordline_capacitance", QueryGateEnergy(zeros)}, {"wordline_drivers", QueryDriverEnergy(zeros)},
        {"select_gate_capacitance", rounds * selectCapPerGroup * electrical.voltagePass * electrical.voltagePass / electrical.supplyEfficiency},
        {"select_gate_drivers", 4 * rounds * electrical.wordlineDriver.energy},
        {"bitline_and_internal_precharge", entries * maximumPrechargeEnergy / electrical.supplyEfficiency},
        {"precharge_overhead", rounds * electrical.precharge.energy},
        {"sensing", entries * electrical.sense.energy}, {"page_buffers", entries * electrical.pageBuffer.energy},
        {"recovery", rounds * electrical.recovery.energy}};
    metrics.searchEnergy = Sum(metrics.searchEnergyBreakdown);
    metrics.leakage = driverCount * electrical.wordlineDriver.leakage
            + metrics.senseAmplifiers * electrical.sense.leakage + device.stringColumns * electrical.pageBuffer.leakage;
    Nonnegative(metrics.leakage, "leakage");
    metrics.programPageEnergy = electrical.programPage.energy;
    metrics.programPageLatency = electrical.programPage.latency;
    metrics.eraseBlockEnergy = electrical.eraseBlock.energy;
    metrics.eraseBlockLatency = electrical.eraseBlock.latency;
    initialized = true;
}

const NandCamMetrics &Nand3dCamModel::Metrics() const {
    if (!initialized) throw std::runtime_error("NAND3D CAM requires successful initialization");
    return metrics;
}

std::vector<double> Nand3dCamModel::Encode(const std::vector<int> &stored,
        const std::vector<int> &query, bool valid) const {
    Require(stored.size() == static_cast<std::size_t>(metrics.keyWidth) && query.size() == stored.size(),
            "stored/query vectors must contain keyWidth symbols");
    auto resistance = allPassResistances;
    const auto &electrical = device.electrical;
    resistance[sourceDummyLayers] = valid ? electrical.resistanceReadOn : electrical.resistanceOff;
    for (std::size_t bit = 0; bit < stored.size(); bit++) {
        Require(stored[bit] >= -1 && stored[bit] <= 1 && query[bit] >= -1 && query[bit] <= 1,
                "symbols must be -1 (wildcard), 0 or 1");
        if (query[bit] != -1) resistance[sourceDummyLayers + 2 + 2 * bit + query[bit]] =
                stored[bit] == -1 || stored[bit] == query[bit]
                ? electrical.resistanceReadOn : electrical.resistanceOff;
    }
    return resistance;
}

Nand3dCamModel::PatternResponse Nand3dCamModel::Simulate(const std::vector<double> &resistances) const {
    const auto &electrical = device.electrical;
    PatternResponse response;
    response.minimumVoltage = electrical.voltagePrecharge;
    std::vector<double> state(capacitances.size(), 0);
    const auto account = [&](const NandRcResult &result) {
        response.steps += result.acceptedSteps + result.rejectedSteps;
        response.maximumLocalError = std::max(response.maximumLocalError, result.maximumEstimatedLocalError);
    };
    for (int round = 0; round < mux; round++) {
        const auto precharge = round == 0 ? initialPrecharge : NandRcLadder::Solve(
                capacitances, allPassResistances, state, electrical.precharge.latency,
                {}, {true, device.prechargeDriverResistance, electrical.voltagePrecharge}, options);
        account(precharge);
        response.prechargeEnergy += electrical.voltagePrecharge * precharge.capacitorChargeChange;
        const auto evaluation = NandRcLadder::Solve(capacitances, resistances, precharge.voltages,
                electrical.decisionTime, {true, electrical.resistanceSelect, 0}, {}, options);
        account(evaluation);
        response.minimumVoltage = std::min(response.minimumVoltage, evaluation.voltages.back());
        response.maximumVoltage = std::max(response.maximumVoltage, evaluation.voltages.back());
        // All-pass recovery explicitly grounds both terminals. Its final
        // state is carried into the next mux round, never silently discarded.
        const auto recovery = NandRcLadder::Solve(capacitances, allPassResistances, evaluation.voltages,
                electrical.recovery.latency, {true, electrical.resistanceSelect, 0},
                {true, device.prechargeDriverResistance, 0}, options);
        account(recovery);
        state = recovery.voltages;
    }
    response.finalResetVoltage = *std::max_element(state.begin(), state.end());
    if (response.finalResetVoltage > options.tolerance) {
        throw std::runtime_error("NAND3D CAM: recovery insufficient to reset internal nodes; increase recovery.latency");
    }
    Nonnegative(response.prechargeEnergy, "integrated precharge supply energy");
    return response;
}

double Nand3dCamModel::QueryGateEnergy(const std::vector<int> &query) const {
    const auto &electrical = device.electrical;
    long long selected = 1;
    for (int symbol : query) selected += symbol != -1;
    // Recovery itself requires all-pass bias after EVERY evaluation, including
    // the last round. Initial charging and final reset are per full query.
    return gateCapacitancePerWordline * (totalGateLayers * electrical.voltagePass * electrical.voltagePass
            + static_cast<double>(metrics.searchRounds) * selected * electrical.voltagePass
                * (electrical.voltagePass - electrical.voltageRead)) / electrical.supplyEfficiency;
}

double Nand3dCamModel::QueryDriverEnergy(const std::vector<int> &query) const {
    long long selected = 1;
    for (int symbol : query) selected += symbol != -1;
    return (2.0 * totalGateLayers + 2.0 * metrics.searchRounds * selected)
            * device.electrical.wordlineDriver.energy;
}

EvaCAMMatchResult Nand3dCamModel::Evaluate(const std::vector<int> &stored,
        const std::vector<int> &query, bool valid) const {
    Metrics();
    const auto resistances = Encode(stored, query, valid);
    const auto response = Simulate(resistances);
    bool hit = valid;
    for (std::size_t bit = 0; bit < stored.size(); bit++) {
        if (stored[bit] != -1 && query[bit] != -1 && stored[bit] != query[bit]) hit = false;
    }
    EvaCAMMatchResult result;
    result.hit = hit;
    result.matchlineDelay = metrics.decisionTime;
    result.searchLatency = metrics.searchLatency;
    result.matchlineVoltage = hit ? response.maximumVoltage : response.minimumVoltage;
    result.matchlineConductance = 1 / (device.electrical.resistanceSelect
            + std::accumulate(resistances.begin(), resistances.end(), 0.0));
    result.senseMargin = (hit ? metrics.referenceVoltage - result.matchlineVoltage
            : result.matchlineVoltage - metrics.referenceVoltage) - device.electrical.senseOffset;
    result.requiredSenseMargin = metrics.requiredSenseMargin;
    result.senseMarginSlack = result.senseMargin - result.requiredSenseMargin;
    result.senseMarginPass = result.senseMarginSlack >= 0;
    // Full-block cost if every string has this representative stored pattern;
    // no stored-pattern distribution or independent string supply interaction
    // is inferred. Scalar run metrics use the maximum sampled precharge cost.
    result.searchDynamicEnergy = metrics.searchEnergy
            - metrics.searchEnergyBreakdown.at("wordline_capacitance") + QueryGateEnergy(query)
            - metrics.searchEnergyBreakdown.at("wordline_drivers") + QueryDriverEnergy(query)
            - metrics.searchEnergyBreakdown.at("bitline_and_internal_precharge")
            + metrics.entries * response.prechargeEnergy / device.electrical.supplyEfficiency;
    return result;
}
