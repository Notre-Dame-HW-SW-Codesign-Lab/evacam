#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <yaml.h>

#include "EvaCamConfig.h"
#include "MemCell.h"
#include "TestSupport.h"
#include "config/EvaCamConfigValidator.h"
#include "config/InputRuleValidator.h"
#include "input/MemoryDeviceYamlLoader.h"
#include "input/PhysicalDomainValidators.h"

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

struct Fixture {
    TestSupport::TemporaryDirectory directory{"nand3d-config"};
    YAML::Node device = YAML::LoadFile("config/NAND_3D_TCAM/NAND_3D_TCAM.memory_device.yaml");
    YAML::Node cell = YAML::LoadFile("config/NAND_3D_TCAM/NAND_3D_TCAM.cell.yaml");

    MemCell Load() {
        YAML::Emitter deviceOutput;
        deviceOutput << device;
        directory.WriteFile("device.memory_device.yaml", deviceOutput.c_str());
        cell["memory_device"] = "device.memory_device.yaml";
        YAML::Emitter cellOutput;
        cellOutput << cell;
        const auto path = directory.WriteFile("cell.yaml", cellOutput.c_str());
        MemCell result;
        result.ReadCellFromFile(path.string(), CAM_chip, 1.0);
        return result;
    }
};

void TestNand3dTypedYamlAndUnits() {
    Fixture fixture;
    const MemCell cell = fixture.Load();
    const auto& device = cell.nand3d;
    Require(cell.memCellType == NAND3D && cell.nandString && device.configured,
            "NAND3D has its own typed memory device");
    Require(!cell.nand.configured && device.electrical.configured,
            "NAND3D electrical values must never populate the planar device");
    Require(device.storageMode == "SLC" && device.electrical.model == "transient_rc",
            "only the requested storage mode and transient backend are selected");
    Require(device.storageLayers == 68 && device.dummyLayers == 2
            && device.stringRows == 4 && device.stringColumns == 16,
            "3D stack and sequential selection groups preserve configured dimensions");
    Require(cell.area == 0 && cell.heightInFeatureSize == 0 && cell.widthInFeatureSize == 0,
            "NAND3D does not fabricate a planar flash footprint");
    Require(cell.camNumRow == 0 && cell.camNumCol == 0 && cell.accessType == none_access,
            "NAND3D does not fabricate generic CAM ports");
    AssertNear(device.holePitchX, 100e-9);
    AssertNear(device.holePitchY, 100e-9);
    AssertNear(device.layerPitch, 50e-9);
    AssertNear(device.staircaseContactLength, 500e-9);
    AssertNear(device.isolationWidth, 200e-9);
    AssertNear(device.prechargeDriverResistance, 1000);
    AssertNear(device.solverMaxStep, 0.5e-9);
    AssertNear(device.solverTolerance, 1e-6);
    Require(device.solverMaxSteps == 100000, "solver step budget is explicit");
    AssertNear(cell.flashProgramTime, device.electrical.programPage.latency);
    AssertNear(cell.flashEraseTime, device.electrical.eraseBlock.latency);
    MemCell direct = cell;
    direct.nand3d = Nand3dMemoryDevice{};
    YamlHelpers::ReadMemoryDeviceFromYaml(direct,
            "config/NAND_3D_TCAM/NAND_3D_TCAM.memory_device.yaml");
    Require(direct.nand3d.configured && !direct.nand.configured,
            "direct memory-device loader uses the separate NAND3D parser");
    for (const char* placement : {"beside", "under_array"}) {
        Fixture placed;
        placed.device["nand3d"]["layout"]["peripheral_placement"] = placement;
        Require(placed.Load().nand3d.peripheralPlacement == placement,
                "both supported placement modes parse");
    }
}

void TestNand3dStrictSchemaAndTypeIsolation() {
    using Case = std::pair<std::function<void(Fixture&)>, const char*>;
    const std::vector<Case> cases = {
        {[](Fixture& f) { f.cell.remove("topology"); }, "cell.topology"},
        {[](Fixture& f) { f.cell["cam_type"] = "MCAM"; }, "cam_type: TCAM"},
        {[](Fixture& f) { f.cell["layout"]["area"] = "4F^2"; }, "planar area"},
        {[](Fixture& f) { f.cell["layout"]["aspect_ratio"] = 1; }, "planar area"},
        {[](Fixture& f) { f.cell["ports"] = YAML::Load("{}"); }, "cell.ports"},
        {[](Fixture& f) { f.device.remove("nand3d"); }, "memory_device.nand3d"},
        {[](Fixture& f) { f.device["nand"] = YAML::Load("{}"); }, "requires type: SLCNAND"},
        {[](Fixture& f) { f.device["read"] = YAML::Load("{mode: voltage}"); }, "memory_device.read"},
        {[](Fixture& f) { f.device["nand3d"]["storage_mode"] = "MLC"; }, "storage_mode"},
        {[](Fixture& f) { f.device["nand3d"]["model"] = "analytical_rc"; }, "transient_rc"},
        {[](Fixture& f) { f.device["nand3d"]["source"] = " "; }, "provenance"},
        {[](Fixture& f) { f.device["nand3d"]["spec"] = "outside.spec"; }, "unknown key"},
        {[](Fixture& f) { f.device["nand3d"]["layout"]["hole_pitch_z"] = "10nm"; }, "unknown key"},
        {[](Fixture& f) { f.device["nand3d"]["solver"]["adaptive"] = true; }, "unknown key"},
        {[](Fixture& f) { f.device["nand3d"]["stack"].remove("dummy_layers"); }, "dummy_layers"},
        {[](Fixture& f) { f.device["nand3d"]["layout"].remove("string_rows"); }, "string_rows"},
        {[](Fixture& f) { f.device["nand3d"]["solver"].remove("max_steps"); }, "max_steps"},
        {[](Fixture& f) { f.device["nand3d"]["layout"]["peripheral_placement"] = "under"; }, "under_array"}
    };
    for (const auto& test : cases) {
        Fixture fixture;
        test.first(fixture);
        AssertThrows<std::runtime_error>([&] { fixture.Load(); }, test.second);
    }
    for (const char* type : {"SLCNAND", "MLCNAND", "SRAM"}) {
        Fixture fixture;
        fixture.device["type"] = type;
        // Non-3D cells retain their required legacy layout fields.
        fixture.cell["layout"]["area"] = "4F^2";
        fixture.cell["layout"]["aspect_ratio"] = 1;
        AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "requires type: NAND3D");
    }
}

void TestNand3dRejectsInvalidDomainsAndOverflow() {
    struct Case { const char* group; const char* field; const char* value; const char* message; };
    const std::vector<Case> cases = {
        {"stack", "storage_layers", "-1", "storage layers"},
        {"stack", "storage_layers", "4097", "4096"},
        {"stack", "storage_layers", "2147483648", "storage_layers"},
        {"stack", "dummy_layers", "-1", "dummy layers"},
        {"stack", "dummy_layers", "2147483647", "4096"},
        {"layout", "string_rows", "-1", "string_rows"},
        {"layout", "string_rows", "2147483647", "1048576"},
        {"layout", "string_columns", "0", "string_columns"},
        {"layout", "string_columns", "15", "byte-aligned"},
        {"layout", "string_columns", "2147483647", "string_columns"},
        {"layout", "hole_pitch_x", "0nm", "hole_pitch_x"},
        {"layout", "hole_pitch_y", "-1nm", "hole_pitch_y"},
        {"layout", "layer_pitch", "0nm", "layer_pitch"},
        {"layout", "staircase_step_width", "-1nm", "staircase_step_width"},
        {"layout", "staircase_contact_length", "0nm", "staircase_contact_length"},
        {"layout", "isolation_width", "-1nm", "isolation_width"},
        {"layout", "hole_pitch_x", "1e308m", "Non-finite"},
        {"layout", "hole_pitch_y", "1e308m", "Non-finite"},
        {"solver", "max_step", "0ns", "solver.max_step"},
        {"solver", "tolerance", "0V", "solver.tolerance"},
        {"solver", "tolerance", "2mV", "tolerance <= 1mV"},
        {"solver", "max_steps", "99", "max_steps >= 100"},
        {"solver", "max_steps", "2147483648", "max_steps"},
        {"capacitance", "internal", "0fF", "capacitance.internal"},
        {"capacitance", "source", "0fF", "capacitance.source"}
    };
    for (const auto& test : cases) {
        Fixture fixture;
        fixture.device["nand3d"][test.group][test.field] = test.value;
        AssertThrows<std::runtime_error>([&] { fixture.Load(); }, test.message);
    }
    Fixture fixture;
    fixture.device["nand3d"]["precharge_driver_resistance"] = "0ohm";
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "precharge_driver_resistance");
    Fixture nominal;
    MemCell direct = nominal.Load();
    direct.nand3d.holePitchX = std::numeric_limits<double>::quiet_NaN();
    AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateMemCell(direct); }, "Non-finite");
    direct = nominal.Load();
    direct.nand3d.solverMaxStep = std::numeric_limits<double>::infinity();
    AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateMemCell(direct); }, "Non-finite");
    direct = nominal.Load();
    direct.nand.configured = true;
    AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateMemCell(direct); }, "nand3d only");
}

void TestNand3dCanonicalGeometryAndCapabilities() {
    auto fresh = [] {
        auto config = std::make_unique<EvaCamConfig>();
        config->ReadConfigFromFile("config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml");
        return config;
    };
    auto config = fresh();
    Require(config->input.pageSize == 16 && config->input.flashBlockSize == 4352,
            "NAND3D page counts selected columns; block counts all row groups and storage layers");
    Require(config->input.capacity == 256 && config->wordGeometry.entryCount == 64,
            "logical capacity counts 64 complete 32-bit keys");
    Require(config->technology.cell->nand3d.configured && !config->technology.cell->nand.configured,
            "technology loading preserves NAND3D type isolation");
    Require(std::isfinite(config->technology.cell->setEnergy), "legacy write-energy division is bypassed");
    const std::vector<std::pair<std::function<void(EvaCamConfig&)>, const char*>> geometryCases = {
        {[](EvaCamConfig& c) { c.input.pageSize = 64; }, "string_columns bits"},
        {[](EvaCamConfig& c) { c.input.flashBlockSize = 1088; }, "strings * storage_layers"},
        {[](EvaCamConfig& c) { c.runtimeSizing.fixedSubarrayRows = 16; }, "string_rows * string_columns"},
        {[](EvaCamConfig& c) { c.runtimeSizing.fixedSubarrayColumns = 34; c.input.wordWidth = 34; }, "complementary pairs"},
        {[](EvaCamConfig& c) { c.input.capacity = 512; }, "logical key capacity"},
        {[](EvaCamConfig& c) { c.exploration.geometry.muxSenseAmp = IntValueDomain::FixedSet({32}); }, "selected page string count"},
        {[](EvaCamConfig& c) { c.exploration.geometry.muxSenseAmp = IntValueDomain::FixedSet({3}); }, "selected page string count"},
        {[](EvaCamConfig& c) { c.exploration.geometry.numRowMat = IntValueDomain::FixedSet({2147483647}); }, "at most 65536"}
    };
    for (const auto& test : geometryCases) {
        config = fresh();
        test.first(*config);
        AssertThrows<std::runtime_error>([&] { InputRuleValidator::Validate(*config); }, test.second);
    }
    config = fresh();
    config->exploration.geometry.muxSenseAmp = IntValueDomain::FixedSet({16});
    InputRuleValidator::Validate(*config);
    const std::vector<std::pair<std::function<void(EvaCamConfig&)>, const char*>> capabilityCases = {
        {[](EvaCamConfig& c) { c.input.searchFunction = BE; }, "exact search"},
        {[](EvaCamConfig& c) { c.peripherals.withInputBuffer = true; }, "generic peripherals"},
        {[](EvaCamConfig& c) { c.variation.enabled = true; }, "variation"},
        {[](EvaCamConfig& c) { c.input.optimizationTarget = full_exploration; }, "exploration"},
        {[](EvaCamConfig& c) { c.technology.cell->nand.configured = true; }, "memory_device.nand3d"},
        {[](EvaCamConfig& c) { c.technology.cell->memCellType = MLCNAND; }, "MLC NAND"}
    };
    for (const auto& test : capabilityCases) {
        config = fresh();
        test.first(*config);
        AssertThrows<std::runtime_error>([&] { EvaCamConfigValidator::Validate(*config); }, test.second);
    }
}

}  // namespace

int main() {
    TestNand3dTypedYamlAndUnits();
    TestNand3dStrictSchemaAndTypeIsolation();
    TestNand3dRejectsInvalidDomainsAndOverflow();
    TestNand3dCanonicalGeometryAndCapabilities();
    return 0;
}
