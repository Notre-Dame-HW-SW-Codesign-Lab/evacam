#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include "model/NandCamModel.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

struct Fixture {
    std::shared_ptr<EvaCamConfig> config = TestModelBuilders::MakeEvaCamConfig();
    Wire wire;
    NandCamModel model;
    long entries = 8;
    long width = 3;
    int mux = 1;

    Fixture() {
        auto &cell = *config->technology.cell;
        cell.memCellType = SLCNAND;
        cell.camType = TCAM;
        cell.nandString = true;
        cell.area = 4;
        cell.aspectRatio = 2;
        config->input.designTarget = CAM_chip;
        config->input.searchFunction = EX;
        config->input.internalSensing = true;
        config->input.pageSize = entries;
        config->input.flashBlockSize = entries * 10;
        config->peripherals.addCapOnML = 0;
        auto &device = cell.nand;
        device.configured = true;
        device.model = "analytical_rc";
        device.calibrationStatus = "uncalibrated";
        device.source = "Synthetic unit-test RC network, not device calibration";
        device.resistanceReadOn = 2000;
        device.resistancePass = 1000;
        device.resistanceOff = 1e8;
        device.resistanceSelect = 500;
        device.capacitanceGate = 2e-15;
        device.capacitanceInternal = 1e-15;
        device.capacitanceBitline = 20e-15;
        device.capacitanceSource = 2e-15;
        device.capacitanceSelect = 1e-15;
        device.thresholdLow = -1;
        device.thresholdHigh = 2;
        device.voltageRead = 1;
        device.voltagePass = 4;
        device.voltagePrecharge = 0.8;
        device.decisionTime = 0.5e-9;
        device.minSenseMargin = 0.05;
        device.senseOffset = 0.005;
        device.supplyEfficiency = 0.8;
        device.wordlineDriver = {2e-12, 2e-9, 3e-15, 1e-9};
        device.sense = {3e-12, 4e-9, 5e-15, 2e-9};
        device.pageBuffer = {4e-12, 6e-9, 7e-15, 3e-9};
        device.query = {1e-9, 1e-15};
        device.setup = {2e-9, 2e-15};
        device.precharge = {3e-9, 3e-15};
        device.recovery = {4e-9, 4e-15};
        device.programPage = {100e-6, 1e-9};
        device.eraseBlock = {2e-3, 2e-6};
        wire.initialized = true;
        wire.capWirePerUnit = 0;
        wire.resWirePerUnit = 0;
    }

    NandDeviceSpec &Device() { return config->technology.cell->nand; }
    void Initialize() { model.Initialize(config, entries, width, mux, wire); }
};

void TestTruthTableAndBounds() {
    Fixture fixture;
    fixture.Initialize();
    const auto &metrics = fixture.model.Metrics();
    Require(metrics.senseMarginPass, "synthetic example must resolve both classes");
    Require(metrics.matchVoltage < metrics.mismatchVoltage, "match must discharge faster");
    AssertNear(metrics.senseMargin,
            (metrics.mismatchVoltage - metrics.matchVoltage) / 2 - fixture.Device().senseOffset);
    // Exhaust every stored/query ternary pattern, not just mismatch counts.
    for (int storedCode = 0; storedCode < 27; storedCode++) {
        for (int queryCode = 0; queryCode < 27; queryCode++) {
            int remainingStored = storedCode;
            int remainingQuery = queryCode;
            bool expected = true;
            std::vector<int> stored(3), query(3);
            for (int bit = 0; bit < 3; bit++) {
                stored[bit] = remainingStored % 3 - 1;
                query[bit] = remainingQuery % 3 - 1;
                remainingStored /= 3;
                remainingQuery /= 3;
                expected = expected && (stored[bit] == -1 || query[bit] == -1
                        || stored[bit] == query[bit]);
            }
            const auto result = fixture.model.Evaluate(stored, query);
            Require(result.hit == expected, "ternary pair truth table");
            Require(result.senseMarginPass, "every supported pattern has sense margin");
            Require(result.senseMargin + 1e-14 >= metrics.senseMargin,
                    "analytical global bound must cover every pattern");
            Require(result.searchDynamicEnergy <= metrics.searchEnergy,
                    "query-specific supply energy must not exceed unmasked-query envelope");
            if (expected) {
                Require(result.matchlineVoltage <= metrics.matchVoltage + 1e-14,
                        "slowest match bound must include wildcard queries");
            } else {
                Require(result.matchlineVoltage >= metrics.mismatchVoltage - 1e-14,
                        "fastest mismatch bound must include wildcard queries");
            }
            const auto invalid = fixture.model.Evaluate(stored, query, false);
            Require(!invalid.hit && invalid.senseMarginPass,
                    "invalid/unused entries must never yield a match");
            Require(invalid.matchlineVoltage >= metrics.mismatchVoltage - 1e-14,
                    "validity condition must be covered by the nonmatch bound");
        }
    }
}

void TestIndependentRcReferenceAndPositions() {
    Fixture fixture;
    fixture.width = 1;
    fixture.config->input.flashBlockSize = fixture.entries * 4;
    fixture.Device().capacitanceInternal = 0;
    fixture.Device().capacitanceSource = 0;
    fixture.Initialize();
    const auto lumped = fixture.model.Evaluate({0}, {0});
    // Four data devices R={2000,1000,2000,1000}, two 500-ohm selects,
    // and only a 20 fF bitline. This network is exactly one R*C pole.
    const double resistance = 7000;
    const double lumpedTau = resistance * 20e-15;
    AssertNear(lumped.matchlineConductance, 1 / resistance);
    AssertNear(lumped.matchlineVoltage, 0.8 * std::exp(-0.5e-9 / lumpedTau));
    AssertNear(fixture.model.Metrics().slowestMatchTimeConstant, lumpedTau);

    fixture.Device().capacitanceInternal = 1e-15;
    fixture.Device().capacitanceSource = 2e-15;
    fixture.Initialize();
    // Internal nodes see source-ground path resistances 2500,3500,5500,6500.
    // Source node sees 500 ohms; bitline sees all 7000 ohms.
    const double ladderTau = 20e-15 * 7000 + 2e-15 * 500
            + 1e-15 * (2500 + 3500 + 5500 + 6500);
    AssertNear(fixture.model.Metrics().slowestMatchTimeConstant, ladderTau);
    AssertNear(fixture.model.Evaluate({0}, {0}).matchlineVoltage,
            0.8 * std::exp(-0.5e-9 / ladderTau));

    Fixture positions;
    positions.Initialize();
    double previousVoltage = 1;
    for (int index = 0; index < 3; index++) {
        std::vector<int> query(3, -1);
        query[index] = 1;
        const auto mismatch = positions.model.Evaluate({0, 0, 0}, query);
        Require(!mismatch.hit, "one mismatch must reject");
        Require(mismatch.matchlineVoltage < previousVoltage,
                "blocking closer to bitline must have less internal-node RC weight");
        previousVoltage = mismatch.matchlineVoltage;
    }
    const auto nearSource = positions.model.Evaluate({1, -1, -1}, {0, -1, -1});
    const auto nearDrain = positions.model.Evaluate({0, -1, -1}, {1, -1, -1});
    AssertNear(nearSource.matchlineConductance, nearDrain.matchlineConductance);
    Require(nearSource.matchlineVoltage > nearDrain.matchlineVoltage,
            "equal series resistance does not erase blocking-device position");
}

void TestGeometryScheduleAndEnergyLedger() {
    Fixture fixture;
    fixture.Initialize();
    const auto single = fixture.model.Metrics();
    assert(single.entries == 8 && single.keyWidth == 3 && single.dataWordlines == 10);
    assert(single.paddingWordlines == 2 && single.physicalCells == 80);
    assert(single.physicalPageBits == 8 && single.physicalBlockBits == 80);
    assert(single.senseAmplifiers == 8 && single.searchRounds == 1);
    AssertNear(single.area, single.width * single.height);
    const double cellArea = 4 * std::pow(90e-9, 2);
    AssertNear(single.area, 96 * cellArea + 12 * 2e-12 + 8 * (3e-12 + 4e-12));
    // Supply energy is C*V^2/efficiency, not half that value, and includes
    // every precharged internal node. It is not supplemented by I*V*t.
    AssertNear(single.searchEnergyBreakdown.at("bitline_and_internal_precharge"),
            8 * (20e-15 + 2e-15 + 10e-15) * 0.8 * 0.8 / 0.8);
    AssertNear(single.searchEnergyBreakdown.at("wordline_capacitance"),
            8 * 2e-15 * 10 * 16 / 0.8);
    const auto masked = fixture.model.Evaluate({0, 0, 0}, {-1, -1, -1});
    const auto unmasked = fixture.model.Evaluate({0, 0, 0}, {0, 0, 0});
    AssertNear(unmasked.searchDynamicEnergy, single.searchEnergy);
    // The first all-pass precharge gate charging is common to both queries;
    // only the three additional query-bias driver update events differ.
    AssertNear(unmasked.searchDynamicEnergy - masked.searchDynamicEnergy,
            3 * fixture.Device().wordlineDriver.energy);
    Require(single.searchEnergyBreakdown.count("string_conduction") == 0,
            "capacitor discharge must not be counted again as conduction supply energy");

    fixture.mux = 4;
    fixture.Initialize();
    const auto multiplexed = fixture.model.Metrics();
    assert(multiplexed.senseAmplifiers == 2 && multiplexed.searchRounds == 4);
    AssertNear(multiplexed.area, single.area - 6 * fixture.Device().sense.area);
    AssertNear(multiplexed.latencyBreakdown.at("setup"), single.latencyBreakdown.at("setup"));
    AssertNear(multiplexed.latencyBreakdown.at("evaluation"),
            4 * single.latencyBreakdown.at("evaluation"));
    AssertNear(multiplexed.searchEnergyBreakdown.at("wordline_capacitance"),
            single.searchEnergyBreakdown.at("wordline_capacitance")
                + 3 * 4 * (8 * 2e-15) * 4 * (4 - 1) / 0.8);
    const auto multiplexedMasked = fixture.model.Evaluate({0, 0, 0}, {-1, -1, -1});
    const auto multiplexedUnmasked = fixture.model.Evaluate({0, 0, 0}, {0, 0, 0});
    AssertNear(multiplexedUnmasked.searchDynamicEnergy - multiplexedMasked.searchDynamicEnergy,
            3 * 3 * (8 * 2e-15) * 4 * (4 - 1) / 0.8
                + 3 * (2 * 4 - 1) * fixture.Device().wordlineDriver.energy);
    AssertNear(multiplexed.searchEnergyBreakdown.at("bitline_and_internal_precharge"),
            4 * single.searchEnergyBreakdown.at("bitline_and_internal_precharge"));
    AssertNear(multiplexed.searchEnergyBreakdown.at("sensing"),
            single.searchEnergyBreakdown.at("sensing"));
    AssertNear(multiplexed.senseMargin, single.senseMargin);

    fixture.entries = 16;
    fixture.config->input.pageSize = 16;
    fixture.config->input.flashBlockSize = 160;
    fixture.Initialize();
    const auto doubled = fixture.model.Metrics();
    AssertNear(doubled.areaBreakdown.at("flash_cells"),
            2 * multiplexed.areaBreakdown.at("flash_cells"));
    AssertNear(doubled.searchEnergyBreakdown.at("bitline_and_internal_precharge"),
            2 * multiplexed.searchEnergyBreakdown.at("bitline_and_internal_precharge"));
    AssertNear(doubled.searchLatency, multiplexed.searchLatency);
}

void TestLoadsOperationsAndSensingFailure() {
    Fixture fixture;
    fixture.Initialize();
    const auto original = fixture.model.Metrics();
    fixture.Device().programPage = {200e-6, 3e-9};
    fixture.Device().eraseBlock = {3e-3, 4e-6};
    fixture.Initialize();
    const auto operations = fixture.model.Metrics();
    AssertNear(operations.searchEnergy, original.searchEnergy);
    AssertNear(operations.searchLatency, original.searchLatency);
    AssertNear(operations.senseMargin, original.senseMargin);
    AssertNear(operations.programPageEnergy, 3e-9);
    AssertNear(operations.programPageLatency, 200e-6);
    AssertNear(operations.eraseBlockEnergy, 4e-6);
    AssertNear(operations.eraseBlockLatency, 3e-3);

    fixture.config->peripherals.addCapOnML = 10e-15;
    fixture.Initialize();
    Require(fixture.model.Metrics().slowestMatchTimeConstant > original.slowestMatchTimeConstant,
            "additional sense capacitance must slow the string");
    Require(fixture.model.Metrics().searchEnergy > original.searchEnergy,
            "additional sense capacitance must consume recharge energy");
    fixture.wire.resWirePerUnit = 1e8;
    fixture.wire.capWirePerUnit = 1e-9;
    const double withoutWireTau = fixture.model.Metrics().slowestMatchTimeConstant;
    fixture.Initialize();
    Require(fixture.model.Metrics().slowestMatchTimeConstant > withoutWireTau,
            "distributed bitline wire loading must affect latency");

    Fixture infeasible;
    infeasible.Device().resistanceOff = 2001;
    infeasible.Initialize();
    Require(infeasible.model.Metrics().senseMargin < 0,
            "overlapping match/nonmatch bounds must keep their negative signed margin");
    Require(!infeasible.model.Metrics().senseMarginPass, "overlapping bounds must fail");
    const auto logicalHit = infeasible.model.Evaluate({0, 0, 0}, {0, 0, 0});
    Require(logicalHit.hit && !logicalHit.senseMarginPass,
            "logical match must remain separate from electrical detectability");

    Fixture offset;
    offset.Device().senseOffset = 0.8;
    offset.Initialize();
    Require(!offset.model.Metrics().senseMarginPass, "comparator offset must reduce margin");
    Fixture reference;
    reference.Device().referenceVoltage = 0.79;
    reference.Initialize();
    AssertNear(reference.model.Metrics().referenceVoltage, 0.79);
    Require(!reference.model.Metrics().senseMarginPass, "fixed reference must be respected");
}

void TestValidationAndLifecycle() {
    Fixture fixture;
    AssertThrows<std::runtime_error>([&] { fixture.model.Metrics(); }, "initialization");
    AssertThrows<std::runtime_error>([&] { fixture.model.Evaluate({0}, {0}); }, "initialization");
    fixture.Initialize();
    AssertThrows<std::invalid_argument>([&] { fixture.model.Evaluate({0}, {0}); }, "keyWidth");
    AssertThrows<std::invalid_argument>([&] {
        fixture.model.Evaluate({0, 1, 2}, {0, 1, 0});
    }, "symbols");
    fixture.width = std::numeric_limits<long>::max();
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "keyWidth");
    AssertThrows<std::runtime_error>([&] { fixture.model.Metrics(); }, "initialization");
    fixture.width = 3;
    fixture.mux = 3;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "divisor");
    fixture.mux = 1;
    fixture.config->input.pageSize = 7;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "page_size");
    fixture.config->input.pageSize = 8;
    fixture.config->input.flashBlockSize = 81;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "integer multiple");
    fixture.config->input.flashBlockSize = 8 * 4097;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "4096");
    fixture.config->input.flashBlockSize = 80;
    fixture.entries = 1048577;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "1048576");
    fixture.entries = 8;
    fixture.Device().capacitanceInternal = std::numeric_limits<double>::quiet_NaN();
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "capacitanceInternal");
    fixture.Device().capacitanceInternal = 0;
    fixture.Device().supplyEfficiency = 1.1;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "at most one");
    fixture.Device().supplyEfficiency = 1;
    fixture.Device().thresholdHigh = 5;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "biases");
    fixture.Device().thresholdHigh = 2;
    fixture.Device().resistanceReadOn = 500;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "resistances");
    fixture.Device().resistanceReadOn = 2000;
    fixture.Device().programPage.energy = 0;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "programPage.energy");
    fixture.Device().programPage.energy = 1e-9;
    fixture.Device().calibrationStatus = "unknown";
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "calibrationStatus");
    fixture.Device().calibrationStatus = "uncalibrated";
    fixture.config->input.searchFunction = TH;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "exact search");
    fixture.config->input.searchFunction = EX;
    fixture.wire.initialized = false;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "wire");
}

}  // namespace

int main() {
    TestTruthTableAndBounds();
    TestIndependentRcReferenceAndPositions();
    TestGeometryScheduleAndEnergyLedger();
    TestLoadsOperationsAndSensingFailure();
    TestValidationAndLifecycle();
    std::cout << "NAND CAM model tests passed\n";
}
