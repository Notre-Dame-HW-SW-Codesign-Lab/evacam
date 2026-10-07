#ifndef INPUT_MEMORYDEVICEYAMLLOADER_H_
#define INPUT_MEMORYDEVICEYAMLLOADER_H_

#include <yaml.h>

#include <memory>
#include <string>

class MemCell;

class EvaCamConfig;

namespace YamlHelpers {

void validate_memory_device_keys(const YAML::Node& root);
void ReadNandSection(MemCell& cell, const YAML::Node& root, bool allowTechnologyDefaults = false);
void ReadNand3dSection(MemCell& cell, const YAML::Node& root, bool allowTechnologyDefaults = false);
void ReadMemoryDeviceFromYaml(MemCell& cell, const std::string& inputFile,
        const std::shared_ptr<EvaCamConfig> &technologyContext = nullptr);

}  // namespace YamlHelpers

#endif  // INPUT_MEMORYDEVICEYAMLLOADER_H_
