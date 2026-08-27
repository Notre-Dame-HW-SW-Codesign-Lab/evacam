#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <sys/wait.h>

#include <yaml-cpp/yaml.h>

#include "EvaCamRun.h"
#include "McamTestConfig.h"
#include "TestSupport.h"

namespace {

const char *kFixedArchitecture =
        "config/2FeFET_TCAM/2FeFET_TCAM_explicit_subarray.architecture.yaml";
const char *kExplorableArchitecture = "config/2FeFET_TCAM/2FeFET_TCAM.architecture.yaml";
const char *kCell = "config/2FeFET_TCAM/2FeFET_TCAM.cell.yaml";
const char *kTechnology = "config/lib/technology/cmos.legacy.yaml";
const char *kNonHtreeConfig =
        "config/2FeFET_TCAM/2FeFET_TCAM_non_h_tree.config.yaml";

std::string ShellQuote(const std::string &value) {
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') {
            quoted += "'\\\"'\\\"'";
        } else {
            quoted += character;
        }
    }
    return quoted + "'";
}

std::string MakeConfig(bool deepExploration, bool impossibleConstraints) {
    std::ostringstream config;
    config << "schema: config\n"
           << "name: run-boundary\n"
           << "architecture: " << std::filesystem::absolute(
                   deepExploration ? kExplorableArchitecture : kFixedArchitecture).string() << "\n"
           << "cell: " << std::filesystem::absolute(kCell).string() << "\n"
           << "technology: " << std::filesystem::absolute(kTechnology).string() << "\n"
           << "optimization:\n"
           << "  target: LeakagePower\n"
           << "  buffer_design: latency\n"
           << "  row_driver: latency\n"
           << "  priority_encoder: latency\n";
    if (deepExploration) {
        config << "  deep_exploration: true\n";
    }
    if (impossibleConstraints) {
        config << "design_constraints:\n"
               << "  enabled: true\n"
               << "  read_latency: 1e-12s\n"
               << "  write_latency: 1e-12s\n"
               << "  read_dynamic_energy: 1e-12J\n"
               << "  write_dynamic_energy: 1e-12J\n"
               << "  read_edp: 1e-24\n"
               << "  write_edp: 1e-24\n"
               << "  area: 1e-12\n"
               << "  leakage: 1e-12W\n";
    }
    return config.str();
}

int RunCommand(const std::string &command) {
    const int status = std::system(command.c_str());
    TestSupport::Require(status != -1, "could not start subprocess");
    TestSupport::Require(WIFEXITED(status), "subprocess did not exit normally");
    return WEXITSTATUS(status);
}

std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input(path);
    TestSupport::Require(input.is_open(), "could not open " + path.string());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void TestRunEvaCamQuietSuccessWritesOverrideAndRestoresStdout() {
    TestSupport::TemporaryDirectory directory("evacam-run-success");
    const std::filesystem::path configPath = directory.WriteFile(
            "run.config.yaml", MakeConfig(false, false));
    const std::filesystem::path yamlPath = directory.Path() / "chosen-output.yaml";

    TestSupport::StreamCapture capture(std::cout);
    EvaCamRunOptions options;
    options.configPath = configPath.string();
    options.threads = 0;  // RunEvaCam translates non-positive threads to one.
    options.outputYamlPath = yamlPath.string();
    options.writeYaml = true;
    options.stdoutOutput = false;
    const EvaCamRunResultDto result = RunEvaCam(options);
    std::cout << "stdout restored";
    capture.Stop();

    assert(capture.Text() == "stdout restored");
    assert(result.numSolutions > 0);
    assert(result.outputYamlPath == yamlPath.string());
    assert(result.explorationCsvPath.empty());
    assert(!result.bestResults.empty());
    const EvaCamDesignResultDto &design = result.bestResults.at("LeakagePower");
    assert(design.optimizationTarget == "LeakagePower");
    assert(design.summary.at("area.total.area_m2") > 0);
    assert(design.geometry.at("capacity_bits") > 0);
    assert(std::filesystem::exists(yamlPath));
    assert(YAML::LoadFile(yamlPath.string())["summary"]);
}

void TestRunEvaCamErrorRestoresStdout() {
    TestSupport::TemporaryDirectory directory("evacam-run-errors");
    TestSupport::StreamCapture capture(std::cout);
    EvaCamRunOptions missing;
    missing.configPath = (directory.Path() / "missing.config.yaml").string();
    missing.stdoutOutput = false;
    TestSupport::AssertThrows<std::runtime_error>([&missing]() {
        (void)RunEvaCam(missing);
    }, "does not exist");
    std::cout << "stdout restored after error";
    capture.Stop();
    assert(capture.Text() == "stdout restored after error");
}

void TestRunEvaCamSupportsNonHtreeSearchRouting() {
    TestSupport::TemporaryDirectory directory("evacam-run-non-htree");
    const std::filesystem::path yamlPath = directory.Path() / "non-htree-output.yaml";

    EvaCamRunOptions options;
    options.configPath = kNonHtreeConfig;
    options.threads = 1;
    options.outputYamlPath = yamlPath.string();
    options.writeYaml = true;
    options.stdoutOutput = false;
    const EvaCamRunResultDto result = RunEvaCam(options);

    assert(result.numSolutions > 0);
    assert(result.bestResults.count("SearchLatency") == 1);
    const EvaCamDesignResultDto &design = result.bestResults.at("SearchLatency");
    assert(design.summary.at("timing.search_latency_s") > 0);
    assert(design.summary.at("energy.search_dynamic_j") > 0);
    assert(design.breakdown.at("search_latency.non_h_tree_s") > 0);
    assert(design.breakdown.at("search_dynamic_energy.non_h_tree_j") > 0);
    assert(design.geometry.at("num_row_mat") == 2);
    assert(design.geometry.at("num_column_mat") == 2);

    const YAML::Node output = YAML::LoadFile(yamlPath.string());
    assert(output["assumptions"]["routing"].as<std::string>() == "non_h_tree");
    assert(output["summary"]["timing"]["search_latency"].IsScalar());
    assert(output["summary"]["timing"]["search_latency_breakdown"]["non_h_tree"].IsScalar());
    assert(output["summary"]["power"]["search_dynamic_energy"].IsScalar());
    assert(output["summary"]["power"]["search_dynamic_energy_breakdown"]["non_h_tree"].IsScalar());
}

void TestExecutableHelpDiagnosticsQuietAndOutputPath() {
    TestSupport::TemporaryDirectory directory("evacam-executable-boundary");
    const std::filesystem::path executable = std::filesystem::absolute("EvaCAM");
    TestSupport::Require(std::filesystem::exists(executable), "EvaCAM executable must be built first");
    const std::filesystem::path helpOutput = directory.Path() / "help.txt";
    const std::filesystem::path errorOutput = directory.Path() / "error.txt";
    const std::filesystem::path configPath = directory.WriteFile(
            "run.config.yaml", MakeConfig(false, false));
    const std::filesystem::path yamlPath = directory.Path() / "cli-output.yaml";
    const std::filesystem::path quietOutput = directory.Path() / "quiet.txt";

    assert(RunCommand(ShellQuote(executable.string()) + " --help > "
            + ShellQuote(helpOutput.string()) + " 2>&1") == 0);
    assert(ReadFile(helpOutput).find("Usage:") != std::string::npos);

    assert(RunCommand(ShellQuote(executable.string()) + " > "
            + ShellQuote(errorOutput.string()) + " 2>&1") == 1);
    const std::string diagnostic = ReadFile(errorOutput);
    assert(diagnostic.find("Missing configuration file.") != std::string::npos);
    assert(diagnostic.find("Usage:") != std::string::npos);

    assert(RunCommand(ShellQuote(executable.string()) + " -q -t 1 -o "
            + ShellQuote(yamlPath.string()) + " " + ShellQuote(configPath.string()) + " > "
            + ShellQuote(quietOutput.string()) + " 2>&1") == 0);
    assert(ReadFile(quietOutput).empty());
    assert(std::filesystem::exists(yamlPath));
    assert(YAML::LoadFile(yamlPath.string())["summary"]);
}

void TestExecutableSubarrayDimensionTestMode() {
    TestSupport::TemporaryDirectory directory("evacam-dimension-mode");
    const std::filesystem::path executable = std::filesystem::absolute("EvaCAM");
    const std::filesystem::path configDirectory =
            std::filesystem::absolute("config/2FeFET_MCAM");
    const std::filesystem::path outputDirectory = directory.Path() / "results";
    const std::filesystem::path mcamSource = McamTestConfig::WriteMcamConfigVariant(
            directory, configDirectory / "2FeFET_MCAM.config.yaml");
    const std::filesystem::path mcamConfig = directory.WriteFile(
            "2FeFET_MCAM.config.yaml", ReadFile(mcamSource));
    std::ostringstream tester;
    tester << "schema: subarray_dimension_test\n"
           << "name: executable_boundary\n"
           << "base_config: "
           << mcamConfig.string() << "\n"
           << "rows: [8]\n"
           << "columns: [8]\n"
           << "threads_per_run: 1\n"
           << "output:\n"
           << "  directory: " << outputDirectory.string() << "\n";
    const std::filesystem::path testerPath =
            directory.WriteFile("tester.yaml", tester.str());
    const std::filesystem::path consolePath = directory.Path() / "console.txt";

    assert(RunCommand(ShellQuote(executable.string())
            + " --subarray-dimension-test --threads 1 "
            + ShellQuote(testerPath.string()) + " > "
            + ShellQuote(consolePath.string()) + " 2>&1") == 0);
    const std::string console = ReadFile(consolePath);
    assert(console.find("Subarray dimension test: executable_boundary")
            != std::string::npos);
    assert(console.find("All 1 configurations completed successfully")
            != std::string::npos);
    assert(std::filesystem::exists(outputDirectory / "summary.csv"));
    assert(std::filesystem::exists(
            outputDirectory / "2FeFET_MCAM_8x8_results.yaml"));

    assert(RunCommand(ShellQuote(executable.string())
            + " --subarray-dimension-test --output ignored.yaml "
            + ShellQuote(testerPath.string()) + " > "
            + ShellQuote(consolePath.string()) + " 2>&1") == 1);
    assert(ReadFile(consolePath).find(
            "--output is not supported in subarray dimension test mode")
            != std::string::npos);
}

}  // namespace

int main() {
    TestRunEvaCamQuietSuccessWritesOverrideAndRestoresStdout();
    TestRunEvaCamErrorRestoresStdout();
    TestRunEvaCamSupportsNonHtreeSearchRouting();
    TestExecutableHelpDiagnosticsQuietAndOutputPath();
    TestExecutableSubarrayDimensionTestMode();
    std::cout << "RunEvaCam boundary tests passed\n";
    return 0;
}
