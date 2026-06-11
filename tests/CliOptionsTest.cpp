#include "input/CliOptions.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

CliOptions Parse(std::vector<std::string> args) {
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
        argv.push_back(arg.data());
    }
    return CliOptionsParser::Parse((int)argv.size(), argv.data());
}

template <typename Func>
void AssertThrowsInvalidArgument(Func func) {
    bool threw = false;
    try {
        func();
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestOutputOptionShortForm() {
    const CliOptions options = Parse({
            "EvaCAM",
            "-o", "results/custom.yaml",
            "config/input.yaml",
    });

    assert(options.inputFileName == "config/input.yaml");
    assert(options.outputYamlFileName == "results/custom.yaml");
}

void TestOutputOptionLongForm() {
    const CliOptions options = Parse({
            "EvaCAM",
            "--output", "/tmp/evacam.yaml",
            "--threads", "2",
            "--quiet",
            "--no-variation-plots",
            "config/input.yaml",
    });

    assert(options.inputFileName == "config/input.yaml");
    assert(options.outputYamlFileName == "/tmp/evacam.yaml");
    assert(options.threads == 2);
    assert(!options.stdoutOutput);
    assert(!options.variationPlots);
}

void TestQuietOptionShortForm() {
    const CliOptions options = Parse({
            "EvaCAM",
            "-q",
            "config/input.yaml",
    });

    assert(options.inputFileName == "config/input.yaml");
    assert(!options.stdoutOutput);
}

void TestOutputOptionValidation() {
    AssertThrowsInvalidArgument([] {
        Parse({"EvaCAM", "-o"});
    });
    AssertThrowsInvalidArgument([] {
        Parse({"EvaCAM", "--output", "a.yaml", "-o", "b.yaml", "config/input.yaml"});
    });
}

void TestUsageIncludesOutputOption() {
    std::ostringstream os;
    CliOptionsParser::PrintUsage(os);

    const std::string usage = os.str();
    assert(usage.find("-o, --output FILE") != std::string::npos);
    assert(usage.find("-q, --quiet") != std::string::npos);
}

}  // namespace

int main() {
    TestOutputOptionShortForm();
    TestOutputOptionLongForm();
    TestQuietOptionShortForm();
    TestOutputOptionValidation();
    TestUsageIncludesOutputOption();

    std::cout << "CLI options tests passed" << std::endl;
    return 0;
}
