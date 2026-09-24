#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <yaml.h>

#include "EvaCamConfig.h"
#include "CAM_SubArray.h"
#include "Mat.h"
#include "MemCell.h"
#include "TestSupport.h"
#include "config/EvaCamConfigValidator.h"
#include "config/InputRuleValidator.h"
#include "input/CellYamlLoader.h"
#include "input/MemoryDeviceYamlLoader.h"
#include "input/PhysicalDomainValidators.h"

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

struct Fixture {
    TestSupport::TemporaryDirectory directory{"nand-config"};
    YAML::Node device = YAML::LoadFile("config/NAND_TCAM/NAND_TCAM.memory_device.yaml");
    YAML::Node cell = YAML::LoadFile("config/NAND_TCAM/NAND_TCAM.cell.yaml");

    MemCell Load() {
        YAML::Emitter deviceOutput;
        deviceOutput << device;
        directory.WriteFile("device.yaml", deviceOutput.c_str());
        cell["memory_device"] = "device.yaml";
        YAML::Emitter cellOutput;
        cellOutput << cell;
        const auto path = directory.WriteFile("cell.yaml", cellOutput.c_str());
        MemCell result;
        result.ReadCellFromFile(path.string(), CAM_chip, 1.0);
        return result;
    }
};

void TestNandCellLoadsWithoutGenericPortsOrWriteFields() {
    Fixture fixture;
    const MemCell cell = fixture.Load();
    Require(cell.nandString && cell.nand.configured && cell.memCellType == SLCNAND,
            "NAND topology and device specification must be present");
    Require(cell.camNumRow == 0 && cell.camNumCol == 0 && cell.accessType == none_access,
            "NAND must not fabricate generic CAM ports");
    Require(cell.resistanceOn == 0 && cell.setPulse == 0 && cell.resetPulse == 0,
            "NAND must not depend on legacy set/reset/resistance fields");
    Require(cell.nand.model == "analytical_rc" && cell.nand.calibrationStatus == "synthetic",
            "model provenance is preserved");
    AssertNear(cell.nand.resistanceOff, 1e9);
    AssertNear(cell.nand.capacitanceBitline, 20e-15);
    AssertNear(cell.nand.wordlineDriver.area, 2e-12);
    AssertNear(cell.nand.programPage.energy, 2e-9);
    AssertNear(cell.flashProgramTime, 200e-6);
    AssertNear(cell.flashEraseTime, 2e-3);
    AssertNear(cell.flashPassVoltage, 5);

    // The independently exposed device loader uses the same NAND parser.
    MemCell direct = cell;
    direct.nand = NandDeviceSpec{};
    YamlHelpers::ReadMemoryDeviceFromYaml(direct,
            "config/NAND_TCAM/NAND_TCAM.memory_device.yaml");
    AssertNear(direct.nand.resistanceReadOn, cell.nand.resistanceReadOn);
}

void TestNandSchemaRejectsUnknownMissingAndConflictingFields() {
    struct Case {
        std::function<void(Fixture&)> mutate;
        const char* message;
    };
    const std::vector<Case> cases = {
        {[](Fixture& f) { f.cell.remove("topology"); }, "cell.topology"},
        {[](Fixture& f) { f.cell["topology"] = "parallel"; }, "cell.topology"},
        {[](Fixture& f) { f.device["type"] = "SRAM"; }, "requires type: SLCNAND"},
        {[](Fixture& f) { f.device.remove("nand"); }, "memory_device.nand"},
        {[](Fixture& f) { f.cell["cam_type"] = "MCAM"; }, "cam_type: TCAM"},
        {[](Fixture& f) { f.cell["ports"] = YAML::Load("{}"); }, "cell.ports"},
        {[](Fixture& f) { f.cell["access_device"] = YAML::Load("{type: none}"); }, "cell.access_device"},
        {[](Fixture& f) { f.device["variation"] = YAML::Load("{with_variation: false}"); }, "memory_device.variation"},
        {[](Fixture& f) { f.device["flash"] = YAML::Load("{program_time: 1us}"); }, "memory_device.flash"},
        {[](Fixture& f) { f.device["nand"]["model"] = "guess"; }, "analytical_rc"},
        {[](Fixture& f) { f.device["nand"]["calibration_status"] = "unknown"; }, "calibration_status"},
        {[](Fixture& f) { f.device["nand"]["source"] = " \t"; }, "provenance"},
        {[](Fixture& f) { f.device["nand"]["unknown"] = 1; }, "unknown key"},
        {[](Fixture& f) { f.device["nand"]["resistance"]["typo"] = "1ohm"; }, "unknown key"},
        {[](Fixture& f) { f.device["nand"]["resistance"].remove("off"); }, "off"},
        {[](Fixture& f) { f.device["nand"]["resistance"] = "1ohm"; }, "mapping"},
        {[](Fixture& f) { f.device["nand"]["sense"]["unknown"] = 1; }, "unknown key"},
        {[](Fixture& f) { f.device["nand"]["program_page"]["unknown"] = 1; }, "unknown key"},
        {[](Fixture& f) { f.device["nand"]["sensing"]["unknown"] = 1; }, "unknown key"},
        {[](Fixture& f) { f.device["nand"].remove("precharge"); }, "precharge"}
    };
    for (const auto& test : cases) {
        Fixture fixture;
        test.mutate(fixture);
        AssertThrows<std::runtime_error>([&] { fixture.Load(); }, test.message);
    }
}

void TestNandDomainsRejectNonfiniteOverflowAndInvalidPhysics() {
    struct Case {
        const char* group;
        const char* field;
        const char* value;
        const char* message;
    };
    const std::vector<Case> cases = {
        {"resistance", "read_on", "0ohm", "positive"},
        {"resistance", "off", "1kohm", "ordering"},
        {"resistance", "pass", "20kohm", "ordering"},
        {"resistance", "select", "-1ohm", "positive"},
        {"resistance", "off", "1e309ohm", "Non-finite"},
        {"capacitance", "gate", "0fF", "positive"},
        {"capacitance", "bitline", "0fF", "positive"},
        {"capacitance", "internal", "-1fF", "non-negative"},
        {"threshold", "low", "0.5V", "ordering"},
        {"threshold", "high", "5V", "ordering"},
        {"bias", "precharge", "0V", "positive"},
        {"bias", "read", "-2V", "non-negative"},
        {"sensing", "min_margin", "0V", "positive"},
        {"sensing", "min_margin", "0.5V", "range"},
        {"sensing", "reference_voltage", "0.8V", "range"},
        {"sensing", "decision_time", "0ns", "positive"},
        {"sensing", "offset", "-0.1V", "non-negative"},
        {"wordline_driver", "area", "-1um^2", "positive"},
        {"sense", "leakage", "-1nW", "non-negative"},
        {"query", "energy", "-1fJ", "non-negative"},
        {"precharge", "latency", "0ns", "positive"},
        {"program_page", "latency", "0ns", "positive"},
        {"erase_block", "energy", "0pJ", "positive"}
    };
    for (const auto& test : cases) {
        Fixture fixture;
        fixture.device["nand"][test.group][test.field] = test.value;
        AssertThrows<std::runtime_error>([&] { fixture.Load(); }, test.message);
    }
    for (double efficiency : {0.0, 1.1, std::numeric_limits<double>::infinity()}) {
        Fixture fixture;
        fixture.device["nand"]["supply_efficiency"] = efficiency;
        AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "supply_efficiency");
    }
    Fixture fixture;
    MemCell cell = fixture.Load();
    cell.nand.capacitanceGate = std::numeric_limits<double>::quiet_NaN();
    AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateMemCell(cell); }, "Non-finite");
    fixture.cell["layout"]["area"] = "1e300F^2";
    fixture.cell["layout"]["aspect_ratio"] = 1e300;
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "Non-finite");
}

void TestNandOptionalSensingAndProvenanceValues() {
    for (const char* status : {"synthetic", "uncalibrated", "calibrated"}) {
        Fixture fixture;
        fixture.device["nand"]["calibration_status"] = status;
        fixture.device["nand"]["sensing"].remove("reference_voltage");
        fixture.device["nand"]["sensing"].remove("offset");
        fixture.device["nand"].remove("supply_efficiency");
        const auto cell = fixture.Load();
        AssertNear(cell.nand.referenceVoltage, 0);
        AssertNear(cell.nand.senseOffset, 0);
        AssertNear(cell.nand.supplyEfficiency, 1);
    }
}

void TestNandCanonicalConfigAndCapabilityChecks() {
    auto fresh = [] {
        auto config = std::make_unique<EvaCamConfig>();
        config->ReadConfigFromFile("config/NAND_TCAM/NAND_TCAM.config.yaml");
        return config;
    };
    auto config = fresh();
    Require(config->input.pageSize == 64 && config->input.flashBlockSize == 4352,
            "flash sizes must convert bytes into physical bit counts");
    Require(config->input.capacity == 256 && config->input.wordWidth == 32,
            "logical capacity and key width must survive config loading");
    Require(std::isfinite(config->technology.cell->setEnergy), "NAND must bypass legacy write-energy division by zero");
    const std::vector<std::pair<std::function<void(EvaCamConfig&)>, const char*>> cases = {
        {[](EvaCamConfig& c) { c.input.searchFunction = BE; }, "exact search"},
        {[](EvaCamConfig& c) { c.peripherals.withWriteDriver = true; }, "generic peripherals"},
        {[](EvaCamConfig& c) { c.peripherals.withPriorityEnc = true; }, "generic peripherals"},
        {[](EvaCamConfig& c) { c.peripherals.fileSenseAmp = "unmodeled.yaml"; }, "generic sense amplifiers"},
        {[](EvaCamConfig& c) { c.peripherals.noPrechargeInc = true; }, "precharge"},
        {[](EvaCamConfig& c) { c.variation.enabled = true; }, "variation"},
        {[](EvaCamConfig& c) { c.runtimeSizing.hasFixedSubarrayDimensions = false; }, "fixed"},
        {[](EvaCamConfig& c) { c.input.optimizationTarget = full_exploration; }, "exploration"},
        {[](EvaCamConfig& c) { c.requestDeepExploration = true; }, "exploration"},
        {[](EvaCamConfig& c) { c.constraints.enabled = true; }, "constraints"},
        {[](EvaCamConfig& c) { c.input.optimizationTarget = read_latency_optimized; }, "conventional read"}
    };
    for (const auto& test : cases) {
        config = fresh();
        test.first(*config);
        AssertThrows<std::runtime_error>([&] { EvaCamConfigValidator::Validate(*config); }, test.second);
    }
}

void TestNandGeometryRejectsUnsupportedAndOversizedOrganization() {
    const std::vector<std::pair<std::function<void(EvaCamConfig&)>, const char*>> cases = {
        {[](EvaCamConfig& c) { c.exploration.geometry.numRowMat = IntValueDomain::FixedSet({65537}); }, "at most 65536"},
        {[](EvaCamConfig& c) {
            c.exploration.geometry.numRowMat = IntValueDomain::FixedSet({512});
            c.exploration.geometry.numColumnMat = IntValueDomain::FixedSet({256});
        }, "65536 physical blocks"},
        {[](EvaCamConfig& c) {
            c.exploration.geometry.numRowMat = IntValueDomain::PowersOfTwo(
                    std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        }, "at most 65536"},
        {[](EvaCamConfig& c) { c.exploration.geometry.muxOutputLev1 = IntValueDomain::FixedSet({2}); }, "output_level1"},
        {[](EvaCamConfig& c) { c.exploration.geometry.muxOutputLev2 = IntValueDomain::FixedSet({2}); }, "output_level2"},
        {[](EvaCamConfig& c) {
            c.runtimeSizing.hasExplicitComparisonColumns = true;
            c.exploration.cam.bitSerialWidth = IntValueDomain::FixedSet({16});
        }, "complete key width"},
        {[](EvaCamConfig& c) { c.exploration.geometry.muxSenseAmp = IntValueDomain::FixedSet({3}); }, "divide"},
        {[](EvaCamConfig& c) {
            c.exploration.wires.isGlobalWireLowSwing = IntValueDomain::FixedSet({1});
            c.exploration.wires.globalWireRepeaterType = IntValueDomain::FixedSet({repeated_5});
        }, "wires.global.low_swing"}
    };
    for (const auto& test : cases) {
        EvaCamConfig config;
        config.ReadConfigFromFile("config/NAND_TCAM/NAND_TCAM.config.yaml");
        test.first(config);
        AssertThrows<std::runtime_error>([&] { InputRuleValidator::Validate(config); }, test.second);
    }
}

void TestNandDirectSubarrayGuardsAndMatReinitialization() {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/NAND_TCAM/NAND_TCAM.config.yaml");
    Wire wire;
    wire.Initialize(config->input.processNode, local_conservative, repeated_none,
            config->input.temperature, false, config);
    const CAM_Opt options{latency_first, latency_first, 32};
    CAM_SubArray subarray;
    auto initializeSubarray = [&](int unsupported) {
        subarray.Initialize(64, 32, unsupported == 1, 1, true,
                unsupported == 2 ? 2 : 1, unsupported == 3 ? 2 : 1,
                latency_first, latency_first, unsupported == 4, encoding_two_bit,
                unsupported == 5, discharge, unsupported == 6, unsupported == 7,
                unsupported == 8, unsupported == 9, latency_first, unsupported == 10,
                unsupported == 11, TCAM, EX, unsupported == 12, config, wire, options);
    };
    initializeSubarray(0);
    Require(subarray.initialized && !subarray.invalid, "canonical NAND direct subarray must initialize");
    for (int unsupported = 1; unsupported <= 12; ++unsupported) {
        AssertThrows<std::invalid_argument>([&] { initializeSubarray(unsupported); }, "Unsupported NAND");
        Require(!subarray.initialized, "failed NAND reinitialization must not retain prior usable state");
    }
    initializeSubarray(0);
    Require(subarray.initialized && !subarray.invalid, "valid NAND reinitialization must recover");

    Mat mat;
    auto initializeMat = [&] {
        mat.Initialize(1, 1, 1, 32, false, 1, 1, 1, true, 1, 1,
                latency_first, TCAM, EX, config, wire, options);
    };
    config->technology.cell->nand.resistanceOff = 10001;
    initializeMat();
    Require(mat.initialized && mat.invalid, "overlapping NAND match/nonmatch classes must invalidate Mat");
    config->technology.cell->nand.resistanceOff = 1e9;
    initializeMat();
    Require(mat.initialized && !mat.invalid, "valid NAND reinitialization must clear Mat rejection");
    TestSupport::AssertFinitePositive(mat.area, "reinitialized NAND Mat area");
}

}  // namespace

int main() {
    TestNandCellLoadsWithoutGenericPortsOrWriteFields();
    TestNandSchemaRejectsUnknownMissingAndConflictingFields();
    TestNandDomainsRejectNonfiniteOverflowAndInvalidPhysics();
    TestNandOptionalSensingAndProvenanceValues();
    TestNandCanonicalConfigAndCapabilityChecks();
    TestNandGeometryRejectsUnsupportedAndOversizedOrganization();
    TestNandDirectSubarrayGuardsAndMatReinitialization();
    return 0;
}
