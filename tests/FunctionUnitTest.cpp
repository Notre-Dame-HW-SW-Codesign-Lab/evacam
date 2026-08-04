#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "FunctionUnit.h"

#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::StreamCapture;

class TestFunctionUnit : public FunctionUnit {
    public:
        static void ThrowDecoderError() {
            ThrowInitializationError("Decoder");
        }
};

void TestConstructorZerosEveryMetricAndLeavesConfigNull() {
    FunctionUnit unit;
    Require(unit.height == 0, "height not zero");
    Require(unit.width == 0, "width not zero");
    Require(unit.area == 0, "area not zero");
    Require(unit.readLatency == 0, "readLatency not zero");
    Require(unit.writeLatency == 0, "writeLatency not zero");
    Require(unit.readDynamicEnergy == 0, "readDynamicEnergy not zero");
    Require(unit.writeDynamicEnergy == 0, "writeDynamicEnergy not zero");
    Require(unit.leakage == 0, "leakage not zero");
    Require(unit.setLatency == 0, "setLatency not zero");
    Require(unit.resetLatency == 0, "resetLatency not zero");
    Require(unit.setDynamicEnergy == 0, "setDynamicEnergy not zero");
    Require(unit.resetDynamicEnergy == 0, "resetDynamicEnergy not zero");
    Require(unit.cellReadEnergy == 0, "cellReadEnergy not zero");
    Require(unit.cellSetEnergy == 0, "cellSetEnergy not zero");
    Require(unit.cellResetEnergy == 0, "cellResetEnergy not zero");
    Require(unit.config == nullptr, "config not null");
}

void TestCopyConstructionPreservesMetricsAndSharedConfigOwnership() {
    FunctionUnit original;
    original.height = 1.0;
    original.width = 2.0;
    original.area = 3.0;
    original.readLatency = 4.0;
    original.writeLatency = 5.0;
    original.readDynamicEnergy = 6.0;
    original.writeDynamicEnergy = 7.0;
    original.leakage = 8.0;
    original.setLatency = 9.0;
    original.resetLatency = 10.0;
    original.setDynamicEnergy = 11.0;
    original.resetDynamicEnergy = 12.0;
    original.cellReadEnergy = 13.0;
    original.cellSetEnergy = 14.0;
    original.cellResetEnergy = 15.0;
    original.config = TestModelBuilders::MakeEvaCamConfig();
    FunctionUnit copy(original);
    Require(copy.height == 1.0, "copy height not preserved");
    Require(copy.width == 2.0, "copy width not preserved");
    Require(copy.area == 3.0, "copy area not preserved");
    Require(copy.readLatency == 4.0, "copy readLatency not preserved");
    Require(copy.writeLatency == 5.0, "copy writeLatency not preserved");
    Require(copy.readDynamicEnergy == 6.0, "copy readDynamicEnergy not preserved");
    Require(copy.writeDynamicEnergy == 7.0, "copy writeDynamicEnergy not preserved");
    Require(copy.leakage == 8.0, "copy leakage not preserved");
    Require(copy.setLatency == 9.0, "copy setLatency not preserved");
    Require(copy.resetLatency == 10.0, "copy resetLatency not preserved");
    Require(copy.setDynamicEnergy == 11.0, "copy setDynamicEnergy not preserved");
    Require(copy.resetDynamicEnergy == 12.0, "copy resetDynamicEnergy not preserved");
    Require(copy.cellReadEnergy == 13.0, "copy cellReadEnergy not preserved");
    Require(copy.cellSetEnergy == 14.0, "copy cellSetEnergy not preserved");
    Require(copy.cellResetEnergy == 15.0, "copy cellResetEnergy not preserved");
    Require(copy.config.get() == original.config.get(), "config pointer not shared");
    original.height = 99.0;
    Require(copy.height == 1.0, "copy height not independent");
}

void TestPrintPropertyReportsScaledValuesAndLabels() {
    FunctionUnit unit;
    unit.height = 2e-6;
    unit.width = 3e-6;
    unit.area = 6e-12;
    unit.readLatency = 4e-9;
    unit.writeLatency = 5e-9;
    unit.readDynamicEnergy = 6e-12;
    unit.writeDynamicEnergy = 7e-12;
    unit.leakage = 8e-3;
    StreamCapture capture(std::cout);
    unit.PrintProperty();
    capture.Stop();
    const std::string text = capture.Text();
    Require(text.find("Area = 2um x 3um = 6e-06mm^2") != std::string::npos,
            "area label not found");
    Require(text.find("Read Latency = 4ns") != std::string::npos,
            "read latency not found");
    Require(text.find("Write Latency = 5ns") != std::string::npos,
            "write latency not found");
    Require(text.find("Read Dynamic Energy = 6pJ") != std::string::npos,
            "read energy not found");
    Require(text.find("Write Dynamic Energy = 7pJ") != std::string::npos,
            "write energy not found");
    Require(text.find("Leakage Power = 8mW") != std::string::npos,
            "leakage not found");
    Require(text.find("Timing:") != std::string::npos, "timing label not found");
    Require(text.find("Power:") != std::string::npos, "power label not found");
}

void TestInitializationErrorIncludesComponentName() {
    AssertThrows<std::runtime_error>([]() { TestFunctionUnit::ThrowDecoderError(); },
        "Decoder Error: Require initialization first!");
}

void TestPrintPropertyDispatchesVirtually() {
    bool called = false;
    class VirtualTest : public FunctionUnit {
        public:
            explicit VirtualTest(bool &called) : calledRef(&called) {}

            void PrintProperty() override {
                *calledRef = true;
            }

        private:
            bool *calledRef;
    };
    VirtualTest test(called);
    FunctionUnit &ref = test;
    ref.PrintProperty();
    Require(called, "virtual dispatch failed");
}

}  // namespace

int main() {
    TestConstructorZerosEveryMetricAndLeavesConfigNull();
    TestCopyConstructionPreservesMetricsAndSharedConfigOwnership();
    TestPrintPropertyReportsScaledValuesAndLabels();
    TestInitializationErrorIncludesComponentName();
    TestPrintPropertyDispatchesVirtually();
    std::cout << "FunctionUnit tests passed\n";
    return 0;
}
