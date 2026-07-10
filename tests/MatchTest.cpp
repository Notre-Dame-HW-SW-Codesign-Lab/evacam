#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "EvaCAM_Match.h"

bool verbose = false;

void AssertNear(double lhs, double rhs) {
    assert(std::fabs(lhs - rhs) <= 1e-18);
}

void AssertSameMetrics(const EvaCAMMatchResult &lhs, const EvaCAMMatchResult &rhs) {
    AssertNear(lhs.searchLatency, rhs.searchLatency);
    AssertNear(lhs.searchDynamicEnergy, rhs.searchDynamicEnergy);
    AssertNear(lhs.matchlineDelay, rhs.matchlineDelay);
    AssertNear(lhs.senseMargin, rhs.senseMargin);
}

void AssertSameResult(const EvaCAMMatchResult &lhs, const EvaCAMMatchResult &rhs) {
    assert(lhs.hit == rhs.hit);
    AssertSameMetrics(lhs, rhs);
}

void PrintResult(const char *label, const EvaCAMMatchResult &result) {
    std::cout << std::left << std::setw(14) << label
              << " hit=" << std::setw(5) << result.hit
              << " search=" << std::right << std::setw(9) << result.searchLatency * 1e12 << " ps"
              << " ml_delay=" << std::setw(9) << result.matchlineDelay * 1e12 << " ps"
              << " energy=" << std::setw(9) << result.searchDynamicEnergy * 1e12 << " pJ"
              << " sep=" << std::setw(9) << result.senseMargin * 1e3 << " mV\n";
}

void PrintMatchTableHeader(const char *title) {
    std::cout << "\n" << title << "\n";
    std::cout << std::right
              << std::setw(10) << "mismatches"
              << std::setw(8) << "hit"
              << std::setw(14) << "search_ps"
              << std::setw(14) << "ml_ps"
              << std::setw(14) << "energy_pJ"
              << std::setw(14) << "sense_sep_mV"
              << "\n";
}

void PrintMatchTableRow(int mismatches, const EvaCAMMatchResult &result) {
    std::cout << std::setw(10) << mismatches
              << std::setw(8) << result.hit
              << std::setw(14) << result.searchLatency * 1e12
              << std::setw(14) << result.matchlineDelay * 1e12
              << std::setw(14) << result.searchDynamicEnergy * 1e12
              << std::setw(14) << result.senseMargin * 1e3
              << "\n";
}

std::vector<int> QueryWithMismatches(const std::vector<int> &stored, size_t mismatchCount) {
    std::vector<int> query = stored;
    for (size_t i = 0; i < mismatchCount && i < query.size(); i++) {
        query[i] = 1 - query[i];
    }
    return query;
}

std::string ReadFile(const std::string &path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void WriteFile(const std::string &path, const std::string &content) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to write " + path);
    }
    output << content;
}

void ReplaceAll(std::string &text, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

void WriteMatchToolVariant(const std::string &sourceToolPath,
        const std::string &toolPath,
        const std::string &architecturePath,
        const std::string &searchFunction,
        const std::string &cellPath) {
    const std::filesystem::path sourceConfig(sourceToolPath);
    const std::string sourceArchitecturePath = std::filesystem::absolute(
            sourceConfig.parent_path() / "2FeFET_TCAM_match.architecture.yaml").string();
    std::string architecture = ReadFile(sourceArchitecturePath);
    for (const std::string token : {"EX", "BE", "TH"}) {
        ReplaceAll(architecture, "search_function: " + token,
                "search_function: " + searchFunction);
    }
    ReplaceAll(architecture,
            "sensing: ./2FeFET_TCAM.sensing.yaml",
            "sensing: " + std::filesystem::absolute(
                    sourceConfig.parent_path() / "2FeFET_TCAM.sensing.yaml").string());
    ReplaceAll(architecture,
            "sensing: ./2FeFET_TCAM_match.sensing.yaml",
            "sensing: " + std::filesystem::absolute(
                    sourceConfig.parent_path() / "2FeFET_TCAM_match.sensing.yaml").string());
    WriteFile(architecturePath, architecture);

    std::string tool = ReadFile(sourceToolPath);
    ReplaceAll(tool,
            "architecture: 2FeFET_TCAM_match.architecture.yaml",
            "architecture: " + architecturePath);
    ReplaceAll(tool,
            "cell: 2FeFET_TCAM.cell.yaml",
            "cell: " + cellPath);
    ReplaceAll(tool,
            "technology: ../lib/technology/cmos.legacy.yaml",
            "technology: " + std::filesystem::absolute(
                    sourceConfig.parent_path() / "../lib/technology/cmos.legacy.yaml").string());
    WriteFile(toolPath, tool);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <config.yaml>\n";
        return 1;
    }

    try {
        EvaCAM_Match matcher(argv[1]);
        std::vector<int> stored(matcher.word_width(), 1);
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Word width: " << matcher.word_width() << " bits\n\n";

        std::cout << "Mismatch sweep:\n";
        std::cout << std::right
                  << std::setw(10) << "mismatches"
                  << std::setw(8) << "hit"
                  << std::setw(14) << "search_ps"
                  << std::setw(14) << "ml_ps"
                  << std::setw(14) << "energy_pJ"
                  << std::setw(14) << "sense_sep_mV"
                  << "\n";

        double minSenseMargin = std::numeric_limits<double>::infinity();
        size_t minSenseMarginMismatches = 0;
        std::vector<int> mismatchCounts;
        std::vector<EvaCAMMatchResult> expectedMismatchResults;
        for (size_t mismatches = 0; mismatches <= matcher.word_width(); mismatches++) {
            const std::vector<int> query = QueryWithMismatches(stored, mismatches);
            const EvaCAMMatchResult result = matcher.evaluate_vector(stored, query);
            const EvaCAMMatchResult mismatchResult = matcher.evaluate_mismatches(static_cast<int>(mismatches));
            AssertSameResult(result, mismatchResult);
            mismatchCounts.push_back(static_cast<int>(mismatches));
            expectedMismatchResults.push_back(result);
            std::cout << std::setw(10) << mismatches
                      << std::setw(8) << result.hit
                      << std::setw(14) << result.searchLatency * 1e12
                      << std::setw(14) << result.matchlineDelay * 1e12
                      << std::setw(14) << result.searchDynamicEnergy * 1e12
                      << std::setw(14) << result.senseMargin * 1e3
                      << "\n";
            if (result.senseMargin < minSenseMargin) {
                minSenseMargin = result.senseMargin;
                minSenseMarginMismatches = mismatches;
            }
        }

        const size_t thresholdMax = std::min<size_t>(2, matcher.word_width());
        for (size_t mismatches = 0; mismatches <= std::min<size_t>(thresholdMax + 1, matcher.word_width()); mismatches++) {
            const std::vector<int> query = QueryWithMismatches(stored, mismatches);
            const EvaCAMMatchResult thresholdByVector = matcher.evaluate_threshold(
                    stored, query, static_cast<int>(thresholdMax));
            const EvaCAMMatchResult thresholdByCount = matcher.evaluate_threshold(
                    static_cast<int>(mismatches), static_cast<int>(thresholdMax));
            AssertSameResult(thresholdByVector, thresholdByCount);
            assert(thresholdByVector.hit == (mismatches <= thresholdMax));
            assert(thresholdByVector.searchLatency == expectedMismatchResults[mismatches].searchLatency);
            assert(thresholdByVector.searchDynamicEnergy == expectedMismatchResults[mismatches].searchDynamicEnergy);
            assert(thresholdByVector.matchlineDelay == expectedMismatchResults[mismatches].matchlineDelay);
            assert(thresholdByVector.senseMargin == expectedMismatchResults[mismatches].senseMargin);
        }

        EvaCAMMatchResult exactThresholdMatch = matcher.evaluate_threshold(stored, stored, 0);
        assert(exactThresholdMatch.hit);
        EvaCAMMatchResult exactThresholdMiss = matcher.evaluate_threshold(stored, QueryWithMismatches(stored, 1), 0);
        assert(!exactThresholdMiss.hit);
        EvaCAMMatchResult allThreshold = matcher.evaluate_threshold(
                static_cast<int>(matcher.word_width()), static_cast<int>(matcher.word_width()));
        assert(allThreshold.hit);

        try {
            matcher.evaluate_threshold(stored, stored, -1);
            assert(false);
        } catch (const std::invalid_argument&) {
        }
        try {
            matcher.evaluate_threshold(stored, stored, static_cast<int>(matcher.word_width()) + 1);
            assert(false);
        } catch (const std::invalid_argument&) {
        }
        try {
            matcher.evaluate_threshold(-1, 0);
            assert(false);
        } catch (const std::invalid_argument&) {
        }

        const std::vector<EvaCAMMatchResult> mismatchArrayResults = matcher.evaluate_array(mismatchCounts);
        assert(mismatchArrayResults.size() == expectedMismatchResults.size());
        for (size_t i = 0; i < mismatchArrayResults.size(); i++) {
            AssertSameResult(mismatchArrayResults[i], expectedMismatchResults[i]);
        }

        try {
            matcher.evaluate_mismatches(-1);
            assert(false);
        } catch (const std::invalid_argument&) {
        }
        try {
            matcher.evaluate_mismatches(static_cast<int>(matcher.word_width()) + 1);
            assert(false);
        } catch (const std::invalid_argument&) {
        }

        std::cout << "\nMinimum sweep sense separation: " << minSenseMargin * 1e3
                  << " mV at " << minSenseMarginMismatches << " mismatch(es)\n";

        PrintMatchTableHeader("Threshold match sample (maxMismatches = 2):");
        for (int mismatches : {0, 1, 2, 3}) {
            const EvaCAMMatchResult result = matcher.evaluate_threshold(
                    static_cast<int>(mismatches), 2);
            PrintMatchTableRow(mismatches, result);
        }

        const std::string sourceCellPath = std::filesystem::absolute(
                "config/2FeFET_TCAM/2FeFET_TCAM.cell.yaml").string();
        const std::string sourceToolPath =
                "config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml";
        const std::string thConfigPath = "/tmp/evacam_match_th_tool_config.yaml";
        const std::string thArchitecturePath = "/tmp/evacam_match_th_architecture_config.yaml";
        WriteMatchToolVariant(sourceToolPath, thConfigPath, thArchitecturePath, "TH", sourceCellPath);

        EvaCAM_Match thMatcher(thConfigPath);
        std::vector<int> thStored(thMatcher.word_width(), 1);
        assert(thMatcher.evaluate_threshold(thStored, thStored, 0).hit);
        assert(!thMatcher.evaluate_threshold(
                    thStored, QueryWithMismatches(thStored, 1), 0).hit);
        assert(thMatcher.evaluate_threshold(
                    thStored, QueryWithMismatches(thStored, 1), 1).hit);
        try {
            thMatcher.evaluate_vector(thStored, thStored);
            assert(false);
        } catch (const std::runtime_error &ex) {
            assert(std::string(ex.what()).find("requires evaluate_threshold") != std::string::npos);
        }

        const std::string beConfigPath = "/tmp/evacam_match_be_tool_config.yaml";
        const std::string beArchitecturePath = "/tmp/evacam_match_be_architecture_config.yaml";
        WriteMatchToolVariant(sourceToolPath, beConfigPath, beArchitecturePath, "BE", sourceCellPath);

        EvaCAM_Match beMatcher(beConfigPath);
        std::vector<int> beQuery(beMatcher.word_width(), 1);
        std::vector<std::vector<int>> beRows = {
            QueryWithMismatches(beQuery, 2),
            beQuery,
            QueryWithMismatches(beQuery, 1),
            QueryWithMismatches(beQuery, 3)
        };
        std::vector<int> beMismatchCounts = {2, 0, 1, 3};
        const std::vector<EvaCAMMatchResult> beVectorResults = beMatcher.evaluate_array(beRows, beQuery);
        const std::vector<EvaCAMMatchResult> beCountResults = beMatcher.evaluate_array(beMismatchCounts);
        assert(beVectorResults.size() == beMismatchCounts.size());
        assert(beCountResults.size() == beMismatchCounts.size());

        PrintMatchTableHeader("Best match unique sample (counts = [2, 0, 1, 3]):");
        for (size_t i = 0; i < beMismatchCounts.size(); i++) {
            const bool isBest = (beMismatchCounts[i] == 0);
            assert(beVectorResults[i].hit == isBest);
            assert(beCountResults[i].hit == isBest);
            AssertSameResult(beVectorResults[i], beCountResults[i]);
            AssertSameMetrics(beVectorResults[i], expectedMismatchResults[beMismatchCounts[i]]);
            PrintMatchTableRow(beMismatchCounts[i], beVectorResults[i]);
        }

        std::vector<int> tiedBestCounts = {1, 1, 2};
        const std::vector<EvaCAMMatchResult> tiedBestResults = beMatcher.evaluate_array(tiedBestCounts);
        assert(tiedBestResults.size() == tiedBestCounts.size());
        assert(tiedBestResults[0].hit);
        assert(tiedBestResults[1].hit);
        assert(!tiedBestResults[2].hit);
        PrintMatchTableHeader("Best match tied sample (counts = [1, 1, 2]):");
        for (size_t i = 0; i < tiedBestCounts.size(); i++) {
            PrintMatchTableRow(tiedBestCounts[i], tiedBestResults[i]);
        }

        try {
            beMatcher.evaluate_vector(beQuery, beQuery);
            assert(false);
        } catch (const std::runtime_error &ex) {
            assert(std::string(ex.what()).find("requires evaluate_array") != std::string::npos);
        }
        try {
            beMatcher.evaluate_array(std::vector<int>{});
            assert(false);
        } catch (const std::invalid_argument&) {
        }
        try {
            beMatcher.evaluate_array(std::vector<int>{0, static_cast<int>(beMatcher.word_width()) + 1});
            assert(false);
        } catch (const std::invalid_argument&) {
        }

        if (matcher.word_width() >= 2
                && expectedMismatchResults[2].senseMargin < expectedMismatchResults[1].senseMargin) {
            const double guardedSenseVoltage = (expectedMismatchResults[1].senseMargin
                    + expectedMismatchResults[2].senseMargin) / 2.0;
            std::ostringstream voltage;
            voltage << std::scientific << guardedSenseVoltage << "V";

            const std::string highSenseCellPath = "/tmp/evacam_match_high_sense_cell_config.yaml";
            const std::string highSenseMemoryDevicePath =
                    "/tmp/evacam_match_high_sense_memory_device.yaml";
            const std::string highSenseConfigPath = "/tmp/evacam_match_high_sense_tool_config.yaml";
            const std::string highSenseArchitecturePath =
                    "/tmp/evacam_match_high_sense_architecture_config.yaml";
            std::string highSenseMemoryDevice =
                    ReadFile("config/2FeFET_TCAM/2FeFET_TCAM.memory_device.yaml");
            ReplaceAll(highSenseMemoryDevice, "min_sense_voltage: 70mV",
                    "min_sense_voltage: " + voltage.str());
            WriteFile(highSenseMemoryDevicePath, highSenseMemoryDevice);

            std::string highSenseCell =
                    ReadFile("config/2FeFET_TCAM/2FeFET_TCAM.cell.yaml");
            ReplaceAll(highSenseCell, "min_sense_voltage: 70mV", "min_sense_voltage: " + voltage.str());
            ReplaceAll(highSenseCell, "memory_device: ./2FeFET_TCAM.memory_device.yaml",
                    "memory_device: " + highSenseMemoryDevicePath);
            WriteFile(highSenseCellPath, highSenseCell);

            WriteMatchToolVariant(sourceToolPath, highSenseConfigPath, highSenseArchitecturePath,
                    "TH", highSenseCellPath);

            EvaCAM_Match highSenseMatcher(highSenseConfigPath);
            assert(highSenseMatcher.evaluate_threshold(0, 0).hit);
            try {
                highSenseMatcher.evaluate_threshold(1, 1);
                assert(false);
            } catch (const std::runtime_error &ex) {
                assert(std::string(ex.what()).find("sense-margin capability") != std::string::npos);
                std::cout << "\nExpected TH sense-margin rejection: " << ex.what() << "\n";
            }

            WriteMatchToolVariant(sourceToolPath, highSenseConfigPath, highSenseArchitecturePath,
                    "BE", highSenseCellPath);
            EvaCAM_Match highSenseBestMatcher(highSenseConfigPath);
            assert(highSenseBestMatcher.evaluate_array(std::vector<int>{0, 1})[0].hit);
            const std::vector<EvaCAMMatchResult> allTiedDetectable = highSenseBestMatcher.evaluate_array(
                    std::vector<int>{1, 1});
            assert(allTiedDetectable[0].hit && allTiedDetectable[1].hit);
            try {
                highSenseBestMatcher.evaluate_array(std::vector<int>{1, 2});
                assert(false);
            } catch (const std::runtime_error &ex) {
                assert(std::string(ex.what()).find("best-match boundary") != std::string::npos);
                std::cout << "Expected BE boundary rejection: " << ex.what() << "\n";
            }
            try {
                highSenseBestMatcher.evaluate_array(std::vector<int>{2, 3});
                assert(false);
            } catch (const std::runtime_error &ex) {
                assert(std::string(ex.what()).find("best match exceeds") != std::string::npos);
                std::cout << "Expected BE detectability rejection: " << ex.what() << "\n";
            }
        }

        return 0;

    } catch (const std::exception &ex) {
        std::cerr << ex.what() << "\n";
        return 2;
    }
}
