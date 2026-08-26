#include "SubarrayDimensionTester.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>

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
         << "config_pattern: "
         << (kConfigDirectory / "2FeFET_MCAM_{rows}x{columns}.config.yaml").string()
         << "\n"
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

std::string MakeRunConfig(
        const std::string &name,
        const std::filesystem::path &architecture) {
    std::ostringstream yaml;
    yaml << "schema: config\n"
         << "name: " << name << "\n"
         << "architecture: " << architecture.string() << "\n"
         << "cell: " << (kConfigDirectory / "2FeFET_MCAM.cell.yaml").string() << "\n"
         << "technology: "
         << std::filesystem::absolute("config/lib/technology/cmos.legacy.yaml").string()
         << "\n"
         << "optimization:\n"
         << "  target: LeakagePower\n"
         << "  buffer_design: latency\n"
         << "  row_driver: latency\n"
         << "  priority_encoder: latency\n";
    return yaml.str();
}

void TestLoadConfigAndBuildCartesianRunSpecs() {
    TestSupport::TemporaryDirectory directory("subarray-dimension-config");
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
    assert(specs[0].configPath.filename() == "2FeFET_MCAM_8x8.config.yaml");
    assert(specs[3].resultPath.filename() == "2FeFET_MCAM_16x16_results.yaml");
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
    const std::filesystem::path architecture8x8 =
            kConfigDirectory / "2FeFET_MCAM_8x8.architecture.yaml";
    directory.WriteFile("case_8x8.config.yaml", MakeRunConfig("case_8x8", architecture8x8));
    directory.WriteFile("case_8x16.config.yaml", MakeRunConfig("case_8x16", architecture8x8));

    std::ostringstream tester;
    tester << "schema: subarray_dimension_test\n"
           << "name: mismatch_test\n"
           << "config_pattern: case_{rows}x{columns}.config.yaml\n"
           << "rows: [8]\n"
           << "columns: [8, 16]\n"
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
    assert(csv.find("8,8,8,8,complete,1") != std::string::npos);
    assert(csv.find("8,16,0,0,failed,1") != std::string::npos);
    assert(csv.find("expected 8x16") != std::string::npos);
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
