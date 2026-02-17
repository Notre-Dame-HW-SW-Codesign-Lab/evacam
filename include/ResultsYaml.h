#ifndef RESULTS_YAML_H_
#define RESULTS_YAML_H_

#include <memory>
#include <ostream>
#include <vector>

#include "Result.h"

void WriteResultsYaml(std::ostream& os, const Result& result);
void WriteResultsYamlMulti(std::ostream& os, const std::vector<std::shared_ptr<Result>>& results);
void WriteResultsYamlNoSolutions(std::ostream& os);

#endif // RESULTS_YAML_H_
