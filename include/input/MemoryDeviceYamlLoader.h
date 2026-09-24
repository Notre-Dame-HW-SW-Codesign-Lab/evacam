#ifndef INPUT_MEMORYDEVICEYAMLLOADER_H_
#define INPUT_MEMORYDEVICEYAMLLOADER_H_

#include <yaml.h>

#include <string>

class MemCell;

namespace YamlHelpers {

void validate_memory_device_keys(const YAML::Node& root);
void ReadNandSection(MemCell& cell, const YAML::Node& root);
void ReadNand3dSection(MemCell& cell, const YAML::Node& root);
void ReadMemoryDeviceFromYaml(MemCell& cell, const std::string& inputFile);

}  // namespace YamlHelpers

#endif  // INPUT_MEMORYDEVICEYAMLLOADER_H_
