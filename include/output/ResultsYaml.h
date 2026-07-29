#ifndef RESULTS_YAML_H_
#define RESULTS_YAML_H_

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Result.h"

class EvaCamConfig;

void WriteResultsYaml(std::ostream& os, const Result& result,
        const std::string &variationSamplesFile = "",
        const std::string &variationPlotFile = "");
void WriteResultsYamlMulti(std::ostream& os, const std::vector<std::shared_ptr<Result>>& results,
        const std::unordered_map<OptimizationTarget, std::string> &variationSamplesFiles = {},
        const std::unordered_map<OptimizationTarget, std::string> &variationPlotFiles = {});
void WriteResultsYamlNoSolutions(std::ostream& os, const EvaCamConfig &config);

#endif // RESULTS_YAML_H_
