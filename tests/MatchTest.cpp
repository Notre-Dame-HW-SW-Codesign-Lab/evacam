#include <iostream>
#include <vector>

#include "EvaCAM_Match.h"

bool verbose = false;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <config.yaml>\n";
        return 1;
    }

    try {
        EvaCAM_Match matcher(argv[1]);
        std::vector<int> stored(matcher.word_width(), 1);
        std::vector<int> query_match = stored;
        std::vector<int> single_miss = stored;
        std::vector<int> double_miss = stored;
        std::vector<int> all_miss(matcher.word_width(), 0);

        if (!single_miss.empty()) {
            single_miss[0] = 0;
        }

        if (!double_miss.empty()) {
            double_miss[0] = double_miss[1] = 0;
        }
        
        EvaCAMMatchResult hit_result = matcher.evaluate(stored, query_match);
        EvaCAMMatchResult single_miss_result = matcher.evaluate(stored, single_miss);
        EvaCAMMatchResult double_miss_result = matcher.evaluate(stored, double_miss);
        EvaCAMMatchResult all_miss_result = matcher.evaluate(stored, all_miss);

        std::cout << "word_width=" << matcher.word_width() << "\n";

        std::cout << "exact_match.hit=" << hit_result.hit << "\n";
        std::cout << "exact_match.search_latency=" << hit_result.searchLatency << "\n";
        std::cout << "exact_match.search_dynamic_energy=" << hit_result.searchDynamicEnergy << "\n";

        std::cout << "single_miss.hit=" << single_miss_result.hit << "\n";
        std::cout << "single_miss.search_latency=" << single_miss_result.searchLatency << "\n";
        std::cout << "single_miss.search_dynamic_energy=" << single_miss_result.searchDynamicEnergy << "\n";

        std::cout << "double_miss.hit=" << double_miss_result.hit << "\n";
        std::cout << "double_miss.search_latency=" << double_miss_result.searchLatency << "\n";
        std::cout << "double_miss.search_dynamic_energy=" << double_miss_result.searchDynamicEnergy << "\n";

        std::cout << "all_miss.hit=" << all_miss_result.hit << "\n";
        std::cout << "all_miss.search_latency=" << all_miss_result.searchLatency << "\n";
        std::cout << "all_miss.search_dynamic_energy=" << all_miss_result.searchDynamicEnergy << "\n";

        return 0;

    } catch (const std::exception &ex) {
        std::cerr << ex.what() << "\n";
        return 2;
    }
}
