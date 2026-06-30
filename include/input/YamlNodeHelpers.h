#ifndef INPUT_YAMLNODEHELPERS_H_
#define INPUT_YAMLNODEHELPERS_H_

#include <yaml.h>

#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "typedef.h"

namespace YamlHelpers {

std::string kind(const YAML::Node& n);
YAML::Node child_required(const YAML::Node& parent, const char* key);
YAML::Node child_optional(const YAML::Node& parent, const char* key);
YAML::Node child_optional_bool_key(const YAML::Node& parent, bool key);
YAML::Node child_required_index(const YAML::Node& parent, size_t idx, const char* what);
bool is_yaml_file(const std::string& path);

template <typename T>
    T read_required(const YAML::Node& parent, const char* key) {
        YAML::Node n = child_required(parent, key);
        try {
            return n.as<T>();
        } catch (const YAML::Exception& e) {
            std::string msg = "Bad conversion for key '" + std::string(key) + "': " + e.what();
            throw std::runtime_error(msg);
        }
    }

template <typename T>
    T read_scalar_required(const YAML::Node& node, const char* what) {
        if (!node) {
            throw std::runtime_error(std::string("Missing node for ") + what);
        }
        try {
            return node.as<T>();
        } catch (const YAML::Exception& e) {
            std::string msg = "Bad conversion for " + std::string(what) + ": " + e.what();
            throw std::runtime_error(msg);
        }
    }

template <typename T>
    T read_required_index(const YAML::Node& parent, size_t idx, const char* what) {
        YAML::Node n = child_required_index(parent, idx, what);
        try {
            return n.as<T>();
        } catch (const YAML::Exception& e) {
            std::string msg = "Bad conversion for " + std::string(what) + ": " + e.what();
            throw std::runtime_error(msg);
        }
    }

template <typename T>
    T read_optional(const YAML::Node& parent, const char* key, const T& def) {
        YAML::Node n = child_optional(parent, key);
        if (!n)
            return def;
        try {
            return n.as<T>();
        } catch (const YAML::Exception& e) {
            std::string msg = "Bad conversion for key '" + std::string(key) + "': " + e.what();
            throw std::runtime_error(msg);
        }
    }

template <typename Enum>
    struct EnumTraits;

template <typename Enum>
    Enum read_enum_required(
            const YAML::Node& parent,
            const char* key,
            bool case_sensitive = true) {
        const std::string raw = read_required<std::string>(parent, key);
        std::string value = raw;
        if (!case_sensitive) {
            for (char& c : value)
                c = static_cast<char>(::tolower(c));
        }

        const auto& mapping = EnumTraits<Enum>::mapping();
        for (const auto& kv : mapping) {
            std::string opt = kv.first;
            if (!case_sensitive) {
                for (char& c : opt)
                    c = static_cast<char>(::tolower(c));
            }
            if (value == opt)
                return kv.second;
        }

        std::string msg = "Invalid value for '" + std::string(key) + "': " + raw;
        throw std::runtime_error(msg);
    }

template <typename Enum>
    Enum read_enum_required(
            const YAML::Node& parent,
            const char* key,
            std::initializer_list<std::pair<const char*, Enum>> mapping,
            bool case_sensitive = true) {

        const std::string raw = read_required<std::string>(parent, key);
        std::string value = raw;
        if (!case_sensitive) {
            for (char& c : value)
                c = static_cast<char>(::tolower(c));
        }

        for (const auto& kv : mapping) {
            std::string opt = kv.first;
            if (!case_sensitive) {
                for (char& c : opt)
                    c = static_cast<char>(::tolower(c));
            }
            if (value == opt)
                return kv.second;
        }

        std::string msg = "Invalid value for '" + std::string(key) + "': " + raw;
        throw std::runtime_error(msg);
    }

template <>
    struct EnumTraits<MemCellType> {
        static const std::vector<std::pair<const char*, MemCellType>>& mapping();
    };
template <>
    struct EnumTraits<CellAccessType> {
        static const std::vector<std::pair<const char*, CellAccessType>>& mapping();
    };
template <>
    struct EnumTraits<DeviceRoadmap> {
        static const std::vector<std::pair<const char*, DeviceRoadmap>>& mapping();
    };
template <>
    struct EnumTraits<WireType> {
        static const std::vector<std::pair<const char*, WireType>>& mapping();
    };
template <>
    struct EnumTraits<WireRepeaterType> {
        static const std::vector<std::pair<const char*, WireRepeaterType>>& mapping();
    };
template <>
    struct EnumTraits<BufferDesignTarget> {
        static const std::vector<std::pair<const char*, BufferDesignTarget>>& mapping();
    };
template <>
    struct EnumTraits<CAMType> {
        static const std::vector<std::pair<const char*, CAMType>>& mapping();
    };
template <>
    struct EnumTraits<SearchFunction> {
        static const std::vector<std::pair<const char*, SearchFunction>>& mapping();
    };
template <>
    struct EnumTraits<RoutingMode> {
        static const std::vector<std::pair<const char*, RoutingMode>>& mapping();
    };
template <>
    struct EnumTraits<WriteScheme> {
        static const std::vector<std::pair<const char*, WriteScheme>>& mapping();
    };
template <>
    struct EnumTraits<DesignTarget> {
        static const std::vector<std::pair<const char*, DesignTarget>>& mapping();
    };
template <>
    struct EnumTraits<OptimizationTarget> {
        static const std::vector<std::pair<const char*, OptimizationTarget>>& mapping();
    };
template <>
    struct EnumTraits<TypeOfInputEncoder> {
        static const std::vector<std::pair<const char*, TypeOfInputEncoder>>& mapping();
    };
template <>
    struct EnumTraits<TypeOfSenseAmp> {
        static const std::vector<std::pair<const char*, TypeOfSenseAmp>>& mapping();
    };
template <>
    struct EnumTraits<CAM_PortType> {
        static const std::vector<std::pair<const char*, CAM_PortType>>& mapping();
    };
template <>
    struct EnumTraits<CAM_CmosRegion> {
        static const std::vector<std::pair<const char*, CAM_CmosRegion>>& mapping();
    };

}  // namespace YamlHelpers

#endif  // INPUT_YAMLNODEHELPERS_H_
