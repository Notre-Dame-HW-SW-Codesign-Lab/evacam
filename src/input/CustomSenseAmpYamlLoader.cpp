#include "input/CustomSenseAmpYamlLoader.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "SenseAmp.h"
#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

namespace {

const std::vector<YamlHelpers::UnitSpec>& AreaUnits() {
    static const std::vector<YamlHelpers::UnitSpec> k = {
        {"m^2", 1.0},
        {"cm^2", 1e-4},
        {"mm^2", 1e-6},
        {"um^2", 1e-12},
        {"nm^2", 1e-18},
    };
    return k;
}

std::vector<YamlHelpers::UnitSpec> FeatureLengthUnits(double featureSize) {
    return {
        {"F", featureSize},
    };
}

double ReadOptionalQuantity(
        const YAML::Node& parent,
        const char* key,
        const std::vector<YamlHelpers::UnitSpec>& units,
        double defaultUnitToBase,
        const char* what) {
    const YAML::Node node = YamlHelpers::child_optional(parent, key);
    if (!node) {
        return 0;
    }
    return YamlHelpers::parse_quantity_node(node, units, defaultUnitToBase, what);
}

}  // namespace

namespace YamlHelpers {

void ReadCustomSenseAmpFromYaml(
        SenseAmp& senseAmp,
        const std::string& inputFile,
        double featureSize) {
    if (!is_yaml_file(inputFile)) {
        throw std::runtime_error(
                "Only YAML custom sense amp files are supported. Please provide a .yaml/.yml custom SA file.");
    }

    const YAML::Node root = YAML::LoadFile(inputFile);
    const YAML::Node customSenseAmpNode = child_optional(root, "custom_sense_amp");
    const YAML::Node customSenseAmp = customSenseAmpNode ? customSenseAmpNode : root;

    if (!customSenseAmp || !customSenseAmp.IsMap()) {
        throw std::runtime_error("custom sense amp file must contain a mapping.");
    }

    senseAmp.height = ReadOptionalQuantity(
            customSenseAmp, "height", FeatureLengthUnits(featureSize), featureSize, "custom_sense_amp.height");
    senseAmp.width = ReadOptionalQuantity(
            customSenseAmp, "width", FeatureLengthUnits(featureSize), featureSize, "custom_sense_amp.width");
    senseAmp.area = ReadOptionalQuantity(
            customSenseAmp, "area", AreaUnits(), 1.0, "custom_sense_amp.area");
    senseAmp.readLatency = ReadOptionalQuantity(
            customSenseAmp, "latency", TimeUnits(), 1.0, "custom_sense_amp.latency");
    senseAmp.readDynamicEnergy = ReadOptionalQuantity(
            customSenseAmp, "energy", EnergyUnits(), 1.0, "custom_sense_amp.energy");
    senseAmp.leakage = ReadOptionalQuantity(
            customSenseAmp, "leakage", PowerUnits(), 1.0, "custom_sense_amp.leakage");
    senseAmp.capLoad = ReadOptionalQuantity(
            customSenseAmp, "cap_load", CapacitanceUnits(), 1.0, "custom_sense_amp.cap_load");

    if (senseAmp.area == 0 && !(senseAmp.height > 0 && senseAmp.width > 0)) {
        throw std::runtime_error(
                "[Input] Error: custom sense amp file is missing required fields.");
    }
    if (senseAmp.readLatency == 0 || senseAmp.readDynamicEnergy == 0 || senseAmp.capLoad == 0) {
        throw std::runtime_error(
                "[Input] Error: custom sense amp file is missing required fields.");
    }

    if (senseAmp.area == 0) {
        senseAmp.area = senseAmp.height * senseAmp.width;
    }
}

}  // namespace YamlHelpers
