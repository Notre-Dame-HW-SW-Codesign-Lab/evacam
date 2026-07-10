#ifndef INPUT_TECHNOLOGYYAMLLOADER_H_
#define INPUT_TECHNOLOGYYAMLLOADER_H_

#include <string>
#include <vector>

#include "TechnologySpec.h"

namespace YamlHelpers {

std::vector<TechnologySpec> ReadTechnologySpecsFromYaml(const std::string& inputFile);

}  // namespace YamlHelpers

#endif  // INPUT_TECHNOLOGYYAMLLOADER_H_
