#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "CAM_BasicEncoder.h"
#include "CAM_Encoder.h"
#include "CAM_PriorityEncoder.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::StreamCapture;

std::shared_ptr<EvaCamConfig> MakeComponentConfig() {
    auto config = TestModelBuilders::MakeEvaCamConfig();
    config->input.maxNmosSize = 1000;
    config->input.temperature = 300;
    return config;
}

void TestEncodersRequireInitialization() {
    CAM_BasicEncoder basic;
    CAM_Encoder encoder;
    CAM_PriorityEncoder priority;

    Require(!basic.initialized, "basic encoder should start uninitialized");
    Require(!encoder.initialized, "encoder should start uninitialized");
    Require(!priority.initialized, "priority encoder should start uninitialized");

    AssertThrows<std::runtime_error>([&] { basic.CalculateArea(); }, "CAM_BasicEncoder");
    AssertThrows<std::runtime_error>([&] { basic.CalculateRC(); }, "CAM_BasicEncoder");
    AssertThrows<std::runtime_error>([&] { basic.CalculateLatency(1e-12); }, "CAM_BasicEncoder");
    AssertThrows<std::runtime_error>([&] { basic.CalculatePower(); }, "CAM_BasicEncoder");
    AssertThrows<std::runtime_error>([&] { encoder.CalculateArea(); }, "CAM_Encoder");
    AssertThrows<std::runtime_error>([&] { encoder.CalculateRC(); }, "CAM_Encoder");
    AssertThrows<std::runtime_error>([&] { encoder.CalculateLatency(1e-12); }, "CAM_Encoder");
    AssertThrows<std::runtime_error>([&] { encoder.CalculatePower(); }, "CAM_Encoder");
    AssertThrows<std::runtime_error>([&] { priority.CalculateArea(); }, "CAM_PriorityEncoder");
    AssertThrows<std::runtime_error>([&] { priority.CalculateRC(); }, "CAM_PriorityEncoder");
    AssertThrows<std::runtime_error>([&] { priority.CalculateLatency(1e-12); }, "CAM_PriorityEncoder");
    AssertThrows<std::runtime_error>([&] { priority.CalculatePower(); }, "CAM_PriorityEncoder");
}

void VerifyBasicEncoder(CAM_BasicEncoder &encoder) {
    Require(encoder.initialized, "basic encoder was not initialized");
    Require(encoder.numInputBit == 8, "basic encoder must model an 8-to-3 block");
    Require(encoder.numNorInput == 0, "basic encoder constructor state changed unexpectedly");
    AssertFinitePositive(encoder.area, "basic encoder area");
    AssertFinitePositive(encoder.width, "basic encoder width");
    AssertFinitePositive(encoder.height, "basic encoder height");

    encoder.CalculateArea();
    encoder.CalculateRC();
    AssertFinitePositive(encoder.capNorInput, "basic encoder NOR input capacitance");
    AssertFinitePositive(encoder.capNandOutput, "basic encoder NAND output capacitance");
    AssertFinitePositive(encoder.capInvInput, "basic encoder inverter input capacitance");

    encoder.CalculateLatency(1e-12);
    encoder.CalculatePower();
    AssertFiniteNonNegative(encoder.readLatency, "basic encoder latency");
    AssertNear(encoder.writeLatency, encoder.readLatency);
    AssertFiniteNonNegative(encoder.rampOutput, "basic encoder output ramp");
    AssertFiniteNonNegative(encoder.leakage, "basic encoder leakage");
    AssertFinitePositive(encoder.readDynamicEnergy, "basic encoder read energy");
    AssertNear(encoder.writeDynamicEnergy, encoder.readDynamicEnergy);

    const double latency = encoder.readLatency;
    const double energy = encoder.readDynamicEnergy;
    encoder.CalculateLatency(1e-12);
    encoder.CalculatePower();
    AssertNear(encoder.readLatency, latency);
    AssertNear(encoder.readDynamicEnergy, energy);
}

void TestBasicEncoderSupportedAndUnsupportedInputs() {
    CAM_BasicEncoder encoder;
    encoder.Initialize(8, 10e-15, 1e3, MakeComponentConfig());
    VerifyBasicEncoder(encoder);

    CAM_BasicEncoder unsupported;
    AssertThrows<std::runtime_error>(
            [&] { unsupported.Initialize(4, 10e-15, 1e3, MakeComponentConfig()); },
            "only 8-to-3 encoder blocks are supported");
    Require(!unsupported.initialized, "unsupported basic encoder must remain uninitialized");
}

void TestBasicEncoderPrintsIdentity() {
    CAM_BasicEncoder encoder;
    encoder.Initialize(8, 10e-15, 1e3, MakeComponentConfig());
    StreamCapture capture(std::cout);
    encoder.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("8 to 3 CAM_BasicEncoder Properties:") != std::string::npos,
            "basic encoder printout should identify the component");
    Require(capture.Text().find("Area =") != std::string::npos,
            "basic encoder printout should include function-unit properties");
}

void VerifyEncoder(CAM_Encoder &encoder, int inputBits, BufferDesignTarget target) {
    Require(encoder.initialized, "encoder was not initialized");
    Require(encoder.numInputBit == inputBits, "encoder input count was not preserved");
    Require(encoder.areaOptimizationLevel == target, "encoder optimization target was not preserved");
    Require(encoder.numBasicEncoder > 0, "encoder must contain basic encoders");
    Require(encoder.numStage > 0, "encoder must contain an encoder stage");
    AssertFinitePositive(encoder.area, "encoder area");
    AssertFinitePositive(encoder.width, "encoder width");
    AssertFinitePositive(encoder.height, "encoder height");

    encoder.CalculateArea();
    encoder.CalculateRC();
    encoder.CalculateLatency(1e-12);
    encoder.CalculatePower();
    AssertFiniteNonNegative(encoder.readLatency, "encoder latency");
    AssertNear(encoder.writeLatency, encoder.readLatency);
    AssertFiniteNonNegative(encoder.rampOutput, "encoder output ramp");
    AssertFiniteNonNegative(encoder.leakage, "encoder leakage");
    AssertFinitePositive(encoder.readDynamicEnergy, "encoder read energy");
    AssertNear(encoder.writeDynamicEnergy, encoder.readDynamicEnergy);

    const double latency = encoder.readLatency;
    const double energy = encoder.readDynamicEnergy;
    encoder.CalculateLatency(1e-12);
    encoder.CalculatePower();
    AssertNear(encoder.readLatency, latency);
    AssertNear(encoder.readDynamicEnergy, energy);
}

void TestEncoderTopologiesAndOptimizationModes() {
    CAM_Encoder oneBlock;
    oneBlock.Initialize(8, latency_first, 10e-15, 1e3, MakeComponentConfig());
    VerifyEncoder(oneBlock, 8, latency_first);
    Require(oneBlock.numStage == 1, "eight inputs should use one encoder stage");

    CAM_Encoder multiBlock;
    multiBlock.Initialize(64, area_first, 10e-15, 1e3, MakeComponentConfig());
    VerifyEncoder(multiBlock, 64, area_first);
    Require(multiBlock.numStage == 2, "sixty-four inputs should use two encoder stages");
    Require(multiBlock.numBasicEncoder > oneBlock.numBasicEncoder,
            "larger encoder should use more basic blocks");
}

void TestEncoderRejectsOversizedInputCounts() {
    for (int inputBits : {0, -1, (1 << 27) + 1}) {
        CAM_Encoder encoder;
        AssertThrows<std::runtime_error>(
                [&] { encoder.Initialize(inputBits, latency_first, 10e-15, 1e3, MakeComponentConfig()); },
                "invalid number of subarray bits");
        Require(!encoder.initialized, "rejected encoder must remain uninitialized");
    }
}

void TestEncoderPrintsIdentity() {
    CAM_Encoder encoder;
    encoder.Initialize(8, latency_first, 10e-15, 1e3, MakeComponentConfig());
    StreamCapture capture(std::cout);
    encoder.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("8 to 3 CAM_Encoder Properties:") != std::string::npos,
            "encoder printout should identify the component");
    Require(capture.Text().find("Area =") != std::string::npos,
            "encoder printout should include function-unit properties");
}

void VerifyPriorityEncoder(CAM_PriorityEncoder &encoder, BufferDesignTarget target) {
    Require(encoder.initialized, "priority encoder was not initialized");
    Require(encoder.numInputBits == 64, "priority encoder input count was not preserved");
    Require(encoder.areaOptimizationLevel == target, "priority optimization target was not preserved");
    AssertFinitePositive(encoder.area, "priority encoder area");
    AssertFinitePositive(encoder.width, "priority encoder width");
    AssertFinitePositive(encoder.height, "priority encoder height");

    encoder.CalculateArea();
    encoder.CalculateRC();
    encoder.CalculateLatency(1e-12);
    encoder.CalculatePower();
    AssertFiniteNonNegative(encoder.readLatency, "priority encoder latency");
    AssertNear(encoder.writeLatency, encoder.readLatency);
    AssertFiniteNonNegative(encoder.rampOutput, "priority encoder output ramp");
    AssertFiniteNonNegative(encoder.leakage, "priority encoder leakage");
    AssertFinitePositive(encoder.readDynamicEnergy, "priority encoder read energy");
    AssertNear(encoder.writeDynamicEnergy, encoder.readDynamicEnergy);

    const double latency = encoder.readLatency;
    const double energy = encoder.readDynamicEnergy;
    encoder.CalculateLatency(1e-12);
    encoder.CalculatePower();
    AssertNear(encoder.readLatency, latency);
    AssertNear(encoder.readDynamicEnergy, energy);
}

void TestPriorityEncoderTopologiesAndOptimizationModes() {
    CAM_PriorityEncoder latencyPriority;
    latencyPriority.Initialize(64, latency_first, 10e-15, 1e3, MakeComponentConfig());
    VerifyPriorityEncoder(latencyPriority, latency_first);

    CAM_PriorityEncoder areaPriority;
    areaPriority.Initialize(64, area_first, 10e-15, 1e3, MakeComponentConfig());
    VerifyPriorityEncoder(areaPriority, area_first);
}

void TestPriorityEncoderPropagatesInvalidInputCounts() {
    for (int inputBits : {0, -1, (1 << 27) + 1}) {
        CAM_PriorityEncoder encoder;
        AssertThrows<std::runtime_error>(
                [&] { encoder.Initialize(inputBits, latency_first, 10e-15, 1e3, MakeComponentConfig()); },
                "invalid number of subarray bits");
        Require(!encoder.initialized, "rejected priority encoder must remain uninitialized");
    }
}

void TestPriorityEncoderPrintsIdentity() {
    CAM_PriorityEncoder encoder;
    encoder.Initialize(64, latency_first, 10e-15, 1e3, MakeComponentConfig());
    StreamCapture capture(std::cout);
    encoder.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("CAM_PriorityEncoder Properties:") != std::string::npos,
            "priority encoder printout should identify the component");
    Require(capture.Text().find("Area =") != std::string::npos,
            "priority encoder printout should include function-unit properties");
}

}  // namespace

int main() {
    TestEncodersRequireInitialization();
    TestBasicEncoderSupportedAndUnsupportedInputs();
    TestBasicEncoderPrintsIdentity();
    TestEncoderTopologiesAndOptimizationModes();
    TestEncoderRejectsOversizedInputCounts();
    TestEncoderPrintsIdentity();
    TestPriorityEncoderTopologiesAndOptimizationModes();
    TestPriorityEncoderPropagatesInvalidInputCounts();
    TestPriorityEncoderPrintsIdentity();

    std::cout << "CAM encoder component tests passed\n";
    return 0;
}
