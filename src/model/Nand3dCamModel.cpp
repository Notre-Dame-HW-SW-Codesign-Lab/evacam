#include <algorithm>
#include <cmath>
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
                    && electrical.model == "analytical_rc", "requires configured SLC analytical_rc NAND3D");
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
            "sense mux must divide string_columns and be at most 256");
    Positive(device.holePitchX, "layout.hole_pitch_x");
    Positive(device.holePitchY, "layout.hole_pitch_y");
    Positive(device.layerPitch, "layout.layer_pitch");
    Positive(device.staircaseStepWidth, "layout.staircase_step_width");
    Positive(device.staircaseContactLength, "layout.staircase_contact_length");
    Nonnegative(device.isolationWidth, "layout.isolation_width");
    Require(device.peripheralPlacement == "beside" || device.peripheralPlacement == "under_array",
            "peripheral placement must be beside or under_array");
    Require(wire.initialized, "local wire must be initialized");
    Nonnegative(wire.resWirePerUnit, "wire resistance");
    Nonnegative(wire.capWirePerUnit, "wire capacitance");
    Nonnegative(config->peripherals.addCapOnML, "additional bitline capacitance");

    mux = muxSenseAmp;
    sourceDummyLayers = device.dummyLayers / 2;
    totalGateLayers = device.storageLayers + device.dummyLayers;
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
        {"sense_polarity", "match_discharges_bitline"}, {"sensing_bound", "global_within_single_exponential_approximation"},
        {"sense_margin_definition", "min(reference-match,mismatch-reference)-offset"},
        {"delay_model", "first_moment_single_exponential"},
        {"numerical_validation", "analytical_limits_and_independent_first_moment_tests"},
        {"device_validation", "not_performed_by_evacam"},
        {"terminal_conductance_model", "dc_linear_resistor_network"},
        {"precharge_initial_condition", "uniform_full_rail_assumed_each_round"},
        {"recovery_condition", "complete_reset_assumed"},
        {"peripheral_placement", device.peripheralPlacement},
        {"internal_capacitance_model", "uniform_shunt_only_no_gate_or_neighbor_coupling"},
        {"wordline_wire_model", "lateral_manhattan_span_plus_all_string_gate_loads"},
        {"bitline_wire_model", "lateral_pi_section_no_vertical_stack_length"},
        {"voltage_evaluation", "single_exponential_at_decision_time"}
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
    // Positive downstream-capacitance weights make a source-side read device
    // the slowest matching member of each complementary pair. As in the
    // planar analytical model, all other queries masked gives the fastest
    // one-device mismatch. These are bounds on this approximation, not on
    // the full distributed transient or measured NAND hardware.
    auto slowMatch = allPassResistances;
    slowMatch[sourceDummyLayers] = electrical.resistanceReadOn;
    for (long bit = 0; bit < keyWidth; bit++) {
        slowMatch[sourceDummyLayers + 2 + 2 * bit] = electrical.resistanceReadOn;
    }
    metrics.slowestMatchTimeConstant = StringTimeConstant(slowMatch);
    auto fastMismatch = allPassResistances;
    fastMismatch[sourceDummyLayers] = electrical.resistanceReadOn;
    const long lastKeyDevice = sourceDummyLayers + 2 * keyWidth + 1;
    fastMismatch[lastKeyDevice] = electrical.resistanceOff;
    metrics.fastestMismatchTimeConstant = StringTimeConstant(fastMismatch);
    fastMismatch[lastKeyDevice] = electrical.resistancePass;
    fastMismatch[sourceDummyLayers] = electrical.resistanceOff;
    metrics.fastestMismatchTimeConstant = std::min(metrics.fastestMismatchTimeConstant,
            StringTimeConstant(fastMismatch));
    metrics.matchVoltage = electrical.voltagePrecharge
            * std::exp(-electrical.decisionTime / metrics.slowestMatchTimeConstant);
    metrics.mismatchVoltage = electrical.voltagePrecharge
            * std::exp(-electrical.decisionTime / metrics.fastestMismatchTimeConstant);
    const std::vector<int> zeros(keyWidth, 0);
    // Full reset and all-pass precharge are assumed sufficient each round.
    // Constant-voltage supply charging costs CV^2, not stored energy CV^2/2.
    const double prechargeEnergy = mux * std::accumulate(capacitances.begin(), capacitances.end(), 0.0)
            * electrical.voltagePrecharge * electrical.voltagePrecharge;
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
        {"bitline_and_internal_precharge", entries * prechargeEnergy / electrical.supplyEfficiency},
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

double Nand3dCamModel::StringTimeConstant(const std::vector<double> &resistances) const {
    // Far-node Elmore first moment: each capacitance sees its resistance to
    // the grounded source, including the lateral bitline pi section.
    double resistanceToGround = device.electrical.resistanceSelect;
    double tau = capacitances.front() * resistanceToGround;
    for (std::size_t index = 0; index < resistances.size(); index++) {
        resistanceToGround += resistances[index];
        tau += capacitances[index + 1] * resistanceToGround;
    }
    Positive(tau, "string RC time constant");
    return tau;
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
    bool hit = valid;
    for (std::size_t bit = 0; bit < stored.size(); bit++) {
        if (stored[bit] != -1 && query[bit] != -1 && stored[bit] != query[bit]) hit = false;
    }
    EvaCAMMatchResult result;
    result.hit = hit;
    result.matchlineDelay = metrics.decisionTime;
    result.searchLatency = metrics.searchLatency;
    result.matchlineVoltage = device.electrical.voltagePrecharge
            * std::exp(-metrics.decisionTime / StringTimeConstant(resistances));
    result.matchlineConductance = 1 / (device.electrical.resistanceSelect
            + std::accumulate(resistances.begin(), resistances.end(), 0.0));
    result.senseMargin = (hit ? metrics.referenceVoltage - result.matchlineVoltage
            : result.matchlineVoltage - metrics.referenceVoltage) - device.electrical.senseOffset;
    result.requiredSenseMargin = metrics.requiredSenseMargin;
    result.senseMarginSlack = result.senseMargin - result.requiredSenseMargin;
    result.senseMarginPass = result.senseMarginSlack >= 0;
    // Full reset/recharge makes CV^2 independent of the stored pattern.
    // Query masks still change the gate and driver switching costs.
    result.searchDynamicEnergy = metrics.searchEnergy
            - metrics.searchEnergyBreakdown.at("wordline_capacitance") + QueryGateEnergy(query)
            - metrics.searchEnergyBreakdown.at("wordline_drivers") + QueryDriverEnergy(query);
    return result;
}
