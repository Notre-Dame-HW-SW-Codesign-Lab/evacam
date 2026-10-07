#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "EvaCamConfig.h"
#include "EvaCamExplorer.h"
#include "EvaCamResultExtractor.h"
#include "Result.h"
#include "ResultsYaml.h"
#include "TestSupport.h"
#include "circuit/formula.h"
#include "input/MemoryDeviceYamlLoader.h"
#include "input/NandTechnologyDefaults.h"

namespace {

using TestSupport::AssertThrows;
using TestSupport::Require;

void RequireClose(double actual, double expected) {
    Require(std::isfinite(actual) && std::abs(actual - expected)
            <= std::max(std::abs(expected), 1e-30) * 1e-12, "expected matching SI values");
}

struct Fixture {
    TestSupport::TemporaryDirectory directory{"nand-technology-defaults"};
    std::string name;
    std::string section;
    YAML::Node device;
    YAML::Node cell;
    YAML::Node architecture;
    YAML::Node root;
    std::string warnings;

    explicit Fixture(bool threeD) : name(threeD ? "NAND_3D_TCAM" : "NAND_TCAM"),
            section(threeD ? "nand3d" : "nand") {
        const auto base = std::filesystem::path("config") / name / name;
        device = YAML::LoadFile(base.string() + ".memory_device.yaml");
        cell = YAML::LoadFile(base.string() + ".cell.yaml");
        architecture = YAML::LoadFile(base.string() + ".architecture.yaml");
        root = YAML::LoadFile(base.string() + ".config.yaml");
        root["technology"] = std::filesystem::absolute("config/lib/technology/cmos.legacy.yaml").string();
        root["architecture"] = "architecture.yaml";
        root["cell"] = "cell.yaml";
        cell["memory_device"] = "device.yaml";
    }

    std::shared_ptr<EvaCamConfig> Load() {
        for (const auto &item : {std::pair<const char*, YAML::Node>{"device.yaml", device},
                {"cell.yaml", cell}, {"architecture.yaml", architecture}, {"config.yaml", root}}) {
            YAML::Emitter yaml;
            yaml << item.second;
            directory.WriteFile(item.first, yaml.c_str());
        }
        auto config = std::make_shared<EvaCamConfig>();
        config->logger.SetOutputEnabled(false);
        TestSupport::StreamCapture captured(std::cerr);
        config->ReadConfigFromFile((directory.Path() / "config.yaml").string());
        captured.Stop();
        warnings = captured.Text();
        return config;
    }
};

NandDeviceSpec &Electrical(const std::shared_ptr<EvaCamConfig> &config) {
    auto &cell = *config->technology.cell;
    return cell.memCellType == NAND3D ? cell.nand3d.electrical : cell.nand;
}

void TestTechnologyFallbacksAndExplicitOverrides() {
    for (bool threeD : {false, true}) {
        Fixture fixture(threeD);
        const auto explicitConfig = fixture.Load();
        Require(fixture.warnings.empty(), "complete inputs do not warn");
        Require(Electrical(explicitConfig).technologyDefaults.empty(), "no default provenance for explicit inputs");
        for (const char *group : {"resistance", "capacitance", "sense", "wordline_driver"}) {
            fixture.device[fixture.section].remove(group);
        }
        fixture.device[fixture.section]["calibration_status"] = "calibrated";
        const auto config = fixture.Load();
        const auto &spec = Electrical(config);
        const auto &tech = *config->technology.tech;
        Require(spec.pendingTechnologyDefaults.empty() && spec.technologyDefaults.size() == 17,
                "all supported omitted fields are resolved and recorded");
        Require(spec.calibrationStatus == "uncalibrated", "CMOS estimates cannot preserve a calibrated label");
        Require(fixture.warnings.find("using technology library defaults") != std::string::npos
                && fixture.warnings.find("CMOS estimates") != std::string::npos
                && fixture.warnings.find("45nm, roadmap HP, 300K") != std::string::npos
                && fixture.warnings.find(spec.technologyDefaultSource) != std::string::npos,
                "warning states the source, process, temperature, and estimate status even with logging disabled");
        for (const auto &item : spec.technologyDefaults) {
            Require(std::isfinite(item.second) && item.second >= 0, "finite nonnegative estimates");
            const auto field = item.first.substr(0, item.first.rfind('_'));
            Require(fixture.warnings.find(field) != std::string::npos, "warning lists each defaulted field");
        }
        RequireClose(spec.resistanceReadOn, CalculateOnResistance(tech.featureSize(), NMOS, 300, tech));
        RequireClose(spec.resistancePass, spec.resistanceReadOn);
        RequireClose(spec.resistanceSelect, spec.resistanceReadOn);
        RequireClose(spec.resistanceOff, tech.vdd() / (tech.currentOffNmos()[0] * tech.featureSize()));
        RequireClose(spec.capacitanceGate, CalculateGateCap(tech.featureSize(), tech));
        RequireClose(spec.capacitanceInternal,
                CalculateDrainCap(tech.featureSize(), NMOS, MAX_TRANSISTOR_HEIGHT * tech.featureSize(), tech));
        RequireClose(spec.capacitanceBitline, spec.capacitanceInternal);
        RequireClose(spec.capacitanceSource, spec.capacitanceInternal);
        RequireClose(spec.capacitanceSelect, spec.capacitanceGate);
        RequireClose(spec.programPage.energy, Electrical(explicitConfig).programPage.energy);
        Require(spec.sense.area > 0 && spec.wordlineDriver.area > 0
                && spec.sense.latency > 0 && spec.wordlineDriver.latency > 0, "positive CMOS peripheral estimates");

        // Partial groups preserve explicit values, including valid zeros.
        fixture.device[fixture.section]["resistance"]["off"] = "5Gohm";
        fixture.device[fixture.section]["capacitance"]["gate"] = "0.2fF";
        fixture.device[fixture.section]["sense"]["energy"] = "0J";
        fixture.device[fixture.section]["wordline_driver"]["latency"] = "0s";
        const auto mixedConfig = fixture.Load();
        const auto &mixed = Electrical(mixedConfig);
        RequireClose(mixed.resistanceOff, 5e9);
        RequireClose(mixed.capacitanceGate, 0.2e-15);
        RequireClose(mixed.sense.energy, 0);
        RequireClose(mixed.wordlineDriver.latency, 0);
        Require(mixed.technologyDefaults.size() == 13, "explicit values are not recorded as defaults");

        // Standalone device loading can use the same initialized context.
        MemCell direct = *mixedConfig->technology.cell;
        TestSupport::StreamCapture captured(std::cerr);
        YamlHelpers::ReadMemoryDeviceFromYaml(direct, (fixture.directory.Path() / "device.yaml").string(), mixedConfig);
        const auto &directSpec = threeD ? direct.nand3d.electrical : direct.nand;
        Require(directSpec.technologyDefaults == mixed.technologyDefaults, "device and full-config loaders agree");
        AssertThrows<std::runtime_error>([&] {
            MemCell noContext;
            noContext.ReadCellFromFile((fixture.directory.Path() / "cell.yaml").string(), CAM_chip, 1.0);
        }, "read_on");
    }
}

void TestTechnologySelectionAndInvalidInputs() {
    Fixture fixture(true);
    fixture.device[fixture.section].remove("resistance");
    fixture.device[fixture.section].remove("capacitance");
    const auto first = fixture.Load();
    fixture.architecture["design"]["system_process_node"] = "65nm";
    fixture.architecture["design"]["device_roadmap"] = "LSTP";
    fixture.architecture["design"]["temperature"] = "350K";
    const auto second = fixture.Load();
    const auto &tech = *second->technology.tech;
    RequireClose(Electrical(second).resistanceReadOn,
            CalculateOnResistance(tech.featureSize(), NMOS, 350, tech));
    RequireClose(Electrical(second).resistanceOff,
            tech.vdd() / (tech.currentOffNmos()[50] * tech.featureSize()));
    Require(Electrical(second).resistanceOff != Electrical(first).resistanceOff,
            "defaults follow the selected technology and temperature");

    using Case = std::pair<std::function<void(YAML::Node)>, const char*>;
    const std::vector<Case> cases = {
        {[](YAML::Node n) { n["resistance"]["read_on"] = "0ohm"; }, "read_on"},
        {[](YAML::Node n) { n["resistance"]["off"] = YAML::Node(); }, "off"},
        {[](YAML::Node n) { n["capacitance"] = YAML::Node(); }, "mapping"},
        {[](YAML::Node n) { n["sense"] = YAML::Node(); }, "mapping"},
        {[](YAML::Node n) { n["sense"]["energy"] = "-1fJ"; }, "energy"},
        {[](YAML::Node n) { n["resistance"]["read_onn"] = "1kohm"; }, "unknown key"},
        {[](YAML::Node n) { n.remove("bias"); }, "bias"},
        {[](YAML::Node n) { n["sensing"].remove("decision_time"); }, "decision_time"},
        {[](YAML::Node n) { n.remove("page_buffer"); }, "page_buffer"},
        {[](YAML::Node n) { n.remove("program_page"); }, "program_page"},
        {[](YAML::Node n) { n["layout"].remove("layer_pitch"); }, "layer_pitch"}
    };
    for (const auto &test : cases) {
        Fixture invalid(true);
        invalid.device[invalid.section]["capacitance"].remove("select");
        test.first(invalid.device[invalid.section]);
        AssertThrows<std::runtime_error>([&] { invalid.Load(); }, test.second);
    }
    auto &cell = *first->technology.cell;
    Electrical(first).pendingTechnologyDefaults.insert("capacitance.gate");
    AssertThrows<std::runtime_error>([&] { ApplyNandTechnologyDefaults(cell, nullptr); }, "initialized");
    first->input.temperature = 299;
    AssertThrows<std::runtime_error>([&] { ApplyNandTechnologyDefaults(cell, first); }, "300..400");
}

void TestDefaultProvenanceInResults() {
    for (bool threeD : {false, true}) {
        Fixture fixture(threeD);
        fixture.device[fixture.section]["capacitance"].remove("gate");
        const auto config = fixture.Load();
        EvaCamExplorer explorer(config, 1);
        const auto run = explorer.Run();
        Require(run.numSolution > 0, "defaulted fixture has a legal design");
        std::shared_ptr<Result> result;
        for (const auto &candidate : run.bestResults) {
            if (candidate && candidate->bank && candidate->bank->initialized) {
                result = candidate;
                break;
            }
        }
        Require(static_cast<bool>(result), "initialized result available");
        const auto &spec = Electrical(config);
        const auto dto = ExtractEvaCamDesignResult(*result);
        Require(dto.metadata.at("parameter_fallback") == "technology_library_cmos_estimates",
                "structured result marks estimates");
        Require(dto.metadata.at("technology_default_source") == spec.technologyDefaultSource,
                "structured result preserves the library source");
        RequireClose(dto.summary.at("technology_defaults.capacitance.gate_f"), spec.capacitanceGate);
        std::ostringstream output;
        WriteResultsYaml(output, *result);
        auto yaml = YAML::Load(output.str());
        RequireClose(yaml["summary"]["technology_defaults"]["capacitance"]["gate_f"].as<double>(),
                spec.capacitanceGate);
        RequireClose(yaml["assumptions"]["nand"]["technology_defaults"]["capacitance"]["gate_f"].as<double>(),
                spec.capacitanceGate);
        std::ostringstream failed;
        WriteResultsYamlNoSolutions(failed, *config);
        yaml = YAML::Load(failed.str());
        Require(yaml["assumptions"]["nand"]["technology_default_source"].as<std::string>()
                == spec.technologyDefaultSource, "failed designs also preserve fallback provenance");
        RequireClose(yaml["assumptions"]["nand"]["technology_defaults"]["capacitance"]["gate_f"].as<double>(),
                spec.capacitanceGate);
    }
}

}  // namespace

int main() {
    TestTechnologyFallbacksAndExplicitOverrides();
    TestTechnologySelectionAndInvalidInputs();
    TestDefaultProvenanceInResults();
    std::cout << "NAND technology-default tests passed\n";
}
