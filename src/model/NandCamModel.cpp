#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

#include "config/EvaCamConfig.h"
#include "circuit/Wire.h"
#include "model/NandCamModel.h"

namespace {

void Require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::invalid_argument("NAND CAM: " + message);
    }
}

void RequireNonnegative(double value, const std::string &field) {
    Require(std::isfinite(value) && value >= 0, field + " must be finite and nonnegative");
}

void RequirePositive(double value, const std::string &field) {
    Require(std::isfinite(value) && value > 0, field + " must be finite and positive");
}

void ValidatePeripheral(const NandPeripheralSpec &peripheral, const std::string &field) {
    RequirePositive(peripheral.area, field + ".area");
    RequireNonnegative(peripheral.latency, field + ".latency");
    RequireNonnegative(peripheral.energy, field + ".energy");
    RequireNonnegative(peripheral.leakage, field + ".leakage");
}

void ValidateOperation(const NandOperationSpec &operation, const std::string &field,
        bool requirePositive) {
    if (requirePositive) {
        RequirePositive(operation.latency, field + ".latency");
        RequirePositive(operation.energy, field + ".energy");
    } else {
        RequireNonnegative(operation.latency, field + ".latency");
        RequireNonnegative(operation.energy, field + ".energy");
    }
}

double Sum(const std::map<std::string, double> &components) {
    double sum = 0;
    for (const auto &component : components) {
        RequireNonnegative(component.second, component.first);
        sum += component.second;
    }
    RequirePositive(sum, "component total");
    return sum;
}

}  // namespace

void NandCamModel::Initialize(std::shared_ptr<EvaCamConfig> config, long long entries,
        long keyWidth, int muxSenseAmp, const Wire &wire) {
    initialized = false;
    metrics = {};
    Require(config && config->technology.cell && config->technology.tech,
            "configuration must include device and technology");
    const auto &cell = *config->technology.cell;
    Require(cell.memCellType == SLCNAND && cell.nandString && cell.camType == TCAM,
            "requires SLCNAND, topology nand_string, and TCAM");
    Require(config->input.designTarget == CAM_chip && config->input.searchFunction == EX,
            "only CAM exact search is supported");
    Require(config->input.internalSensing, "requires internal sensing");
    Require(!cell.withVariation, "variation is not supported");
    device = cell.nand;
    Require(device.configured && device.model == "analytical_rc",
            "requires an explicitly configured analytical_rc model");
    Require(device.calibrationStatus == "synthetic"
                    || device.calibrationStatus == "uncalibrated"
                    || device.calibrationStatus == "calibrated",
            "calibrationStatus must be synthetic, uncalibrated, or calibrated");
    Require(!device.source.empty(), "model source must be identified");
    RequirePositive(device.resistanceReadOn, "resistanceReadOn");
    RequirePositive(device.resistancePass, "resistancePass");
    RequirePositive(device.resistanceOff, "resistanceOff");
    RequirePositive(device.resistanceSelect, "resistanceSelect");
    Require(device.resistanceReadOn >= device.resistancePass
                    && device.resistanceOff > device.resistanceReadOn,
            "resistances must satisfy off > read_on >= pass > 0");
    RequirePositive(device.capacitanceGate, "capacitanceGate");
    RequireNonnegative(device.capacitanceInternal, "capacitanceInternal");
    RequirePositive(device.capacitanceBitline, "capacitanceBitline");
    RequireNonnegative(device.capacitanceSource, "capacitanceSource");
    RequireNonnegative(device.capacitanceSelect, "capacitanceSelect");
    Require(std::isfinite(device.thresholdLow) && std::isfinite(device.thresholdHigh),
            "thresholds must be finite");
    RequireNonnegative(device.voltageRead, "voltageRead");
    RequirePositive(device.voltagePass, "voltagePass");
    Require(device.thresholdLow < device.voltageRead
                    && device.voltageRead < device.thresholdHigh
                    && device.thresholdHigh < device.voltagePass,
            "biases must satisfy threshold_low < read < threshold_high < pass");
    RequirePositive(device.voltagePrecharge, "voltagePrecharge");
    RequirePositive(device.decisionTime, "decisionTime");
    RequirePositive(device.minSenseMargin, "minSenseMargin");
    RequireNonnegative(device.referenceVoltage, "referenceVoltage");
    Require(device.referenceVoltage < device.voltagePrecharge,
            "referenceVoltage must be below precharge voltage (zero selects automatic)");
    RequireNonnegative(device.senseOffset, "senseOffset");
    RequirePositive(device.supplyEfficiency, "supplyEfficiency");
    Require(device.supplyEfficiency <= 1, "supplyEfficiency must be at most one");
    ValidatePeripheral(device.wordlineDriver, "wordlineDriver");
    ValidatePeripheral(device.sense, "sense");
    ValidatePeripheral(device.pageBuffer, "pageBuffer");
    ValidateOperation(device.query, "query", false);
    ValidateOperation(device.setup, "setup", false);
    ValidateOperation(device.precharge, "precharge", false);
    RequirePositive(device.precharge.latency, "precharge.latency");
    ValidateOperation(device.recovery, "recovery", false);
    ValidateOperation(device.programPage, "programPage", true);
    ValidateOperation(device.eraseBlock, "eraseBlock", true);

    Require(entries > 0 && entries <= 1048576,
            "entries per block must be between 1 and 1048576 strings");
    Require(config->input.pageSize == entries,
            "entries per subarray must equal flash.page_size in physical bits");
    Require(config->input.flashBlockSize > 0
                    && config->input.flashBlockSize % entries == 0,
            "flash.block_size must be a positive integer multiple of page_size");
    const long long wordlines = config->input.flashBlockSize / entries;
    Require(wordlines >= 4 && wordlines <= 4096,
            "data wordlines per string must be between 4 and 4096");
    Require(keyWidth > 0 && keyWidth <= (wordlines - 2) / 2,
            "keyWidth must fit complementary pairs plus the reserved validity pair");
    Require(muxSenseAmp > 0 && muxSenseAmp <= entries && entries % muxSenseAmp == 0,
            "sense amplifier mux must be a positive divisor of strings per block");
    Require(wire.initialized, "local wire must be initialized");
    RequireNonnegative(wire.resWirePerUnit, "local wire resistance");
    RequireNonnegative(wire.capWirePerUnit, "local wire capacitance");
    RequireNonnegative(config->peripherals.addCapOnML, "additional bitline capacitance");
    RequirePositive(cell.area, "planar cell area");
    RequirePositive(cell.aspectRatio, "planar cell aspect ratio");
    RequirePositive(config->technology.tech->featureSize(), "technology feature size");

    metrics.modelBackend = device.model;
    metrics.calibrationStatus = device.calibrationStatus;
    metrics.modelSource = device.source;
    metrics.metadata = {
        {"model_identifier", "evacam-nand-tcam-v1"},
        {"topology", "nand_string"},
        {"encoding", "complementary_pair_with_validity_pair"},
        {"sense_polarity", "match_discharges_bitline"},
        {"sense_margin_definition", "min(reference-match,mismatch-reference)-offset"},
        {"array_layout", "planar_approximation"}
    };
    metrics.entries = entries;
    metrics.keyWidth = keyWidth;
    metrics.dataWordlines = static_cast<long>(wordlines);
    metrics.paddingWordlines = static_cast<long>(wordlines - 2 * (keyWidth + 1));
    metrics.physicalCells = entries * wordlines;
    metrics.physicalPageBits = entries;
    metrics.physicalBlockBits = metrics.physicalCells;
    metrics.senseAmplifiers = entries / muxSenseAmp;
    metrics.searchRounds = muxSenseAmp;

    // The exploratory layout is planar. Select devices use one cell pitch each;
    // supplied peripheral areas are placed alongside the array, without overlap.
    const double feature = config->technology.tech->featureSize();
    const double cellArea = cell.area * feature * feature;
    const double cellHeight = std::sqrt(cellArea * cell.aspectRatio);
    const double cellWidth = std::sqrt(cellArea / cell.aspectRatio);
    const double arrayWidth = entries * cellWidth;
    const double arrayHeight = (wordlines + 2) * cellHeight;
    const double driverCount = wordlines + 2;
    metrics.areaBreakdown = {
        {"flash_cells", metrics.physicalCells * cellArea},
        {"select_devices", entries * 2 * cellArea},
        {"wordline_drivers", driverCount * device.wordlineDriver.area},
        {"sense_amplifiers", metrics.senseAmplifiers * device.sense.area},
        {"page_buffers", entries * device.pageBuffer.area}
    };
    metrics.area = Sum(metrics.areaBreakdown);
    metrics.width = arrayWidth + metrics.areaBreakdown.at("wordline_drivers") / arrayHeight;
    metrics.height = metrics.area / metrics.width;
    RequirePositive(metrics.width, "array width");
    RequirePositive(metrics.height, "array height");

    bitlineWireResistance = arrayHeight * wire.resWirePerUnit;
    bitlineWireCapacitance = arrayHeight * wire.capWirePerUnit;
    bitlineCapacitance = device.capacitanceBitline + config->peripherals.addCapOnML;
    gateCapacitancePerWordline = entries * device.capacitanceGate
            + arrayWidth * wire.capWirePerUnit;
    RequirePositive(bitlineCapacitance + bitlineWireCapacitance, "bitline load");
    RequirePositive(gateCapacitancePerWordline, "wordline gate load");
    RequireNonnegative(bitlineWireResistance, "bitline wire resistance");

    std::vector<double> slowMatch(wordlines, device.resistancePass);
    slowMatch[0] = device.resistanceReadOn; // Fixed query for the validity pair.
    for (long bit = 0; bit < keyWidth; bit++) {
        // Source-side members have the greater distributed-capacitance weight.
        slowMatch[2 + 2 * bit] = device.resistanceReadOn;
    }
    metrics.slowestMatchTimeConstant = StringTimeConstant(slowMatch);

    // These bounds order first moments within the exponential approximation;
    // they are not conservative voltage bounds on the full RC transient.
    // Fastest nonmatch has exactly one blocking device and all other key
    // queries masked. Consider both invalid entries and either key polarity.
    std::vector<double> fastMismatch(wordlines, device.resistancePass);
    fastMismatch[0] = device.resistanceReadOn;
    fastMismatch[2 * keyWidth + 1] = device.resistanceOff;
    metrics.fastestMismatchTimeConstant = StringTimeConstant(fastMismatch);
    fastMismatch[2 * keyWidth + 1] = device.resistancePass;
    fastMismatch[0] = device.resistanceOff;
    metrics.fastestMismatchTimeConstant = std::min(metrics.fastestMismatchTimeConstant,
            StringTimeConstant(fastMismatch));

    metrics.decisionTime = device.decisionTime;
    metrics.matchVoltage = device.voltagePrecharge
            * std::exp(-device.decisionTime / metrics.slowestMatchTimeConstant);
    metrics.mismatchVoltage = device.voltagePrecharge
            * std::exp(-device.decisionTime / metrics.fastestMismatchTimeConstant);
    metrics.referenceVoltage = device.referenceVoltage == 0
            ? (metrics.matchVoltage + metrics.mismatchVoltage) / 2 : device.referenceVoltage;
    metrics.senseMargin = std::min(metrics.referenceVoltage - metrics.matchVoltage,
            metrics.mismatchVoltage - metrics.referenceVoltage) - device.senseOffset;
    metrics.requiredSenseMargin = device.minSenseMargin;
    metrics.senseMarginPass = metrics.senseMargin >= metrics.requiredSenseMargin;

    const double rounds = muxSenseAmp;
    metrics.latencyBreakdown = {
        {"query", device.query.latency},
        {"setup", device.setup.latency},
        {"wordline_precharge_bias", rounds * device.wordlineDriver.latency},
        {"wordline_query_bias", rounds * device.wordlineDriver.latency},
        {"wordline_reset", device.wordlineDriver.latency},
        {"select_gate_drivers", 2 * rounds * device.wordlineDriver.latency},
        {"precharge", rounds * device.precharge.latency},
        {"evaluation", rounds * device.decisionTime},
        {"sensing", rounds * device.sense.latency},
        {"page_buffers", rounds * device.pageBuffer.latency},
        {"recovery", rounds * device.recovery.latency}
    };
    metrics.searchLatency = Sum(metrics.latencyBreakdown);
    // A complete charging/discharging cycle costs C*V^2 at the supply.
    // No additional I*V*t string term is added: that would count discharge
    // already paid for by the capacitor recharge. Internal ladder nodes are
    // precharged through the string with ALL wordlines pass-biased and the
    // source select disabled. The supplied precharge time must be sufficient
    // to establish that initial condition; the query biases are applied only
    // afterward. Query latches and voltage-supply startup remain shared.
    const double prechargedCapacitance = bitlineCapacitance + bitlineWireCapacitance
            + device.capacitanceSource + wordlines * device.capacitanceInternal;
    const double selectCapacitance = 2 * (entries * device.capacitanceSelect
            + arrayWidth * wire.capWirePerUnit);
    metrics.searchEnergyBreakdown = {
        {"query", device.query.energy},
        {"setup", device.setup.energy},
        {"wordline_capacitance", QueryGateEnergy(std::vector<int>(keyWidth, 0))},
        {"wordline_drivers", QueryDriverEnergy(std::vector<int>(keyWidth, 0))},
        {"select_gate_drivers", 4 * rounds * device.wordlineDriver.energy},
        {"select_gate_capacitance", rounds * selectCapacitance
                * device.voltagePass * device.voltagePass / device.supplyEfficiency},
        {"bitline_and_internal_precharge", rounds * entries * prechargedCapacitance
                * device.voltagePrecharge * device.voltagePrecharge / device.supplyEfficiency},
        {"precharge_overhead", rounds * device.precharge.energy},
        {"sensing", entries * device.sense.energy},
        {"page_buffers", entries * device.pageBuffer.energy},
        {"recovery", rounds * device.recovery.energy}
    };
    metrics.searchEnergy = Sum(metrics.searchEnergyBreakdown);
    metrics.leakage = driverCount * device.wordlineDriver.leakage
            + metrics.senseAmplifiers * device.sense.leakage
            + entries * device.pageBuffer.leakage;
    RequireNonnegative(metrics.leakage, "peripheral leakage");
    metrics.programPageLatency = device.programPage.latency;
    metrics.programPageEnergy = device.programPage.energy;
    metrics.eraseBlockLatency = device.eraseBlock.latency;
    metrics.eraseBlockEnergy = device.eraseBlock.energy;
    initialized = true;
}

const NandCamMetrics &NandCamModel::Metrics() const {
    if (!initialized) {
        throw std::runtime_error("NAND CAM metrics require successful initialization");
    }
    return metrics;
}

double NandCamModel::StringResistance(const std::vector<double> &resistances) const {
    return 2 * device.resistanceSelect + bitlineWireResistance
            + std::accumulate(resistances.begin(), resistances.end(), 0.0);
}

double NandCamModel::StringTimeConstant(const std::vector<double> &resistances) const {
    // Ground--Rselect--Csource--R0--Cinternal--...--Rn--Cinternal--Rselect--BL.
    // Each capacitor is weighted by resistance between it and grounded source.
    // The bitline wire's own distributed capacitance sees half its resistance.
    double resistanceToGround = device.resistanceSelect;
    double tau = device.capacitanceSource * resistanceToGround;
    for (double resistance : resistances) {
        resistanceToGround += resistance;
        tau += device.capacitanceInternal * resistanceToGround;
    }
    resistanceToGround += device.resistanceSelect;
    tau += bitlineCapacitance * (resistanceToGround + bitlineWireResistance)
            + bitlineWireCapacitance * (resistanceToGround + bitlineWireResistance / 2);
    RequirePositive(tau, "string RC time constant");
    return tau;
}

std::vector<double> NandCamModel::EncodeResistances(const std::vector<int> &stored,
        const std::vector<int> &query, bool valid) const {
    std::vector<double> resistances(metrics.dataWordlines, device.resistancePass);
    // Programmed validity = (L,H); invalid/unused = (H,L), always queried 0.
    // Erased L,L is NOT an invalid marker: the caller must initialize validity
    // before searching and explicitly mark invalid slots. Erase state is not
    // tracked by this stateless operation model.
    resistances[0] = valid ? device.resistanceReadOn : device.resistanceOff;
    for (std::size_t bit = 0; bit < stored.size(); bit++) {
        Require(stored[bit] >= -1 && stored[bit] <= 1
                        && query[bit] >= -1 && query[bit] <= 1,
                "stored and query symbols must be -1 (wildcard), 0, or 1");
        if (query[bit] != -1) {
            const std::size_t selected = 2 + 2 * bit + query[bit];
            resistances[selected] = stored[bit] == -1 || stored[bit] == query[bit]
                    ? device.resistanceReadOn : device.resistanceOff;
        }
    }
    return resistances;
}

double NandCamModel::QueryGateEnergy(const std::vector<int> &query) const {
    long long selectedWordlines = 1; // The validity query is never masked.
    for (int symbol : query) {
        selectedWordlines += symbol != -1;
    }
    // First precharge charges every gate to Vpass from ground. Later rounds
    // restore only selected gates from Vread to Vpass, drawing C*Vp*(Vp-Vr)
    // from the pass supply. Lowering gates and final reset dissipate charge;
    // no charge-recovery credit is claimed. Pass-biased padding remains held.
    const double initialCharge = metrics.dataWordlines
            * device.voltagePass * device.voltagePass;
    const double recharge = (metrics.searchRounds - 1) * selectedWordlines
            * device.voltagePass * (device.voltagePass - device.voltageRead);
    return gateCapacitancePerWordline * (initialCharge + recharge) / device.supplyEfficiency;
}

double NandCamModel::QueryDriverEnergy(const std::vector<int> &query) const {
    long long selectedWordlines = 1;
    for (int symbol : query) {
        selectedWordlines += symbol != -1;
    }
    // Per-driver supplied overhead is per voltage-update event: all gates
    // initially pass-biased and finally reset, plus each query transition
    // and each inter-round return to pass bias.
    const long long events = 2 * metrics.dataWordlines
            + (2 * static_cast<long long>(metrics.searchRounds) - 1) * selectedWordlines;
    return events * device.wordlineDriver.energy;
}

EvaCAMMatchResult NandCamModel::Evaluate(const std::vector<int> &stored,
        const std::vector<int> &query, bool valid) const {
    Metrics();
    Require(stored.size() == static_cast<std::size_t>(metrics.keyWidth)
                    && query.size() == static_cast<std::size_t>(metrics.keyWidth),
            "stored and query vectors must have keyWidth symbols");
    const auto resistances = EncodeResistances(stored, query, valid);
    bool logicalMatch = valid;
    for (std::size_t bit = 0; bit < stored.size(); bit++) {
        if (stored[bit] != -1 && query[bit] != -1 && stored[bit] != query[bit]) {
            logicalMatch = false;
        }
    }
    EvaCAMMatchResult result;
    result.hit = logicalMatch;
    result.searchLatency = metrics.searchLatency;
    result.searchDynamicEnergy = metrics.searchEnergy
            - (metrics.searchEnergyBreakdown.at("wordline_capacitance") - QueryGateEnergy(query))
            - (metrics.searchEnergyBreakdown.at("wordline_drivers") - QueryDriverEnergy(query));
    result.matchlineDelay = metrics.decisionTime;
    result.matchlineConductance = 1 / StringResistance(resistances);
    result.matchlineVoltage = device.voltagePrecharge
            * std::exp(-device.decisionTime / StringTimeConstant(resistances));
    result.senseMargin = (logicalMatch ? metrics.referenceVoltage - result.matchlineVoltage
            : result.matchlineVoltage - metrics.referenceVoltage) - device.senseOffset;
    result.requiredSenseMargin = metrics.requiredSenseMargin;
    result.senseMarginSlack = result.senseMargin - result.requiredSenseMargin;
    result.senseMarginPass = result.senseMarginSlack >= 0;
    return result;
}
