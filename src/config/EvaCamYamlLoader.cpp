#include "config/EvaCamYamlLoader.h"

#include <filesystem>
#include <initializer_list>
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

using YamlHelpers::reject_unknown_keys;

void ValidateRunConfigKeys(const YAML::Node &root) {
    reject_unknown_keys(root,
            {"schema", "name", "architecture", "cell", "technology", "optimization",
             "design_constraints", "exploration", "modeling", "output"},
            "run config");
    reject_unknown_keys(YamlHelpers::child_optional(root, "optimization"),
            {"target", "deep_exploration", "buffer_design", "row_driver", "priority_encoder"},
            "optimization");
    reject_unknown_keys(YamlHelpers::child_optional(root, "design_constraints"),
            {"enabled", "read_latency", "write_latency", "read_dynamic_energy",
             "write_dynamic_energy", "leakage", "area", "read_edp", "write_edp"},
            "design_constraints");
    reject_unknown_keys(YamlHelpers::child_optional(root, "exploration"),
            {"use_cacti_assumption", "enable_pruning"}, "exploration");
    reject_unknown_keys(YamlHelpers::child_optional(root, "modeling"),
            {"exclude_precharge_latency", "include_leakage", "scaled_voltage", "use_updated_lib"},
            "modeling");
    reject_unknown_keys(YamlHelpers::child_optional(root, "output"),
            {"results", "exploration_csv_prefix", "yaml_file"}, "output");
}

void ValidateArchitectureConfigKeys(const YAML::Node &root) {
    reject_unknown_keys(root,
            {"schema", "name", "design", "memory", "routing", "peripherals", "sensing",
             "wires", "organization", "matchline", "flash", "physical_limits"},
            "architecture config");
    reject_unknown_keys(YamlHelpers::child_optional(root, "design"),
            {"target", "search_function", "system_process_node", "device_roadmap", "temperature"},
            "design");
    reject_unknown_keys(YamlHelpers::child_optional(root, "memory"),
            {"capacity", "physical_capacity", "word_width", "vector_dimensions"}, "memory");
    reject_unknown_keys(YamlHelpers::child_optional(root, "routing"), {"type"}, "routing");

    const YAML::Node peripherals = YamlHelpers::child_optional(root, "peripherals");
    reject_unknown_keys(peripherals, {"write_driver", "input", "output"}, "peripherals");
    reject_unknown_keys(YamlHelpers::child_optional(peripherals, "input"),
            {"buffer", "encoder", "custom_encoder", "encoder_type"}, "peripherals.input");
    reject_unknown_keys(YamlHelpers::child_optional(peripherals, "output"),
            {"buffer", "priority_encoder", "accumulator"}, "peripherals.output");

    const YAML::Node wires = YamlHelpers::child_optional(root, "wires");
    reject_unknown_keys(wires, {"local", "global"}, "wires");
    reject_unknown_keys(YamlHelpers::child_optional(wires, "local"),
            {"type", "repeater", "low_swing"}, "wires.local");
    reject_unknown_keys(YamlHelpers::child_optional(wires, "global"),
            {"type", "repeater", "low_swing"}, "wires.global");

    const YAML::Node organization = YamlHelpers::child_optional(root, "organization");
    reject_unknown_keys(organization, {"banks", "mats", "mux", "subarray", "bit_serial_width",
            "comparison_columns_per_step"},
            "organization");
    for (const char *section : {"banks", "mats"}) {
        reject_unknown_keys(YamlHelpers::child_optional(organization, section), {"total", "active"},
                std::string("organization.") + section);
    }
    reject_unknown_keys(YamlHelpers::child_optional(organization, "mux"),
            {"sense_amp", "output_level1", "output_level2"}, "organization.mux");
    reject_unknown_keys(YamlHelpers::child_optional(organization, "subarray"), {"dimensions"},
            "organization.subarray");

    const YAML::Node matchline = YamlHelpers::child_optional(root, "matchline");
    reject_unknown_keys(matchline, {"additional_cap", "match_transistor"}, "matchline");
    reject_unknown_keys(YamlHelpers::child_optional(matchline, "match_transistor"), {"cmos_width"},
            "matchline.match_transistor");
    reject_unknown_keys(YamlHelpers::child_optional(root, "flash"), {"page_size", "block_size"}, "flash");
    reject_unknown_keys(YamlHelpers::child_optional(root, "physical_limits"),
            {"max_nmos_size", "max_driver_current"}, "physical_limits");
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
    reject_unknown_keys(resolved,
            {"schema", "name", "internal", "custom_sense_amp", "sensing_mode",
             "sense_amplifier", "worst_case_sense_margin", "strict_sense_margin"},
            "sensing");
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
    ValidateRunConfigKeys(root);

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
    ValidateArchitectureConfigKeys(architectureRoot);

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
        if (YamlHelpers::child_optional(organization, "bit_serial_width")
                && YamlHelpers::child_optional(organization, "comparison_columns_per_step")) {
            throw std::runtime_error(
                    "[Input] Error: organization cannot specify both comparison_columns_per_step and bit_serial_width.");
        }
        CopyMappedIfPresent(organization, "comparison_columns_per_step",
                advanced, "bit_serial_width");
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
