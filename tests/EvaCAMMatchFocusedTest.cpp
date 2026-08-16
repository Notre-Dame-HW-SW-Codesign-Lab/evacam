#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "EvaCAM_Match.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertThrows;
using TestSupport::Require;

constexpr const char *kTcamConfig = "config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml";
constexpr const char *kBcamConfig = "config/10T-BCAM_28nm/10T-BCAM_28nm.config.yaml";
constexpr const char *kMcamConfig = "config/2FeFET_MCAM/2FeFET_MCAM.config.yaml";

std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input(path);
    Require(input.is_open(), "could not open fixture " + path.string());
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void ReplaceAll(std::string &text, const std::string &from, const std::string &to) {
    for (size_t position = 0; (position = text.find(from, position)) != std::string::npos;
            position += to.size()) {
        text.replace(position, from.size(), to);
    }
}

std::filesystem::path WriteSearchVariant(
        TestSupport::TemporaryDirectory &temporary,
        const std::string &searchFunction) {
    const std::filesystem::path source(kTcamConfig);
    const std::filesystem::path sourceDirectory = source.parent_path();
    std::string architecture = ReadFile(sourceDirectory / "2FeFET_TCAM_match.architecture.yaml");
    for (const std::string value : {"EX", "BE", "TH"}) {
        ReplaceAll(architecture, "search_function: " + value,
                "search_function: " + searchFunction);
    }
    ReplaceAll(architecture, "sensing: ./2FeFET_TCAM.sensing.yaml",
            "sensing: " + std::filesystem::absolute(
                    sourceDirectory / "2FeFET_TCAM.sensing.yaml").string());
    ReplaceAll(architecture, "sensing: ./2FeFET_TCAM_match.sensing.yaml",
            "sensing: " + std::filesystem::absolute(
                    sourceDirectory / "2FeFET_TCAM_match.sensing.yaml").string());

    const std::filesystem::path architecturePath = temporary.WriteFile(
            searchFunction + ".architecture.yaml", architecture);
    std::string tool = ReadFile(source);
    ReplaceAll(tool, "architecture: 2FeFET_TCAM_match.architecture.yaml",
            "architecture: " + architecturePath.string());
    ReplaceAll(tool, "cell: 2FeFET_TCAM.cell.yaml",
            "cell: " + std::filesystem::absolute(sourceDirectory / "2FeFET_TCAM.cell.yaml").string());
    ReplaceAll(tool, "technology: ../lib/technology/cmos.legacy.yaml",
            "technology: " + std::filesystem::absolute(
                    sourceDirectory / "../lib/technology/cmos.legacy.yaml").string());
    return temporary.WriteFile(searchFunction + ".config.yaml", tool);
}

std::filesystem::path WriteMcamSearchVariant(
        TestSupport::TemporaryDirectory &temporary,
        const std::string &searchFunction) {
    const std::filesystem::path source(kMcamConfig);
    const std::filesystem::path sourceDirectory = source.parent_path();
    std::string architecture = ReadFile(sourceDirectory / "2FeFET_MCAM.architecture.yaml");
    for (const std::string value : {"EX", "BE", "TH"}) {
        ReplaceAll(architecture, "search_function: " + value,
                "search_function: " + searchFunction);
    }
    ReplaceAll(architecture, "sensing: ./2FeFET_MCAM.sensing.yaml",
            "sensing: " + std::filesystem::absolute(
                    sourceDirectory / "2FeFET_MCAM.sensing.yaml").string());
    const std::filesystem::path architecturePath = temporary.WriteFile(
            "mcam-" + searchFunction + ".architecture.yaml", architecture);
    std::string tool = ReadFile(source);
    ReplaceAll(tool, "architecture: 2FeFET_MCAM.architecture.yaml",
            "architecture: " + architecturePath.string());
    ReplaceAll(tool, "cell: 2FeFET_MCAM.cell.yaml",
            "cell: " + std::filesystem::absolute(sourceDirectory / "2FeFET_MCAM.cell.yaml").string());
    ReplaceAll(tool, "technology: ../lib/technology/cmos.legacy.yaml",
            "technology: " + std::filesystem::absolute(
                    sourceDirectory / "../lib/technology/cmos.legacy.yaml").string());
    return temporary.WriteFile("mcam-" + searchFunction + ".config.yaml", tool);
}

std::vector<int> Bits(size_t width, int bit = 0) {
    return std::vector<int>(width, bit);
}

std::vector<int> McamSymbols(size_t width) {
    std::vector<int> symbols(width);
    for (size_t index = 0; index < width; index++) {
        symbols[index] = static_cast<int>(index % 8);
    }
    return symbols;
}

std::vector<int> WithMismatches(std::vector<int> bits, size_t mismatches) {
    for (size_t index = 0; index < mismatches; index++) {
        bits[index] = 1 - bits[index];
    }
    return bits;
}

void AssertMetrics(const EvaCAMMatchResult &result) {
    AssertFiniteNonNegative(result.searchLatency, "search latency");
    AssertFiniteNonNegative(result.searchDynamicEnergy, "search energy");
    AssertFiniteNonNegative(result.matchlineDelay, "matchline delay");
    AssertFiniteNonNegative(result.senseMargin, "sense margin");
}

void TestConstructorWordWidthAndExactPublicOverloads() {
    EvaCAM_Match matcher(kTcamConfig);
    Require(matcher.word_width() > 3, "TCAM fixture must provide a usable word width");
    const std::vector<int> stored = Bits(matcher.word_width());
    const std::vector<int> oneMiss = WithMismatches(stored, 1);

    const EvaCAMMatchResult exact = matcher.evaluate_vector(stored, stored);
    const EvaCAMMatchResult miss = matcher.evaluate_vector(stored, oneMiss);
    const EvaCAMMatchResult byCount = matcher.evaluate_mismatches(1);
    assert(exact.hit);
    assert(!miss.hit);
    assert(!matcher.match(stored, oneMiss));
    assert(miss.hit == byCount.hit);
    AssertMetrics(exact);
    AssertMetrics(miss);
    assert(miss.searchLatency == byCount.searchLatency);
    assert(miss.searchDynamicEnergy == byCount.searchDynamicEnergy);
}

void TestExactArrayOverloadsAndLutBehavior() {
    EvaCAM_Match matcher(kTcamConfig);
    const std::vector<int> query = Bits(matcher.word_width());
    const std::vector<int> counts = {0, 1, 2};
    const auto byCounts = matcher.evaluate_array(counts);
    const auto byRows = matcher.evaluate_array(std::vector<std::vector<int>>{
            query, WithMismatches(query, 1), WithMismatches(query, 2)}, query);
    assert(byCounts.size() == counts.size());
    assert(byRows.size() == counts.size());
    for (size_t index = 0; index < counts.size(); index++) {
        assert(byCounts[index].hit == (counts[index] == 0));
        assert(byRows[index].hit == byCounts[index].hit);
        assert(byRows[index].searchLatency == byCounts[index].searchLatency);
    }
}

void TestExactTcamValidationRules() {
    EvaCAM_Match matcher(kTcamConfig);
    const std::vector<int> value = Bits(matcher.word_width());
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_vector({}, value); }, "stored vector length");
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_vector(value, {}); }, "query vector length");
    std::vector<int> invalidStored = value;
    invalidStored[0] = 2;
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_vector(invalidStored, value); }, "0, 1, or -1");
    std::vector<int> invalidQuery = value;
    invalidQuery[0] = -1;
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_vector(value, invalidQuery); }, "binary values");
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_mismatches(-1); }, "out of range");
    AssertThrows<std::invalid_argument>([&] {
        matcher.evaluate_mismatches(static_cast<int>(matcher.word_width()) + 1);
    }, "out of range");
}

void TestThresholdOverloadsAndValidationRules() {
    TestSupport::TemporaryDirectory temporary("evacam-match-th");
    EvaCAM_Match matcher(WriteSearchVariant(temporary, "TH").string());
    const std::vector<int> stored = Bits(matcher.word_width());
    const EvaCAMMatchResult allowed = matcher.evaluate_threshold(stored, WithMismatches(stored, 1), 1);
    const EvaCAMMatchResult rejected = matcher.evaluate_threshold(2, 1);
    assert(allowed.hit);
    assert(!rejected.hit);
    AssertThrows<std::runtime_error>([&] { matcher.evaluate_vector(stored, stored); }, "requires evaluate_threshold");
    AssertThrows<std::runtime_error>([&] { matcher.evaluate_array(std::vector<int>{0}); }, "requires evaluate_threshold");
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_threshold(-1, 0); }, "mismatch count");
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_threshold(0, -1); }, "maxMismatches");
    AssertThrows<std::invalid_argument>([&] {
        matcher.evaluate_threshold(0, static_cast<int>(matcher.word_width()) + 1);
    }, "maxMismatches");
}

void TestBestArrayOverloadsAndValidationRules() {
    TestSupport::TemporaryDirectory temporary("evacam-match-be");
    EvaCAM_Match matcher(WriteSearchVariant(temporary, "BE").string());
    const std::vector<int> query = Bits(matcher.word_width());
    const std::vector<int> counts = {2, 0, 0, 1};
    const auto byCounts = matcher.evaluate_array(counts);
    const auto byRows = matcher.evaluate_array(std::vector<std::vector<int>>{
            WithMismatches(query, 2), query, query, WithMismatches(query, 1)}, query);
    for (size_t index = 0; index < counts.size(); index++) {
        assert(byCounts[index].hit == (counts[index] == 0));
        assert(byRows[index].hit == byCounts[index].hit);
    }
    AssertThrows<std::runtime_error>([&] { matcher.evaluate_vector(query, query); }, "requires evaluate_array");
    AssertThrows<std::runtime_error>([&] { matcher.evaluate_mismatches(0); }, "exact search only");
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_array(std::vector<int>{}); }, "must not be empty");
    AssertThrows<std::invalid_argument>([&] {
        matcher.evaluate_array(std::vector<int>{static_cast<int>(matcher.word_width()) + 1});
    }, "out of range");
}

void TestMcamExactMatchAndValidationRules() {
    EvaCAM_Match matcher(kMcamConfig);
    const std::vector<int> stored = McamSymbols(matcher.word_width());
    std::vector<int> oneMismatch = stored;
    oneMismatch[0] = (oneMismatch[0] + 1) % 8;

    const EvaCAMMatchResult exact = matcher.evaluate_vector(stored, stored);
    const EvaCAMMatchResult miss = matcher.evaluate_vector(stored, oneMismatch);
    assert(exact.hit);
    assert(!miss.hit);
    assert(matcher.match(stored, stored));
    assert(!matcher.match(stored, oneMismatch));
    AssertMetrics(exact);
    AssertMetrics(miss);

    const auto rows = matcher.evaluate_array(
            std::vector<std::vector<int>>{stored, oneMismatch}, stored);
    assert(rows.size() == 2);
    assert(rows[0].hit);
    assert(!rows[1].hit);

    AssertThrows<std::invalid_argument>(
            [&] { matcher.evaluate_vector({}, stored); }, "stored vector length");
    std::vector<int> negative = stored;
    negative[0] = -1;
    AssertThrows<std::invalid_argument>(
            [&] { matcher.evaluate_vector(negative, stored); }, "between 0 and 7");
    std::vector<int> tooLarge = stored;
    tooLarge[0] = 8;
    AssertThrows<std::invalid_argument>(
            [&] { matcher.evaluate_vector(stored, tooLarge); }, "between 0 and 7");
    AssertThrows<std::invalid_argument>(
            [&] { matcher.evaluate_mismatches(0); }, "only valid for TCAM");
}

void TestUnimplementedCamPublicOverloads() {
    // The application can explore this shipped BCAM configuration, but the
    // matcher rejects its configured bank before any BCAM public operation.
    AssertThrows<std::runtime_error>([] { EvaCAM_Match bcam(kBcamConfig); }, "configured bank is invalid");

    TestSupport::TemporaryDirectory temporary("evacam-match-mcam");
    for (const std::string searchFunction : {"BE", "TH"}) {
        EvaCAM_Match mcam(WriteMcamSearchVariant(temporary, searchFunction).string());
        const std::vector<int> mcamBits = Bits(mcam.word_width());
        const std::string operation = searchFunction == "BE" ? "best" : "threshold";
        AssertThrows<std::runtime_error>([&] { mcam.evaluate_vector(mcamBits, mcamBits); },
                operation + " MCAM vector evaluation is not implemented");
        AssertThrows<std::runtime_error>([&] {
            mcam.evaluate_array(std::vector<std::vector<int>>{mcamBits}, mcamBits);
        }, operation + " MCAM vector evaluation is not implemented");
    }

    EvaCAM_Match tcam(kTcamConfig);
    const std::vector<std::pair<double, double>> ranges(tcam.word_width(), {0.0, 1.0});
    const std::vector<double> analog(tcam.word_width(), 0.5);
    AssertThrows<std::invalid_argument>([&] { tcam.evaluate_vector(ranges, analog); }, "only valid for ACAM");
    AssertThrows<std::invalid_argument>([&] {
        tcam.evaluate_array(std::vector<std::vector<std::pair<double, double>>>{ranges}, analog);
    }, "only valid for ACAM");
}

void TestMoveAndConstructionFailures() {
    EvaCAM_Match original(kTcamConfig);
    EvaCAM_Match moved(std::move(original));
    const std::vector<int> bits = Bits(moved.word_width());
    assert(moved.match(bits, bits));
    EvaCAM_Match assigned(kTcamConfig);
    assigned = std::move(moved);
    assert(assigned.match(bits, bits));
    bool threw = false;
    try {
        EvaCAM_Match missing("/tmp/evacam-no-such-match-config.yaml");
    } catch (const std::exception &) {
        threw = true;
    }
    Require(threw, "missing matcher configuration must fail construction");
}

}  // namespace

int main() {
    TestConstructorWordWidthAndExactPublicOverloads();
    TestExactArrayOverloadsAndLutBehavior();
    TestExactTcamValidationRules();
    TestThresholdOverloadsAndValidationRules();
    TestBestArrayOverloadsAndValidationRules();
    TestMcamExactMatchAndValidationRules();
    TestUnimplementedCamPublicOverloads();
    TestMoveAndConstructionFailures();
    return 0;
}
