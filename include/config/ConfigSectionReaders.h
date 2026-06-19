#ifndef CONFIG_CONFIGSECTIONREADERS_H_
#define CONFIG_CONFIGSECTIONREADERS_H_

#include <yaml.h>

class EvaCamConfig;

namespace ConfigSectionReaders {

void ReadDesignSection(const YAML::Node &root, EvaCamConfig &config);
void ReadMemorySection(const YAML::Node &root, EvaCamConfig &config);
void ReadRoutingSection(const YAML::Node &root, EvaCamConfig &config);
void ReadPeripheralSection(const YAML::Node &root, EvaCamConfig &config);
void ReadSensingSection(const YAML::Node &root, EvaCamConfig &config);
void ReadOptimizationSection(const YAML::Node &root, EvaCamConfig &config);
void ReadWireSection(const YAML::Node &root, EvaCamConfig &config);
void ReadOrganizationSection(const YAML::Node &root, EvaCamConfig &config);
void ReadMatchlineSection(const YAML::Node &root, EvaCamConfig &config);
void ReadConstraintSection(const YAML::Node &root, EvaCamConfig &config);
void ReadAdvancedSection(const YAML::Node &root, EvaCamConfig &config);
void ReadFlashSection(const YAML::Node &root, EvaCamConfig &config);
void ReadExtraSection(const YAML::Node &root, EvaCamConfig &config);

}  // namespace ConfigSectionReaders

#endif  // CONFIG_CONFIGSECTIONREADERS_H_
