#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "BankWithHtree.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestModelBuilders::MakeWire;
using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::StreamCapture;

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

void Initialize(BankWithHtree &bank, const std::shared_ptr<EvaCamConfig> &config,
        int rows, int columns, int activeRows = 1, int activeColumns = 1,
        bool internalSenseAmp = true, WireRepeaterType repeaterType = repeated_none,
        long long capacity = kCapacityBits, long blockSize = kBlockSizeBits) {
    const Wire localWire = MakeWire(config);
    const Wire globalWire = MakeWire(config, global_aggressive, repeaterType);
    bank.Initialize(rows, columns, capacity, blockSize,
            activeColumns, activeRows, 1, internalSenseAmp, 1, 1, 1, 1, 1, 1,
            area_first, config->technology.cell->camType, EX, config, localWire,
            globalWire, MakeCamOptions());
}

void AssertValidCalculatedBank(BankWithHtree &bank) {
    Require(bank.initialized && !bank.invalid, "H-tree bank initializes as valid");
    Require(bank.mat != nullptr && bank.mat->subarray != nullptr,
            "H-tree bank creates its mat and subarray");
    AssertFinitePositive(bank.area, "H-tree bank area");
    AssertFinitePositive(bank.height, "H-tree bank height");
    AssertFinitePositive(bank.width, "H-tree bank width");

    bank.CalculateRC();
    bank.CalculateLatencyAndPower();
    AssertFiniteNonNegative(bank.readLatency, "H-tree bank read latency");
    AssertFiniteNonNegative(bank.writeLatency, "H-tree bank write latency");
    AssertFiniteNonNegative(bank.searchLatency, "H-tree bank search latency");
    AssertFiniteNonNegative(bank.readDynamicEnergy, "H-tree bank read energy");
    AssertFiniteNonNegative(bank.writeDynamicEnergy, "H-tree bank write energy");
    AssertFiniteNonNegative(bank.searchDynamicEnergy, "H-tree bank search energy");
    AssertFiniteNonNegative(bank.leakage, "H-tree bank leakage");
}

void TestConstructorAndPreInitializationGuards() {
    BankWithHtree bank;
    assert(!bank.initialized);
    assert(!bank.invalid);
    assert(bank.horizontalLevels.empty());
    assert(bank.verticalLevels.empty());
    AssertThrows<std::runtime_error>([&]() { bank.CalculateArea(); }, "Require initialization");
    AssertThrows<std::runtime_error>([&]() { bank.CalculateRC(); }, "Require initialization");
    AssertThrows<std::runtime_error>([&]() { bank.CalculateLatencyAndPower(); },
            "Require initialization");
}

void TestNoLevelTopologyAndPrintBehavior() {
    auto config = MakeBankConfig();
    BankWithHtree bank;
    Initialize(bank, config, 1, 1);
    AssertValidCalculatedBank(bank);

    assert(bank.numAddressBit == 7);
    assert(bank.numDataBit == kBlockSizeBits);
    assert(bank.levelHorizontal == 0);
    assert(bank.levelVertical == 0);
    assert(bank.horizontalLevels.empty());
    assert(bank.verticalLevels.empty());
    assert(bank.mat->numAddressBit == 7);
    assert(bank.mat->numDataBit == kBlockSizeBits);

    StreamCapture capture(std::cout);
    bank.PrintProperty();
    bank.printbreakdown();
    capture.Stop();
    Require(capture.Text().find("Bank Properties:") != std::string::npos,
            "bank property printer identifies the bank");
    Require(capture.Text().find("Subarray Area Breakdown") != std::string::npos,
            "bank breakdown prints subarray information");
    Require(capture.Text().find("Search Dynamic Energy Breakdown") != std::string::npos,
            "bank breakdown prints search energy information");
}

void TestPairedRoutingLevelsAndRepeatStability() {
    auto config = MakeBankConfig();
    BankWithHtree bank;
    Initialize(bank, config, 2, 2, 2, 2);
    AssertValidCalculatedBank(bank);

    assert(bank.levelHorizontal == 1);
    assert(bank.levelVertical == 1);
    assert(bank.horizontalLevels.size() == 1);
    assert(bank.verticalLevels.size() == 1);
    const auto &horizontal = bank.horizontalLevels.at(0);
    const auto &vertical = bank.verticalLevels.at(0);
    assert(horizontal.addressBits == 7 && horizontal.dataBits == kBlockSizeBits);
    assert(horizontal.wireGroups == 1 && horizontal.totalWireGroups == 1);
    assert(horizontal.activeWireGroups == 1);
    assert(vertical.addressBits == 7 && vertical.dataBits == kBlockSizeBits / 2);
    assert(vertical.wireGroups == 1 && vertical.totalWireGroups == 2);
    assert(vertical.activeWireGroups == 2);
    assert(bank.mat->numAddressBit == 7);
    assert(bank.mat->numDataBit == kBlockSizeBits / 4);
    AssertFinitePositive(horizontal.length, "paired horizontal wire length");
    AssertFinitePositive(vertical.length, "paired vertical wire length");

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

void TestExtraHorizontalAndVerticalRoutingLevels() {
    auto horizontalConfig = MakeBankConfig();
    BankWithHtree extraHorizontal;
    Initialize(extraHorizontal, horizontalConfig, 4, 8, 4, 8, true, repeated_none,
            2097152, 16384);
    AssertValidCalculatedBank(extraHorizontal);
    assert(extraHorizontal.levelHorizontal == 3);
    assert(extraHorizontal.levelVertical == 2);
    assert(extraHorizontal.horizontalLevels.size() == 3);
    assert(extraHorizontal.verticalLevels.size() == 2);
    assert(extraHorizontal.horizontalLevels.at(1).totalWireGroups
            > extraHorizontal.horizontalLevels.at(0).totalWireGroups);
    assert(extraHorizontal.verticalLevels.at(1).totalWireGroups
            > extraHorizontal.verticalLevels.at(0).totalWireGroups);
    assert(extraHorizontal.mat->numDataBit == 512);
    assert(extraHorizontal.mat->numAddressBit > 0);

    auto verticalConfig = MakeBankConfig();
    BankWithHtree extraVertical;
    Initialize(extraVertical, verticalConfig, 8, 4, 8, 4, true, repeated_none,
            2097152, 16384);
    AssertValidCalculatedBank(extraVertical);
    assert(extraVertical.levelHorizontal == 2);
    assert(extraVertical.levelVertical == 3);
    assert(extraVertical.horizontalLevels.size() == 2);
    assert(extraVertical.verticalLevels.size() == 3);
    assert(extraVertical.verticalLevels.at(1).totalWireGroups
            > extraVertical.verticalLevels.at(0).totalWireGroups);
    assert(extraVertical.horizontalLevels.at(1).totalWireGroups
            > extraVertical.horizontalLevels.at(0).totalWireGroups);
    assert(extraVertical.mat->numDataBit == 512);
    assert(extraVertical.mat->numAddressBit > 0);
}

void TestActiveCountClampingAndWireAreaModels() {
    auto config = MakeBankConfig();
    BankWithHtree clamped;
    Initialize(clamped, config, 2, 2, 9, 8);
    AssertValidCalculatedBank(clamped);
    assert(clamped.numActiveMatPerRow == 2);
    assert(clamped.numActiveMatPerColumn == 2);

    auto nonRepeatedConfig = MakeBankConfig();
    BankWithHtree nonRepeated;
    Initialize(nonRepeated, nonRepeatedConfig, 2, 2);
    AssertValidCalculatedBank(nonRepeated);

    auto repeatedConfig = MakeBankConfig();
    BankWithHtree repeated;
    Initialize(repeated, repeatedConfig, 2, 2, 1, 1, true, repeated_opt);
    AssertValidCalculatedBank(repeated);
    Require(repeated.area != nonRepeated.area,
            "repeated global wire model changes H-tree wire area");
}

void TestInvalidConfigurationsAndReinitialization() {
    auto externalConfig = MakeBankConfig();
    BankWithHtree externalSenseAmp;
    Initialize(externalSenseAmp, externalConfig, 1, 1, 1, 1, false);
    assert(externalSenseAmp.invalid);
    assert(!externalSenseAmp.initialized);

    auto smallCapacityConfig = MakeBankConfig();
    const Wire localWire = MakeWire(smallCapacityConfig);
    const Wire globalWire = MakeWire(smallCapacityConfig, global_aggressive);
    BankWithHtree noAddressBits;
    noAddressBits.Initialize(1, 1, 64, 64, 1, 1, 1, true, 1, 1, 1, 1, 1, 1,
            area_first, TCAM, EX, smallCapacityConfig, localWire, globalWire,
            MakeCamOptions());
    assert(noAddressBits.invalid);
    assert(noAddressBits.initialized);
    noAddressBits.CalculateArea();
    noAddressBits.CalculateRC();
    noAddressBits.CalculateLatencyAndPower();
    AssertNear(noAddressBits.area, 1e41);
    AssertNear(noAddressBits.readLatency, 1e41);

    auto reinitializeConfig = MakeBankConfig();
    BankWithHtree reinitialized;
    Initialize(reinitialized, reinitializeConfig, 1, 1);
    AssertValidCalculatedBank(reinitialized);
    Initialize(reinitialized, reinitializeConfig, 2, 2);
    AssertValidCalculatedBank(reinitialized);
    assert(reinitialized.levelHorizontal == 1);
    assert(reinitialized.levelVertical == 1);
}

}  // namespace

int main() {
    TestConstructorAndPreInitializationGuards();
    TestNoLevelTopologyAndPrintBehavior();
    TestPairedRoutingLevelsAndRepeatStability();
    TestExtraHorizontalAndVerticalRoutingLevels();
    TestActiveCountClampingAndWireAreaModels();
    TestInvalidConfigurationsAndReinitialization();
    std::cout << "Bank with H-tree coverage tests passed\n";
    return 0;
}
