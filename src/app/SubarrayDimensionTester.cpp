#include "SubarrayDimensionTester.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "EvaCamRun.h"
#include "Logger.h"
#include "config/OutputFileLock.h"
#include "input/YamlNodeHelpers.h"

namespace {

constexpr int kMinimumDimension = 8;
constexpr int kMaximumDimension = 512;

struct RunOutcome {
    std::string status = "failed";
    long long numSolutions = 0;
    int comparisonColumns = 0;
    int wordWidth = 0;
    int bitsPerCell = 1;
    double elapsedSeconds = 0;
    double searchLatency = 0;
    double exactMatchSenseMargin = 0;
    double minimumRequiredSenseMargin = 0;
    double totalArea = 0;
    std::string message;
};

std::filesystem::path AbsoluteNormalized(const std::filesystem::path &path) {
    return std::filesystem::absolute(path).lexically_normal();
}

std::filesystem::path ResolveReference(
        const std::filesystem::path &owner,
        const std::filesystem::path &reference) {
    if (reference.is_absolute()) {
        return reference.lexically_normal();
    }
    return AbsoluteNormalized(owner).parent_path() / reference;
}

void ValidateRegularFile(const std::filesystem::path &path, const std::string &label) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(label + " does not exist: " + path.string());
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error(label + " is not a regular file: " + path.string());
    }
}

std::vector<int> ReadDimensions(const YAML::Node &root, const char *key) {
    const YAML::Node values = YamlHelpers::child_required(root, key);
    if (!values.IsSequence() || values.size() == 0) {
        throw std::runtime_error(
                std::string("[SubarrayDimensionTester] Error: ") + key
                + " must be a non-empty sequence.");
    }

    std::vector<int> dimensions;
    dimensions.reserve(values.size());
    std::unordered_set<int> seen;
    for (std::size_t index = 0; index < values.size(); index++) {
        const int value = YamlHelpers::read_required_index<int>(values, index, key);
        if (value < kMinimumDimension || value > kMaximumDimension) {
            throw std::runtime_error(
                    std::string("[SubarrayDimensionTester] Error: ") + key
                    + " values must be between " + std::to_string(kMinimumDimension)
                    + " and " + std::to_string(kMaximumDimension) + ".");
        }
        if (!seen.insert(value).second) {
            throw std::runtime_error(
                    std::string("[SubarrayDimensionTester] Error: ") + key
                    + " contains duplicate value " + std::to_string(value) + ".");
        }
        dimensions.push_back(value);
    }
    return dimensions;
}

void ReplaceAll(std::string &value, const std::string &needle, const std::string &replacement) {
    std::size_t position = 0;
    while ((position = value.find(needle, position)) != std::string::npos) {
        value.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

std::string ExpandConfigPattern(const std::string &pattern, int rows, int columns) {
    std::string expanded = pattern;
    ReplaceAll(expanded, "{rows}", std::to_string(rows));
    ReplaceAll(expanded, "{columns}", std::to_string(columns));
    if (expanded.find('{') != std::string::npos || expanded.find('}') != std::string::npos) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: config_pattern contains an unsupported placeholder.");
    }
    return expanded;
}

std::string ResultStem(const std::filesystem::path &configPath) {
    const std::string filename = configPath.filename().string();
    const std::string suffix = ".config.yaml";
    if (filename.size() >= suffix.size()
            && filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return filename.substr(0, filename.size() - suffix.size());
    }
    return configPath.stem().string();
}

bool PathStaysWithin(
        const std::filesystem::path &parent,
        const std::filesystem::path &candidate) {
    const std::filesystem::path relative = candidate.lexically_relative(parent);
    if (relative.empty()) {
        return candidate == parent;
    }
    return *relative.begin() != "..";
}

double RequiredMetric(
        const std::unordered_map<std::string, double> &metrics,
        const std::string &key) {
    const auto found = metrics.find(key);
    if (found == metrics.end()) {
        throw std::runtime_error("run result is missing metric " + key);
    }
    return found->second;
}

RunOutcome RunOne(
        const SubarrayDimensionRunSpec &spec,
        const SubarrayDimensionTesterOptions &testerOptions,
        int threadsPerRun) {
    RunOutcome outcome;
    const auto start = std::chrono::steady_clock::now();
    try {
        EvaCamRunOptions runOptions;
        runOptions.configPath = spec.configPath.string();
        runOptions.threads = threadsPerRun;
        runOptions.outputYamlPath = spec.resultPath.string();
        if (spec.overrideDimensions) {
            runOptions.subarrayRows = spec.rows;
            runOptions.subarrayColumns = spec.columns;
        }
        runOptions.writeYaml = true;
        runOptions.stdoutOutput = testerOptions.stdoutOutput && testerOptions.verbose;
        runOptions.verbose = testerOptions.verbose;
        runOptions.variationPlots = testerOptions.variationPlots;

        const EvaCamRunResultDto result = RunEvaCam(runOptions);
        outcome.numSolutions = result.numSolutions;
        if (result.numSolutions <= 0) {
            throw std::runtime_error("EvaCAM reported no valid solutions");
        }
        const auto selectedResult = result.bestResults.find(spec.optimizationTarget);
        if (selectedResult == result.bestResults.end()) {
            throw std::runtime_error(
                    "run result does not contain optimization target "
                    + spec.optimizationTarget);
        }

        const EvaCamDesignResultDto &design = selectedResult->second;
        const int reportedRows = static_cast<int>(std::llround(
                RequiredMetric(design.geometry, "subarray_rows")));
        const int reportedColumns = static_cast<int>(std::llround(
                RequiredMetric(design.geometry, "subarray_columns")));
        if (reportedRows != spec.rows || reportedColumns != spec.columns) {
            throw std::runtime_error(
                    "result reported subarray dimensions "
                    + std::to_string(reportedRows) + "x" + std::to_string(reportedColumns)
                    + "; expected " + spec.Label());
        }

        outcome.comparisonColumns = static_cast<int>(std::llround(
                RequiredMetric(design.geometry, "comparison_columns_per_step")));
        outcome.wordWidth = static_cast<int>(std::llround(
                RequiredMetric(design.geometry, "logical_word_width_bits")));
        outcome.bitsPerCell = static_cast<int>(std::llround(
                RequiredMetric(design.geometry, "bits_per_cell")));
        if (outcome.comparisonColumns != spec.columns) {
            throw std::runtime_error(
                    "result reported comparison-column width "
                    + std::to_string(outcome.comparisonColumns)
                    + "; expected column count " + std::to_string(spec.columns));
        }
        const int availableWordBits = spec.columns * outcome.bitsPerCell;
        if (outcome.wordWidth > availableWordBits) {
            throw std::runtime_error(
                    "result reported logical word width "
                    + std::to_string(outcome.wordWidth)
                    + ", which exceeds the " + std::to_string(availableWordBits)
                    + " bits available from " + std::to_string(spec.columns)
                    + " columns at " + std::to_string(outcome.bitsPerCell)
                    + " bits per cell");
        }

        outcome.searchLatency = RequiredMetric(design.summary, "timing.search_latency_s");
        outcome.exactMatchSenseMargin = RequiredMetric(
                design.summary, "timing.exact_match_sense_margin_v");
        outcome.minimumRequiredSenseMargin = RequiredMetric(
                design.summary, "timing.minimum_required_sense_margin_v");
        outcome.totalArea = RequiredMetric(design.summary, "area.total.area_m2");
        outcome.status = "complete";
    } catch (const std::exception &error) {
        outcome.status = "failed";
        outcome.message = error.what();
    }
    outcome.elapsedSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
    return outcome;
}

std::string CsvEscape(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char character : value) {
        if (character == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

std::string FormatDouble(double value) {
    std::ostringstream formatted;
    formatted << std::setprecision(17) << value;
    return formatted.str();
}

std::filesystem::path DisplayPath(const std::filesystem::path &path) {
    const std::filesystem::path absolute = AbsoluteNormalized(path);
    const std::filesystem::path relative = absolute.lexically_relative(
            AbsoluteNormalized(std::filesystem::current_path()));
    if (!relative.empty() && *relative.begin() != "..") {
        return relative;
    }
    return absolute;
}

void WriteSummaryCsv(
        const SubarrayDimensionTestConfig &config,
        const std::vector<SubarrayDimensionRunSpec> &specs,
        const std::vector<RunOutcome> &outcomes) {
    std::filesystem::create_directories(config.summaryCsvPath.parent_path());
    const std::filesystem::path temporary = config.summaryCsvPath.string() + ".tmp";
    std::ofstream output(temporary);
    if (!output) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: cannot open summary CSV: "
                + temporary.string());
    }

    output << "rows,columns,comparison_columns_per_step,logical_word_width_bits,"
              "status,num_solutions,bits_per_cell,elapsed_seconds,config,result,"
              "search_latency_s,exact_match_sense_margin_v,"
              "minimum_required_sense_margin_v,total_area_m2,message\n";
    for (std::size_t index = 0; index < specs.size(); index++) {
        const SubarrayDimensionRunSpec &spec = specs[index];
        const RunOutcome &outcome = outcomes[index];
        output << spec.rows << ','
               << spec.columns << ','
               << outcome.comparisonColumns << ','
               << outcome.wordWidth << ','
               << outcome.status << ','
               << outcome.numSolutions << ','
               << outcome.bitsPerCell << ','
               << FormatDouble(outcome.elapsedSeconds) << ','
               << CsvEscape(DisplayPath(spec.configPath).string()) << ','
               << CsvEscape(DisplayPath(spec.resultPath).string()) << ',';
        if (outcome.status == "complete") {
            output << FormatDouble(outcome.searchLatency) << ','
                   << FormatDouble(outcome.exactMatchSenseMargin) << ','
                   << FormatDouble(outcome.minimumRequiredSenseMargin) << ','
                   << FormatDouble(outcome.totalArea) << ',';
        } else {
            output << ",,,,";
        }
        output << CsvEscape(outcome.message) << '\n';
    }
    output.close();
    if (!output) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: failed writing summary CSV: "
                + temporary.string());
    }
    std::filesystem::rename(temporary, config.summaryCsvPath);
}

void PrintPlan(
        const SubarrayDimensionTestConfig &config,
        const std::vector<SubarrayDimensionRunSpec> &specs) {
    std::lock_guard<std::mutex> lock(Logger::OutputMutex());
    std::cout << "Subarray dimension test: " << config.name
              << " (" << specs.size() << " configurations)" << std::endl;
    for (const int rows : config.rows) {
        std::cout << "  ";
        for (std::size_t index = 0; index < config.columns.size(); index++) {
            if (index > 0) {
                std::cout << "  ";
            }
            std::cout << rows << "x" << config.columns[index];
        }
        std::cout << std::endl;
    }
}

}  // namespace

std::string SubarrayDimensionRunSpec::Label() const {
    return std::to_string(rows) + "x" + std::to_string(columns);
}

SubarrayDimensionTestConfig SubarrayDimensionTester::LoadConfig(
        const std::filesystem::path &configPath) {
    const std::filesystem::path normalizedConfigPath = AbsoluteNormalized(configPath);
    ValidateRegularFile(normalizedConfigPath, "Subarray dimension tester config");

    std::lock_guard<std::recursive_mutex> parserLock(YamlHelpers::ParserMutex());
    const YAML::Node root = YAML::LoadFile(normalizedConfigPath.string());
    YamlHelpers::require_schema(
            root, "subarray_dimension_test", "subarray dimension tester config");
    YamlHelpers::reject_unknown_keys(
            root,
            {"schema", "name", "config_pattern", "base_config", "rows", "columns",
             "threads_per_run", "output"},
            "subarray dimension tester config");

    SubarrayDimensionTestConfig config;
    config.name = YamlHelpers::read_required<std::string>(root, "name");
    if (config.name.empty()) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: name must not be empty.");
    }
    config.configPath = normalizedConfigPath;
    config.configPattern = YamlHelpers::read_optional<std::string>(
            root, "config_pattern", "");
    const std::string baseConfigReference = YamlHelpers::read_optional<std::string>(
            root, "base_config", "");
    if (config.configPattern.empty() == baseConfigReference.empty()) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: specify exactly one of "
                "config_pattern or base_config.");
    }
    if (!config.configPattern.empty()
            && (config.configPattern.find("{rows}") == std::string::npos
                || config.configPattern.find("{columns}") == std::string::npos)) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: config_pattern must contain "
                "{rows} and {columns}.");
    }
    if (!baseConfigReference.empty()) {
        config.baseConfigPath = ResolveReference(
                normalizedConfigPath, baseConfigReference).lexically_normal();
        ValidateRegularFile(config.baseConfigPath, "Dimension base config");
    }
    config.rows = ReadDimensions(root, "rows");
    config.columns = ReadDimensions(root, "columns");
    config.threadsPerRun = YamlHelpers::read_optional<int>(root, "threads_per_run", 1);
    if (config.threadsPerRun <= 0) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: threads_per_run must be positive.");
    }

    const YAML::Node output = YamlHelpers::child_required(root, "output");
    YamlHelpers::reject_unknown_keys(output, {"directory", "summary_csv"}, "output");
    const std::filesystem::path outputReference =
            YamlHelpers::read_required<std::string>(output, "directory");
    if (outputReference.empty()) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: output.directory must not be empty.");
    }
    config.outputDirectory = ResolveReference(normalizedConfigPath, outputReference)
            .lexically_normal();

    const std::filesystem::path summaryReference =
            YamlHelpers::read_optional<std::string>(output, "summary_csv", "summary.csv");
    if (summaryReference.empty() || summaryReference.is_absolute()) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: output.summary_csv must be a non-empty "
                "path relative to output.directory.");
    }
    config.summaryCsvPath = (config.outputDirectory / summaryReference).lexically_normal();
    if (!PathStaysWithin(config.outputDirectory, config.summaryCsvPath)) {
        throw std::runtime_error(
                "[SubarrayDimensionTester] Error: output.summary_csv must stay within "
                "output.directory.");
    }
    return config;
}

std::vector<SubarrayDimensionRunSpec> SubarrayDimensionTester::BuildRunSpecs(
        const SubarrayDimensionTestConfig &config) {
    std::vector<SubarrayDimensionRunSpec> specs;
    specs.reserve(config.rows.size() * config.columns.size());
    std::unordered_set<std::string> configPaths;
    std::unordered_set<std::string> resultPaths;
    for (const int rows : config.rows) {
        for (const int columns : config.columns) {
            const bool overrideDimensions = !config.baseConfigPath.empty();
            std::filesystem::path runConfigPath = config.baseConfigPath;
            if (!overrideDimensions) {
                const std::filesystem::path configReference =
                        ExpandConfigPattern(config.configPattern, rows, columns);
                runConfigPath = ResolveReference(
                        config.configPath, configReference).lexically_normal();
                ValidateRegularFile(runConfigPath, "Dimension run config");
            }
            if (!overrideDimensions && !configPaths.insert(runConfigPath.string()).second) {
                throw std::runtime_error(
                        "[SubarrayDimensionTester] Error: config_pattern maps multiple "
                        "dimensions to " + runConfigPath.string() + ".");
            }

            std::string resultStem = ResultStem(runConfigPath);
            if (overrideDimensions) {
                resultStem += "_" + std::to_string(rows) + "x"
                        + std::to_string(columns);
            }
            const std::filesystem::path resultPath = config.outputDirectory
                    / (resultStem + "_results.yaml");
            if (!resultPaths.insert(resultPath.string()).second) {
                throw std::runtime_error(
                        "[SubarrayDimensionTester] Error: multiple runs map to result path "
                        + resultPath.string() + ".");
            }

            std::string optimizationTarget;
            {
                std::lock_guard<std::recursive_mutex> parserLock(YamlHelpers::ParserMutex());
                const YAML::Node runRoot = YAML::LoadFile(runConfigPath.string());
                YamlHelpers::require_schema(runRoot, "config", "dimension run config");
                const YAML::Node optimization =
                        YamlHelpers::child_required(runRoot, "optimization");
                optimizationTarget = YamlHelpers::read_required<std::string>(
                        optimization, "target");
            }
            specs.push_back({rows, columns, runConfigPath, resultPath,
                    optimizationTarget, overrideDimensions});
        }
    }
    return specs;
}

SubarrayDimensionTestSummary SubarrayDimensionTester::Run(
        const SubarrayDimensionTesterOptions &options) {
    const SubarrayDimensionTestConfig config = LoadConfig(options.configPath);
    const std::vector<SubarrayDimensionRunSpec> specs = BuildRunSpecs(config);
    if (options.stdoutOutput) {
        PrintPlan(config, specs);
    }

    std::filesystem::create_directories(config.outputDirectory);
    auto summaryLock = OutputFileLock::Acquire(config.summaryCsvPath.string());
    std::vector<RunOutcome> outcomes(specs.size());
    std::atomic<std::size_t> nextIndex{0};
    std::atomic<int> finishedRuns{0};
    const int requestedJobs = options.jobs > 0 ? options.jobs : 1;
    const int workerCount = std::min<int>(requestedJobs, static_cast<int>(specs.size()));

    std::vector<std::thread> workers;
    workers.reserve(workerCount);
    for (int worker = 0; worker < workerCount; worker++) {
        workers.emplace_back([&]() {
            while (true) {
                const std::size_t index = nextIndex.fetch_add(1);
                if (index >= specs.size()) {
                    return;
                }
                outcomes[index] = RunOne(specs[index], options, config.threadsPerRun);
                const int finished = finishedRuns.fetch_add(1) + 1;
                if (options.stdoutOutput) {
                    std::lock_guard<std::mutex> lock(Logger::OutputMutex());
                    std::cout << '[' << finished << '/' << specs.size() << "] "
                              << specs[index].Label() << ": " << outcomes[index].status
                              << std::endl;
                }
            }
        });
    }
    for (std::thread &worker : workers) {
        worker.join();
    }

    WriteSummaryCsv(config, specs, outcomes);
    SubarrayDimensionTestSummary summary;
    summary.totalRuns = static_cast<int>(outcomes.size());
    summary.summaryCsvPath = config.summaryCsvPath;
    for (const RunOutcome &outcome : outcomes) {
        if (outcome.status == "complete") {
            summary.completedRuns++;
        } else {
            summary.failedRuns++;
        }
    }

    std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
    if (summary.failedRuns > 0) {
        std::cerr << "[SubarrayDimensionTester] " << summary.failedRuns << " of "
                  << summary.totalRuns << " configurations failed. Summary: "
                  << DisplayPath(summary.summaryCsvPath).string() << std::endl;
    } else if (options.stdoutOutput) {
        std::cout << "All " << summary.totalRuns
                  << " configurations completed successfully. Summary: "
                  << DisplayPath(summary.summaryCsvPath).string() << std::endl;
    }
    return summary;
}
