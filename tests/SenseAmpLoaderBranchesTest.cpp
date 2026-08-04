#include "SenseAmp.h"
#include "input/CustomSenseAmpYamlLoader.h"
#include "input/SenseAmpYamlLoader.h"

#include "TestSupport.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::TemporaryDirectory;

SenseAmp LoadCustom(const std::filesystem::path& path, double featureSize = 50e-9) {
    SenseAmp senseAmp;
    YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, path.string(), featureSize);
    return senseAmp;
}

std::string ReplaceOnce(std::string text, const std::string& from, const std::string& to) {
    const std::size_t position = text.find(from);
    Require(position != std::string::npos, "fixture replacement target must exist");
    text.replace(position, from.size(), to);
    return text;
}

std::string DefaultModel(const std::string& layout =
        "  min_pitch: 3F\n  same_type_diff_gap: 2F\n  p_to_n_diff_gap: 1F\n",
        const std::string& transistors =
        "  p_sense_width: 31F\n  n_sense_width: 16F\n  isolation_width: 8F\n"
        "  enable_width: 6F\n  mux_width: 10F\n",
        const std::string& area = "  area: 6000F^2\n") {
    const std::string layoutNode = layout.empty() ? "layout: {}\n" : "layout:\n" + layout;
    const std::string transistorNode = transistors.empty()
            ? "transistors: {}\n" : "transistors:\n" + transistors;
    return "schema: sense_amp\nname: branch-model\nmodel: nvsim_cmos\n"
        "supported_modes:\n  voltage_sense: {current_sense: false}\n"
        "  current_sense: {current_sense: true}\n  discharge: {current_sense: false}\n"
        + layoutNode + transistorNode + "iv_converter:\n" + area
        + "  current_sense_latency:\n"
        "    - {min_node: 90nm, latency: 2ns}\n"
        "    - {min_node: null, latency: 0.000000003}\n"
        "  current_sense_energy:\n"
        "    - {min_node: 90nm, energy: 4pJ}\n"
        "    - {energy: 0.000000000000005}\n"
        "  current_sense_leakage:\n"
        "    - {min_node: 90nm, leakage: 6nW}\n"
        "    - {min_node: null, leakage: 0.000000007}\n";
}

void TestCustomNestedAndTopLevelLegacyShapesConvertUnits() {
    TemporaryDirectory temp("sense-amp-custom-shapes");
    const auto nested = temp.WriteFile("nested.yaml",
        "schema: sense_amp\nname: nested\ncustom_sense_amp:\n"
        "  height: 3F\n  width: 4F\n  latency: 2ns\n  energy: 5pJ\n"
        "  leakage: 7nW\n  cap_load: 8fF\n");
    const SenseAmp nestedAmp = LoadCustom(nested);
    AssertNear(nestedAmp.height, 150e-9); AssertNear(nestedAmp.width, 200e-9);
    AssertNear(nestedAmp.area, 30e-15); AssertNear(nestedAmp.readLatency, 2e-9);
    AssertNear(nestedAmp.readDynamicEnergy, 5e-12); AssertNear(nestedAmp.leakage, 7e-9);
    AssertNear(nestedAmp.capLoad, 8e-15);

    const auto topLevel = temp.WriteFile("top.yaml",
        "schema: sense_amp\nname: top\nheight: 2\nwidth: 3\narea: 4mm^2\n"
        "latency: 5\nenergy: 6\nleakage: 7\ncap_load: 8\n");
    const SenseAmp topAmp = LoadCustom(topLevel);
    AssertNear(topAmp.height, 100e-9); AssertNear(topAmp.width, 150e-9);
    AssertNear(topAmp.area, 4e-6); AssertNear(topAmp.readLatency, 5);
    AssertNear(topAmp.readDynamicEnergy, 6); AssertNear(topAmp.leakage, 7); AssertNear(topAmp.capLoad, 8);
}

void TestCustomScalarV2OptionalValuesAndAreaFallback() {
    TemporaryDirectory temp("sense-amp-scalar");
    const auto path = temp.WriteFile("scalar.yaml",
        "schema: sense_amp\nname: scalar\nmodel: scalar\n"
        "geometry: {height: 2F, width: 3F, area: null}\n"
        "timing: {latency: 4ps}\n"
        "power: {read_dynamic_energy: 5fJ, leakage: null}\n"
        "load: {capacitance: 6fF}\n");
    const SenseAmp amp = LoadCustom(path);
    AssertNear(amp.height, 100e-9); AssertNear(amp.width, 150e-9); AssertNear(amp.area, 15e-15);
    AssertNear(amp.readLatency, 4e-12); AssertNear(amp.readDynamicEnergy, 5e-15);
    AssertNear(amp.leakage, 0); AssertNear(amp.capLoad, 6e-15);
}

void TestCustomScalarRejectsSchemaShapeUnitsAndPhysicalDomains() {
    TemporaryDirectory temp("sense-amp-custom-errors");
    const auto valid = temp.WriteFile("valid.yaml",
        "schema: sense_amp\nmodel: scalar\ngeometry: {area: 1um^2}\n"
        "timing: {latency: 1ps}\npower: {read_dynamic_energy: 1fJ}\nload: {capacitance: 1fF}\n");
    AssertThrows<std::runtime_error>([&] { (void)LoadCustom(temp.WriteFile("schema.yaml", "schema: wrong\n")); }, "schema");
    AssertThrows<std::runtime_error>([&] { (void)LoadCustom(temp.WriteFile("unknown.yaml",
            "schema: sense_amp\nmodel: scalar\ngeometry: {area: 1um^2, typo: 1}\n"
            "timing: {latency: 1ps}\npower: {read_dynamic_energy: 1fJ}\nload: {capacitance: 1fF}\n")); }, "unknown key 'sense_amp.geometry.typo'");
    AssertThrows<std::runtime_error>([&] { (void)LoadCustom(temp.WriteFile("missing.yaml",
            "schema: sense_amp\nmodel: scalar\ngeometry: {area: 1um^2}\n")); }, "Missing key: timing");
    AssertThrows<std::runtime_error>([&] { (void)LoadCustom(temp.WriteFile("unit.yaml",
            "schema: sense_amp\nmodel: scalar\ngeometry: {area: 1acre}\n"
            "timing: {latency: 1ps}\npower: {read_dynamic_energy: 1fJ}\nload: {capacitance: 1fF}\n")); }, "Unknown unit 'acre'");
    AssertThrows<std::runtime_error>([&] { (void)LoadCustom(temp.WriteFile("domain.yaml",
            "schema: sense_amp\nmodel: scalar\ngeometry: {height: 0F, width: 2F}\n"
            "timing: {latency: 0ps}\npower: {read_dynamic_energy: -1fJ}\nload: {capacitance: 0fF}\n")); }, "sense_amp.timing.latency must be positive");
    AssertThrows<std::runtime_error>([&] { (void)LoadCustom(valid, 0); }, "feature size must be positive");
    AssertThrows<std::runtime_error>([&] { (void)LoadCustom(temp.WriteFile("not-yaml.txt", "x")); }, "Only YAML custom sense amp files");
    AssertThrows<std::runtime_error>([&] { (void)LoadCustom(temp.WriteFile("nan.yaml",
            "schema: sense_amp\nmodel: scalar\ngeometry: {area: 1um^2}\n"
            "timing: {latency: nanps}\npower: {read_dynamic_energy: 1fJ}\nload: {capacitance: 1fF}\n")); }, "Non-finite value");
}

void TestDefaultModelReadsTablesUnitsAndFallbacks() {
    TemporaryDirectory temp("sense-amp-default");
    const SenseAmpModel model = YamlHelpers::ReadSenseAmpModelFromYaml(
            temp.WriteFile("model.yaml", DefaultModel()).string());
    Require(model.loaded && model.model == "nvsim_cmos", "model metadata loads");
    AssertNear(model.minPitch, 3); AssertNear(model.sameTypeDiffGap, 2); AssertNear(model.pToNDiffGap, 1);
    AssertNear(model.pSenseWidth, 31); AssertNear(model.muxWidth, 10); AssertNear(model.ivConverterArea, 6000);
    Require(model.currentSenseLatency.size() == 2 && model.currentSenseEnergy.size() == 2
            && model.currentSenseLeakage.size() == 2, "all node tables load");
    AssertNear(model.currentSenseLatency[0].minFeatureSize, 90e-9); AssertNear(model.currentSenseLatency[1].minFeatureSize, 0);
    AssertNear(model.currentSenseLatency[1].value, 3e-9); AssertNear(model.currentSenseEnergy[1].value, 5e-15);
    AssertNear(model.currentSenseLeakage[1].value, 7e-9);

    const SenseAmpModel defaults = YamlHelpers::ReadSenseAmpModelFromYaml(temp.WriteFile("defaults.yaml",
        DefaultModel("", "", "")).string());
    AssertNear(defaults.minPitch, 2); AssertNear(defaults.sameTypeDiffGap, 1.5);
    AssertNear(defaults.pSenseWidth, 30); AssertNear(defaults.ivConverterArea, 5000);
}

void TestDefaultModelRejectsMalformedUnknownAndInvalidTables() {
    TemporaryDirectory temp("sense-amp-default-errors");
    AssertThrows<std::runtime_error>([&] { (void)YamlHelpers::ReadSenseAmpModelFromYaml(temp.WriteFile("model.yaml",
            DefaultModel("  min_pitch: 0F\n", "", "")).string()); }, "min_pitch must be positive");
    AssertThrows<std::runtime_error>([&] { (void)YamlHelpers::ReadSenseAmpModelFromYaml(temp.WriteFile("model.yaml",
            DefaultModel() + "unknown: 1\n").string()); }, "unknown key 'sense_amp.unknown'");
    AssertThrows<std::runtime_error>([&] { (void)YamlHelpers::ReadSenseAmpModelFromYaml(temp.WriteFile("model.yaml",
            ReplaceOnce(DefaultModel(), "model: nvsim_cmos", "model: scalar")).string()); }, "not nvsim_cmos");
    AssertThrows<std::runtime_error>([&] { (void)YamlHelpers::ReadSenseAmpModelFromYaml(temp.WriteFile("model.yaml",
            ReplaceOnce(DefaultModel(), "- {min_node: 90nm, latency: 2ns}",
                "- {min_node: 0nm, latency: 2ns}")).string()); }, "strictly descending");
    AssertThrows<std::runtime_error>([&] { (void)YamlHelpers::ReadSenseAmpModelFromYaml(temp.WriteFile("model.yaml",
            ReplaceOnce(DefaultModel(), "- {min_node: null, leakage: 0.000000007}",
                "- {min_node: 1nm, leakage: 7nW}")).string()); }, "must end with a null min_node fallback");
    AssertThrows<std::runtime_error>([&] { (void)YamlHelpers::ReadSenseAmpModelFromYaml(temp.WriteFile("model.yaml",
            ReplaceOnce(DefaultModel(), "latency: 2ns", "latency: infs")).string()); }, "Non-finite value");
}

}  // namespace

int main() {
    TestCustomNestedAndTopLevelLegacyShapesConvertUnits();
    TestCustomScalarV2OptionalValuesAndAreaFallback();
    TestCustomScalarRejectsSchemaShapeUnitsAndPhysicalDomains();
    TestDefaultModelReadsTablesUnitsAndFallbacks();
    TestDefaultModelRejectsMalformedUnknownAndInvalidTables();
    std::cout << "Sense amp loader branch tests passed\n";
    return 0;
}
