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

    const YAML::Node ivConverter = YamlHelpers::child_required(root, "iv_converter");
    reject_unknown_keys(ivConverter,
            {"area", "current_sense_latency", "current_sense_energy",
             "current_sense_leakage"},
            "sense_amp.iv_converter");
    model.ivConverterArea = ReadOptionalFeatureArea(
            ivConverter, "area", model.ivConverterArea);
    model.currentSenseLatency = ReadNodeTable(
            ivConverter, "current_sense_latency", "latency", TimeUnits(), 1.0);
    model.currentSenseEnergy = ReadNodeTable(
            ivConverter, "current_sense_energy", "energy", EnergyUnits(), 1.0);
    model.currentSenseLeakage = ReadNodeTable(
            ivConverter, "current_sense_leakage", "leakage", PowerUnits(), 1.0);
    return model;
}

}  // namespace YamlHelpers
