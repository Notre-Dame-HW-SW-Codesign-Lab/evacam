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
		std::vector<int> query_miss = stored;

		if (!query_miss.empty())
			query_miss[0] = 0;

		EvaCAMMatchResult hit_result = matcher.evaluate(stored, query_match);
		EvaCAMMatchResult miss_result = matcher.evaluate(stored, query_miss);

		std::cout << "word_width=" << matcher.word_width() << "\n";
		std::cout << "exact_match.hit=" << hit_result.hit << "\n";
		std::cout << "exact_match.search_latency=" << hit_result.searchLatency << "\n";
		std::cout << "exact_match.search_dynamic_energy=" << hit_result.searchDynamicEnergy << "\n";
		std::cout << "single_miss.hit=" << miss_result.hit << "\n";
		std::cout << "single_miss.search_latency=" << miss_result.searchLatency << "\n";
		std::cout << "single_miss.search_dynamic_energy=" << miss_result.searchDynamicEnergy << "\n";
		return 0;
	} catch (const std::exception &ex) {
		std::cerr << ex.what() << "\n";
		return 2;
	}
}
