#include "app/EvaCAM_Match.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace {

struct MonteCarloFixture {
    std::filesystem::path configPath;
    std::filesystem::path outputPath;
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

bool Near(double a, double b, double relTol = 1e-12, double absTol = 1e-18) {
    const double diff = std::fabs(a - b);
    if (diff <= absTol)
        return true;
    return diff <= relTol * std::max(std::fabs(a), std::fabs(b));
}

double ParseLeadingDouble(const YAML::Node &node) {
    return std::stod(node.as<std::string>());
}

MonteCarloFixture WriteMonteCarloConfig(
        const std::string &tag,
        bool variationEnabled = true,
        uint32_t seed = 0,
        const std::string &mode = "monte_carlo",
        int samples = 9,
        bool memoryOnlyVariation = false) {
    const std::filesystem::path repoRoot = std::filesystem::current_path();
    const std::filesystem::path sourceConfig = repoRoot / "config/2FeFET_TCAM/2FeFET_TCAM_system_config.yaml";
    const std::filesystem::path sourceCell = repoRoot / "config/2FeFET_TCAM/2FeFET_TCAM_cell_config.yaml";

    const std::filesystem::path tmpDir = std::filesystem::temp_directory_path() / ("mc_regression_" + tag);
    std::filesystem::remove_all(tmpDir);
    std::filesystem::create_directories(tmpDir);

    const std::filesystem::path testCell = tmpDir / "cell.yaml";
    const std::filesystem::path testConfig = tmpDir / "config.yaml";
    const std::filesystem::path testOutput = tmpDir / "results.yaml";

    std::string cellText = ReadFile(sourceCell);
    cellText +=
        "\nvariation:\n"
        "  with_variation: " + std::string(variationEnabled ? "true" : "false") + "\n";
    if (seed != 0) {
        cellText += "  seed: " + std::to_string(seed) + "\n";
    }
    if (variationEnabled) {
        cellText += "  mode: " + mode + "\n";
        if (mode == "monte_carlo") {
            cellText += "  samples: " + std::to_string(samples) + "\n";
        }
    }
    if (memoryOnlyVariation) {
        cellText +=
            "  memory_device_resistance_on_stdev: 20%\n"
            "  memory_device_resistance_off_stdev: 20%\n"
            "  matchline_wire_resistance_stdev: 0%\n"
            "  device_access_resistance_stdev: 0%\n"
            "  device_match_resistance_stdev: 0%\n";
    } else {
        cellText +=
            "  memory_device_resistance_on_stdev: 15%\n"
            "  memory_device_resistance_off_stdev: 20%\n"
            "  matchline_wire_resistance_stdev: 10%\n"
            "  device_access_resistance_stdev: 12%\n"
            "  device_match_resistance_stdev: 8%\n";
    }
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

    return {testConfig, testOutput};
}

void test_monte_carlo_deterministic_for_fixed_seed() {
    const MonteCarloFixture fixture = WriteMonteCarloConfig("seed_a", true, 12345u);
    EvaCAM_Match matcherA(fixture.configPath.string());
    EvaCAM_Match matcherB(fixture.configPath.string());

    std::vector<int> stored(matcherA.word_width(), 1);
    std::vector<int> query(stored);
    EvaCAMMatchResult resultA = matcherA.evaluate_vector(stored, query);
    EvaCAMMatchResult resultB = matcherB.evaluate_vector(stored, query);

    assert(resultA.hit == resultB.hit);
    assert(Near(resultA.searchLatency, resultB.searchLatency));
    assert(Near(resultA.searchDynamicEnergy, resultB.searchDynamicEnergy));
    assert(Near(resultA.matchlineDelay, resultB.matchlineDelay));
    assert(Near(resultA.senseMargin, resultB.senseMargin));
}

void test_monte_carlo_changes_with_variation_toggle() {
    const MonteCarloFixture fixtureA = WriteMonteCarloConfig("var_on", true, 12345u);
    const MonteCarloFixture fixtureB = WriteMonteCarloConfig("var_off", false, 12345u);
    EvaCAM_Match matcherA(fixtureA.configPath.string());
    EvaCAM_Match matcherB(fixtureB.configPath.string());

    std::vector<int> stored(matcherA.word_width(), 1);
    std::vector<int> query(stored);
    EvaCAMMatchResult resultA = matcherA.evaluate_vector(stored, query);
    EvaCAMMatchResult resultB = matcherB.evaluate_vector(stored, query);

    const bool anyMetricDiffers =
        !Near(resultA.searchLatency, resultB.searchLatency, 1e-9, 1e-18)
        || !Near(resultA.searchDynamicEnergy, resultB.searchDynamicEnergy, 1e-9, 1e-18)
        || !Near(resultA.matchlineDelay, resultB.matchlineDelay, 1e-9, 1e-18)
        || !Near(resultA.senseMargin, resultB.senseMargin, 1e-9, 1e-18);
    assert(anyMetricDiffers);
}

void test_monte_carlo_output_summary_is_emitted() {
    const MonteCarloFixture fixture = WriteMonteCarloConfig("yaml", true, 33333u);

    const std::string command = "./EvaCAM " + fixture.configPath.string() + " >/dev/null 2>&1";
    const int rc = std::system(command.c_str());
    assert(rc == 0);
    assert(std::filesystem::exists(fixture.outputPath));

    const YAML::Node root = YAML::LoadFile(fixture.outputPath.string());
    const YAML::Node variation = root["summary"]["timing"]["variation"];
    assert(variation);
    assert(variation["mode"].as<std::string>() == "monte_carlo");
    assert(variation["samples"].as<int>() == 9);
    assert(variation["sample_file"]);
    assert(variation["plot_file"]);

    const std::filesystem::path samplePath = variation["sample_file"].as<std::string>();
    const std::filesystem::path plotPath = variation["plot_file"].as<std::string>();
    assert(std::filesystem::exists(samplePath));
    assert(std::filesystem::exists(plotPath));

    std::ifstream sampleCsv(samplePath);
    assert(sampleCsv);
    std::string line;
    assert(std::getline(sampleCsv, line));
    assert(line == "sample,corner_label,matchline_wire_res_corner,access_res_on_corner,access_res_off_corner,match_res_on_corner,match_res_off_corner,matchline_delay_s,search_latency_s,search_dynamic_energy_j,sense_margin_v,reference_delay_s");
    int rowCount = 0;
    while (std::getline(sampleCsv, line)) {
        assert(!line.empty());
        rowCount++;
    }
    assert(rowCount == 9);

    const std::vector<std::string> metricNames = {
        "matchline_delay",
        "search_latency",
        "search_dynamic_energy",
        "sense_margin",
    };
    const std::vector<std::string> statNames = {
        "nominal",
        "mean",
        "stddev",
        "min",
        "max",
        "p95",
    };

    for (const auto &metricName : metricNames) {
        const YAML::Node metric = variation[metricName];
        assert(metric);
        for (const auto &statName : statNames) {
            const YAML::Node stat = metric[statName];
            assert(stat);
            assert(!stat.as<std::string>().empty());
        }
    }
}

void test_memory_device_variation_affects_matchline_distribution() {
    const MonteCarloFixture fixture = WriteMonteCarloConfig(
            "memory_only",
            true,
            33333u,
            "monte_carlo",
            21,
            true);

    const std::string command = "./EvaCAM " + fixture.configPath.string() + " --no-variation-plots >/dev/null 2>&1";
    const int rc = std::system(command.c_str());
    assert(rc == 0);
    assert(std::filesystem::exists(fixture.outputPath));

    const YAML::Node variation = YAML::LoadFile(fixture.outputPath.string())["summary"]["timing"]["variation"];
    assert(variation);

    const double matchlineStddev = ParseLeadingDouble(variation["matchline_delay"]["stddev"]);
    const double searchLatencyStddev = ParseLeadingDouble(variation["search_latency"]["stddev"]);
    const double searchEnergyStddev = ParseLeadingDouble(variation["search_dynamic_energy"]["stddev"]);
    assert(matchlineStddev > 0.0);
    assert(searchLatencyStddev > 0.0);
    assert(searchEnergyStddev > 0.0);
}

void test_single_point_output_summary_is_emitted() {
    const MonteCarloFixture fixture = WriteMonteCarloConfig("single_point", true, 55555u, "single_point");

    const std::string command = "./EvaCAM " + fixture.configPath.string() + " >/dev/null 2>&1";
    const int rc = std::system(command.c_str());
    assert(rc == 0);
    assert(std::filesystem::exists(fixture.outputPath));

    const YAML::Node root = YAML::LoadFile(fixture.outputPath.string());
    const YAML::Node variation = root["summary"]["timing"]["variation"];
    assert(variation);
    assert(variation["mode"].as<std::string>() == "single_point");
    assert(variation["samples"].as<int>() == 1);
    assert(!variation["sample_file"]);
    assert(!variation["plot_file"]);

    const std::vector<std::string> metricNames = {
        "matchline_delay",
        "search_latency",
        "search_dynamic_energy",
        "sense_margin",
    };

    for (const auto &metricName : metricNames) {
        const YAML::Node metric = variation[metricName];
        assert(metric);
        assert(metric["nominal"]);
        assert(metric["sample"]);
        assert(!metric["mean"]);
        assert(!metric["stddev"]);
        assert(!metric["min"]);
        assert(!metric["max"]);
        assert(!metric["p95"]);
    }
}

void test_variation_output_summary_is_absent_when_disabled() {
    const MonteCarloFixture fixture = WriteMonteCarloConfig("yaml_off", false, 44444u);

    const std::string command = "./EvaCAM " + fixture.configPath.string() + " >/dev/null 2>&1";
    const int rc = std::system(command.c_str());
    assert(rc == 0);
    assert(std::filesystem::exists(fixture.outputPath));

    const YAML::Node root = YAML::LoadFile(fixture.outputPath.string());
    const YAML::Node timing = root["summary"]["timing"];
    assert(timing);
    assert(!timing["variation"]);
    assert(!std::filesystem::exists(fixture.outputPath.parent_path() / "results_variation_samples.csv"));
}

}  // namespace

int main() {
    test_monte_carlo_deterministic_for_fixed_seed();
    test_monte_carlo_changes_with_variation_toggle();
    test_monte_carlo_output_summary_is_emitted();
    test_memory_device_variation_affects_matchline_distribution();
    test_single_point_output_summary_is_emitted();
    test_variation_output_summary_is_absent_when_disabled();
    std::cout << "Monte Carlo regression tests passed" << std::endl;
    return 0;
}
