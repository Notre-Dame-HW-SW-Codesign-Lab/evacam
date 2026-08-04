#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "Precharger.h"
#include "RowDecoder.h"
#include "SenseAmp.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestModelBuilders::MakeEvaCamConfig;
using TestModelBuilders::MakeWire;
using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::StreamCapture;

void TestUninitializedComponentsRejectCalculations() {
    Precharger precharger;
    AssertThrows<std::runtime_error>([&]() { precharger.CalculateArea(); }, "Precharger");
    AssertThrows<std::runtime_error>([&]() { precharger.CalculateRC(); }, "Precharger");
    AssertThrows<std::runtime_error>([&]() { precharger.CalculateLatency(1e-12); }, "Precharger");
    AssertThrows<std::runtime_error>([&]() { precharger.CalculatePower(); }, "Precharger");

    RowDecoder decoder;
    AssertThrows<std::runtime_error>([&]() { decoder.CalculateArea(); }, "Row Decoder");
    AssertThrows<std::runtime_error>([&]() { decoder.CalculateRC(); }, "Row Decoder");
    AssertThrows<std::runtime_error>([&]() { decoder.CalculateLatency(1e-12); }, "Row Decoder");
    AssertThrows<std::runtime_error>([&]() { decoder.CalculatePower(); }, "Row Decoder");

    SenseAmp senseAmp;
    AssertThrows<std::runtime_error>([&]() { senseAmp.CalculateArea(); }, "Sense Amp");
    AssertThrows<std::runtime_error>([&]() { senseAmp.CalculateRC(); }, "Sense Amp");
    AssertThrows<std::runtime_error>([&]() { senseAmp.CalculateLatency(); }, "Sense Amp");
    AssertThrows<std::runtime_error>([&]() { senseAmp.CalculatePower(); }, "Sense Amp");
}

void TestPrechargerCalculatesGeometryLatencyAndPower() {
    const auto config = MakeEvaCamConfig();
    const Wire wire = MakeWire(config);
    Precharger precharger;
    precharger.Initialize(0.9, 64, 2e-15, 500, config, wire);

    Require(precharger.initialized, "precharger initializes");
    assert(precharger.numColumn == 64);
    AssertFinitePositive(precharger.area, "precharger area");
    AssertFinitePositive(precharger.width, "precharger width");
    AssertFinitePositive(precharger.height, "precharger height");

    precharger.CalculateLatency(2e-12);
    precharger.CalculatePower();
    AssertFiniteNonNegative(precharger.enableLatency, "precharger enable latency");
    AssertFiniteNonNegative(precharger.readLatency, "precharger read latency");
    AssertNear(precharger.writeLatency, precharger.readLatency);
    AssertFiniteNonNegative(precharger.readDynamicEnergy, "precharger read energy");
    AssertNear(precharger.writeDynamicEnergy, 0);
    AssertFiniteNonNegative(precharger.leakage, "precharger leakage");

    const double firstArea = precharger.area;
    const double firstEnergy = precharger.readDynamicEnergy;
    precharger.CalculateArea();
    precharger.CalculatePower();
    AssertNear(precharger.area, firstArea);
    AssertNear(precharger.readDynamicEnergy, firstEnergy);

    StreamCapture capture(std::cout);
    precharger.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("Precharger Properties") != std::string::npos,
            "precharger property output identifies component");
}

void TestPrechargerScalesWithColumnCount() {
    const auto config = MakeEvaCamConfig();
    const Wire wire = MakeWire(config);
    Precharger oneColumn;
    Precharger manyColumns;
    oneColumn.Initialize(0.9, 1, 2e-15, 500, config, wire);
    manyColumns.Initialize(0.9, 128, 2e-15, 500, config, wire);
    oneColumn.CalculatePower();
    manyColumns.CalculatePower();
    Require(manyColumns.area > oneColumn.area, "precharger area scales with columns");
    Require(manyColumns.readDynamicEnergy > oneColumn.readDynamicEnergy,
            "precharger dynamic energy scales with columns");
}

void TestRowDecoderTopologiesAndInputValidation() {
    const auto config = MakeEvaCamConfig();
    for (int nandInputs : {0, 2, 3}) {
        RowDecoder decoder;
        decoder.Initialize(64, 2e-15, 500, nandInputs, latency_first, 0, config);
        Require(decoder.initialized && !decoder.invalid, "row decoder initializes");
        assert(decoder.numNandInput == nandInputs);
        AssertFinitePositive(decoder.area, "row decoder area");
        decoder.CalculateLatency(2e-12);
        decoder.CalculatePower();
        AssertFiniteNonNegative(decoder.readLatency, "row decoder latency");
        AssertNear(decoder.writeLatency, decoder.readLatency);
        AssertFiniteNonNegative(decoder.readDynamicEnergy, "row decoder energy");
        AssertNear(decoder.writeDynamicEnergy, decoder.readDynamicEnergy);
        AssertFiniteNonNegative(decoder.leakage, "row decoder leakage");
        if (nandInputs == 0) {
            AssertNear(decoder.capNandInput, 0);
            AssertNear(decoder.capNandOutput, 0);
        } else {
            AssertFinitePositive(decoder.capNandInput, "NAND input capacitance");
            AssertFinitePositive(decoder.capNandOutput, "NAND output capacitance");
        }
    }

    RowDecoder invalid;
    AssertThrows<std::invalid_argument>([&]() {
        invalid.Initialize(64, 2e-15, 500, 1, latency_first, 0, config);
    }, "NAND input count");
}

void TestRowDecoderScalesAndPrints() {
    const auto config = MakeEvaCamConfig();
    RowDecoder small;
    RowDecoder large;
    small.Initialize(1, 2e-15, 500, 2, latency_first, 0, config, false);
    large.Initialize(128, 2e-15, 500, 2, latency_first, 0, config, false);
    small.CalculatePower();
    large.CalculatePower();
    Require(large.area > small.area, "row decoder area scales with rows");
    Require(large.leakage > small.leakage, "row decoder leakage scales with rows");

    StreamCapture capture(std::cout);
    large.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("Row Decoder Properties") != std::string::npos,
            "row decoder property output identifies component");
}

void TestSenseAmpVoltageAndCurrentModes() {
    const auto config = MakeEvaCamConfig();
    const double pitch = 20 * config->technology.tech->featureSize();
    SenseAmp voltage;
    SenseAmp current;
    voltage.Initialize(64, false, 0.05, pitch, config);
    current.Initialize(64, true, 0.05, pitch, config);
    Require(voltage.initialized && current.initialized, "sense amplifiers initialize");
    assert(!voltage.invalid && !current.invalid);
    voltage.CalculateLatency();
    current.CalculateLatency();
    voltage.CalculatePower();
    current.CalculatePower();
    AssertFinitePositive(voltage.area, "voltage sense area");
    AssertFinitePositive(current.area, "current sense area");
    AssertFinitePositive(voltage.capLoad, "voltage sense capacitance");
    AssertFinitePositive(voltage.readLatency, "voltage sense latency");
    AssertFinitePositive(current.readLatency, "current sense latency");
    AssertNear(voltage.writeLatency, 0);
    AssertNear(current.writeLatency, 0);
    Require(current.area > voltage.area, "current sense includes IV converter area");
    Require(current.readDynamicEnergy > voltage.readDynamicEnergy,
            "current sense includes converter energy");
    Require(current.leakage > voltage.leakage, "current sense includes converter leakage");

    const double firstEnergy = current.readDynamicEnergy;
    current.CalculatePower();
    AssertNear(current.readDynamicEnergy, firstEnergy);

    StreamCapture capture(std::cout);
    current.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("Sense Amplifier Properties") != std::string::npos,
            "sense amp property output identifies component");
}

void TestSenseAmpRejectsInsufficientPitch() {
    const auto config = MakeEvaCamConfig();
    SenseAmp senseAmp;
    senseAmp.Initialize(8, false, 0.05, config->technology.tech->featureSize(), config);
    Require(senseAmp.initialized && senseAmp.invalid, "insufficient pitch invalidates sense amp");
    AssertNear(senseAmp.area, 1e41);
    AssertNear(senseAmp.capLoad, 1e41);
    senseAmp.CalculatePower();
    AssertNear(senseAmp.readDynamicEnergy, 1e41);
    AssertNear(senseAmp.writeDynamicEnergy, 1e41);
    AssertNear(senseAmp.leakage, 1e41);
}

}  // namespace

int main() {
    TestUninitializedComponentsRejectCalculations();
    TestPrechargerCalculatesGeometryLatencyAndPower();
    TestPrechargerScalesWithColumnCount();
    TestRowDecoderTopologiesAndInputValidation();
    TestRowDecoderScalesAndPrints();
    TestSenseAmpVoltageAndCurrentModes();
    TestSenseAmpRejectsInsufficientPitch();
    std::cout << "Charging and sensing component tests passed" << std::endl;
    return 0;
}
