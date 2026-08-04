#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "CAM_DataBuffer.h"
#include "CAM_InputEncoder.h"
#include "CAM_LevelShifter.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestModelBuilders::MakeEvaCamConfig;
using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::StreamCapture;

void TestUninitializedComponentsRejectCalculations() {
    CAM_InputEncoder encoder;
    CAM_DataBuffer buffer;
    CAM_LevelShifter levelShifter;

    assert(!encoder.initialized);
    assert(!buffer.initialized);
    assert(!levelShifter.initialized);
    AssertThrows<std::runtime_error>([&] { encoder.CalculateArea(); }, "CAM_InputEncoder");
    AssertThrows<std::runtime_error>([&] { encoder.CalculateRC(); }, "CAM_InputEncoder");
    AssertThrows<std::runtime_error>([&] { encoder.CalculateLatency(1e-12); }, "CAM_InputEncoder");
    AssertThrows<std::runtime_error>([&] { encoder.CalculatePower(); }, "CAM_InputEncoder");
    AssertThrows<std::runtime_error>([&] { buffer.CalculateArea(); }, "CAM_DataBuffer");
    AssertThrows<std::runtime_error>([&] { buffer.CalculateRC(); }, "CAM_DataBuffer");
    AssertThrows<std::runtime_error>([&] { buffer.CalculateLatency(1e-12); }, "CAM_DataBuffer");
    AssertThrows<std::runtime_error>([&] { buffer.CalculatePower(); }, "CAM_DataBuffer");
    AssertThrows<std::runtime_error>([&] { levelShifter.CalculateArea(); }, "CAM_LevelShifter");
    AssertThrows<std::runtime_error>([&] { levelShifter.CalculateRC(); }, "CAM_LevelShifter");
    AssertThrows<std::runtime_error>([&] { levelShifter.CalculateLatency(1e-12); }, "CAM_LevelShifter");
    AssertThrows<std::runtime_error>([&] { levelShifter.CalculatePower(); }, "CAM_LevelShifter");
}

void TestInputEncoderStandardAndUnsupportedDesignPaths() {
    const auto config = MakeEvaCamConfig();
    CAM_InputEncoder encoder;
    encoder.Initialize(encoding_two_bit, false, 10e-15, 1e3, config);

    Require(encoder.initialized, "two-bit input encoder initializes");
    assert(encoder.typeEncoder == encoding_two_bit);
    assert(!encoder.isCustom);
    assert(encoder.numInputBits == 2);
    encoder.CalculateArea();
    AssertFinitePositive(encoder.area, "input encoder area");
    AssertFinitePositive(encoder.capNandIn, "input encoder NAND input capacitance");
    AssertFinitePositive(encoder.capNandOut, "input encoder NAND output capacitance");
    encoder.CalculateRC();
    encoder.CalculateLatency(2e-12);
    encoder.CalculatePower();
    AssertFiniteNonNegative(encoder.readLatency, "input encoder latency");
    AssertNear(encoder.writeLatency, encoder.readLatency);
    AssertFiniteNonNegative(encoder.rampOutput, "input encoder output ramp");
    AssertFiniteNonNegative(encoder.readDynamicEnergy, "input encoder energy");
    AssertNear(encoder.writeDynamicEnergy, encoder.readDynamicEnergy);
    AssertFiniteNonNegative(encoder.leakage, "input encoder leakage");

    const double firstArea = encoder.area;
    const double firstLatency = encoder.readLatency;
    const double firstEnergy = encoder.readDynamicEnergy;
    encoder.CalculateArea();
    encoder.CalculateLatency(2e-12);
    encoder.CalculatePower();
    AssertNear(encoder.area, firstArea);
    AssertNear(encoder.readLatency, firstLatency);
    AssertNear(encoder.readDynamicEnergy, firstEnergy);

    CAM_InputEncoder custom;
    StreamCapture customCapture(std::cout);
    custom.Initialize(encoding_two_bit, true, 10e-15, 1e3, config);
    customCapture.Stop();
    Require(!custom.initialized, "unsupported custom encoder must remain uninitialized");
    Require(customCapture.Text().find("under development") != std::string::npos,
            "custom encoder reports unsupported design path");
    AssertThrows<std::runtime_error>([&] { custom.CalculateArea(); }, "CAM_InputEncoder");

    CAM_InputEncoder unsupported;
    StreamCapture unsupportedCapture(std::cout);
    unsupported.Initialize(static_cast<TypeOfInputEncoder>(99), false, 10e-15, 1e3, config);
    unsupportedCapture.Stop();
    Require(!unsupported.initialized, "unsupported encoder type must remain uninitialized");
    Require(unsupportedCapture.Text().find("only scheme supported") != std::string::npos,
            "unsupported encoder type reports its limitation");
    AssertThrows<std::runtime_error>([&] { unsupported.CalculatePower(); }, "CAM_InputEncoder");
}

void TestDataBufferSingleAndDifferentialModes() {
    const auto config = MakeEvaCamConfig();
    CAM_DataBuffer single;
    CAM_DataBuffer differential;
    single.Initialize(false, 10e-15, 1e3, config);
    differential.Initialize(true, 10e-15, 1e3, config);

    Require(single.initialized && differential.initialized, "data buffers initialize");
    assert(!single.differential);
    assert(differential.differential);
    AssertFinitePositive(single.area, "single-ended buffer area");
    AssertFinitePositive(differential.area, "differential buffer area");
    Require(differential.area > single.area, "differential buffer consumes more area");
    for (CAM_DataBuffer *buffer : {&single, &differential}) {
        buffer->CalculateRC();
        buffer->CalculateLatency(2e-12);
        buffer->CalculatePower();
        AssertFinitePositive(buffer->capNandIn, "buffer NAND input capacitance");
        AssertFinitePositive(buffer->capNandOut, "buffer NAND output capacitance");
        AssertFiniteNonNegative(buffer->readLatency, "buffer latency");
        AssertNear(buffer->writeLatency, buffer->readLatency);
        AssertFiniteNonNegative(buffer->readDynamicEnergy, "buffer energy");
        AssertNear(buffer->writeDynamicEnergy, buffer->readDynamicEnergy);
        AssertFiniteNonNegative(buffer->leakage, "buffer leakage");
    }
    Require(differential.readDynamicEnergy > single.readDynamicEnergy,
            "differential buffer switches an additional driver");

    const double firstLatency = differential.readLatency;
    const double firstEnergy = differential.readDynamicEnergy;
    differential.CalculateLatency(2e-12);
    differential.CalculatePower();
    AssertNear(differential.readLatency, firstLatency);
    AssertNear(differential.readDynamicEnergy, firstEnergy);
}

void TestLevelShifterSizesAndZeroBitEdgeCase() {
    const auto config = MakeEvaCamConfig();
    CAM_LevelShifter oneBit;
    CAM_LevelShifter manyBits;
    CAM_LevelShifter zeroBits;
    oneBit.Initialize(1, 10e-15, 1e3, config);
    manyBits.Initialize(16, 10e-15, 1e3, config);
    zeroBits.Initialize(0, 10e-15, 1e3, config);

    Require(oneBit.initialized && manyBits.initialized && zeroBits.initialized,
            "level shifters initialize");
    AssertFinitePositive(oneBit.area, "one-bit level-shifter area");
    AssertFinitePositive(manyBits.area, "many-bit level-shifter area");
    Require(manyBits.area > oneBit.area, "level-shifter area scales with bit count");
    AssertNear(zeroBits.area, 0);

    for (CAM_LevelShifter *levelShifter : {&oneBit, &manyBits, &zeroBits}) {
        levelShifter->CalculateRC();
        levelShifter->CalculateLatency(2e-12);
        levelShifter->CalculatePower();
        AssertFinitePositive(levelShifter->capNandIn, "level-shifter NAND input capacitance");
        AssertFinitePositive(levelShifter->capNandOut, "level-shifter NAND output capacitance");
        AssertFiniteNonNegative(levelShifter->readLatency, "level-shifter latency");
        AssertNear(levelShifter->writeLatency, levelShifter->readLatency);
        AssertFiniteNonNegative(levelShifter->readDynamicEnergy, "level-shifter read energy");
        AssertFiniteNonNegative(levelShifter->writeDynamicEnergy, "level-shifter write energy");
        AssertNear(levelShifter->leakage, 0);
    }
    Require(manyBits.readDynamicEnergy > oneBit.readDynamicEnergy,
            "level-shifter read energy scales with bit count");
    Require(manyBits.writeDynamicEnergy > oneBit.writeDynamicEnergy,
            "level-shifter write energy scales with bit count");
    AssertNear(zeroBits.readDynamicEnergy, 0);
    AssertNear(zeroBits.writeDynamicEnergy, 0);

    CAM_LevelShifter invalid;
    AssertThrows<std::invalid_argument>([&] {
        invalid.Initialize(-1, 10e-15, 1e3, config);
    }, "number of input bits cannot be negative");

    const double firstArea = manyBits.area;
    const double firstEnergy = manyBits.readDynamicEnergy;
    manyBits.CalculateArea();
    manyBits.CalculatePower();
    AssertNear(manyBits.area, firstArea);
    AssertNear(manyBits.readDynamicEnergy, firstEnergy);
}

void TestInputPeripheralComponentsPrintProperties() {
    const auto config = MakeEvaCamConfig();
    CAM_InputEncoder encoder;
    CAM_DataBuffer buffer;
    CAM_LevelShifter levelShifter;
    encoder.Initialize(encoding_two_bit, false, 10e-15, 1e3, config);
    buffer.Initialize(false, 10e-15, 1e3, config);
    levelShifter.Initialize(2, 10e-15, 1e3, config);

    StreamCapture capture(std::cout);
    encoder.PrintProperty();
    buffer.PrintProperty();
    levelShifter.PrintProperty();
    capture.Stop();
    const std::string output = capture.Text();
    Require(output.find("CAM_InputEncoder Properties:") != std::string::npos,
            "input encoder property output identifies component");
    Require(output.find("CAM_DataBuffer Properties:") != std::string::npos,
            "data buffer property output identifies component");
    Require(output.find("CAM_LevelShifter Properties:") != std::string::npos,
            "level-shifter property output identifies component");
    Require(output.find("Area =") != std::string::npos,
            "property output includes function-unit measurements");
}

}  // namespace

int main() {
    TestUninitializedComponentsRejectCalculations();
    TestInputEncoderStandardAndUnsupportedDesignPaths();
    TestDataBufferSingleAndDifferentialModes();
    TestLevelShifterSizesAndZeroBitEdgeCase();
    TestInputPeripheralComponentsPrintProperties();
    std::cout << "CAM input peripheral component tests passed\n";
    return 0;
}
