#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "BasicDecoder.h"
#include "PredecodeBlock.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::StreamCapture;

void TestUninitializedDecodersRejectCalculations() {
    BasicDecoder basic;
    PredecodeBlock predecode;

    AssertThrows<std::runtime_error>([&] { basic.CalculateArea(); }, "Basic Decoder");
    AssertThrows<std::runtime_error>([&] { basic.CalculateRC(); }, "Basic Decoder");
    AssertThrows<std::runtime_error>([&] { basic.CalculateLatency(0); }, "Basic Decoder");
    AssertThrows<std::runtime_error>([&] { basic.CalculatePower(); }, "Basic Decoder");
    AssertThrows<std::runtime_error>([&] { predecode.CalculateArea(); }, "Predecoder Block");
    AssertThrows<std::runtime_error>([&] { predecode.CalculateRC(); }, "Predecoder Block");
    AssertThrows<std::runtime_error>([&] { predecode.CalculateLatency(0); }, "Predecoder Block");
    AssertThrows<std::runtime_error>([&] { predecode.CalculatePower(); }, "Predecoder Block");
}

void VerifyBasicDecoder(BasicDecoder &decoder, int addressBits) {
    Require(decoder.initialized, "basic decoder was not initialized");
    Require(decoder.numNandInput == (addressBits == 1 ? 0 : addressBits),
            "unexpected NAND input count");
    AssertFinitePositive(decoder.area, "basic decoder area");
    AssertFinitePositive(decoder.height, "basic decoder height");
    AssertFinitePositive(decoder.width, "basic decoder width");

    decoder.CalculateRC();
    decoder.CalculateLatency(1e-12);
    AssertFiniteNonNegative(decoder.readLatency, "basic decoder latency");
    AssertNear(decoder.writeLatency, decoder.readLatency);
    AssertFiniteNonNegative(decoder.rampOutput, "basic decoder output ramp");

    decoder.CalculatePower();
    AssertFiniteNonNegative(decoder.leakage, "basic decoder leakage");
    AssertFiniteNonNegative(decoder.readDynamicEnergy, "basic decoder read energy");
    AssertNear(decoder.writeDynamicEnergy, decoder.readDynamicEnergy);

    const double latency = decoder.readLatency;
    const double energy = decoder.readDynamicEnergy;
    decoder.CalculateLatency(1e-12);
    decoder.CalculatePower();
    AssertNear(decoder.readLatency, latency);
    AssertNear(decoder.readDynamicEnergy, energy);
}

void TestBasicDecoderTopologiesAndLoads() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    BasicDecoder inverter;
    inverter.Initialize(1, 2e-15, 500, config);
    VerifyBasicDecoder(inverter, 1);
    Require(inverter.numNandGate == 0, "one-bit decoder should use only an inverter");

    BasicDecoder nand2;
    nand2.Initialize(2, 2e-15, 500, config);
    VerifyBasicDecoder(nand2, 2);
    Require(nand2.numNandGate == 4, "two-bit decoder should use four NAND gates");
    AssertFinitePositive(nand2.capNandInput, "NAND2 input capacitance");
    AssertFinitePositive(nand2.capNandOutput, "NAND2 output capacitance");

    BasicDecoder nand3;
    nand3.Initialize(3, 2e-15, 500, config);
    VerifyBasicDecoder(nand3, 3);
    Require(nand3.numNandGate == 8, "three-bit decoder should use eight NAND gates");

    BasicDecoder largerLoad;
    largerLoad.Initialize(3, 20e-15, 500, config);
    largerLoad.CalculateLatency(1e-12);
    largerLoad.CalculatePower();
    Require(largerLoad.readLatency >= nand3.readLatency,
            "larger capacitive load should not reduce decoder latency");
    Require(largerLoad.readDynamicEnergy >= nand3.readDynamicEnergy,
            "larger capacitive load should not reduce decoder energy");
}

void TestBasicDecoderRejectsUnsupportedAddressCounts() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    for (int addressBits : {0, 4}) {
        BasicDecoder decoder;
        AssertThrows<std::invalid_argument>(
                [&] { decoder.Initialize(addressBits, 2e-15, 500, config); },
                "address bit count must be 1, 2, or 3");
    }
}

void TestBasicDecoderPrintsIdentity() {
    BasicDecoder decoder;
    decoder.Initialize(2, 2e-15, 500, TestModelBuilders::MakeEvaCamConfig());
    decoder.CalculateLatency(1e-12);
    decoder.CalculatePower();
    StreamCapture capture(std::cout);
    decoder.PrintProperty();
    capture.Stop();
    const std::string output = capture.Text();
    Require(output.find("2 to 4 Decoder Properties:") != std::string::npos,
            "basic decoder identity missing from printed output");
    Require(output.find("Area =") != std::string::npos,
            "basic decoder area missing from printed output");
}

void VerifyPredecodeBlock(PredecodeBlock &block) {
    Require(block.initialized, "predecode block was not initialized");
    block.CalculateArea();
    block.CalculateRC();
    block.CalculateLatency(1e-12);
    block.CalculatePower();
    AssertFinitePositive(block.area, "predecode block area");
    AssertFiniteNonNegative(block.readLatency, "predecode block latency");
    AssertFiniteNonNegative(block.readDynamicEnergy, "predecode block read energy");
    AssertFiniteNonNegative(block.leakage, "predecode block leakage");
    AssertNear(block.writeLatency, block.readLatency);
    AssertNear(block.writeDynamicEnergy, block.readDynamicEnergy);

    const double latency = block.readLatency;
    const double energy = block.readDynamicEnergy;
    block.CalculateLatency(1e-12);
    block.CalculatePower();
    AssertNear(block.readLatency, latency);
    AssertNear(block.readDynamicEnergy, energy);
}

void TestPredecodeBlockTopologies() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    PredecodeBlock oneBit;
    oneBit.Initialize(1, 2e-15, 500, config);
    VerifyPredecodeBlock(oneBit);
    Require(oneBit.numDecoder12 == 1, "one-bit predecode should use a 1-to-2 decoder");
    Require(oneBit.rowDecoderStage1A == nullptr, "one-bit predecode should not need a row-decoder stage");

    PredecodeBlock medium;
    medium.Initialize(14, 2e-15, 500, config);
    VerifyPredecodeBlock(medium);
    Require(medium.rowDecoderStage1A != nullptr, "medium predecode missing stage 1A");
    Require(medium.rowDecoderStage1B != nullptr, "medium predecode missing stage 1B");
    Require(medium.rowDecoderStage2 != nullptr, "medium predecode missing stage 2");
    Require(medium.basicDecoderA1 != nullptr, "medium predecode missing basic decoder A1");
    Require(medium.basicDecoderB != nullptr, "medium predecode missing basic decoder B");
}

void TestPredecodeBlockZeroAndInvalidAddressCounts() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    PredecodeBlock zero;
    zero.Initialize(0, 2e-15, 500, config);
    zero.CalculateArea();
    zero.CalculateRC();
    zero.CalculateLatency(3e-12);
    zero.CalculatePower();
    Require(zero.initialized, "zero-bit predecode was not initialized");
    AssertNear(zero.area, 0);
    AssertNear(zero.readLatency, 0);
    AssertNear(zero.writeLatency, 0);
    AssertNear(zero.rampOutput, 3e-12);
    AssertNear(zero.readDynamicEnergy, 0);
    AssertNear(zero.writeDynamicEnergy, 0);
    AssertNear(zero.leakage, 0);

    PredecodeBlock invalid;
    AssertThrows<std::runtime_error>([&] { invalid.Initialize(28, 2e-15, 500, config); },
            "invalid number of address bits");
    AssertThrows<std::runtime_error>([&] { invalid.Initialize(-1, 2e-15, 500, config); },
            "invalid number of address bits");
}

void TestPredecodeBlockPrintsIdentity() {
    PredecodeBlock block;
    block.Initialize(2, 2e-15, 500, TestModelBuilders::MakeEvaCamConfig());
    block.CalculateArea();
    block.CalculateLatency(1e-12);
    block.CalculatePower();
    StreamCapture capture(std::cout);
    block.PrintProperty();
    capture.Stop();
    const std::string output = capture.Text();
    Require(output.find("Predecoding Block Properties:") != std::string::npos,
            "predecode identity missing from printed output");
    Require(output.find("Area =") != std::string::npos,
            "predecode area missing from printed output");
}

}  // namespace

int main() {
    TestUninitializedDecodersRejectCalculations();
    TestBasicDecoderTopologiesAndLoads();
    TestBasicDecoderRejectsUnsupportedAddressCounts();
    TestBasicDecoderPrintsIdentity();
    TestPredecodeBlockTopologies();
    TestPredecodeBlockZeroAndInvalidAddressCounts();
    TestPredecodeBlockPrintsIdentity();
    std::cout << "Decoder component tests passed\n";
    return 0;
}
