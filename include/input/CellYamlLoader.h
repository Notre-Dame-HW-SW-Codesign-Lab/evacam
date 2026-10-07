#ifndef INPUT_CELLYAMLLOADER_H_
#define INPUT_CELLYAMLLOADER_H_

#include <memory>
#include <string>

class MemCell;

class EvaCamConfig;

namespace YamlHelpers {

void ReadMemCellFromYaml(MemCell& cell, const std::string& inputFile,
        const std::shared_ptr<EvaCamConfig> &technologyContext = nullptr);

}  // namespace YamlHelpers

#endif  // INPUT_CELLYAMLLOADER_H_
