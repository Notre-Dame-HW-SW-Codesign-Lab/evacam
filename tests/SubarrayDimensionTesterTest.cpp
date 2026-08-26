#include "SubarrayDimensionTester.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>

#include "McamTestConfig.h"
#include "TestSupport.h"

namespace {

const std::filesystem::path kConfigDirectory =
        std::filesystem::absolute("config/2FeFET_MCAM");
const std::filesystem::path kTcamBaseConfig =
        std::filesystem::absolute("config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml");

std::string MakeTesterConfig(
        const std::filesystem::path &outputDirectory,
        const std::string &rows = "[8, 16]",
        const std::string &columns = "[8, 16]") {
    std::ostringstream yaml;
    yaml << "schema: subarray_dimension_test\n"
         << "name: focused_dimension_test\n"
         << "config_pattern: case_{rows}x{columns}.config.yaml\n"
         << "rows: " << rows << "\n"
         << "columns: " << columns << "\n"
         << "threads_per_run: 1\n"
         << "output:\n"
         << "  directory: " << outputDirectory.string() << "\n"
         << "  summary_csv: nested/summary.csv\n";
    return yaml.str();
}

std::string MakeBaseTesterConfig(
        const std::filesystem::path &outputDirectory,
        const std::string &rows = "[8]",
        const std::string &columns = "[8, 16]") {
    std::ostringstream yaml;
    yaml << "schema: subarray_dimension_test\n"
         << "name: base_dimension_test\n"
         << "base_config: " << kTcamBaseConfig.string() << "\n"
         << "rows: " << rows << "\n"
         << "columns: " << columns << "\n"
         << "threads_per_run: 1\n"
         << "output:\n"
         << "  directory: " << outputDirectory.string() << "\n"
         << "  summary_csv: nested/summary.csv\n";
    return yaml.str();
}

std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input(path);
    TestSupport::Require(input.is_open(), "could not read " + path.string());
    return std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
}

std::string ReplaceOnce(
        std::string value,
        const std::string &needle,
        const std::string &replacement) {
    const std::size_t position = value.find(needle);
    TestSupport::Require(position != std::string::npos, "test fixture text was not found");
    value.replace(position, needle.size(), replacement);
    return value;
}

void TestLoadConfigAndBuildCartesianRunSpecs() {
    TestSupport::TemporaryDirectory directory("subarray-dimension-config");
    const std::string runConfig =
            "schema: config\n"
            "optimization:\n"
            "  target: LeakagePower\n";
    directory.WriteFile("case_8x8.config.yaml", runConfig);
    directory.WriteFile("case_8x16.config.yaml", runConfig);
    directory.WriteFile("case_16x8.config.yaml", runConfig);
    directory.WriteFile("case_16x16.config.yaml", runConfig);
    const std::filesystem::path testerPath = directory.WriteFile(
            "tester.yaml", MakeTesterConfig(directory.Path() / "results"));

    const SubarrayDimensionTestConfig config =
            SubarrayDimensionTester::LoadConfig(testerPath);
    assert(config.name == "focused_dimension_test");
    assert(config.rows == std::vector<int>({8, 16}));
    assert(config.columns == std::vector<int>({8, 16}));
    assert(config.threadsPerRun == 1);
    assert(config.summaryCsvPath == directory.Path() / "results/nested/summary.csv");

    const std::vector<SubarrayDimensionRunSpec> specs =
            SubarrayDimensionTester::BuildRunSpecs(config);
    assert(specs.size() == 4);
    assert(specs[0].Label() == "8x8");
    assert(specs[1].Label() == "8x16");
    assert(specs[2].Label() == "16x8");
    assert(specs[3].Label() == "16x16");
    assert(specs[0].optimizationTarget == "LeakagePower");
    assert(specs[0].configPath.filename() == "case_8x8.config.yaml");
    assert(specs[3].resultPath.filename() == "case_16x16_results.yaml");
}

void TestConfigValidationRejectsInvalidTesterInputs() {
    TestSupport::TemporaryDirectory directory("subarray-dimension-errors");
    const std::filesystem::path outputDirectory = directory.Path() / "results";

    const std::filesystem::path missingPlaceholder = directory.WriteFile(
            "missing-placeholder.yaml",
            "schema: subarray_dimension_test\n"
            "name: invalid\n"
            "config_pattern: config_{rows}.config.yaml\n"
            "rows: [8]\n"
            "columns: [8]\n"
            "output:\n"
            "  directory: results\n");
    TestSupport::AssertThrows<std::runtime_error>([&]() {
        (void)SubarrayDimensionTester::LoadConfig(missingPlaceholder);
    }, "must contain {rows} and {columns}");

    const std::filesystem::path bothSources = directory.WriteFile(
            "both-sources.yaml",
            ReplaceOnce(
                MakeTesterConfig(outputDirectory, "[8]", "[8]"),
                "rows: [8]",
                "base_config: " + kTcamBaseConfig.string() + "\nrows: [8]"));
    TestSupport::AssertThrows<std::runtime_error>([&]() {
        (void)SubarrayDimensionTester::LoadConfig(bothSources);
    }, "exactly one of config_pattern or base_config");

    const std::filesystem::path duplicate = directory.WriteFile(
            "duplicate.yaml", MakeTesterConfig(outputDirectory, "[8, 8]", "[8]"));
    TestSupport::AssertThrows<std::runtime_error>([&]() {
        (void)SubarrayDimensionTester::LoadConfig(duplicate);
    }, "duplicate value 8");

    const std::filesystem::path outOfRange = directory.WriteFile(
            "out-of-range.yaml", MakeTesterConfig(outputDirectory, "[4]", "[8]"));
    TestSupport::AssertThrows<std::runtime_error>([&]() {
        (void)SubarrayDimensionTester::LoadConfig(outOfRange);
    }, "between 8 and 512");

    const std::filesystem::path escapedSummary = directory.WriteFile(
            "escaped-summary.yaml",
            ReplaceOnce(
                MakeTesterConfig(outputDirectory, "[8]", "[8]"),
                "summary_csv: nested/summary.csv",
                "summary_csv: ../summary.csv"));
    TestSupport::AssertThrows<std::runtime_error>([&]() {
        (void)SubarrayDimensionTester::LoadConfig(escapedSummary);
    }, "must stay within output.directory");
}

void TestRunWritesPerDimensionYamlAndRawSiSummary() {
    TestSupport::TemporaryDirectory directory("subarray-dimension-run");
    const std::filesystem::path outputDirectory = directory.Path() / "results";
    const std::filesystem::path testerPath = directory.WriteFile(
            "tester.yaml", MakeBaseTesterConfig(outputDirectory));

    SubarrayDimensionTesterOptions options;
    options.configPath = testerPath.string();
    options.jobs = 2;
    options.stdoutOutput = true;
    options.variationPlots = false;

    TestSupport::StreamCapture capture(std::cout);
    const SubarrayDimensionTestSummary summary = SubarrayDimensionTester::Run(options);
    capture.Stop();
    assert(capture.Text().find("Subarray dimension test: base_dimension_test")
            != std::string::npos);
    assert(capture.Text().find("8x8  8x16") != std::string::npos);
    assert(capture.Text().find("All 2 configurations completed successfully")
            != std::string::npos);
    assert(summary.totalRuns == 2);
    assert(summary.completedRuns == 2);
    assert(summary.failedRuns == 0);
    assert(summary.summaryCsvPath == outputDirectory / "nested/summary.csv");

    const std::filesystem::path result8x8 =
            outputDirectory / "2FeFET_TCAM_match_8x8_results.yaml";
    const std::filesystem::path result8x16 =
            outputDirectory / "2FeFET_TCAM_match_8x16_results.yaml";
    assert(YAML::LoadFile(result8x8.string())["summary"]["area"]["subarray"]
            ["dimensions"].as<std::string>() == "8x8");
    assert(YAML::LoadFile(result8x16.string())["summary"]["area"]["subarray"]
            ["dimensions"].as<std::string>() == "8x16");

    const std::string csv = ReadFile(summary.summaryCsvPath);
    assert(csv.find("search_latency_s") != std::string::npos);
    assert(csv.find("exact_match_sense_margin_v") != std::string::npos);
    assert(csv.find("8,8,8,8,complete,1") != std::string::npos);
    assert(csv.find("8,16,16,16,complete,1") != std::string::npos);
}

void TestRunContinuesAfterDimensionMismatchAndReportsFailure() {
    TestSupport::TemporaryDirectory directory("subarray-dimension-failure");
    const std::filesystem::path outputDirectory = directory.Path() / "results";
    const std::filesystem::path architecture64x32 =
            kConfigDirectory / "2FeFET_MCAM_64x32.architecture.yaml";
    const std::filesystem::path zeroSenseConfig = McamTestConfig::WriteZeroSenseVariant(
            directory, kConfigDirectory / "2FeFET_MCAM.config.yaml");
    std::string zeroSenseRun = ReadFile(zeroSenseConfig);
    zeroSenseRun = ReplaceOnce(zeroSenseRun,
            "architecture: " + (directory.Path() / "mcam.architecture.yaml").string(),
            "architecture: " + architecture64x32.string());
    directory.WriteFile(
            "case_64x32.config.yaml",
            ReplaceOnce(zeroSenseRun, "name: 2FeFET_MCAM", "name: case_64x32"));
    directory.WriteFile(
            "case_64x64.config.yaml",
            ReplaceOnce(zeroSenseRun, "name: 2FeFET_MCAM", "name: case_64x64"));

    std::ostringstream tester;
    tester << "schema: subarray_dimension_test\n"
           << "name: mismatch_test\n"
           << "config_pattern: case_{rows}x{columns}.config.yaml\n"
           << "rows: [64]\n"
           << "columns: [32, 64]\n"
           << "output:\n"
           << "  directory: " << outputDirectory.string() << "\n";
    const std::filesystem::path testerPath =
            directory.WriteFile("mismatch-tester.yaml", tester.str());

    SubarrayDimensionTesterOptions options;
    options.configPath = testerPath.string();
    options.jobs = 2;
    options.stdoutOutput = false;
    options.variationPlots = false;

    TestSupport::StreamCapture errorCapture(std::cerr);
    const SubarrayDimensionTestSummary summary = SubarrayDimensionTester::Run(options);
    errorCapture.Stop();
    assert(summary.totalRuns == 2);
    assert(summary.completedRuns == 1);
    assert(summary.failedRuns == 1);
    assert(errorCapture.Text().find("1 of 2 configurations failed") != std::string::npos);

    const std::string csv = ReadFile(summary.summaryCsvPath);
    assert(csv.find("64,32,32,32,complete,1") != std::string::npos);
    assert(csv.find("64,64,0,0,failed,1") != std::string::npos);
    assert(csv.find("expected 64x64") != std::string::npos);
}

}  // namespace

int main() {
    TestLoadConfigAndBuildCartesianRunSpecs();
    TestConfigValidationRejectsInvalidTesterInputs();
    TestRunWritesPerDimensionYamlAndRawSiSummary();
    TestRunContinuesAfterDimensionMismatchAndReportsFailure();

    std::cout << "Subarray dimension tester tests passed" << std::endl;
    return 0;
}
