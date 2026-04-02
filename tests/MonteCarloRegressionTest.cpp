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

MonteCarloFixture WriteMonteCarloConfig(uint32_t seed, const std::string &tag, bool variationEnabled = true, int samples = 9) {
    const std::filesystem::path repoRoot = std::filesystem::current_path();
    const std::filesystem::path sourceConfig = repoRoot / "config/ReRAM-2T2R/ReRAM-2T2R_config.yaml";
    const std::filesystem::path sourceCell = repoRoot / "config/ReRAM-2T2R/ReRAM-2T2R_cell.yaml";

    const std::filesystem::path tmpDir = std::filesystem::temp_directory_path() / ("mc_regression_" + tag);
    std::filesystem::create_directories(tmpDir);

    const std::filesystem::path testCell = tmpDir / "cell.yaml";
    const std::filesystem::path testConfig = tmpDir / "config.yaml";
    const std::filesystem::path testOutput = tmpDir / "results.yaml";

    std::string cellText = ReadFile(sourceCell);
    cellText +=
        "\nvariation:\n"
        "  with_variation: true\n"
        "  cell_resistance_on_sigma: 15%\n"
        "  cell_resistance_off_sigma: 20%\n"
        "  matchline_wire_resistance_sigma: 10%\n"
        "  device_access_resistance_sigma: 12%\n"
        "  device_match_resistance_sigma: 8%\n";
    WriteFile(testCell, cellText);

    std::string configText = ReadFile(sourceConfig);
    ReplaceAll(configText,
            "cell_file: ./config/ReRAM-2T2R/ReRAM-2T2R_cell.yaml",
            "cell_file: " + testCell.string());
    configText +=
        "\nvariation:\n"
        "  enabled: " + std::string(variationEnabled ? "true" : "false") + "\n"
        "  mode: monte_carlo\n"
        "  seed: " + std::to_string(seed) + "\n"
        "  samples: " + std::to_string(samples) + "\n"
        "  distribution: lognormal\n";
    WriteFile(testConfig, configText);

    return {testConfig, testOutput};
}

void test_monte_carlo_deterministic_for_fixed_seed() {
    const MonteCarloFixture fixture = WriteMonteCarloConfig(12345, "seed_a");
    EvaCAM_Match matcherA(fixture.configPath.string());
    EvaCAM_Match matcherB(fixture.configPath.string());

    std::vector<int> stored(matcherA.word_width(), 1);
    std::vector<int> query(stored);
    EvaCAMMatchResult resultA = matcherA.evaluate(stored, query);
    EvaCAMMatchResult resultB = matcherB.evaluate(stored, query);

    assert(resultA.hit == resultB.hit);
    assert(Near(resultA.searchLatency, resultB.searchLatency));
    assert(Near(resultA.searchDynamicEnergy, resultB.searchDynamicEnergy));
    assert(Near(resultA.matchlineDelay, resultB.matchlineDelay));
    assert(Near(resultA.senseMargin, resultB.senseMargin));
}

void test_monte_carlo_changes_with_seed() {
    const MonteCarloFixture fixtureA = WriteMonteCarloConfig(11111, "seed_b");
    const MonteCarloFixture fixtureB = WriteMonteCarloConfig(22222, "seed_c");
    EvaCAM_Match matcherA(fixtureA.configPath.string());
    EvaCAM_Match matcherB(fixtureB.configPath.string());

    std::vector<int> stored(matcherA.word_width(), 1);
    std::vector<int> query(stored);
    EvaCAMMatchResult resultA = matcherA.evaluate(stored, query);
    EvaCAMMatchResult resultB = matcherB.evaluate(stored, query);

    const bool anyMetricDiffers =
        !Near(resultA.searchLatency, resultB.searchLatency, 1e-9, 1e-18)
        || !Near(resultA.searchDynamicEnergy, resultB.searchDynamicEnergy, 1e-9, 1e-18)
        || !Near(resultA.matchlineDelay, resultB.matchlineDelay, 1e-9, 1e-18)
        || !Near(resultA.senseMargin, resultB.senseMargin, 1e-9, 1e-18);
    assert(anyMetricDiffers);
}

void test_monte_carlo_output_summary_is_emitted() {
    const MonteCarloFixture fixture = WriteMonteCarloConfig(33333, "yaml");

    const std::string command = "./EvaCAM -o " + fixture.outputPath.string()
        + " " + fixture.configPath.string() + " >/dev/null 2>&1";
    const int rc = std::system(command.c_str());
    assert(rc == 0);
    assert(std::filesystem::exists(fixture.outputPath));

    const YAML::Node root = YAML::LoadFile(fixture.outputPath.string());
    const YAML::Node variation = root["summary"]["timing"]["variation"];
    assert(variation);
    assert(variation["mode"].as<std::string>() == "monte_carlo");
    assert(variation["samples"].as<int>() == 9);

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

void test_variation_output_summary_is_absent_when_disabled() {
    const MonteCarloFixture fixture = WriteMonteCarloConfig(44444, "yaml_off", false, 1);

    const std::string command = "./EvaCAM -o " + fixture.outputPath.string()
        + " " + fixture.configPath.string() + " >/dev/null 2>&1";
    const int rc = std::system(command.c_str());
    assert(rc == 0);
    assert(std::filesystem::exists(fixture.outputPath));

    const YAML::Node root = YAML::LoadFile(fixture.outputPath.string());
    const YAML::Node timing = root["summary"]["timing"];
    assert(timing);
    assert(!timing["variation"]);
}

}  // namespace

int main() {
    test_monte_carlo_deterministic_for_fixed_seed();
    test_monte_carlo_changes_with_seed();
    test_monte_carlo_output_summary_is_emitted();
    test_variation_output_summary_is_absent_when_disabled();
    std::cout << "Monte Carlo regression tests passed" << std::endl;
    return 0;
}
