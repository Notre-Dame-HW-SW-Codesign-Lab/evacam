#ifndef INPUT_MEMORYDEVICEYAMLLOADER_H_
#define INPUT_MEMORYDEVICEYAMLLOADER_H_

#include <string>

class MemCell;

namespace YamlHelpers {

void ReadMemoryDeviceFromYaml(MemCell& cell, const std::string& inputFile);

}  // namespace YamlHelpers

#endif  // INPUT_MEMORYDEVICEYAMLLOADER_H_
