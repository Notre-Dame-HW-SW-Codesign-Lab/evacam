#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "CAM_BasicMMR.h"
#include "CAM_MMR.h"
#include "CAM_OutputAccumulator.h"
#include "CAM_SenseAmp.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::StreamCapture;
using TestSupport::TemporaryDirectory;

void TestUninitializedCamComponentsRejectCalculations() {
    CAM_BasicMMR basic;
    CAM_MMR mmr;
    CAM_OutputAccumulator accumulator;
    CAM_SenseAmp senseAmp;

    AssertThrows<std::runtime_error>([&] { basic.CalculateArea(); }, "CAM_BasicMMR");
    AssertThrows<std::runtime_error>([&] { basic.CalculateRC(); }, "CAM_BasicMMR");
    AssertThrows<std::runtime_error>([&] { basic.CalculateLatency(0); }, "CAM_BasicMMR");
    AssertThrows<std::runtime_error>([&] { basic.CalculatePower(); }, "CAM_BasicMMR");
    AssertThrows<std::runtime_error>([&] { mmr.CalculateArea(); }, "CAM_MMR");
    AssertThrows<std::runtime_error>([&] { mmr.CalculateRC(); }, "CAM_MMR");
    AssertThrows<std::runtime_error>([&] { mmr.CalculateLatency(0); }, "CAM_MMR");
    AssertThrows<std::runtime_error>([&] { mmr.CalculatePower(); }, "CAM_MMR");
    AssertThrows<std::runtime_error>([&] { accumulator.CalculateArea(); }, "CAM_OutputAccumulator");
    AssertThrows<std::runtime_error>([&] { accumulator.CalculateRC(); }, "CAM_OutputAccumulator");
    AssertThrows<std::runtime_error>([&] { accumulator.CalculateLatency(0); }, "CAM_OutputAccumulator");
    AssertThrows<std::runtime_error>([&] { accumulator.CalculatePower(); }, "CAM_OutputAccumulator");
    AssertThrows<std::runtime_error>([&] { senseAmp.CalculateArea(); }, "CAM_SenseAmp");
    AssertThrows<std::runtime_error>([&] { senseAmp.CalculateRC(); }, "CAM_SenseAmp");
    AssertThrows<std::runtime_error>([&] { senseAmp.CalculateLatency(); }, "CAM_SenseAmp");
    AssertThrows<std::runtime_error>([&] { senseAmp.CalculatePower(); }, "CAM_SenseAmp");
}

void VerifyRepeatedCalculations(CAM_BasicMMR &component) {
    component.CalculateArea();
    component.CalculateRC();
    component.CalculateLatency(1e-12);
    component.CalculatePower();
    AssertFinitePositive(component.area, "basic MMR area");
    AssertFinitePositive(component.height, "basic MMR height");
    AssertFinitePositive(component.width, "basic MMR width");
    AssertFinitePositive(component.capIn, "basic MMR input capacitance");
    AssertFiniteNonNegative(component.readLatency, "basic MMR latency");
    AssertFiniteNonNegative(component.LookAheadLatency, "basic MMR look-ahead latency");
    AssertFiniteNonNegative(component.readDynamicEnergy, "basic MMR energy");
    AssertFiniteNonNegative(component.leakage, "basic MMR leakage");
    AssertNear(component.writeLatency, component.readLatency);
    AssertNear(component.writeDynamicEnergy, component.readDynamicEnergy);
    const double area = component.area;
    const double latency = component.readLatency;
    const double energy = component.readDynamicEnergy;
    component.CalculateArea();
    component.CalculateLatency(1e-12);
    component.CalculatePower();
    AssertNear(component.area, area);
    AssertNear(component.readLatency, latency);
    AssertNear(component.readDynamicEnergy, energy);
}

void TestBasicMmrSupportedAndUnsupportedInputCounts() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    CAM_BasicMMR basic;
    basic.Initialize(8, 10e-15, 1e3, 5e-15, 500, config);
    Require(basic.initialized, "basic MMR was not initialized");
    Require(basic.numInputBits == 8, "basic MMR must retain its supported input count");
    VerifyRepeatedCalculations(basic);

    CAM_BasicMMR largerLoad;
    largerLoad.Initialize(8, 100e-15, 1e3, 50e-15, 500, config);
    largerLoad.CalculateLatency(1e-12);
    largerLoad.CalculatePower();
    Require(largerLoad.readLatency >= basic.readLatency,
            "larger basic-MMR load should not reduce latency");
    Require(largerLoad.readDynamicEnergy >= basic.readDynamicEnergy,
            "larger basic-MMR load should not reduce energy");

    CAM_BasicMMR unsupported;
    AssertThrows<std::runtime_error>([&] {
        unsupported.Initialize(4, 10e-15, 1e3, 5e-15, 500, config);
    }, "only 8-input MMR blocks are supported");
}

void TestBasicMmrPrintsIdentity() {
    CAM_BasicMMR basic;
    basic.Initialize(8, 10e-15, 1e3, 5e-15, 500, TestModelBuilders::MakeEvaCamConfig());
    basic.CalculateLatency(1e-12);
    basic.CalculatePower();
    StreamCapture capture(std::cout);
    basic.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("CAM_MMR Properties:") != std::string::npos,
            "basic MMR property output lacks identity");
    Require(capture.Text().find("Area =") != std::string::npos,
            "basic MMR property output lacks area");
}

void VerifyMmr(CAM_MMR &mmr) {
    Require(mmr.initialized, "MMR was not initialized");
    Require(mmr.numBasicMMR > 0, "MMR needs at least one basic block");
    mmr.CalculateArea();
    mmr.CalculateRC();
    mmr.CalculateLatency(1e-12);
    mmr.CalculatePower();
    AssertFinitePositive(mmr.area, "MMR area");
    AssertFinitePositive(mmr.height, "MMR height");
    AssertFinitePositive(mmr.width, "MMR width");
    AssertFiniteNonNegative(mmr.readLatency, "MMR latency");
    AssertFiniteNonNegative(mmr.readDynamicEnergy, "MMR energy");
    AssertFiniteNonNegative(mmr.leakage, "MMR leakage");
    AssertNear(mmr.writeLatency, mmr.readLatency);
    AssertNear(mmr.writeDynamicEnergy, mmr.readDynamicEnergy);
    const double latency = mmr.readLatency;
    const double energy = mmr.readDynamicEnergy;
    mmr.CalculateLatency(1e-12);
    mmr.CalculatePower();
    AssertNear(mmr.readLatency, latency);
    AssertNear(mmr.readDynamicEnergy, energy);
}

void TestMmrInputSizesAndOptimizationModes() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    for (BufferDesignTarget target : {latency_first, latency_area_trade_off, area_first}) {
        CAM_MMR mmr;
        mmr.Initialize(64, target, 10e-15, 1e3, config);
        VerifyMmr(mmr);
        Require(mmr.areaOptimizationLevel == target, "MMR did not retain optimization mode");
    }

    CAM_MMR rounded;
    rounded.Initialize(9, latency_first, 10e-15, 1e3, config);
    Require(rounded.initialized, "rounded MMR was not initialized");
    AssertFinitePositive(rounded.area, "rounded MMR area");
    Require(rounded.numBasicMMR == 2, "nine-input MMR must round up to two basic blocks");
    rounded.CalculateLatency(1e-12);
    AssertFiniteNonNegative(rounded.readLatency, "rounded MMR latency");

    CAM_MMR invalid;
    AssertThrows<std::invalid_argument>([&] {
        invalid.Initialize(0, latency_first, 10e-15, 1e3, config);
    }, "number of input bits must be positive");
}

void TestMmrPrintsIdentity() {
    CAM_MMR mmr;
    mmr.Initialize(64, area_first, 10e-15, 1e3, TestModelBuilders::MakeEvaCamConfig());
    mmr.CalculateLatency(1e-12);
    mmr.CalculatePower();
    StreamCapture capture(std::cout);
    mmr.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("CAM_MMR Properties:") != std::string::npos,
            "MMR property output lacks identity");
    Require(capture.Text().find("Area =") != std::string::npos,
            "MMR property output lacks area");
}

void TestOutputAccumulatorCalculationsAndLoadScaling() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    CAM_OutputAccumulator accumulator;
    accumulator.Initialize(10e-15, 1e3, config);
    Require(accumulator.initialized, "output accumulator was not initialized");
    accumulator.CalculateArea();
    accumulator.CalculateRC();
    accumulator.CalculateLatency(1e-12);
    accumulator.CalculatePower();
    AssertFinitePositive(accumulator.area, "output accumulator area");
    AssertFinitePositive(accumulator.capNandIn, "output accumulator input capacitance");
    AssertFinitePositive(accumulator.capNandOut, "output accumulator output capacitance");
    AssertFiniteNonNegative(accumulator.readLatency, "output accumulator latency");
    AssertFiniteNonNegative(accumulator.readDynamicEnergy, "output accumulator energy");
    AssertFiniteNonNegative(accumulator.leakage, "output accumulator leakage");
    AssertNear(accumulator.writeLatency, accumulator.readLatency);
    AssertNear(accumulator.writeDynamicEnergy, accumulator.readDynamicEnergy);
    const double area = accumulator.area;
    const double latency = accumulator.readLatency;
    const double energy = accumulator.readDynamicEnergy;
    accumulator.CalculateArea();
    accumulator.CalculateLatency(1e-12);
    accumulator.CalculatePower();
    AssertNear(accumulator.area, area);
    AssertNear(accumulator.readLatency, latency);
    AssertNear(accumulator.readDynamicEnergy, energy);

    CAM_OutputAccumulator largerLoad;
    largerLoad.Initialize(100e-15, 1e3, config);
    largerLoad.CalculateLatency(1e-12);
    largerLoad.CalculatePower();
    Require(largerLoad.readLatency >= accumulator.readLatency,
            "larger accumulator load should not reduce latency");
    Require(largerLoad.readDynamicEnergy >= accumulator.readDynamicEnergy,
            "larger accumulator load should not reduce energy");

    StreamCapture capture(std::cout);
    accumulator.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("CAM_OutputAccumulator Properties:") != std::string::npos,
            "output accumulator property output lacks identity");
}

void VerifySupportedCamSenseAmp(CAM_SenseAmp &senseAmp) {
    Require(senseAmp.initialized && !senseAmp.invalid, "CAM sense amp must initialize validly");
    senseAmp.CalculateArea();
    senseAmp.CalculateRC();
    senseAmp.CalculateLatency();
    senseAmp.CalculatePower();
    AssertFinitePositive(senseAmp.area, "CAM sense-amp area");
    AssertFinitePositive(senseAmp.capLoad, "CAM sense-amp load capacitance");
    AssertFiniteNonNegative(senseAmp.readLatency, "CAM sense-amp latency");
    AssertFiniteNonNegative(senseAmp.readDynamicEnergy, "CAM sense-amp energy");
    AssertFiniteNonNegative(senseAmp.leakage, "CAM sense-amp leakage");
    const double energy = senseAmp.readDynamicEnergy;
    senseAmp.CalculatePower();
    AssertNear(senseAmp.readDynamicEnergy, energy);
}

void TestCamSenseAmpSupportedModesAndInvalidPitch() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    const double pitch = 20 * config->technology.tech->featureSize();
    for (TypeOfSenseAmp type : {nvsim_voltage_sense, nvsim_current_sense, discharge}) {
        CAM_SenseAmp senseAmp;
        senseAmp.Initialize(64, type, false, 0.05, pitch, "", config);
        VerifySupportedCamSenseAmp(senseAmp);
        senseAmp.CalculateLatency(0.01);
        AssertFinitePositive(senseAmp.readLatency,
                "CAM sense-amp observed-signal latency");
    }

    CAM_SenseAmp zeroThreshold;
    zeroThreshold.Initialize(64, nvsim_voltage_sense, false, 0, pitch, "", config);
    zeroThreshold.CalculateLatency(0.01);
    AssertFinitePositive(zeroThreshold.readLatency,
            "zero threshold with positive observed signal");

    CAM_SenseAmp tooNarrow;
    tooNarrow.Initialize(8, nvsim_voltage_sense, false, 0.05,
            config->technology.tech->featureSize(), "", config);
    Require(tooNarrow.invalid, "too-narrow CAM sense amp should be invalid");
    AssertNear(tooNarrow.area, 1e41);
    AssertNear(tooNarrow.capLoad, 1e41);
    tooNarrow.CalculatePower();
    AssertNear(tooNarrow.readDynamicEnergy, 1e41);

    CAM_SenseAmp unsupported;
    AssertThrows<std::runtime_error>([&] {
        unsupported.Initialize(8, static_cast<TypeOfSenseAmp>(99), false, 0.05, pitch, "", config);
    }, "sensing type is not supported");
}

void TestCamSenseAmpCustomAndUnderDevelopmentModes() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    const double pitch = 20 * config->technology.tech->featureSize();
    TemporaryDirectory directory("evacam-cam-sense-amp");
    const auto customFile = directory.WriteFile("custom.yaml",
            "schema: sense_amp\nname: custom\nmodel: scalar\n"
            "geometry: {height: 10F, width: 4F}\n"
            "timing: {latency: 20ps}\n"
            "power: {read_dynamic_energy: 4pJ, leakage: 10pW}\n"
            "load: {capacitance: 2fF}\n");
    CAM_SenseAmp custom;
    custom.Initialize(16, nvsim_voltage_sense, true, 0.05, pitch, customFile.string(), config);
    VerifySupportedCamSenseAmp(custom);
    AssertNear(custom.readLatency, 20e-12);
    AssertNear(custom.readDynamicEnergy, 16 * 4e-12);
    AssertNear(custom.leakage, 16 * 10e-12);

    for (TypeOfSenseAmp type : {self_clock_sense, dual_threshold_sense}) {
        CAM_SenseAmp pending;
        pending.Initialize(8, type, false, 0.05, pitch, "", config);
        Require(pending.initialized && !pending.invalid,
                "under-development sense amp should retain a valid object state");
        AssertNear(pending.area, 1e41);
        AssertNear(pending.capLoad, 1e41);
        pending.CalculateLatency();
        pending.CalculatePower();
        AssertNear(pending.readLatency, 0);
        AssertNear(pending.readDynamicEnergy, 0);
        AssertNear(pending.leakage, 0);
    }
}

void TestCamSenseAmpPrintsIdentity() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    CAM_SenseAmp senseAmp;
    senseAmp.Initialize(8, nvsim_voltage_sense, false, 0.05,
            20 * config->technology.tech->featureSize(), "", config);
    senseAmp.CalculateLatency();
    senseAmp.CalculatePower();
    StreamCapture capture(std::cout);
    senseAmp.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("Sense Amplifier Properties:") != std::string::npos,
            "CAM sense amp property output lacks identity");
    Require(capture.Text().find("Area =") != std::string::npos,
            "CAM sense amp property output lacks area");
}

}  // namespace

int main() {
    TestUninitializedCamComponentsRejectCalculations();
    TestBasicMmrSupportedAndUnsupportedInputCounts();
    TestBasicMmrPrintsIdentity();
    TestMmrInputSizesAndOptimizationModes();
    TestMmrPrintsIdentity();
    TestOutputAccumulatorCalculationsAndLoadScaling();
    TestCamSenseAmpSupportedModesAndInvalidPitch();
    TestCamSenseAmpCustomAndUnderDevelopmentModes();
    TestCamSenseAmpPrintsIdentity();
    std::cout << "CAM MMR and sense component tests passed\n";
    return 0;
}
