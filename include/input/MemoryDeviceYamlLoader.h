#ifndef INPUT_MEMORYDEVICEYAMLLOADER_H_
#define INPUT_MEMORYDEVICEYAMLLOADER_H_

#include <yaml.h>

#include <string>

class MemCell;

namespace YamlHelpers {

void validate_memory_device_keys(const YAML::Node& root);
void ReadMemoryDeviceFromYaml(MemCell& cell, const std::string& inputFile);

}  // namespace YamlHelpers

#endif  // INPUT_MEMORYDEVICEYAMLLOADER_H_
