#include <algorithm>
#include <cassert>
#include <exception>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "EvaCamConfig.h"
#include "EvaCamContextBuilder.h"
#include "Logger.h"
#include "TestSupport.h"

namespace {

const char *kConfigFile = "config/PCM-2T2R-JSSC11/PCM-2T2R-JSSC11.config.yaml";

std::vector<std::string> Lines(const std::string &text) {
    std::istringstream input(text);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    return lines;
}

void TestLoggerOutputAndManipulators() {
    TestSupport::StreamCapture capture(std::cout);

    Logger disabled;
    assert(!disabled.IsVerbose());
    disabled.Verbose() << "hidden";
    disabled.Log() << "always shown";
    disabled.SetVerbose(true);
    assert(disabled.IsVerbose());
    disabled.Verbose() << "shown " << std::hex << 255 << std::dec << ' ' << 10;
    disabled.Verbose() << "manipulator" << std::endl;

    const std::vector<std::string> lines = Lines(capture.Text());
    assert(lines.size() == 4);
    assert(lines[0] == "always shown");
    assert(lines[1] == "shown ff 10");
    assert(lines[2] == "manipulator");
    assert(lines[3].empty());
}

void TestLoggerMoveFlushesOnce() {
    TestSupport::StreamCapture capture(std::cout);

    Logger logger;
    {
        Logger::Line original = logger.Log();
        original << "moved line";
        Logger::Line moved(std::move(original));
    }

    assert(capture.Text() == "moved line\n");
}

void TestLoggerConcurrentWritesAreCompleteLines() {
    TestSupport::StreamCapture capture(std::cout);
    Logger logger;
    constexpr int kThreads = 8;
    constexpr int kLinesPerThread = 40;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int thread = 0; thread < kThreads; thread++) {
        threads.emplace_back([&logger, thread]() {
            for (int line = 0; line < kLinesPerThread; line++) {
                logger.Log() << "thread=" << thread << ",line=" << line;
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }

    const std::vector<std::string> lines = Lines(capture.Text());
    assert(lines.size() == static_cast<size_t>(kThreads * kLinesPerThread));
    std::set<std::string> actual(lines.begin(), lines.end());
    assert(actual.size() == lines.size());
    for (int thread = 0; thread < kThreads; thread++) {
        for (int line = 0; line < kLinesPerThread; line++) {
            assert(actual.count("thread=" + std::to_string(thread)
                    + ",line=" + std::to_string(line)) == 1);
        }
    }
}

void TestContextBuilderRejectsMissingAndUnreadableInputs() {
    CliOptions missing;
    missing.inputFileName = "missing-context-builder-config.yaml";
    TestSupport::AssertThrows<std::runtime_error>([&missing]() {
        EvaCamContextBuilder::Build(missing);
    }, "does not exist");

    TestSupport::TemporaryDirectory temporaryDirectory;
    CliOptions unreadable;
    unreadable.inputFileName = temporaryDirectory.Path().string();
    TestSupport::AssertThrows<std::runtime_error>([&unreadable]() {
        EvaCamContextBuilder::Build(unreadable);
    }, "not a regular file");
}

void TestContextBuilderLoadsOwnedNormalizedConfigAndOptions() {
    CliOptions options;
    options.inputFileName = kConfigFile;
    options.outputYamlFileName = "custom-results.yaml";
    options.threads = 7;
    options.variationPlots = false;

    EvaCamContext context = EvaCamContextBuilder::Build(options);
    assert(context.config);
    assert(context.inputFileName == options.inputFileName);
    assert(context.outputYamlFileName == options.outputYamlFileName);
    assert(!context.config->variationPlots);
    assert(context.config->technology.tech);
    assert(context.config->technology.cell);

    // The loader preserves and resolves the explicit 4-by-2 mat organization.
    assert(!context.config->exploration.deepExploration);
    assert(context.config->resolvedExploration.geometry.numRowMatValues
            == std::vector<int>({4}));
    assert(context.config->resolvedExploration.geometry.numColumnMatValues
            == std::vector<int>({2}));
    assert(context.config->input.capacity == 128 * 1024);
    assert(context.config->input.wordWidth == 64);
}

void TestContextBuilderDefaultOutputAndVerboseOverride() {
    CliOptions options;
    options.inputFileName = kConfigFile;
    options.verbose = true;

    TestSupport::StreamCapture capture(std::cout);
    EvaCamContext context = EvaCamContextBuilder::Build(options);

    assert(context.outputYamlFileName == "results/PCM-2T2R-JSSC11_results.yaml");
    assert(context.config->logger.IsVerbose());
    const std::string output = capture.Text();
    assert(output.find("Verbose output enabled") != std::string::npos);
    assert(output.find("User-defined configuration file") != std::string::npos);
}

}  // namespace

int main() {
    TestLoggerOutputAndManipulators();
    TestLoggerMoveFlushesOnce();
    TestLoggerConcurrentWritesAreCompleteLines();
    TestContextBuilderRejectsMissingAndUnreadableInputs();
    TestContextBuilderLoadsOwnedNormalizedConfigAndOptions();
    TestContextBuilderDefaultOutputAndVerboseOverride();

    std::cout << "Application service tests passed\n";
    return 0;
}
