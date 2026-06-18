#ifndef EVACAM_OUTPUT_H_
#define EVACAM_OUTPUT_H_

#include <memory>
#include <string>
#include <vector>

class EvaCamConfig;
class Result;

class EvaCamOutput {
    public:
        static void PrintConsoleSummary(const EvaCamConfig &config,
                long long numSolution,
                const std::vector<std::shared_ptr<Result>> &bestResults,
                const std::string &explorationOutputFileName);

        static void WriteYamlResults(const EvaCamConfig &config,
                const std::string &outputYamlFileName,
                long long numSolution,
                const std::vector<std::shared_ptr<Result>> &bestResults);
};

#endif /* EVACAM_OUTPUT_H_ */
