#ifndef INPUT_YAMLNODEHELPERS_H_
#define INPUT_YAMLNODEHELPERS_H_

#include <yaml.h>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "typedef.h"

namespace YamlHelpers {

std::recursive_mutex &ParserMutex();

template <typename T>
    std::string value_string(T value) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    }

std::string kind(const YAML::Node& n);
YAML::Node child_required(const YAML::Node& parent, const char* key);
YAML::Node child_optional(const YAML::Node& parent, const char* key);
YAML::Node child_optional_bool_key(const YAML::Node& parent, bool key);
YAML::Node child_required_index(const YAML::Node& parent, size_t idx, const char* what);
bool is_yaml_file(const std::string& path);
bool schema_matches(const YAML::Node& root, const std::string& canonical);
void require_schema(const YAML::Node& root, const std::string& canonical, const char* fileKind);
void reject_unknown_keys(const YAML::Node& node,
        std::initializer_list<const char*> allowedKeys, const std::string& path);

template <typename T>
    T require_finite(T value, const std::string& what) {
        if constexpr (std::is_floating_point_v<T>) {
            if (!std::isfinite(value)) {
                throw std::runtime_error("Non-finite value for " + what);
            }
        }
        return value;
    }

template <typename T>
    T require_positive(T value, const std::string& what) {
        require_finite(value, what);
        if (value <= 0) {
            throw std::runtime_error(
                    "[Input] Error: " + what + " must be positive; got "
                    + value_string(value) + ".");
        }
        return value;
    }

template <typename T>
    T require_non_negative(T value, const std::string& what) {
        require_finite(value, what);
        if (value < 0) {
            throw std::runtime_error(
                    "[Input] Error: " + what + " must be non-negative; got "
                    + value_string(value) + ".");
        }
        return value;
    }

template <typename T>
    T require_non_zero(T value, const std::string& what) {
        require_finite(value, what);
        if (value == 0) {
            throw std::runtime_error(
                    "[Input] Error: " + what + " must be non-zero; got 0.");
        }
        return value;
    }

template <typename T>
    T require_range(T value, T minimum, T maximum, const std::string& what,
            const std::string& rangeDescription = "") {
        require_finite(value, what);
        if (value < minimum || value > maximum) {
            const std::string expected = rangeDescription.empty()
                    ? "between " + value_string(minimum) + " and " + value_string(maximum)
                    : rangeDescription;
            throw std::runtime_error(
                    "[Input] Error: " + what + " must be " + expected + "; got "
                    + value_string(value) + ".");
        }
        return value;
    }

template <typename Integer>
    Integer checked_integer(double value, const std::string& what) {
        static_assert(std::is_integral_v<Integer>, "checked_integer requires an integral type");
        require_finite(value, what);
        const double rounded = std::round(value);
        constexpr double tolerance = 1e-9;
        if (std::fabs(value - rounded) > tolerance) {
            throw std::runtime_error(
                    "[Input] Error: " + what + " must resolve to a whole number; got "
                    + value_string(value) + ".");
        }
        const long double wideRounded = rounded;
        if (wideRounded < static_cast<long double>(std::numeric_limits<Integer>::lowest())
                || wideRounded > static_cast<long double>(std::numeric_limits<Integer>::max())) {
            throw std::runtime_error(
                    "[Input] Error: " + what + " is outside the supported integer range; got "
                    + value_string(value) + ".");
        }
        return static_cast<Integer>(rounded);
    }

template <typename T>
    T read_required(const YAML::Node& parent, const char* key) {
        YAML::Node n = child_required(parent, key);
        try {
            return require_finite(n.as<T>(), "key '" + std::string(key) + "'");
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
            return require_finite(node.as<T>(), what);
        } catch (const YAML::Exception& e) {
            std::string msg = "Bad conversion for " + std::string(what) + ": " + e.what();
            throw std::runtime_error(msg);
        }
    }

template <typename T>
    T read_required_index(const YAML::Node& parent, size_t idx, const char* what) {
        YAML::Node n = child_required_index(parent, idx, what);
        try {
            return require_finite(n.as<T>(), what);
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
            return require_finite(n.as<T>(), "key '" + std::string(key) + "'");
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
