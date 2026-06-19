#include "config/EvaCamYamlLoader.h"

#include "config/ConfigNormalizer.h"
#include "config/ConfigSectionReaders.h"
#include "config/InputRuleValidator.h"
#include "EvaCamConfig.h"

void EvaCamYamlLoader::Load(const std::string &inputFile, EvaCamConfig &config) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    config.exploration.useCactiAssumption = false;

    ConfigSectionReaders::ReadDesignSection(root, config);
    ConfigSectionReaders::ReadMemorySection(root, config);
    ConfigSectionReaders::ReadRoutingSection(root, config);
    ConfigSectionReaders::ReadPeripheralSection(root, config);
    ConfigSectionReaders::ReadSensingSection(root, config);
    ConfigSectionReaders::ReadOptimizationSection(root, config);
    ConfigSectionReaders::ReadWireSection(root, config);
    ConfigSectionReaders::ReadOrganizationSection(root, config);
    ConfigSectionReaders::ReadMatchlineSection(root, config);
    ConfigSectionReaders::ReadConstraintSection(root, config);
    ConfigSectionReaders::ReadAdvancedSection(root, config);
    ConfigSectionReaders::ReadFlashSection(root, config);
    ConfigSectionReaders::ReadExtraSection(root, config);
    ConfigNormalizer::Normalize(config);
    InputRuleValidator::Validate(config);
    config.resolvedExploration = ExplorationSpaceResolver::Resolve(config.exploration);
}
