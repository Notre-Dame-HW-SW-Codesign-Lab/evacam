#include "config/EvaCamYamlLoader.h"

#include <filesystem>
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

YAML::Node ResolveSensingNode(const std::string &architectureFile, const YAML::Node &sensingNode) {
    YAML::Node resolved = sensingNode;
    std::string sensingFile = architectureFile;
    if (sensingNode.IsScalar()) {
        sensingFile = ResolveReference(architectureFile, sensingNode.as<std::string>());
        resolved = YAML::LoadFile(sensingFile);
        YamlHelpers::require_schema(resolved, "sensing", "sensing config");
    }
    if (HasKey(resolved, "amplifier_type")) {
        throw std::runtime_error(
                "[Input] Error: sensing.amplifier_type was renamed; use sensing.sensing_mode.");
    }

    YAML::Node senseAmpReference = YamlHelpers::child_optional(resolved, "sense_amplifier");
    if (senseAmpReference && senseAmpReference.IsScalar()) {
        const std::string senseAmpFile = ResolveReference(
                sensingFile, senseAmpReference.as<std::string>());
        const YAML::Node senseAmp = YAML::LoadFile(senseAmpFile);
        YamlHelpers::require_schema(senseAmp, "sense_amp", "sense amp config");
        if (!YamlHelpers::child_optional(resolved, "sensing_mode")) {
            resolved["sensing_mode"] = YamlHelpers::read_optional<std::string>(
                    senseAmp, "name", "nvsim_vol");
        }
        resolved["custom_sense_amp"] = false;
        resolved["sense_amp_input_file"] = senseAmpFile;
        resolved.remove("sense_amplifier");
    }

    return resolved;
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

YAML::Node BuildMergedRootV2(const std::string &configFile, const YAML::Node &root) {
    YamlHelpers::require_schema(root, "config", "run config");
    RejectKeys(root,
            {"design", "memory", "routing", "peripherals", "sensing", "wires",
             "organization", "array", "matchline", "flash", "physical_limits",
             "constraints", "advanced", "extra"},
            "run");
    RejectKeys(root,
            {"architecture_file", "cell_file", "custom_sense_amplifier_file"},
            "run");

    const std::string architectureReference =
            YamlHelpers::read_required<std::string>(root, "architecture");
    const std::string cellReference = YamlHelpers::read_required<std::string>(root, "cell");
    const std::string architectureFile = ResolveReference(configFile, architectureReference);
    const std::string cellFile = ResolveReference(configFile, cellReference);

    const YAML::Node architectureRoot = YAML::LoadFile(architectureFile);
    YamlHelpers::require_schema(architectureRoot, "architecture", "architecture config");
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
            ResolveSensingNode(architectureFile, YamlHelpers::child_required(architectureRoot, "sensing"));
    if (HasKey(architectureSensing, "custom_sense_amp")
            && YamlHelpers::read_required<bool>(architectureSensing, "custom_sense_amp")) {
        throw std::runtime_error(
                "[Input] Error: architecture config must not contain sensing.custom_sense_amp.");
    }

    YAML::Node merged(YAML::NodeType::Map);
    for (const char *key : {"design", "memory", "routing", "peripherals", "sensing",
                            "wires", "organization", "matchline", "flash"}) {
        CopyIfPresent(architectureRoot, merged, key);
    }
    merged["sensing"] = architectureSensing;
    merged["memory"]["cell_file"] = cellFile;
    merged["optimization"] = YamlHelpers::child_required(root, "optimization");

    const YAML::Node designConstraints =
            YamlHelpers::child_optional(root, "design_constraints");
    if (designConstraints) {
        merged["constraints"] = designConstraints;
    }

    YAML::Node advanced(YAML::NodeType::Map);
    YAML::Node extra(YAML::NodeType::Map);
    const YAML::Node exploration = YamlHelpers::child_optional(root, "exploration");
    if (exploration) {
        CopyIfPresent(exploration, advanced, "use_cacti_assumption");
        CopyIfPresent(exploration, advanced, "enable_pruning");
    }
    const YAML::Node modeling = YamlHelpers::child_optional(root, "modeling");
    if (modeling) {
        if (HasKey(modeling, "use_updated_lib")) {
            throw std::runtime_error(
                    "[Input] Error: modeling.use_updated_lib was removed; select a technology file instead.");
        }
        CopyIfPresent(modeling, advanced, "exclude_precharge_latency");
        CopyIfPresent(modeling, advanced, "include_leakage");
        CopyIfPresent(modeling, advanced, "scaled_voltage");
    }
    const std::string technologyReference = YamlHelpers::read_required<std::string>(root, "technology");
    const std::string technologyFile = ResolveReference(configFile, technologyReference);
    const YAML::Node technologyRoot = YAML::LoadFile(technologyFile);
    YamlHelpers::require_schema(technologyRoot, "technology", "technology config");
    const std::string libraryModel = YamlHelpers::read_optional<std::string>(
            technologyRoot, "library_model", "");
    if (libraryModel != "updated" && libraryModel != "legacy" && !libraryModel.empty()) {
        throw std::runtime_error("[Input] Error: unsupported technology library_model: "
                + libraryModel);
    }
    extra["technology_file"] = technologyFile;

    const YAML::Node output = YamlHelpers::child_optional(root, "output");
    if (output) {
        if (HasKey(output, "yaml_file")) {
            throw std::runtime_error(
                    "[Input] Error: output.yaml_file was removed; use output.results.");
        }
        const YAML::Node results = YamlHelpers::child_optional(output, "results");
        if (results) {
            extra["output_yaml_file"] = results;
            extra["output_yaml_file_from_config"] = true;
        }
        CopyMappedIfPresent(output, "exploration_csv_prefix", extra, "output_file_prefix");
    }

    CopyMappedIfPresent(architectureMemory, "physical_capacity", extra, "real_capacity");
    const YAML::Node peripherals =
            YamlHelpers::child_required(architectureRoot, "peripherals");
    const YAML::Node peripheralInput = YamlHelpers::child_required(peripherals, "input");
    CopyMappedIfPresent(peripheralInput, "encoder_type", advanced, "input_encoder_type");
    CopyMappedIfPresent(
            architectureSensing, "worst_case_sense_margin", extra, "worst_case_sense_margin");
    CopyMappedIfPresent(
            architectureSensing, "sense_amp_input_file", advanced, "sense_amp_input_file");
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
    if (advanced.size() != 0) {
        merged["advanced"] = advanced;
    }
    if (extra.size() != 0) {
        merged["extra"] = extra;
    }
    return merged;
}

}  // namespace

void EvaCamYamlLoader::Load(const std::string &inputFile, EvaCamConfig &config) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    ReadMergedConfig(BuildMergedRootV2(inputFile, root), config);
}
