#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "BankFactory.h"
#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestModelBuilders::MakeWire;
using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

constexpr long long kCapacityBits = 8192;
constexpr long kBlockSizeBits = 64;

std::shared_ptr<EvaCamConfig> MakeBankConfig() {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/2FeFET_TCAM/2FeFET_TCAM.config.yaml");
    return config;
}

CAM_Opt MakeCamOptions() {
    return {area_first, area_first, 1};
}

void AssertCalculatedBank(BankWithoutHtree &bank);

double ExpectedSearchEnergy(BankWithoutHtree &bank) {
    const auto &subarray = *bank.mat->subarray;
    double localEnergy = subarray.searchDynamicEnergy * bank.mat->muxSenseAmp
        - (subarray.inputBuf->readDynamicEnergy + subarray.inputEnc->readDynamicEnergy)
        * (bank.mat->muxSenseAmp - 1);
    if (bank.config->peripherals.withOutputAcc) {
        localEnergy *= bank.config->input.wordWidth / bank.CAM_opt.BitSerialWidth;
    }

    double expected = localEnergy * bank.numRowMat * bank.numColumnMat
        * bank.numRowSubarray * bank.numColumnSubarray;
    double length = bank.mat->height * (bank.numRowMat + 1);
    const int routedBits = bank.numAddressBitRouteToMat + bank.numDataBitRouteToMat;
    for (int row = 0; row < bank.numRowMat; row++) {
        length -= bank.mat->height;
        double routeEnergy = 0;
        bank.globalWire.CalculateLatencyAndPower(length, nullptr, &routeEnergy, nullptr);
        expected += routeEnergy * routedBits * bank.numColumnMat;
    }
    return expected;
}

void Initialize(BankWithoutHtree &bank, const std::shared_ptr<EvaCamConfig> &config,
        int rows = 1, int columns = 1, int activeRows = 1, int activeColumns = 1,
        int subarrayRows = 1, int subarrayColumns = 1, int activeSubarrayRows = 1,
        int activeSubarrayColumns = 1, bool internalSenseAmp = true,
        WireRepeaterType repeaterType = repeated_none) {
    const Wire localWire = MakeWire(config);
    const Wire globalWire = MakeWire(config, global_aggressive, repeaterType);
    bank.Initialize(rows, columns, kCapacityBits, kBlockSizeBits, activeColumns, activeRows,
            1, internalSenseAmp, 1, 1, subarrayRows, subarrayColumns,
            activeSubarrayColumns, activeSubarrayRows, area_first,
            config->technology.cell->camType, EX, config, localWire, globalWire,
            MakeCamOptions());
}

void TestRepeatedRoutesContributeAreaAndLeakage() {
    auto config = MakeBankConfig();
    BankWithoutHtree unrepeated;
    BankWithoutHtree repeated;
    Initialize(unrepeated, config, 2, 2);
    Initialize(repeated, config, 2, 2, 1, 1, 1, 1, 1, 1, true, repeated_opt);

    AssertCalculatedBank(unrepeated);
    AssertCalculatedBank(repeated);
    Require(repeated.area > unrepeated.area,
            "repeater tracks add physical routing area");
    Require(repeated.leakage > unrepeated.leakage,
            "route repeaters add leakage");
}

void TestPreInitializationGuards() {
    BankWithoutHtree bank;
    AssertThrows<std::runtime_error>([&]() { bank.CalculateArea(); }, "BankWithoutHtree");
    AssertThrows<std::runtime_error>([&]() { bank.CalculateRC(); }, "BankWithoutHtree");
    AssertThrows<std::runtime_error>([&]() { bank.CalculateLatencyAndPower(); }, "BankWithoutHtree");
}

void AssertCalculatedBank(BankWithoutHtree &bank) {
    Require(bank.initialized && !bank.invalid, "bank initializes as valid");
    AssertFinitePositive(bank.area, "bank area");
    AssertFinitePositive(bank.height, "bank height");
    AssertFinitePositive(bank.width, "bank width");
    AssertFinitePositive(bank.readLatency, "bank read latency");
    AssertFinitePositive(bank.writeLatency, "bank write latency");
    AssertFinitePositive(bank.searchLatency, "bank search latency");
    AssertFinitePositive(bank.readDynamicEnergy, "bank read energy");
    AssertFinitePositive(bank.writeDynamicEnergy, "bank write energy");
    AssertFinitePositive(bank.searchDynamicEnergy, "bank search energy");
    AssertNear(bank.searchDynamicEnergy, ExpectedSearchEnergy(bank));
    AssertFiniteNonNegative(bank.leakage, "bank leakage");
    assert(bank.numBitSerial == bank.CAM_opt.BitSerialWidth);
}

void TestSingleMatInitializationRoutingAndRepeatStability() {
    auto config = MakeBankConfig();
    BankWithoutHtree bank;
    Initialize(bank, config);

    assert(bank.numAddressBit == 7);
    assert(bank.numAddressBitRouteToMat == 7);
    assert(bank.numDataBitRouteToMat == kBlockSizeBits);
    assert((long long)bank.mat->subarray->numRow * bank.mat->subarray->numColumn
            * bank.numRowMat * bank.numColumnMat * bank.numRowSubarray
            * bank.numColumnSubarray == kCapacityBits);
    assert(bank.numRowMat == 1 && bank.numColumnMat == 1);
    AssertCalculatedBank(bank);

    const double area = bank.area;
    const double latency = bank.readLatency;
    const double energy = bank.readDynamicEnergy;
    bank.CalculateArea();
    bank.CalculateRC();
    bank.CalculateLatencyAndPower();
    AssertNear(bank.area, area);
    AssertNear(bank.readLatency, latency);
    AssertNear(bank.readDynamicEnergy, energy);
}

void TestMultiMatRoutingAndActiveCountClamping() {
    auto config = MakeBankConfig();
    BankWithoutHtree bank;
    Initialize(bank, config, 2, 2, 9, 8, 1, 1, 6, 5);

    Require(bank.initialized && !bank.invalid, "multi-mat bank initializes");
    assert(bank.numActiveMatPerRow == 2);
    assert(bank.numActiveMatPerColumn == 2);
    assert(bank.numActiveSubarrayPerRow == 1);
    assert(bank.numActiveSubarrayPerColumn == 1);
    assert(bank.numAddressBit == 7);
    assert(bank.numAddressBitRouteToMat == 7);
    assert(bank.numDataBitRouteToMat == kBlockSizeBits / 4);
    assert((long long)bank.mat->subarray->numRow * bank.mat->subarray->numColumn
            * bank.numRowMat * bank.numColumnMat * bank.numRowSubarray
            * bank.numColumnSubarray == kCapacityBits);
    AssertCalculatedBank(bank);
}

void TestExternalSenseAmplificationAndInvalidRouting() {
    auto repeatedConfig = MakeBankConfig();
    const Wire localWire = MakeWire(repeatedConfig);
    Wire repeatedGlobal = MakeWire(repeatedConfig, global_aggressive);
    repeatedGlobal.wireRepeaterType = repeated_opt;
    BankWithoutHtree invalidRouting;
    invalidRouting.Initialize(1, 1, kCapacityBits, kBlockSizeBits, 1, 1, 1, false,
            1, 1, 1, 1, 1, 1, area_first, TCAM, EX, repeatedConfig, localWire,
            repeatedGlobal, MakeCamOptions());
    Require(invalidRouting.initialized && invalidRouting.invalid,
            "external sensing is rejected for CAM bank routing");
    invalidRouting.CalculateArea();
    invalidRouting.CalculateRC();
    invalidRouting.CalculateLatencyAndPower();
    AssertNear(invalidRouting.area, 1e41);
    AssertNear(invalidRouting.readLatency, 1e41);
}

void TestInvalidMatTopologiesAreRejected() {
    auto config = MakeBankConfig();
    BankWithoutHtree tooFewAddressBits;
    const Wire localWire = MakeWire(config);
    const Wire globalWire = MakeWire(config, global_aggressive);
    tooFewAddressBits.Initialize(2, 2, 256, kBlockSizeBits, 1, 1, 1, true,
            1, 1, 1, 1, 1, 1, area_first, TCAM, EX, config, localWire,
            globalWire, MakeCamOptions());
    Require(tooFewAddressBits.initialized && tooFewAddressBits.invalid,
            "topology which gates every address bit is rejected");

    BankWithoutHtree tooSmallCapacity;
    tooSmallCapacity.Initialize(1, 1, 64, 64, 1, 1, 1, true, 1, 1, 1, 1, 1, 1,
            area_first, TCAM, EX, config, localWire, globalWire, MakeCamOptions());
    Require(tooSmallCapacity.initialized && tooSmallCapacity.invalid,
            "capacity yielding no usable address bits is rejected");
}

void TestFactorySelectsAndInitializesBothSupportedModes() {
    for (RoutingMode routingMode : {h_tree, non_h_tree}) {
        auto config = MakeBankConfig();
        config->input.routingMode = routingMode;
        config->input.internalSensing = true;
        const auto bank = BankFactory::CreateBank(*config);
        Require(bank != nullptr, "factory creates a bank");
        if (routingMode == h_tree) {
            Require(dynamic_cast<BankWithHtree *>(bank.get()) != nullptr,
                    "H-tree selects BankWithHtree");
        } else {
            Require(dynamic_cast<BankWithoutHtree *>(bank.get()) != nullptr,
                    "non-H-tree selects BankWithoutHtree");
        }
        const Wire localWire = MakeWire(config);
        const Wire globalWire = MakeWire(config, global_aggressive);
        BankFactory::InitializeBank(config, bank, 1, 1, kCapacityBits, kBlockSizeBits,
                1, 1, 1, 1, 1, 1, 1, 1, 1, area_first, localWire, globalWire,
                MakeCamOptions());
        Require(bank->initialized && !bank->invalid, "factory-initialized bank is valid");
        AssertFinitePositive(bank->area, "factory bank area");
        AssertFinitePositive(bank->searchLatency, "factory bank search latency");
        AssertFinitePositive(bank->searchDynamicEnergy, "factory bank search energy");
    }

    auto invalidConfig = MakeBankConfig();
    invalidConfig->input.routingMode = static_cast<RoutingMode>(99);
    AssertThrows<std::invalid_argument>([&] {
        BankFactory::CreateBank(*invalidConfig);
    }, "routing mode");
}

}  // namespace

int main() {
    TestPreInitializationGuards();
    TestExternalSenseAmplificationAndInvalidRouting();
    TestSingleMatInitializationRoutingAndRepeatStability();
    TestMultiMatRoutingAndActiveCountClamping();
    TestRepeatedRoutesContributeAreaAndLeakage();
    TestInvalidMatTopologiesAreRejected();
    TestFactorySelectsAndInitializesBothSupportedModes();
    std::cout << "Bank without H-tree and factory tests passed\n";
    return 0;
}
