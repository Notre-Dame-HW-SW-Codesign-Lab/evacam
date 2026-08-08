#include <array>
#include <atomic>
#include <cassert>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include "EvaCAM_Match.h"
#include "EvaCamRun.h"
#include "TestSupport.h"

namespace {

constexpr std::size_t kConcurrentRuns = 4;
const char *kConfigPath =
    "config/2FeFET_TCAM/2FeFET_TCAM.config.yaml";

struct ConcurrentRunResults {
    std::array<EvaCamRunResultDto, kConcurrentRuns> results;
    std::string output;
};

ConcurrentRunResults RunConcurrently(
        const std::array<bool, kConcurrentRuns> &stdoutOptions,
        const std::filesystem::path &outputDirectory = {}) {
    std::array<EvaCamRunResultDto, kConcurrentRuns> results;
    std::array<std::exception_ptr, kConcurrentRuns> errors;
    std::array<std::thread, kConcurrentRuns> callers;
    std::atomic<int> ready = 0;
    std::atomic<bool> start = false;

    TestSupport::StreamCapture capture(std::cout);
    for (std::size_t index = 0; index < callers.size(); ++index) {
        callers[index] = std::thread([&results, &errors, &ready, &start,
                &stdoutOptions, &outputDirectory, index]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            try {
                EvaCamRunOptions options;
                options.configPath = kConfigPath;
                options.threads = 2;
                options.stdoutOutput = stdoutOptions[index];
                if (!outputDirectory.empty()) {
                    options.writeYaml = true;
                    options.outputYamlPath = (outputDirectory
                            / ("run-" + std::to_string(index) + ".yaml")).string();
                }
                results[index] = RunEvaCam(options);
            } catch (...) {
                errors[index] = std::current_exception();
            }
        });
    }
    while (ready.load(std::memory_order_acquire)
            != static_cast<int>(callers.size())) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (std::thread &caller : callers) {
        caller.join();
    }
    capture.Stop();

    for (const std::exception_ptr &error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
    return {std::move(results), capture.Text()};
}

void AssertEquivalentResults(
        const std::array<EvaCamRunResultDto, kConcurrentRuns> &results,
        bool expectOutputFiles = false) {
    const EvaCamRunResultDto &baseline = results.front();
    TestSupport::Require(baseline.numSolutions > 0,
            "concurrent Python boundary run found no solution");
    TestSupport::Require(!baseline.bestResults.empty(),
            "concurrent Python boundary run returned no best results");

    for (const EvaCamRunResultDto &result : results) {
        assert(result.numSolutions == baseline.numSolutions);
        assert(result.explorationCsvPath == baseline.explorationCsvPath);
        assert(expectOutputFiles
                ? !result.outputYamlPath.empty()
                : result.outputYamlPath.empty());
        assert(result.bestResults.size() == baseline.bestResults.size());
        for (const auto &[target, expected] : baseline.bestResults) {
            const EvaCamDesignResultDto &actual = result.bestResults.at(target);
            assert(actual.optimizationTarget == expected.optimizationTarget);
            assert(actual.summary == expected.summary);
            assert(actual.breakdown == expected.breakdown);
            assert(actual.geometry == expected.geometry);
        }
    }
}

void TestConcurrentQuietRunsDoNotRedirectProcessStdout() {
    const ConcurrentRunResults runs = RunConcurrently({false, false, false, false});
    assert(runs.output.empty());
    AssertEquivalentResults(runs.results);
}

void TestVisibleRunDoesNotExposeConcurrentQuietRuns() {
    const ConcurrentRunResults runs = RunConcurrently({true, false, false, false});
    const std::string marker = "DESIGN SPECIFICATION";
    const std::size_t first = runs.output.find(marker);
    assert(first != std::string::npos);
    assert(runs.output.find(marker, first + marker.size()) == std::string::npos);
    AssertEquivalentResults(runs.results);
}

void TestConcurrentRunsWriteIndependentYamlFiles() {
    TestSupport::TemporaryDirectory directory("evacam-concurrent-output");
    const ConcurrentRunResults runs = RunConcurrently(
            {false, false, false, false}, directory.Path());
    assert(runs.output.empty());
    AssertEquivalentResults(runs.results, true);
    for (const EvaCamRunResultDto &result : runs.results) {
        assert(std::filesystem::exists(result.outputYamlPath));
        assert(std::filesystem::file_size(result.outputYamlPath) > 0);
    }
}

void TestConcurrentRunAndMatcherConfigurationLoads() {
    constexpr std::size_t callerCount = 4;
    std::array<std::exception_ptr, callerCount> errors;
    std::array<std::thread, callerCount> callers;
    std::atomic<int> ready = 0;
    std::atomic<bool> start = false;

    for (std::size_t index = 0; index < callers.size(); ++index) {
        callers[index] = std::thread([&errors, &ready, &start, index]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            try {
                if (index < 2) {
                    EvaCamRunOptions options;
                    options.configPath = kConfigPath;
                    options.threads = 2;
                    options.stdoutOutput = false;
                    const EvaCamRunResultDto result = RunEvaCam(options);
                    TestSupport::Require(result.numSolutions > 0,
                            "mixed concurrent run found no solution");
                } else {
                    EvaCAM_Match matcher(kConfigPath);
                    TestSupport::Require(matcher.word_width() > 0,
                            "mixed concurrent matcher has no word width");
                }
            } catch (...) {
                errors[index] = std::current_exception();
            }
        });
    }
    while (ready.load(std::memory_order_acquire)
            != static_cast<int>(callers.size())) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (std::thread &caller : callers) {
        caller.join();
    }
    for (const std::exception_ptr &error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
}

}  // namespace

int main() {
    TestConcurrentQuietRunsDoNotRedirectProcessStdout();
    TestVisibleRunDoesNotExposeConcurrentQuietRuns();
    TestConcurrentRunsWriteIndependentYamlFiles();
    TestConcurrentRunAndMatcherConfigurationLoads();
    std::cout << "RunEvaCam concurrency tests passed" << std::endl;
    return 0;
}
