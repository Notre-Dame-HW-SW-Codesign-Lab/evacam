#include <yaml.h>

#include "config/EvaCamYamlLoader.h"
#include "config/EvaCamConfig.h"
#include "config/TechnologyLoader.h"
#include "input/TechnologyYamlLoader.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool Near(double a, double b) {
    if (std::isinf(a) || std::isinf(b)) {
        return std::isinf(a) && std::isinf(b);
    }
    return std::fabs(a - b) < 1e-12 * std::max(1.0, std::fabs(b));
}

const TechnologySpec& Find(
        const std::vector<TechnologySpec>& specs,
        int processNode,
        DeviceRoadmap roadmap) {
    for (const TechnologySpec& spec : specs) {
        if (spec.featureSizeInNano == processNode && spec.roadmap == roadmap) {
            return spec;
        }
    }
    assert(false);
    return specs.front();
}

void CompareFile(const std::string& path,
        const std::vector<std::pair<int, DeviceRoadmap>>& expectedSpecs,
        bool useUpdatedLib) {
    const auto specs = YamlHelpers::ReadTechnologySpecsFromYaml(path);
    assert(specs.size() == expectedSpecs.size());
    for (const auto& expected : expectedSpecs) {
        const TechnologySpec& spec = Find(specs, expected.first, expected.second);
        assert(spec.useUpdatedLib == useUpdatedLib);
        assert(spec.featureSizeInNano == expected.first);
        assert(spec.roadmap == expected.second);
        assert(Near(spec.featureSize, expected.first * 1e-9));
        assert(spec.vdd > 0);
        assert(spec.vth > 0);
        assert(spec.phyGateLength > 0);
        assert(spec.capIdealGate >= 0);
        assert(spec.currentOnNmos[0] >= 0);
    }
}

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream output(path);
    assert(output.good());
    output << content;
}

void ExpectTechnologyLoadFailure(const std::string& path) {
    bool failed = false;
    try {
        (void)YamlHelpers::ReadTechnologySpecsFromYaml(path);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);
}

void ExpectTechnologyLoadFailureWithMessage(
        const std::string& path, const std::string& expected) {
    try {
        (void)YamlHelpers::ReadTechnologySpecsFromYaml(path);
    } catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(expected) != std::string::npos);
        return;
    }
    assert(false && "Expected technology load to fail");
}

void TestValidationFailures() {
    WriteFile("/tmp/evacam_bad_technology_grid.yaml",
            "schema: technology\n"
            "name: bad\n"
            "library_model: updated\n"
            "temperature_grid: [300K]\n"
            "roadmaps: {}\n");
    ExpectTechnologyLoadFailure("/tmp/evacam_bad_technology_grid.yaml");

    WriteFile("/tmp/evacam_bad_technology_duplicate.yaml",
            "schema: technology\n"
            "name: bad\n"
            "library_model: updated\n"
            "temperature_grid: [300K, 310K, 320K, 330K, 340K, 350K, 360K, 370K, 380K, 390K, 400K]\n"
            "roadmaps:\n"
            "  HP:\n"
            "    nodes:\n"
            "      - process_node: 45nm\n"
            "        roadmap: HP\n"
            "        operating_point: {vdd: 1V, vth: 0.1V, physical_gate_length: 1e-08m}\n"
            "        capacitance: {ideal_gate: 1e-10F/m, fringe: 1e-10F/m, oxide: 0F/m}\n"
            "        mobility: {electron: 0, hole: 0}\n"
            "        sizing: {pn_size_ratio: 2, effective_resistance_multiplier: 1}\n"
            "        gm: {nmos: 1, pmos: 1}\n"
            "        fin: {height: 0m, width: 0m, pitch: 0m, polywire_cap: 0F/m}\n"
            "        currents: &currents\n"
            "          on_nmos: [1,1,1,1,1,1,1,1,1,1,1]\n"
            "          on_pmos: [1,1,1,1,1,1,1,1,1,1,1]\n"
            "          off_nmos: [1,1,1,1,1,1,1,1,1,1,1]\n"
            "          off_pmos: [1,1,1,1,1,1,1,1,1,1,1]\n"
            "      - process_node: 45nm\n"
            "        roadmap: HP\n"
            "        operating_point: {vdd: 1V, vth: 0.1V, physical_gate_length: 1e-08m}\n"
            "        capacitance: {ideal_gate: 1e-10F/m, fringe: 1e-10F/m, oxide: 0F/m}\n"
            "        mobility: {electron: 0, hole: 0}\n"
            "        sizing: {pn_size_ratio: 2, effective_resistance_multiplier: 1}\n"
            "        gm: {nmos: 1, pmos: 1}\n"
            "        fin: {height: 0m, width: 0m, pitch: 0m, polywire_cap: 0F/m}\n"
            "        currents: *currents\n");
    ExpectTechnologyLoadFailure("/tmp/evacam_bad_technology_duplicate.yaml");
}

void TestUnknownTechnologyKeyThrows() {
    YAML::Node root = YAML::LoadFile("config/lib/technology/cmos.updated.yaml");
    root["roadmaps"]["HP"]["nodes"][0]["operating_point"]["supply_voltage"] = "1V";
    std::ofstream output("/tmp/evacam_unknown_technology_key.yaml");
    output << root;
    output.close();

    ExpectTechnologyLoadFailureWithMessage(
            "/tmp/evacam_unknown_technology_key.yaml",
            "unknown key 'technology.roadmaps.nodes.operating_point.supply_voltage'");
}

void TestRuntimeUsesTechnologyFile() {
    EvaCamConfig config;
    config.ReadConfigFromFile("config/2FeFET_TCAM/2FeFET_TCAM.config.yaml");
    assert(!config.input.fileTechnology.empty());
    assert(config.technology.tech != nullptr);
    assert(config.technology.tech->featureSizeInNano() == 45);
    assert(config.technology.tech->deviceRoadmap() == HP);
    assert(!config.technology.tech->useUpdatedLib());
}

void TestRuntimeRequiresTechnologyFile() {
    InputConfig input;
    input.processNode = 45;
    input.deviceRoadmap = HP;
    PeripheralConfig peripherals;

    bool failed = false;
    try {
        (void)TechnologyLoader::Load(input, peripherals, nullptr);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);
}

void TestMissingRoadmapFailsAtRuntime() {
    InputConfig input;
    input.fileTechnology = "config/lib/technology/cmos.updated.yaml";
    input.processNode = 45;
    input.deviceRoadmap = LSTP;
    PeripheralConfig peripherals;

    bool failed = false;
    try {
        (void)TechnologyLoader::Load(input, peripherals, nullptr);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);
}

}  // namespace

int main() {
    CompareFile("config/lib/technology/cmos.updated.yaml",
            {{45, HP}, {32, HP}, {22, HP}, {14, HP}, {10, HP}, {7, HP},
             {45, LP}, {45, FEFET}},
            true);
    CompareFile("config/lib/technology/cmos.legacy_planar.yaml",
            {{200, HP}, {120, HP}, {90, HP}, {65, HP}, {45, HP}, {32, HP}, {22, HP},
             {200, LSTP}, {120, LSTP}, {90, LSTP}, {65, LSTP}, {45, LSTP},
             {32, LSTP}, {22, LSTP}},
            false);
    CompareFile("config/lib/technology/cmos.legacy_fefet.yaml",
            {{45, FEFET}, {22, FEFET}},
            false);
    CompareFile("config/lib/technology/cmos.legacy.yaml",
            {{200, HP}, {120, HP}, {90, HP}, {65, HP}, {45, HP}, {32, HP}, {22, HP},
             {200, LSTP}, {120, LSTP}, {90, LSTP}, {65, LSTP}, {45, LSTP},
             {32, LSTP}, {22, LSTP}, {45, FEFET}, {22, FEFET}},
            false);
    TestValidationFailures();
    TestUnknownTechnologyKeyThrows();
    TestRuntimeUsesTechnologyFile();
    TestRuntimeRequiresTechnologyFile();
    TestMissingRoadmapFailsAtRuntime();
    return 0;
}
