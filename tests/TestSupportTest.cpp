#include "TestModelBuilders.h"
#include "TestSupport.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void TestTemporaryDirectoryCreatesFilesAndCleansUp() {
    std::filesystem::path directoryPath;
    {
        TestSupport::TemporaryDirectory directory("evacam-support-test");
        directoryPath = directory.Path();
        assert(std::filesystem::is_directory(directoryPath));

        const std::filesystem::path filePath = directory.WriteFile(
                "nested/example.txt", "fixture contents");
        assert(std::filesystem::is_regular_file(filePath));
    }
    assert(!std::filesystem::exists(directoryPath));
}

void TestAssertNearSupportsAbsoluteAndRelativeTolerance() {
    TestSupport::AssertNear(1e-15, 0, 1e-14, 0);
    TestSupport::AssertNear(1'000'000.5, 1'000'000.0, 0, 1e-6);
    TestSupport::AssertThrows<std::runtime_error>(
            []() { TestSupport::AssertNear(2, 1, 1e-3, 1e-3); },
            "values differ");
}

void TestPhysicalValueAssertionsRejectInvalidValues() {
    TestSupport::AssertFiniteNonNegative(0, "energy");
    TestSupport::AssertFinitePositive(1e-18, "area");
    TestSupport::AssertThrows<std::runtime_error>(
            []() { TestSupport::AssertFiniteNonNegative(-1, "energy"); },
            "energy must be non-negative");
    TestSupport::AssertThrows<std::runtime_error>(
            []() { TestSupport::AssertFinitePositive(0, "area"); },
            "area must be positive");
}

void TestStreamCaptureRestoresTheOriginalStream() {
    TestSupport::StreamCapture capture(std::cout);
    std::cout << "captured output";
    capture.Stop();
    assert(capture.Text() == "captured output");
}

void TestAssertThrowsChecksTypeAndDiagnostic() {
    TestSupport::AssertThrows<std::invalid_argument>(
            []() { throw std::invalid_argument("invalid fixture value"); },
            "fixture value");
    TestSupport::AssertThrows<std::runtime_error>(
            []() {
                TestSupport::AssertThrows<std::invalid_argument>(
                        []() { throw std::runtime_error("wrong type"); },
                        "wrong type");
            },
            "unexpected exception type");
}

void TestModelBuildersCreateIndependentInitializedObjects() {
    const auto firstConfig = TestModelBuilders::MakeEvaCamConfig();
    const auto secondConfig = TestModelBuilders::MakeEvaCamConfig();
    assert(firstConfig != secondConfig);
    assert(firstConfig->technology.tech != secondConfig->technology.tech);
    assert(firstConfig->technology.cell != secondConfig->technology.cell);
    assert(firstConfig->technology.tech->initialized());
    assert(firstConfig->technology.tech->featureSizeInNano() == 90);
    assert(firstConfig->technology.cell->resistanceOn == 1e4);

    const Wire wire = TestModelBuilders::MakeWire(firstConfig);
    assert(wire.initialized);
    assert(wire.config == firstConfig);
    TestSupport::AssertFinitePositive(wire.resWirePerUnit, "wire resistance");
    TestSupport::AssertFinitePositive(wire.capWirePerUnit, "wire capacitance");
}

void TestCamBuildersReturnInitializedComponents() {
    const auto config = TestModelBuilders::MakeEvaCamConfig();
    const auto encoder = TestModelBuilders::MakeCamBasicEncoder(config);
    const auto dataBuffer = TestModelBuilders::MakeCamDataBuffer(config);
    const auto accumulator = TestModelBuilders::MakeCamOutputAccumulator(config);

    assert(encoder->initialized);
    assert(dataBuffer->initialized);
    assert(accumulator->initialized);
    TestSupport::AssertFinitePositive(encoder->area, "encoder area");
    TestSupport::AssertFinitePositive(dataBuffer->area, "data buffer area");
    TestSupport::AssertFinitePositive(accumulator->area, "accumulator area");
}

}  // namespace

int main() {
    TestTemporaryDirectoryCreatesFilesAndCleansUp();
    TestAssertNearSupportsAbsoluteAndRelativeTolerance();
    TestPhysicalValueAssertionsRejectInvalidValues();
    TestStreamCaptureRestoresTheOriginalStream();
    TestAssertThrowsChecksTypeAndDiagnostic();
    TestModelBuildersCreateIndependentInitializedObjects();
    TestCamBuildersReturnInitializedComponents();
    std::cout << "Test support tests passed" << std::endl;
    return 0;
}
