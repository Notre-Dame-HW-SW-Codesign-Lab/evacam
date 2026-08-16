#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Bank.h"
#include "BankWithoutHtree.h"
#include "EvaCamContextBuilder.h"
#include "Mat.h"
#include "TestSupport.h"
#include "UnitFormatter.h"
#include "Wire.h"
#include "input/CliOptions.h"

namespace {

using TestSupport::AssertFinitePositive;
using TestSupport::AssertThrows;
using TestSupport::Require;

constexpr const char *kConfigPath =
        "config/2FeFET_TCAM/2FeFET_TCAM_explicit_subarray.config.yaml";

struct ModelFixture {
    std::shared_ptr<EvaCamConfig> config;
    Wire localWire;
    Wire globalWire;
    CAM_Opt camOpt{latency_first, latency_first, 64};

    ModelFixture() {
        CliOptions options;
        options.inputFileName = kConfigPath;
        config = EvaCamContextBuilder::Build(options).config;
        localWire.Initialize(config->input.processNode, local_conservative, repeated_none,
                config->input.temperature, false, config);
        globalWire.Initialize(config->input.processNode, global_aggressive, repeated_none,
                config->input.temperature, false, config);
    }

    void Initialize(Mat &mat, int addressBits = 6, long dataBits = 64,
            int activeRows = 1, int activeColumns = 1, int muxSenseAmp = 1) const {
        mat.Initialize(1, 1, addressBits, dataBits, false, activeRows, activeColumns,
                muxSenseAmp, true, 1, 1, latency_first, TCAM, BE, config, localWire, camOpt);
    }
};

class MinimalBank final : public Bank {
    public:
        void Initialize(int, int, long long, long, int, int, int, bool, int, int, int, int,
                int, int, BufferDesignTarget, CAMType, SearchFunction,
                std::shared_ptr<EvaCamConfig>, const Wire &, const Wire &, const CAM_Opt &) override {}
        void CalculateArea() override {}
        void CalculateRC() override {}
        void CalculateLatencyAndPower() override {}
};

void TestMatGuardsAndInvalidConfigurations() {
    Mat mat;
    assert(!mat.initialized && !mat.invalid);
    AssertThrows<std::runtime_error>([&] { mat.CalculateArea(); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { mat.CalculateRC(); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { mat.CalculateLatency(0); }, "Require initialization");
    AssertThrows<std::runtime_error>([&] { mat.CalculatePower(); }, "Require initialization");

    ModelFixture fixture;
    fixture.Initialize(mat, 0);
    assert(mat.initialized && mat.invalid);
    mat.CalculateArea();
    mat.CalculateLatency(0);
    mat.CalculatePower();
    assert(mat.area == 1e41 && mat.readLatency == 1e41 && mat.readDynamicEnergy == 1e41);

    fixture.config->runtimeSizing.hasFixedSubarrayDimensions = false;
    Mat narrow;
    fixture.Initialize(narrow, 3, 64);
    assert(narrow.initialized && narrow.invalid);
    Mat wide;
    fixture.Initialize(wide, 6, 513);
    assert(wide.initialized && wide.invalid);
}

void TestMatTopologyAndMetrics() {
    ModelFixture fixture;
    Mat mat;
    fixture.Initialize(mat);
    Require(mat.initialized && !mat.invalid && mat.subarray != nullptr, "valid Mat must initialize");
    assert(mat.subarray->numRow == 64 && mat.subarray->numColumn == 64);
    // Six row bits split evenly; zero-bit mux decoders remain explicit blocks.
    assert(mat.rowPredecoderBlock1->numAddressBit == 3);
    assert(mat.rowPredecoderBlock2->numAddressBit == 3);
    assert(mat.bitlineMuxPredecoderBlock1->numAddressBit == 0);
    assert(mat.bitlineMuxPredecoderBlock2->numAddressBit == 0);

    mat.CalculateArea();
    const double area = mat.area;
    mat.CalculateArea();
    assert(mat.area == area);
    AssertFinitePositive(mat.area, "Mat area");
    AssertFinitePositive(mat.areaAllPredecoderBlocks, "predecoder area");

    mat.CalculateRC();
    mat.CalculateRC();
    mat.CalculateLatency(1e41);
    const double latency = mat.readLatency;
    mat.CalculateLatency(1e41);
    assert(mat.readLatency == latency);
    AssertFinitePositive(mat.predecoderLatency, "predecoder latency");
    AssertFinitePositive(mat.readLatency, "Mat latency");

    mat.CalculatePower();
    const double readEnergy = mat.readDynamicEnergy;
    mat.CalculatePower();
    assert(mat.readDynamicEnergy == readEnergy);
    AssertFinitePositive(mat.readDynamicEnergy, "Mat read energy");
    AssertFinitePositive(mat.leakage, "Mat leakage");

    TestSupport::StreamCapture capture(std::cout);
    mat.PrintProperty();
    capture.Stop();
    Require(capture.Text().find("Mat Properties:") != std::string::npos,
            "Mat PrintProperty must identify the component");
}

void TestMatVariantsAndBankBaseBehavior() {
    ModelFixture fixture;
    Mat activeClamp;
    fixture.Initialize(activeClamp, 6, 64, 9, 9);
    assert(activeClamp.initialized && !activeClamp.invalid);
    assert(activeClamp.numActiveSubarrayPerRow == 1);
    assert(activeClamp.numActiveSubarrayPerColumn == 1);

    MinimalBank bare;
    assert(!bare.initialized && !bare.invalid);
    AssertThrows<std::runtime_error>([&] { bare.evaluate({0}, {0}); }, "not initialized");
    AssertThrows<std::runtime_error>([&] { bare.match({0}, {0}); }, "not initialized");
    TestSupport::StreamCapture propertyCapture(std::cout);
    bare.PrintProperty();
    propertyCapture.Stop();
    Require(propertyCapture.Text().find("Bank Properties:") != std::string::npos,
            "Bank PrintProperty must identify the component");

    BankWithoutHtree bank;
    bank.Initialize(1, 1, 512, 64, 1, 1, 1, true, 1, 1, 1, 1, 1, 1,
            latency_first, TCAM, BE, fixture.config, fixture.localWire, fixture.globalWire,
            fixture.camOpt);
    Require(bank.initialized && !bank.invalid && bank.mat && bank.mat->subarray,
            "Bank fixture must initialize");
    bank.CalculateRC();
    bank.CalculateLatencyAndPower();
    const std::vector<int> exact(64, 0);
    const std::vector<int> oneMismatch = [] { std::vector<int> value(64, 0); value[0] = 1; return value; }();
    const EvaCAMMatchResult exactResult = bank.evaluate(exact, exact);
    const EvaCAMMatchResult missResult = bank.evaluate(exact, oneMismatch);
    const EvaCAMMatchResult directExact = bank.mat->subarray->EvaluateBinaryMatch(exact, exact);
    const EvaCAMMatchResult directMiss = bank.mat->subarray->EvaluateBinaryMatch(exact, oneMismatch);
    assert(exactResult.hit == directExact.hit && bank.match(exact, exact) == exactResult.hit);
    assert(missResult.hit == directMiss.hit && bank.match(exact, oneMismatch) == missResult.hit);
    Require(missResult.searchDynamicEnergy > exactResult.searchDynamicEnergy,
            "Bank must delegate mismatch search behavior to CAM subarray");

    TestSupport::StreamCapture breakdown(std::cout);
    bank.printbreakdown();
    breakdown.Stop();
    const std::string text = breakdown.Text();
    Require(text.find("Subarray Area Breakdown") != std::string::npos,
            "breakdown must include area aggregate");
    Require(text.find("Search Latency Breakdown") != std::string::npos,
            "breakdown must include latency aggregate");
    Require(text.find("Leakage Breakdown") != std::string::npos,
            "breakdown must include leakage aggregate");
    double expectedRowDriverLatency = 0;
    for (const auto &driver : bank.mat->subarray->RowDriver) {
        if (driver) {
            expectedRowDriverLatency = std::max(
                    expectedRowDriverLatency, driver->readLatency);
        }
    }
    std::ostringstream expectedRowDriver;
    expectedRowDriver << "Row Driver Latency         = "
                      << ToSecond(expectedRowDriverLatency);
    Require(text.find(expectedRowDriver.str()) != std::string::npos,
            "breakdown must report the component row-driver latency");
    double expectedColumnMuxLatency = 0;
    for (const auto &mux : bank.mat->subarray->ColMux) {
        if (mux) {
            expectedColumnMuxLatency = std::max(
                    expectedColumnMuxLatency, mux->readLatency);
        }
    }
    std::ostringstream expectedColumnMux;
    expectedColumnMux << "Column Mux Latency         = "
                      << ToSecond(expectedColumnMuxLatency);
    Require(text.find(expectedColumnMux.str()) != std::string::npos,
            "breakdown must report the component column-mux latency");
}

}  // namespace

int main() {
    TestMatGuardsAndInvalidConfigurations();
    TestMatTopologyAndMetrics();
    TestMatVariantsAndBankBaseBehavior();
    std::cout << "Mat and Bank base tests passed\n";
    return 0;
}
