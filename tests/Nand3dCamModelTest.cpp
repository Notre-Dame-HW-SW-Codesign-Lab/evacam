#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include "model/Nand3dCamModel.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

struct Fixture {
    std::shared_ptr<EvaCamConfig> config = TestModelBuilders::MakeEvaCamConfig();
    Nand3dCamModel model;
    Wire wire;
    long width = 2;
    int mux = 1;

    Fixture() {
        auto &cell = *config->technology.cell;
        cell.memCellType = NAND3D;
        cell.nandString = true;
        cell.camType = TCAM;
        cell.area = 0; // A planar F^2 device area is neither needed nor used.
        cell.aspectRatio = 0;
        config->input.designTarget = CAM_chip;
        config->input.searchFunction = EX;
        config->input.internalSensing = true;
        config->peripherals.addCapOnML = 0;
        auto &device = cell.nand3d;
        device.configured = true;
        device.storageMode = "SLC";
        device.storageLayers = 6;
        device.dummyLayers = 2;
        device.stringRows = 2;
        device.stringColumns = 8;
        device.holePitchX = 100e-9;
        device.holePitchY = 120e-9;
        device.layerPitch = 50e-9;
        device.staircaseStepWidth = 80e-9;
        device.staircaseContactLength = 500e-9;
        device.isolationWidth = 200e-9;
        device.peripheralPlacement = "beside";
        auto &electrical = device.electrical;
        electrical.configured = true;
        electrical.model = "analytical_rc";
        electrical.calibrationStatus = "synthetic";
        electrical.source = "Synthetic independent unit-test circuit";
        electrical.resistanceReadOn = 10000;
        electrical.resistancePass = 5000;
        electrical.resistanceOff = 1e9;
        electrical.resistanceSelect = 1000;
        electrical.capacitanceGate = 0.1e-15;
        electrical.capacitanceInternal = 0.05e-15;
        electrical.capacitanceSource = 1e-15;
        electrical.capacitanceBitline = 20e-15;
        electrical.capacitanceSelect = 0.1e-15;
        electrical.thresholdLow = -1;
        electrical.thresholdHigh = 2;
        electrical.voltageRead = 0.5;
        electrical.voltagePass = 5;
        electrical.voltagePrecharge = 0.8;
        electrical.decisionTime = 5e-9;
        electrical.minSenseMargin = 0.1;
        electrical.senseOffset = 0.01;
        electrical.supplyEfficiency = 0.8;
        electrical.wordlineDriver = {2e-12, 2e-9, 2e-15, 1e-9};
        electrical.sense = {1e-12, 2e-9, 5e-15, 1e-9};
        electrical.pageBuffer = {1e-12, 1e-9, 2e-15, 1e-9};
        electrical.query = {2e-9, 5e-15};
        electrical.setup = {5e-9, 10e-15};
        electrical.precharge = {5e-9, 5e-15};
        electrical.recovery = {10e-9, 10e-15};
        electrical.programPage = {200e-6, 2e-9};
        electrical.eraseBlock = {2e-3, 20e-9};
        wire.initialized = true;
        wire.resWirePerUnit = 0;
        wire.capWirePerUnit = 0;
    }

    Nand3dMemoryDevice &Device() { return config->technology.cell->nand3d; }
    void Initialize() {
        const long entries = static_cast<long>(Device().stringRows) * Device().stringColumns;
        config->input.pageSize = Device().stringColumns;
        config->input.flashBlockSize = entries * Device().storageLayers;
        model.Initialize(config, entries, width, mux, wire);
    }
};

void TestGeometryAndPlacement() {
    Fixture fixture;
    fixture.Initialize();
    const auto beside = fixture.model.Metrics();
    assert(beside.entries == 16 && beside.physicalPageBits == 8 && beside.physicalBlockBits == 96);
    assert(beside.physicalCells == 96 && beside.searchRounds == 2 && beside.senseAmplifiers == 8);
    AssertNear(beside.geometryMetrics.at("dummy_devices_per_block"), 32);
    AssertNear(beside.geometryMetrics.at("vertical_stack_height_m"), 10 * 50e-9);
    AssertNear(beside.geometryMetrics.at("block_core_width_m"), 8 * 100e-9);
    AssertNear(beside.geometryMetrics.at("block_core_height_m"), 2 * 120e-9);
    AssertNear(beside.geometryMetrics.at("staircase_area_m2"), 10 * 80e-9 * 500e-9);
    AssertNear(beside.geometryMetrics.at("lateral_bitline_length_m"), 2 * 120e-9);
    AssertNear(beside.width * beside.height, beside.area);
    double areaLedger = 0;
    for (const auto &component : beside.areaBreakdown) areaLedger += component.second;
    AssertNear(areaLedger, beside.area);
    const double peripheral = beside.geometryMetrics.at("peripheral_area_m2");
    const double arrayArea = beside.area - peripheral;
    fixture.Device().peripheralPlacement = "under_array";
    fixture.Initialize();
    const auto under = fixture.model.Metrics();
    AssertNear(under.area, std::max(arrayArea, peripheral));
    Require(under.area < beside.area, "under-array CMOS must share footprint without double counting");
    AssertNear(under.searchEnergy, beside.searchEnergy);
    AssertNear(under.searchLatency, beside.searchLatency);
    fixture.Device().layerPitch *= 2;
    fixture.Initialize();
    const auto tallerPitch = fixture.model.Metrics();
    AssertNear(tallerPitch.area, under.area);
    AssertNear(tallerPitch.geometryMetrics.at("lateral_bitline_length_m"),
            under.geometryMetrics.at("lateral_bitline_length_m"));
    AssertNear(tallerPitch.matchVoltage, under.matchVoltage);
    AssertNear(tallerPitch.geometryMetrics.at("vertical_stack_height_m"),
            2 * under.geometryMetrics.at("vertical_stack_height_m"));
    fixture.Device().storageLayers += 2;
    fixture.Initialize();
    const auto moreLayers = fixture.model.Metrics();
    AssertNear(moreLayers.geometryMetrics.at("block_core_width_m"), tallerPitch.geometryMetrics.at("block_core_width_m"));
    Require(moreLayers.geometryMetrics.at("staircase_area_m2") > tallerPitch.geometryMetrics.at("staircase_area_m2"),
            "additional layers must add staircase footprint");
    Require(moreLayers.matchVoltage > tallerPitch.matchVoltage, "additional pass devices alter the analytical first moment");
}

void TestTernaryTruthAndAnalyticalMargin() {
    Fixture fixture;
    fixture.Initialize();
    const auto &metrics = fixture.model.Metrics();
    Require(metrics.senseMarginPass, "synthetic short string must have a detectable analytical margin");
    Require(metrics.metadata.at("sensing_bound") == "global_within_single_exponential_approximation",
            "bounds apply to the analytical approximation only");
    Require(metrics.diagnosticMetrics.empty(), "analytical estimates do not claim solver diagnostics");
    for (int storedCode = 0; storedCode < 9; storedCode++) {
        for (int queryCode = 0; queryCode < 9; queryCode++) {
            const std::vector<int> stored{storedCode % 3 - 1, storedCode / 3 - 1};
            const std::vector<int> query{queryCode % 3 - 1, queryCode / 3 - 1};
            bool expected = true;
            for (int bit = 0; bit < 2; bit++) expected = expected
                    && (stored[bit] == -1 || query[bit] == -1 || stored[bit] == query[bit]);
            const auto result = fixture.model.Evaluate(stored, query);
            Require(result.hit == expected, "complete ternary truth table");
            Require(result.senseMarginPass, "all short-string patterns should resolve electrically");
            if (expected) Require(result.matchlineVoltage <= metrics.matchVoltage + 1e-14,
                    "analytical slowest-match bound covers every ternary match");
            else Require(result.matchlineVoltage >= metrics.mismatchVoltage - 1e-14,
                    "analytical fastest-mismatch bound covers every ternary mismatch");
            Require(result.matchlineVoltage >= 0 && result.matchlineVoltage <= 0.8,
                    "analytical response stays inside supply envelope");
            const auto invalid = fixture.model.Evaluate(stored, query, false);
            Require(!invalid.hit && invalid.senseMarginPass, "programmed invalid marker rejects even masked query");
            Require(invalid.matchlineVoltage >= metrics.mismatchVoltage - 1e-14,
                    "analytical mismatch bound includes invalid entries");
        }
    }
    const auto sourceMismatch = fixture.model.Evaluate({1, -1}, {0, -1});
    const auto drainMismatch = fixture.model.Evaluate({-1, 0}, {-1, 1});
    Require(sourceMismatch.matchlineVoltage > drainMismatch.matchlineVoltage,
            "blocking-device position must survive analytical RC evaluation");
}

void TestIndependentLumpedPhaseLimitAndEnergy() {
    Fixture fixture;
    fixture.width = 1;
    auto &device = fixture.Device();
    device.storageLayers = 4;
    device.dummyLayers = 0;
    device.stringRows = 1;
    device.electrical.capacitanceSource = 1e-20;
    device.electrical.capacitanceInternal = 1e-20;
    device.electrical.precharge.latency = 0.1e-9;
    device.electrical.decisionTime = 0.3e-9;
    device.electrical.minSenseMargin = 0.01;
    fixture.Initialize();
    const auto &metrics = fixture.model.Metrics();
    // Vanishing internal capacitances recover the closed-form lumped limit.
    // Full precharge is the explicit assumption even for this short schedule.
    const double expected = 0.8 * std::exp(-0.3e-9 / (32000 * 20e-15));
    const auto result = fixture.model.Evaluate({0}, {0});
    AssertNear(result.matchlineVoltage, expected, 2e-6);
    const double chargedCapacitance = 20e-15 + 5e-20;
    AssertNear(metrics.searchEnergyBreakdown.at("bitline_and_internal_precharge"),
            8 * chargedCapacitance * 0.8 * 0.8 / 0.8, 1e-25);
    Require(metrics.metadata.at("precharge_initial_condition") == "uniform_full_rail_assumed_each_round",
            "full-rail precharge is an assumption, not a simulated claim");
    Require(metrics.searchEnergyBreakdown.count("string_conduction") == 0,
            "CV squared charging already accounts for subsequent discharge");
    device.electrical.precharge.latency = 1e-15;
    device.electrical.recovery.latency = 1e-15;
    fixture.Initialize();
    const auto shortSchedule = fixture.model.Evaluate({0}, {0});
    AssertNear(shortSchedule.matchlineVoltage, result.matchlineVoltage, 1e-14);
    AssertNear(shortSchedule.searchDynamicEnergy, result.searchDynamicEnergy, 1e-25);
    Require(shortSchedule.searchLatency < result.searchLatency,
            "supplied phase times affect the schedule without simulating state");
}

void TestMuxGroupsAndOperations() {
    Fixture fixture;
    fixture.Initialize();
    const auto base = fixture.model.Metrics();
    fixture.mux = 2;
    fixture.Initialize();
    const auto muxed = fixture.model.Metrics();
    assert(muxed.searchRounds == 4 && muxed.senseAmplifiers == 4);
    AssertNear(muxed.geometryMetrics.at("page_buffer_area_m2"), 8 * fixture.Device().electrical.pageBuffer.area);
    AssertNear(muxed.latencyBreakdown.at("precharge"), 2 * base.latencyBreakdown.at("precharge"));
    AssertNear(muxed.searchEnergyBreakdown.at("bitline_and_internal_precharge"),
            2 * base.searchEnergyBreakdown.at("bitline_and_internal_precharge"), 1e-20);
    AssertNear(muxed.searchEnergyBreakdown.at("sensing"), base.searchEnergyBreakdown.at("sensing"));
    Require(muxed.diagnosticMetrics.empty(), "analytical operation requires no integration steps");
    AssertNear(muxed.matchVoltage, base.matchVoltage, 1e-14);
    fixture.Device().electrical.programPage = {400e-6, 4e-9};
    fixture.Device().electrical.eraseBlock = {4e-3, 40e-9};
    fixture.Initialize();
    const auto operations = fixture.model.Metrics();
    AssertNear(operations.searchEnergy, muxed.searchEnergy);
    AssertNear(operations.searchLatency, muxed.searchLatency);
    AssertNear(operations.programPageEnergy, 4e-9);
    AssertNear(operations.eraseBlockEnergy, 40e-9);
    fixture.wire.resWirePerUnit = 1e8;
    fixture.wire.capWirePerUnit = 1e-9;
    fixture.Initialize();
    Require(fixture.model.Metrics().matchVoltage > operations.matchVoltage,
            "lateral wire pi-section must contribute to the first moment");

}

void TestAgreementWithPlanarAnalyticalCircuit() {
    Fixture fixture;
    fixture.Device().dummyLayers = 0;
    fixture.Initialize();
    auto config = TestModelBuilders::MakeEvaCamConfig();
    auto &cell = *config->technology.cell;
    cell.memCellType = SLCNAND;
    cell.nandString = true;
    cell.camType = TCAM;
    cell.area = 4;
    cell.aspectRatio = 2;
    cell.nand = fixture.Device().electrical;
    config->input = fixture.config->input;
    config->input.pageSize = fixture.model.Metrics().entries;
    config->peripherals.addCapOnML = 0;
    NandCamModel planar;
    planar.Initialize(config, config->input.pageSize, fixture.width, fixture.mux, fixture.wire);
    const auto &vertical = fixture.model.Metrics();
    AssertNear(vertical.slowestMatchTimeConstant, planar.Metrics().slowestMatchTimeConstant, 1e-24);
    AssertNear(vertical.fastestMismatchTimeConstant, planar.Metrics().fastestMismatchTimeConstant, 1e-24);
    AssertNear(vertical.matchVoltage, planar.Metrics().matchVoltage, 1e-14);
    AssertNear(vertical.mismatchVoltage, planar.Metrics().mismatchVoltage, 1e-14);
    AssertNear(vertical.searchEnergyBreakdown.at("bitline_and_internal_precharge"),
            planar.Metrics().searchEnergyBreakdown.at("bitline_and_internal_precharge"), 1e-25);
    for (const auto &pattern : std::vector<std::vector<int>>{{0, 1}, {1, 0}, {-1, -1}}) {
        AssertNear(fixture.model.Evaluate(pattern, {0, 1}).matchlineVoltage,
                planar.Evaluate(pattern, {0, 1}).matchlineVoltage, 1e-14);
    }
}

void TestValidationAndLifecycle() {
    Fixture fixture;
    AssertThrows<std::runtime_error>([&] { fixture.model.Metrics(); }, "initialization");
    fixture.Initialize();
    AssertThrows<std::invalid_argument>([&] { fixture.model.Evaluate({0}, {0}); }, "keyWidth");
    AssertThrows<std::invalid_argument>([&] { fixture.model.Evaluate({0, 2}, {0, 0}); }, "symbols");
    fixture.Device().peripheralPlacement = "under";
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "under_array");
    AssertThrows<std::runtime_error>([&] { fixture.model.Metrics(); }, "initialization");
    fixture.Device().peripheralPlacement = "beside";
    fixture.Device().dummyLayers = 4096;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "4096");
    fixture.Device().dummyLayers = 2;
    fixture.Device().electrical.capacitanceInternal = 0;
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "capacitance.internal");
    fixture.Device().electrical.capacitanceInternal = 0.05e-15;
    fixture.Device().electrical.model = "transient_rc";
    AssertThrows<std::invalid_argument>([&] { fixture.Initialize(); }, "analytical_rc");
    fixture.Device().electrical.model = "analytical_rc";
    fixture.Device().electrical.senseOffset = 0.8;
    fixture.Initialize();
    Require(!fixture.model.Metrics().senseMarginPass && fixture.model.Metrics().senseMargin < 0,
            "infeasible analytical margins remain signed");
    Require(fixture.model.Evaluate({0, 0}, {0, 0}).hit,
            "logical match remains distinct from electrical margin failure");
}
}  // namespace

int main() {
    TestGeometryAndPlacement();
    TestTernaryTruthAndAnalyticalMargin();
    TestIndependentLumpedPhaseLimitAndEnergy();
    TestMuxGroupsAndOperations();
    TestAgreementWithPlanarAnalyticalCircuit();
    TestValidationAndLifecycle();
    std::cout << "NAND3D CAM model tests passed\n";
}
