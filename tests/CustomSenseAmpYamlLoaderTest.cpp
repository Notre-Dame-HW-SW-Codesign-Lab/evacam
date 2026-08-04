#include <yaml.h>

#include "SenseAmp.h"
#include "config/EvaCamConfig.h"
#include "input/CustomSenseAmpYamlLoader.h"
#include "input/SenseAmpYamlLoader.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char *kCustomSenseAmpPath = "tests/tmp_custom_sense_amp_loader.yaml";
const char *kMissingFieldPath = "tests/tmp_custom_sense_amp_loader_missing.yaml";

bool AlmostEqual(double lhs, double rhs, double tolerance) {
    return std::fabs(lhs - rhs) <= tolerance;
}

void TestNestedYamlLoadsQuantities() {
    std::ofstream out(kCustomSenseAmpPath);
    out <<
        "schema: sense_amp\n"
        "name: scalar_test\n"
        "model: scalar\n"
        "geometry:\n"
        "  height: 10F\n"
        "  width: 4F\n"
        "timing:\n"
        "  latency: 20ps\n"
        "power:\n"
        "  read_dynamic_energy: 4pJ\n"
        "  leakage: 10pW\n"
        "load:\n"
        "  capacitance: 2fF\n";
    out.close();

    SenseAmp senseAmp;
    YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, kCustomSenseAmpPath, 45e-9);

    assert(AlmostEqual(senseAmp.height, 450e-9, 1e-18));
    assert(AlmostEqual(senseAmp.width, 180e-9, 1e-18));
    assert(AlmostEqual(senseAmp.area, 450e-9 * 180e-9, 1e-24));
    assert(AlmostEqual(senseAmp.readLatency, 20e-12, 1e-24));
    assert(AlmostEqual(senseAmp.readDynamicEnergy, 4e-12, 1e-24));
    assert(AlmostEqual(senseAmp.leakage, 10e-12, 1e-24));
    assert(AlmostEqual(senseAmp.capLoad, 2e-15, 1e-27));
}

void TestTopLevelYamlLoadsArea() {
    std::ofstream out(kCustomSenseAmpPath);
    out <<
        "schema: sense_amp\n"
        "name: scalar_test\n"
        "model: scalar\n"
        "geometry:\n"
        "  area: 0.001um^2\n"
        "timing:\n"
        "  latency: 12ps\n"
        "power:\n"
        "  read_dynamic_energy: 3pJ\n"
        "load:\n"
        "  capacitance: 7fF\n";
    out.close();

    SenseAmp senseAmp;
    YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, kCustomSenseAmpPath, 45e-9);

    assert(AlmostEqual(senseAmp.area, 0.001e-12, 1e-27));
    assert(senseAmp.height == 0);
    assert(senseAmp.width == 0);
    assert(AlmostEqual(senseAmp.readLatency, 12e-12, 1e-24));
    assert(AlmostEqual(senseAmp.readDynamicEnergy, 3e-12, 1e-24));
    assert(AlmostEqual(senseAmp.capLoad, 7e-15, 1e-27));
}

void TestV2ScalarYamlLoadsQuantities() {
    std::ofstream out(kCustomSenseAmpPath);
    out <<
        "schema: sense_amp\n"
        "name: scalar_test\n"
        "model: scalar\n"
        "geometry:\n"
        "  height: 10F\n"
        "  width: 4F\n"
        "  area: null\n"
        "timing:\n"
        "  latency: 20ps\n"
        "power:\n"
        "  read_dynamic_energy: 4pJ\n"
        "  leakage: 10pW\n"
        "load:\n"
        "  capacitance: 2fF\n";
    out.close();

    SenseAmp senseAmp;
    YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, kCustomSenseAmpPath, 45e-9);

    assert(AlmostEqual(senseAmp.height, 450e-9, 1e-18));
    assert(AlmostEqual(senseAmp.width, 180e-9, 1e-18));
    assert(AlmostEqual(senseAmp.area, 450e-9 * 180e-9, 1e-24));
    assert(AlmostEqual(senseAmp.readLatency, 20e-12, 1e-24));
    assert(AlmostEqual(senseAmp.readDynamicEnergy, 4e-12, 1e-24));
    assert(AlmostEqual(senseAmp.leakage, 10e-12, 1e-24));
    assert(AlmostEqual(senseAmp.capLoad, 2e-15, 1e-27));
}

void TestMissingRequiredFieldThrows() {
    std::ofstream out(kMissingFieldPath);
    out <<
        "schema: sense_amp\n"
        "name: scalar_test\n"
        "model: scalar\n"
        "geometry:\n"
        "  height: 10F\n"
        "  width: 4F\n"
        "timing:\n"
        "  latency: 20ps\n";
    out.close();

    try {
        SenseAmp senseAmp;
        YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, kMissingFieldPath, 45e-9);
    } catch (const std::runtime_error &e) {
        assert(std::string(e.what()).find("Missing key: power") != std::string::npos);
        return;
    }
    assert(false && "expected missing required fields error");
}

void TestUnknownScalarSenseAmpKeyThrows() {
    std::ofstream out(kCustomSenseAmpPath);
    out <<
        "schema: sense_amp\n"
        "name: scalar_test\n"
        "model: scalar\n"
        "geometry:\n"
        "  height: 10F\n"
        "  width: 4F\n"
        "  widht: 4F\n"
        "timing:\n"
        "  latency: 20ps\n"
        "power:\n"
        "  read_dynamic_energy: 4pJ\n"
        "load:\n"
        "  capacitance: 2fF\n";
    out.close();

    try {
        SenseAmp senseAmp;
        YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, kCustomSenseAmpPath, 45e-9);
    } catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(
                "unknown key 'sense_amp.geometry.widht'") != std::string::npos);
        return;
    }
    assert(false && "Expected unknown scalar sense amp key to throw");
}

void TestDefaultSenseAmpYamlParses() {
    const SenseAmpModel model = YamlHelpers::ReadSenseAmpModelFromYaml(
            "config/lib/sense_amp/nvsim_vol.sense_amp.yaml");
    assert(model.loaded);
    assert(model.model == "nvsim_cmos");
    assert(AlmostEqual(model.pSenseWidth, 30, 1e-12));
    assert(AlmostEqual(model.currentSenseLatency[3].value, 0.80e-9, 1e-24));
    assert(AlmostEqual(model.currentSenseEnergy[4].value, 12.56e-14, 1e-24));
    assert(AlmostEqual(model.currentSenseLeakage[5].value, 150e-9, 1e-18));
}

void TestUnknownDefaultSenseAmpKeyThrows() {
    YAML::Node root = YAML::LoadFile("config/lib/sense_amp/nvsim_vol.sense_amp.yaml");
    root["transistors"]["p_sense_wdith"] = "10F";
    std::ofstream out(kCustomSenseAmpPath);
    out << root;
    out.close();

    try {
        (void)YamlHelpers::ReadSenseAmpModelFromYaml(kCustomSenseAmpPath);
    } catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(
                "unknown key 'sense_amp.transistors.p_sense_wdith'") != std::string::npos);
        return;
    }
    assert(false && "Expected unknown default sense amp key to throw");
}

void TestDefaultSenseAmpYamlInitializes() {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/2FeFET_TCAM/2FeFET_TCAM.config.yaml");
    assert(!config->peripherals.fileSenseAmp.empty());

    SenseAmp voltage;
    voltage.Initialize(64, false, 0.07, 1e-6, config);
    assert(voltage.area > 0);
    assert(voltage.capLoad > 0);
    voltage.CalculateLatency();
    voltage.CalculatePower();
    assert(voltage.readLatency > 0);
    assert(voltage.readDynamicEnergy > 0);

    SenseAmp current;
    current.Initialize(64, true, 0.07, 1e-6, config);
    current.CalculateLatency();
    current.CalculatePower();
    assert(current.readLatency > 0);
    assert(current.readDynamicEnergy > 0);
}

}  // namespace

int main() {
    TestNestedYamlLoadsQuantities();
    TestTopLevelYamlLoadsArea();
    TestV2ScalarYamlLoadsQuantities();
    TestMissingRequiredFieldThrows();
    TestUnknownScalarSenseAmpKeyThrows();
    TestDefaultSenseAmpYamlParses();
    TestUnknownDefaultSenseAmpKeyThrows();
    TestDefaultSenseAmpYamlInitializes();
    std::cout << "CustomSenseAmpYamlLoader tests passed" << std::endl;
    return 0;
}
