#include "input/YamlNodeHelpers.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace YamlHelpers {

std::string kind(const YAML::Node& n) {
    if (!n)
        return "missing";
    if (n.IsNull())
        return "null";
    if (n.IsScalar())
        return "scalar";
    if (n.IsSequence())
        return "sequence";
    if (n.IsMap())
        return "map";
    return "other";
}

YAML::Node child_required(const YAML::Node& parent, const char* key) {
    if (!parent) {
        throw std::runtime_error(std::string("Parent node missing; cannot read key: ") + key);
    }
    if (!parent.IsMap()) {
        throw std::runtime_error(
                std::string("Cannot read key '") + key + "' from parent of type " + kind(parent) + " (expected map)");
    }

    const YAML::Node cparent = parent;
    YAML::Node child = cparent[key];

    if (!child) {
        std::ostringstream oss;
        oss << "Missing key: " << key;
        if (parent.Mark().line != -1) {
            oss << " (near line " << (parent.Mark().line + 1) << ", col " << (parent.Mark().column + 1) << ")";
        }
        throw std::runtime_error(oss.str());
    }
    return child;
}

YAML::Node child_optional(const YAML::Node& parent, const char* key) {
    if (!parent || !parent.IsMap())
        return YAML::Node();
    const YAML::Node cparent = parent;
    return cparent[key];
}

YAML::Node child_optional_bool_key(const YAML::Node& parent, bool key) {
    if (!parent || !parent.IsMap())
        return YAML::Node();
    const YAML::Node cparent = parent;
    return cparent[key];
}

YAML::Node child_required_index(const YAML::Node& parent, size_t idx, const char* what) {
    if (!parent) {
        throw std::runtime_error(std::string("Parent node missing; cannot read index for ") + what);
    }
    if (!parent.IsSequence()) {
        throw std::runtime_error(
                std::string("Cannot read index for ") + what + " from parent of type " + kind(parent) + " (expected sequence)");
    }
    if (idx >= parent.size()) {
        throw std::runtime_error(std::string("Index out of range for ") + what);
    }
    const YAML::Node cparent = parent;
    YAML::Node child = cparent[idx];
    if (!child) {
        throw std::runtime_error(std::string("Missing value for ") + what);
    }
    return child;
}

bool is_yaml_file(const std::string& path) {
    if (path.size() < 4)
        return false;
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".yaml")
        || (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".yml");
}

const std::vector<std::pair<const char*, MemCellType>>& EnumTraits<MemCellType>::mapping() {
    static const std::vector<std::pair<const char*, MemCellType>> k = {
        {"SRAM", SRAM},
        {"DRAM", DRAM},
        {"eDRAM", eDRAM},
        {"MRAM", MRAM},
        {"PCRAM", PCRAM},
        {"ReRAM", memristor},
        {"memristor", memristor},
        {"FBRAM", FBRAM},
        {"SLCNAND", SLCNAND},
        {"MLCNAND", MLCNAND},
        {"FEFETRAM", FEFETRAM},
    };
    return k;
}

const std::vector<std::pair<const char*, CellAccessType>>& EnumTraits<CellAccessType>::mapping() {
    static const std::vector<std::pair<const char*, CellAccessType>> k = {
        {"CMOS", CMOS_access},
        {"BJT", BJT_access},
        {"diode", diode_access},
        {"none", none_access},
    };
    return k;
}

const std::vector<std::pair<const char*, DeviceRoadmap>>& EnumTraits<DeviceRoadmap>::mapping() {
    static const std::vector<std::pair<const char*, DeviceRoadmap>> k = {
        {"HP", HP},
        {"LSTP", LSTP},
        {"LOP", LOP},
        {"FEFET", FEFET},
        {"LP", LP},
    };
    return k;
}

const std::vector<std::pair<const char*, WireType>>& EnumTraits<WireType>::mapping() {
    static const std::vector<std::pair<const char*, WireType>> k = {
        {"LocalAggressive", local_aggressive},
        {"LocalConservative", local_conservative},
        {"SemiAggressive", semi_aggressive},
        {"SemiConservative", semi_conservative},
        {"GlobalAggressive", global_aggressive},
        {"GlobalConservative", global_conservative},
        {"DramWordline", dram_wordline},
    };
    return k;
}

const std::vector<std::pair<const char*, WireRepeaterType>>& EnumTraits<WireRepeaterType>::mapping() {
    static const std::vector<std::pair<const char*, WireRepeaterType>> k = {
        {"RepeatedNone", repeated_none},
        {"RepeatedOpt", repeated_opt},
        {"Repeated5%Penalty", repeated_5},
        {"Repeated10%Penalty", repeated_10},
        {"Repeated20%Penalty", repeated_20},
        {"Repeated30%Penalty", repeated_30},
        {"Repeated40%Penalty", repeated_40},
        {"Repeated50%Penalty", repeated_50},
    };
    return k;
}

const std::vector<std::pair<const char*, BufferDesignTarget>>& EnumTraits<BufferDesignTarget>::mapping() {
    static const std::vector<std::pair<const char*, BufferDesignTarget>> k = {
        {"latency", latency_first},
        {"balance", latency_area_trade_off},
        {"area", area_first},
    };
    return k;
}

const std::vector<std::pair<const char*, MemoryType>>& EnumTraits<MemoryType>::mapping() {
    static const std::vector<std::pair<const char*, MemoryType>> k = {
        {"mem_data", mem_data},
        {"tag", tag},
        {"CAM", CAM},
    };
    return k;
}

const std::vector<std::pair<const char*, CAMType>>& EnumTraits<CAMType>::mapping() {
    static const std::vector<std::pair<const char*, CAMType>> k = {
        {"TCAM", TCAM},
        {"BCAM", TCAM},
        {"MCAM", MCAM},
        {"ACAM", ACAM},
    };
    return k;
}

const std::vector<std::pair<const char*, SearchFunction>>& EnumTraits<SearchFunction>::mapping() {
    static const std::vector<std::pair<const char*, SearchFunction>> k = {
        {"EX", EX},
        {"BE", BE},
        {"TH", TH},
    };
    return k;
}

const std::vector<std::pair<const char*, RoutingMode>>& EnumTraits<RoutingMode>::mapping() {
    static const std::vector<std::pair<const char*, RoutingMode>> k = {
        {"H-tree", h_tree},
        {"NonH-tree", non_h_tree},
        {"non_h_tree", non_h_tree},
    };
    return k;
}

const std::vector<std::pair<const char*, WriteScheme>>& EnumTraits<WriteScheme>::mapping() {
    static const std::vector<std::pair<const char*, WriteScheme>> k = {
        {"SetBeforeReset", set_before_reset},
        {"set_before_reset", set_before_reset},
        {"ResetBeforeSet", reset_before_set},
        {"reset_before_set", reset_before_set},
        {"EraseBeforeSet", erase_before_set},
        {"erase_before_set", erase_before_set},
        {"EraseBeforeReset", erase_before_reset},
        {"erase_before_reset", erase_before_reset},
        {"WriteAndVerify", write_and_verify},
        {"write_and_verify", write_and_verify},
        {"Normal", normal_write},
        {"normal", normal_write},
        {"normal_write", normal_write},
    };
    return k;
}

const std::vector<std::pair<const char*, DesignTarget>>& EnumTraits<DesignTarget>::mapping() {
    static const std::vector<std::pair<const char*, DesignTarget>> k = {
        {"cache", cache},
        {"RAM", RAM_chip},
        {"CAM", CAM_chip},
    };
    return k;
}

const std::vector<std::pair<const char*, OptimizationTarget>>& EnumTraits<OptimizationTarget>::mapping() {
    static const std::vector<std::pair<const char*, OptimizationTarget>> k = {
        {"ReadLatency", read_latency_optimized},
        {"WriteLatency", write_latency_optimized},
        {"ReadDynamicEnergy", read_energy_optimized},
        {"WriteDynamicEnergy", write_energy_optimized},
        {"ReadEDP", read_edp_optimized},
        {"WriteEDP", write_edp_optimized},
        {"LeakagePower", leakage_optimized},
        {"Area", area_optimized},
        {"SearchLatency", search_latency_optimized},
        {"SearchEnergy", search_energy_optimized},
        {"SearchEDP", search_edp_optimized},
        {"Exploration", full_exploration},
    };
    return k;
}

const std::vector<std::pair<const char*, CacheAccessMode>>& EnumTraits<CacheAccessMode>::mapping() {
    static const std::vector<std::pair<const char*, CacheAccessMode>> k = {
        {"normal", normal_access_mode},
        {"sequential", sequential_access_mode},
        {"fast", fast_access_mode},
    };
    return k;
}

const std::vector<std::pair<const char*, TypeOfInputEncoder>>& EnumTraits<TypeOfInputEncoder>::mapping() {
    static const std::vector<std::pair<const char*, TypeOfInputEncoder>> k = {
        {"encoding_two_bit", encoding_two_bit},
    };
    return k;
}

const std::vector<std::pair<const char*, TypeOfSenseAmp>>& EnumTraits<TypeOfSenseAmp>::mapping() {
    static const std::vector<std::pair<const char*, TypeOfSenseAmp>> k = {
        {"nvsim_vol", nvsim_voltage_sense},
        {"nvsim_cur", nvsim_current_sense},
        {"self_clock", self_clock_sense},
        {"dual_the", dual_threshold_sense},
        {"discharge", discharge},
    };
    return k;
}

const std::vector<std::pair<const char*, CAM_PortType>>& EnumTraits<CAM_PortType>::mapping() {
    static const std::vector<std::pair<const char*, CAM_PortType>> k = {
        {"Wordline", Wordline},
        {"Searchline", Searchline},
        {"Bitline", Bitline},
        {"Dataline", Dataline},
        {"Sourceline", Sourceline},
        {"Matchline", Matchline},
        {"Matchline_Bitline", Matchline_Bitline},
        {"Searchline_Bitline", Searchline_Bitline},
    };
    return k;
}

const std::vector<std::pair<const char*, CAM_CmosRegion>>& EnumTraits<CAM_CmosRegion>::mapping() {
    static const std::vector<std::pair<const char*, CAM_CmosRegion>> k = {
        {"gate", gate},
        {"source", source},
        {"drain", drain},
        {"diode", diode},
        {"none", none},
    };
    return k;
}

}  // namespace YamlHelpers
