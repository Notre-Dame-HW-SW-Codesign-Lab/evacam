#include "input/SenseAmpYamlLoader.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

namespace {

double ReadOptionalFeatureWidth(const YAML::Node& node, const char* key, double fallback) {
    const YAML::Node value = YamlHelpers::child_optional(node, key);
    if (!value) {
        return fallback;
    }
    return YamlHelpers::parse_quantity_node(value, {{"F", 1.0}}, 1.0, key);
}

double ReadOptionalFeatureArea(const YAML::Node& node, const char* key, double fallback) {
    const YAML::Node value = YamlHelpers::child_optional(node, key);
    if (!value) {
        return fallback;
    }
    return YamlHelpers::parse_quantity_node(value, {{"F^2", 1.0}}, 1.0, key);
}

std::vector<SenseAmpModel::NodeValue> ReadNodeTable(
        const YAML::Node& parent,
        const char* key,
        const char* valueKey,
        const std::vector<YamlHelpers::UnitSpec>& valueUnits,
        double defaultUnitToBase) {
    std::vector<SenseAmpModel::NodeValue> table;
    const YAML::Node nodes = YamlHelpers::child_required(parent, key);
    if (!nodes.IsSequence()) {
        throw std::runtime_error(std::string("sense_amp.") + key + " must be a sequence");
    }
    for (const YAML::Node& entry : nodes) {
        YamlHelpers::reject_unknown_keys(entry, {"min_node", valueKey},
                std::string("sense_amp.iv_converter.") + key);
        SenseAmpModel::NodeValue item;
        const YAML::Node minNode = YamlHelpers::child_optional(entry, "min_node");
        item.minFeatureSize = minNode && !minNode.IsNull()
                ? YamlHelpers::parse_quantity_node(
                        minNode, YamlHelpers::LengthUnits(), 1.0, "sense_amp.min_node")
                : 0;
        item.value = YamlHelpers::read_quantity_required(
                entry, valueKey, valueUnits, defaultUnitToBase, valueKey);
        table.push_back(item);
    }
    if (table.empty()) {
        throw std::runtime_error(std::string("[Input] Error: sense_amp.") + key
                + " must contain at least one entry.");
    }
    for (std::size_t i = 0; i < table.size(); ++i) {
        YamlHelpers::require_non_negative(
                table[i].minFeatureSize,
                std::string("sense_amp.iv_converter.") + key + "["
                        + std::to_string(i) + "].min_node");
        YamlHelpers::require_positive(
                table[i].value,
                std::string("sense_amp.iv_converter.") + key + "["
                        + std::to_string(i) + "]." + valueKey);
        if (i > 0 && table[i].minFeatureSize >= table[i - 1].minFeatureSize) {
            throw std::runtime_error(std::string("[Input] Error: sense_amp.iv_converter.")
                    + key + " min_node thresholds must be strictly descending.");
        }
    }
    if (table.back().minFeatureSize != 0) {
        throw std::runtime_error(std::string("[Input] Error: sense_amp.iv_converter.")
                + key + " must end with a null min_node fallback.");
    }
    return table;
}

}  // namespace

namespace YamlHelpers {

SenseAmpModel ReadSenseAmpModelFromYaml(const std::string& inputFile) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    require_schema(root, "sense_amp", "sense amp config");
    reject_unknown_keys(root,
            {"schema", "name", "model", "supported_modes", "layout", "transistors",
             "iv_converter"},
            "sense_amp");

    const YAML::Node supportedModes = child_optional(root, "supported_modes");
    reject_unknown_keys(supportedModes, {"voltage_sense", "current_sense", "discharge"},
            "sense_amp.supported_modes");
    for (const char* mode : {"voltage_sense", "current_sense", "discharge"}) {
        reject_unknown_keys(child_optional(supportedModes, mode), {"current_sense"},
                std::string("sense_amp.supported_modes.") + mode);
    }
    SenseAmpModel model;
    model.loaded = true;
    model.model = YamlHelpers::read_required<std::string>(root, "model");
    if (model.model != "nvsim_cmos") {
        throw std::runtime_error("sense_amp model is not nvsim_cmos");
    }

    const YAML::Node layout = YamlHelpers::child_required(root, "layout");
    reject_unknown_keys(layout, {"min_pitch", "same_type_diff_gap", "p_to_n_diff_gap"},
            "sense_amp.layout");
    model.minPitch = ReadOptionalFeatureWidth(layout, "min_pitch", model.minPitch);
    model.sameTypeDiffGap = ReadOptionalFeatureWidth(
            layout, "same_type_diff_gap", model.sameTypeDiffGap);
    model.pToNDiffGap = ReadOptionalFeatureWidth(layout, "p_to_n_diff_gap", model.pToNDiffGap);
    YamlHelpers::require_positive(model.minPitch, "sense_amp.layout.min_pitch");
    YamlHelpers::require_non_negative(
            model.sameTypeDiffGap, "sense_amp.layout.same_type_diff_gap");
    YamlHelpers::require_non_negative(
            model.pToNDiffGap, "sense_amp.layout.p_to_n_diff_gap");

    const YAML::Node transistors = YamlHelpers::child_required(root, "transistors");
    reject_unknown_keys(transistors,
            {"p_sense_width", "n_sense_width", "isolation_width", "enable_width",
             "mux_width"},
            "sense_amp.transistors");
    model.pSenseWidth = ReadOptionalFeatureWidth(
            transistors, "p_sense_width", model.pSenseWidth);
    model.nSenseWidth = ReadOptionalFeatureWidth(
            transistors, "n_sense_width", model.nSenseWidth);
    model.isolationWidth = ReadOptionalFeatureWidth(
            transistors, "isolation_width", model.isolationWidth);
    model.enableWidth = ReadOptionalFeatureWidth(
            transistors, "enable_width", model.enableWidth);
    model.muxWidth = ReadOptionalFeatureWidth(transistors, "mux_width", model.muxWidth);
    YamlHelpers::require_positive(
            model.pSenseWidth, "sense_amp.transistors.p_sense_width");
    YamlHelpers::require_positive(
            model.nSenseWidth, "sense_amp.transistors.n_sense_width");
    YamlHelpers::require_positive(
            model.isolationWidth, "sense_amp.transistors.isolation_width");
    YamlHelpers::require_positive(
            model.enableWidth, "sense_amp.transistors.enable_width");
    YamlHelpers::require_positive(model.muxWidth, "sense_amp.transistors.mux_width");

    const YAML::Node ivConverter = YamlHelpers::child_required(root, "iv_converter");
    reject_unknown_keys(ivConverter,
            {"area", "current_sense_latency", "current_sense_energy",
             "current_sense_leakage"},
            "sense_amp.iv_converter");
    model.ivConverterArea = ReadOptionalFeatureArea(
            ivConverter, "area", model.ivConverterArea);
    YamlHelpers::require_positive(model.ivConverterArea, "sense_amp.iv_converter.area");
    model.currentSenseLatency = ReadNodeTable(
            ivConverter, "current_sense_latency", "latency", TimeUnits(), 1.0);
    model.currentSenseEnergy = ReadNodeTable(
            ivConverter, "current_sense_energy", "energy", EnergyUnits(), 1.0);
    model.currentSenseLeakage = ReadNodeTable(
            ivConverter, "current_sense_leakage", "leakage", PowerUnits(), 1.0);
    return model;
}

}  // namespace YamlHelpers
