#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>

#include "EvaCAM_Match.h"

bool verbose = false;

void PrintResult(const char *label, const EvaCAMMatchResult &result) {
    std::cout << std::left << std::setw(14) << label
              << " hit=" << std::setw(5) << result.hit
              << " search=" << std::right << std::setw(9) << result.searchLatency * 1e12 << " ps"
              << " ml_delay=" << std::setw(9) << result.matchlineDelay * 1e12 << " ps"
              << " energy=" << std::setw(9) << result.searchDynamicEnergy * 1e12 << " pJ"
              << " sep=" << std::setw(9) << result.senseMargin * 1e3 << " mV\n";
}

std::vector<int> QueryWithMismatches(const std::vector<int> &stored, size_t mismatchCount) {
    std::vector<int> query = stored;
    for (size_t i = 0; i < mismatchCount && i < query.size(); i++) {
        query[i] = 1 - query[i];
    }
    return query;
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
        for (size_t mismatches = 0; mismatches <= matcher.word_width(); mismatches++) {
            const std::vector<int> query = QueryWithMismatches(stored, mismatches);
            const EvaCAMMatchResult result = matcher.evaluate_vector(stored, query);
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

        std::cout << "\nMinimum sweep sense separation: " << minSenseMargin * 1e3
                  << " mV at " << minSenseMarginMismatches << " mismatch(es)\n";

        return 0;

    } catch (const std::exception &ex) {
        std::cerr << ex.what() << "\n";
        return 2;
    }
}
