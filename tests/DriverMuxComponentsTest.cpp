#include <cassert>
#include <iostream>
#include <stdexcept>

#include "Mux.h"
#include "OutputDriver.h"
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

void TestMuxRequiresInitialization() {
    Mux mux;
    assert(!mux.initialized);
    assert(mux.capForPreviousPowerCalculation == 0);
    assert(mux.capForPreviousDelayCalculation == 0);

    AssertThrows<std::runtime_error>([&] { mux.CalculateArea(); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { mux.CalculateRC(); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { mux.CalculateLatency(1e-12); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { mux.CalculatePower(); }, "Require initialization");
}

void TestMuxModelsActiveAndAbsentTopologies() {
    const auto config = MakeComponentConfig();
    Mux mux;
    mux.Initialize(4, 8, 2e-15, 3e-15, 10e-6, config);

    assert(mux.initialized);
    assert(mux.numInput == 4);
    assert(mux.numMux == 8);
    AssertFinitePositive(mux.widthNMOSPassTransistor, "mux pass-transistor width");
    AssertFinitePositive(mux.area, "mux area");
    AssertFinitePositive(mux.capOutput, "mux output capacitance");
    AssertFinitePositive(mux.resNMOSPassTransistor, "mux pass-transistor resistance");

    mux.CalculateLatency(4e-12);
    mux.CalculatePower();
    AssertFinitePositive(mux.readLatency, "mux latency");
    AssertNear(mux.writeLatency, mux.readLatency);
    AssertFinitePositive(mux.readDynamicEnergy, "mux read energy");
    AssertNear(mux.writeDynamicEnergy, mux.readDynamicEnergy);
    assert(mux.leakage == 0);

    const double firstEnergy = mux.readDynamicEnergy;
    mux.CalculatePower();
    AssertNear(mux.readDynamicEnergy, firstEnergy);

    Mux absentMux;
    absentMux.Initialize(1, 8, 2e-15, 3e-15, 10e-6, config);
    assert(absentMux.area == 0);
    assert(absentMux.width == 0);
    assert(absentMux.height == 0);
    absentMux.CalculateLatency(7e-12);
    absentMux.CalculatePower();
    assert(absentMux.readLatency == 0);
    assert(absentMux.writeLatency == 0);
    assert(absentMux.readDynamicEnergy == 0);
    assert(absentMux.writeDynamicEnergy == 0);
    assert(absentMux.leakage == 0);
}

void TestMuxPrintsIdentityAndFunctionUnitProperties() {
    const auto config = MakeComponentConfig();
    Mux mux;
    mux.Initialize(2, 1, 1e-15, 1e-15, 10e-6, config);

    StreamCapture capture(std::cout);
    mux.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("Mux Properties:") != std::string::npos,
            "mux printout should identify the component");
    Require(capture.Text().find("Area =") != std::string::npos,
            "mux printout should include function-unit properties");
}

void TestOutputDriverRequiresInitialization() {
    OutputDriver driver;
    assert(!driver.initialized);
    assert(!driver.invalid);
    AssertThrows<std::runtime_error>([&] { driver.CalculateArea(); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { driver.CalculateRC(); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { driver.CalculateLatency(1e-12); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { driver.CalculatePower(); }, "Require initialization");
}

void TestOutputDriverModelsLatencyAndAreaTargets() {
    const auto config = MakeComponentConfig();
    OutputDriver latencyDriver;
    latencyDriver.Initialize(4, 1e-15, 80e-15, 200, true, latency_first, 10e-6, config);

    assert(latencyDriver.initialized);
    assert(!latencyDriver.invalid);
    assert(latencyDriver.numStage > 0);
    AssertFinitePositive(latencyDriver.area, "latency-first driver area");
    latencyDriver.CalculateLatency(2e-12);
    latencyDriver.CalculatePower();
    AssertFiniteNonNegative(latencyDriver.readLatency, "latency-first driver latency");
    AssertNear(latencyDriver.writeLatency, latencyDriver.readLatency);
    AssertFinitePositive(latencyDriver.readDynamicEnergy, "latency-first driver energy");
    AssertNear(latencyDriver.writeDynamicEnergy, latencyDriver.readDynamicEnergy);
    AssertFiniteNonNegative(latencyDriver.leakage, "latency-first driver leakage");
    const double firstEnergy = latencyDriver.readDynamicEnergy;
    latencyDriver.CalculatePower();
    AssertNear(latencyDriver.readDynamicEnergy, firstEnergy);

    OutputDriver areaDriver;
    areaDriver.Initialize(1, 2e-15, 4e-15, 50, false, area_first, 10e-6, config);
    assert(areaDriver.initialized);
    assert(!areaDriver.invalid);
    assert(areaDriver.numStage == 1);
    AssertFinitePositive(areaDriver.widthNMOS[0], "area-first NMOS width");
    AssertFinitePositive(areaDriver.widthPMOS[0], "area-first PMOS width");
    areaDriver.CalculateLatency(2e-12);
    areaDriver.CalculatePower();
    AssertFinitePositive(areaDriver.area, "area-first driver area");
    AssertFiniteNonNegative(areaDriver.readLatency, "area-first driver latency");
    AssertFinitePositive(areaDriver.readDynamicEnergy, "area-first driver energy");
}

void TestOutputDriverModelsSmallLatencyFirstLoad() {
    const auto config = MakeComponentConfig();
    OutputDriver driver;
    driver.Initialize(1, 1e-15, 1e-15, 0, false, latency_first, 0, config);

    assert(driver.initialized);
    assert(!driver.invalid);
    assert(driver.numStage > 0);
    AssertFinitePositive(driver.area, "small-load driver area");

    driver.CalculateLatency(2e-12);
    driver.CalculatePower();
    AssertFinitePositive(driver.readLatency, "small-load driver latency");
    AssertFinitePositive(driver.rampOutput, "small-load driver output ramp");
    AssertFinitePositive(driver.readDynamicEnergy, "small-load driver energy");
}

void TestOutputDriverMarksOversizedMinimumCurrentInvalid() {
    const auto config = MakeComponentConfig();
    config->input.maxNmosSize = 1;
    OutputDriver driver;
    driver.Initialize(1, 1e-15, 1e-15, 10, false, area_first, 1, config);

    assert(driver.invalid);
    assert(driver.initialized);
    driver.CalculateArea();
    driver.CalculateRC();
    driver.CalculateLatency(1e-12);
    driver.CalculatePower();
    AssertNear(driver.area, 1e41);
    AssertNear(driver.readLatency, 1e41);
    AssertNear(driver.writeLatency, 1e41);
    AssertNear(driver.readDynamicEnergy, 1e41);
    AssertNear(driver.writeDynamicEnergy, 1e41);
    AssertNear(driver.leakage, 1e41);
}

void TestOutputDriverPrintsIdentityAndStageCount() {
    const auto config = MakeComponentConfig();
    OutputDriver driver;
    driver.Initialize(1, 2e-15, 4e-15, 50, false, area_first, 10e-6, config);

    StreamCapture capture(std::cout);
    driver.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("Output Driver Properties:") != std::string::npos,
            "output-driver printout should identify the component");
    Require(capture.Text().find("Number of inverter stage: 1") != std::string::npos,
            "output-driver printout should include its stage count");
}

}  // namespace

int main() {
    TestMuxRequiresInitialization();
    TestMuxModelsActiveAndAbsentTopologies();
    TestMuxPrintsIdentityAndFunctionUnitProperties();
    TestOutputDriverRequiresInitialization();
    TestOutputDriverModelsLatencyAndAreaTargets();
    TestOutputDriverModelsSmallLatencyFirstLoad();
    TestOutputDriverMarksOversizedMinimumCurrentInvalid();
    TestOutputDriverPrintsIdentityAndStageCount();

    std::cout << "Driver and mux component tests passed" << std::endl;
    return 0;
}
