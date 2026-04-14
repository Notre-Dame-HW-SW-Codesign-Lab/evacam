#ifndef INPUT_CELLYAMLLOADER_H_
#define INPUT_CELLYAMLLOADER_H_

#include <string>

class MemCell;

namespace YamlHelpers {

void ReadMemCellFromYaml(MemCell& cell, const std::string& inputFile);

}  // namespace YamlHelpers

#endif  // INPUT_CELLYAMLLOADER_H_
