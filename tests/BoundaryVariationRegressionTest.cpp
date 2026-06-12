#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

namespace {

struct BoundaryFixture {
    std::filesystem::path configPath;
    std::filesystem::path outputPath;
    std::filesystem::path logPath;
};

std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("Failed to open file: " + path.string());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void WriteFile(const std::filesystem::path &path, const std::string &contents) {
    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("Failed to write file: " + path.string());
    out << contents;
}

void ReplaceAll(std::string &text, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

BoundaryFixture WriteBoundaryConfig(const std::string &tag, const std::string &maxVarFields) {
    const std::filesystem::path repoRoot = std::filesystem::current_path();
    const std::filesystem::path sourceConfig = repoRoot / "config/2FeFET_TCAM/2FeFET_TCAM_system_config.yaml";
    const std::filesystem::path sourceCell = repoRoot / "config/2FeFET_TCAM/2FeFET_TCAM_cell_config.yaml";

    const std::filesystem::path tmpDir = std::filesystem::temp_directory_path() / ("boundary_variation_" + tag);
    std::filesystem::remove_all(tmpDir);
    std::filesystem::create_directories(tmpDir);

    const std::filesystem::path testCell = tmpDir / "cell.yaml";
    const std::filesystem::path testConfig = tmpDir / "config.yaml";
    const std::filesystem::path testOutput = tmpDir / "results.yaml";
    const std::filesystem::path testLog = tmpDir / "run.log";

    std::string cellText = ReadFile(sourceCell);
    cellText +=
        "\nvariation:\n"
        "  with_variation: true\n"
        "  mode: boundary\n"
        "  seed: 12345\n"
        "  samples: 99\n"
        + maxVarFields;
    WriteFile(testCell, cellText);

    std::string configText = ReadFile(sourceConfig);
    ReplaceAll(configText,
            "cell_file: ./config/2FeFET_TCAM/2FeFET_TCAM_cell_config.yaml",
            "cell_file: " + testCell.string());
    ReplaceAll(configText, "search_function: BE", "search_function: EX");
    configText +=
        "\nextra:\n"
        "  output_yaml_file: " + testOutput.string() + "\n";
    WriteFile(testConfig, configText);

    return {testConfig, testOutput, testLog};
}

int RunEvaCamVerbose(const BoundaryFixture &fixture) {
    const std::string command = "./EvaCAM -v " + fixture.configPath.string()
        + " > " + fixture.logPath.string() + " 2>&1";
    return std::system(command.c_str());
}

int CountCsvRows(const std::filesystem::path &path, std::string *firstDataRow = nullptr) {
    std::ifstream csv(path);
    assert(csv);

    std::string line;
    assert(std::getline(csv, line));
    assert(line == "sample,corner_label,matchline_wire_res_corner,access_res_on_corner,access_res_off_corner,match_res_on_corner,match_res_off_corner,matchline_delay_s,search_latency_s,search_dynamic_energy_j,sense_margin_v,reference_delay_s");

    int rows = 0;
    while (std::getline(csv, line)) {
        assert(!line.empty());
        if (rows == 0 && firstDataRow) {
            *firstDataRow = line;
        }
        rows++;
    }
    return rows;
}

void AssertBoundaryOutput(
        const BoundaryFixture &fixture,
        int expectedSamples,
        const std::string &expectedFirstCornerLabel) {
    assert(std::filesystem::exists(fixture.outputPath));

    const YAML::Node variation = YAML::LoadFile(fixture.outputPath.string())["summary"]["timing"]["variation"];
    assert(variation);
    assert(variation["mode"].as<std::string>() == "boundary");
    assert(variation["samples"].as<int>() == expectedSamples);
    assert(variation["sample_file"]);
    assert(!variation["plot_file"]);
    assert(variation["matchline_delay"]["min"]);
    assert(variation["matchline_delay"]["max"]);
    assert(variation["search_dynamic_energy"]["min"]);
    assert(variation["search_dynamic_energy"]["max"]);

    const std::filesystem::path samplePath = variation["sample_file"].as<std::string>();
    assert(std::filesystem::exists(samplePath));

    std::string firstDataRow;
    assert(CountCsvRows(samplePath, &firstDataRow) == expectedSamples);
    assert(firstDataRow.find("\"" + expectedFirstCornerLabel + "\"") != std::string::npos);
}

void test_boundary_access_variation_outputs_four_corners() {
    const BoundaryFixture fixture = WriteBoundaryConfig(
            "access",
            "  device_access_resistance_max_var: 5%\n");

    assert(RunEvaCamVerbose(fixture) == 0);
    AssertBoundaryOutput(
            fixture,
            4,
            "ml=nominal,access_on=low,access_off=low,match_on=nominal,match_off=nominal");

    const std::string log = ReadFile(fixture.logPath);
    assert(log.find("variation.samples is ignored because boundary mode is deterministic") != std::string::npos);
    assert(log.find("variation.seed is ignored because boundary mode is deterministic") != std::string::npos);
}

void test_boundary_memory_on_variation_outputs_two_corners() {
    const BoundaryFixture fixture = WriteBoundaryConfig(
            "memory_on",
            "  memory_device_resistance_on_max_var: 5%\n");

    assert(RunEvaCamVerbose(fixture) == 0);
    AssertBoundaryOutput(
            fixture,
            2,
            "ml=nominal,access_on=nominal,access_off=nominal,match_on=low,match_off=nominal");
}

}  // namespace

int main() {
    test_boundary_access_variation_outputs_four_corners();
    test_boundary_memory_on_variation_outputs_two_corners();
    std::cout << "Boundary variation regression tests passed" << std::endl;
    return 0;
}
