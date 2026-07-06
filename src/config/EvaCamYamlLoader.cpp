#include "config/EvaCamYamlLoader.h"

#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "config/ConfigNormalizer.h"
#include "config/ConfigSectionReaders.h"
#include "config/InputRuleValidator.h"
#include "EvaCamConfig.h"
#include "input/YamlNodeHelpers.h"

namespace {

bool HasKey(const YAML::Node &node, const char *key) {
    return static_cast<bool>(YamlHelpers::child_optional(node, key));
}

void RejectKeys(const YAML::Node &node, const std::vector<const char *> &keys,
        const char *fileKind) {
    for (const char *key : keys) {
        if (HasKey(node, key)) {
            throw std::runtime_error(
                    std::string("[Input] Error: ") + fileKind
                    + " config contains field owned by the other config: " + key);
        }
    }
}

std::string ResolveReference(const std::string &toolFile, const std::string &reference) {
    const std::filesystem::path referencePath(reference);
    if (referencePath.is_absolute()) {
        return referencePath.lexically_normal().string();
    }

    const std::filesystem::path toolPath = std::filesystem::absolute(toolFile);
    return (toolPath.parent_path() / referencePath).lexically_normal().string();
}

void CopyIfPresent(const YAML::Node &source, YAML::Node &destination, const char *key) {
    const YAML::Node value = YamlHelpers::child_optional(source, key);
    if (value) {
        destination[key] = value;
    }
}

void CopyMappedIfPresent(const YAML::Node &source, const char *sourceKey,
        YAML::Node &destination, const char *destinationKey) {
    const YAML::Node value = YamlHelpers::child_optional(source, sourceKey);
    if (value) {
        destination[destinationKey] = value;
    }
}

YAML::Node BuildMergedRoot(const std::string &toolFile, const YAML::Node &toolRoot) {
    RejectKeys(toolRoot,
            {"design", "memory", "routing", "peripherals", "sensing", "wires",
             "organization", "array", "matchline", "flash", "physical_limits",
             "constraints", "advanced", "extra"},
            "tool");

    const std::string architectureReference =
            YamlHelpers::read_required<std::string>(toolRoot, "architecture_file");
    const std::string cellReference =
            YamlHelpers::read_required<std::string>(toolRoot, "cell_file");
    const std::string architectureFile = ResolveReference(toolFile, architectureReference);
    const std::string cellFile = ResolveReference(toolFile, cellReference);

    const YAML::Node architectureRoot = YAML::LoadFile(architectureFile);
    RejectKeys(architectureRoot,
            {"architecture_file", "cell_file", "custom_sense_amplifier_file",
             "optimization", "design_constraints", "exploration", "modeling", "output",
             "constraints", "advanced", "extra", "array"},
            "architecture");

    const YAML::Node architectureMemory =
            YamlHelpers::child_required(architectureRoot, "memory");
    if (HasKey(architectureMemory, "cell_file")) {
        throw std::runtime_error(
                "[Input] Error: architecture config must not contain memory.cell_file.");
    }
    if (HasKey(architectureMemory, "real_capacity")) {
        throw std::runtime_error(
                "[Input] Error: architecture config uses legacy memory.real_capacity; "
                "use memory.physical_capacity.");
    }
    const YAML::Node architectureSensing =
            YamlHelpers::child_required(architectureRoot, "sensing");
    if (HasKey(architectureSensing, "custom_sense_amp")) {
        throw std::runtime_error(
                "[Input] Error: architecture config must not contain sensing.custom_sense_amp.");
    }

    YAML::Node merged(YAML::NodeType::Map);
    for (const char *key : {"design", "memory", "routing", "peripherals", "sensing",
                            "wires", "organization", "matchline", "flash"}) {
        CopyIfPresent(architectureRoot, merged, key);
    }

    merged["memory"]["cell_file"] = cellFile;
    merged["optimization"] = YamlHelpers::child_required(toolRoot, "optimization");

    const YAML::Node designConstraints =
            YamlHelpers::child_optional(toolRoot, "design_constraints");
    if (designConstraints) {
        merged["constraints"] = designConstraints;
    }

    YAML::Node advanced(YAML::NodeType::Map);
    YAML::Node extra(YAML::NodeType::Map);

    const YAML::Node exploration = YamlHelpers::child_optional(toolRoot, "exploration");
    if (exploration) {
        CopyIfPresent(exploration, advanced, "use_cacti_assumption");
        CopyIfPresent(exploration, advanced, "enable_pruning");
    }

    const YAML::Node modeling = YamlHelpers::child_optional(toolRoot, "modeling");
    if (modeling) {
        CopyIfPresent(modeling, advanced, "use_updated_lib");
        CopyIfPresent(modeling, advanced, "exclude_precharge_latency");
        CopyIfPresent(modeling, advanced, "include_leakage");
        CopyIfPresent(modeling, advanced, "scaled_voltage");
    }

    const YAML::Node output = YamlHelpers::child_optional(toolRoot, "output");
    if (output) {
        CopyMappedIfPresent(output, "yaml_file", extra, "output_yaml_file");
        CopyMappedIfPresent(output, "exploration_csv_prefix", extra, "output_file_prefix");
    }

    CopyMappedIfPresent(architectureMemory, "physical_capacity", extra, "real_capacity");

    const YAML::Node peripherals =
            YamlHelpers::child_required(architectureRoot, "peripherals");
    const YAML::Node peripheralInput = YamlHelpers::child_required(peripherals, "input");
    CopyMappedIfPresent(peripheralInput, "encoder_type", advanced, "input_encoder_type");

    CopyMappedIfPresent(
            architectureSensing, "worst_case_sense_margin", extra, "worst_case_sense_margin");

    const YAML::Node organization =
            YamlHelpers::child_optional(architectureRoot, "organization");
    if (organization) {
        CopyMappedIfPresent(organization, "bit_serial_width", advanced, "bit_serial_width");
    }

    const YAML::Node physicalLimits =
            YamlHelpers::child_optional(architectureRoot, "physical_limits");
    if (physicalLimits) {
        CopyIfPresent(physicalLimits, advanced, "max_nmos_size");
        CopyIfPresent(physicalLimits, extra, "max_driver_current");
    }

    const YAML::Node customSenseAmp =
            YamlHelpers::child_optional(toolRoot, "custom_sense_amplifier_file");
    merged["sensing"]["custom_sense_amp"] = static_cast<bool>(customSenseAmp);
    if (customSenseAmp) {
        const std::string customSenseAmpReference =
                YamlHelpers::read_required<std::string>(toolRoot, "custom_sense_amplifier_file");
        advanced["custom_sa_input_file"] = ResolveReference(toolFile, customSenseAmpReference);
    }

    if (advanced.size() != 0) {
        merged["advanced"] = advanced;
    }
    if (extra.size() != 0) {
        merged["extra"] = extra;
    }

    return merged;
}

void ReadMergedConfig(const YAML::Node &root, EvaCamConfig &config) {
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

void WarnLegacyConfig(EvaCamConfig &config) {
    static std::once_flag warningFlag;
    std::call_once(warningFlag, [&config]() {
        config.logger.Log() << "[Input] Warning: unified system configs are deprecated; "
                            << "use a tool config with architecture_file and cell_file.";
    });
}

}  // namespace

void EvaCamYamlLoader::Load(const std::string &inputFile, EvaCamConfig &config) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    const bool hasArchitectureReference = HasKey(root, "architecture_file");
    const bool hasCellReference = HasKey(root, "cell_file");
    if (hasArchitectureReference != hasCellReference) {
        throw std::runtime_error(
                "[Input] Error: tool config requires both architecture_file and cell_file.");
    }

    if (hasArchitectureReference) {
        ReadMergedConfig(BuildMergedRoot(inputFile, root), config);
        return;
    }

    WarnLegacyConfig(config);
    ReadMergedConfig(root, config);
}
