#include "input/YamlUnitParsers.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "input/YamlNodeHelpers.h"

namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    return s.substr(start, end - start);
}

bool unit_suffix_matches(const std::string& actual, const char* expected) {
    if (actual == expected) {
        return true;
    }

    const std::string expectedStr(expected);
    return actual.size() == expectedStr.size()
        && !actual.empty()
        && actual[0] == 'K'
        && expectedStr[0] == 'k'
        && actual.substr(1) == expectedStr.substr(1);
}

double parse_quantity_string(
        const std::string& raw,
        const std::vector<YamlHelpers::UnitSpec>& units,
        double default_unit_to_base,
        const char* what) {

    std::string s = trim(raw);
    if (s.empty())
        throw std::runtime_error(std::string("Empty value for ") + what);

    const char* begin = s.c_str();
    char* end = nullptr;
    double value = std::strtod(begin, &end);
    if (end == begin) {
        throw std::runtime_error(std::string("Invalid numeric value for ") + what + ": " + raw);
    }
    if (!std::isfinite(value)) {
        throw std::runtime_error(std::string("Non-finite value for ") + what + ": " + raw);
    }

    std::string unit = trim(std::string(end));
    if (unit.empty()) {
        const double converted = value * default_unit_to_base;
        if (!std::isfinite(converted)) {
            throw std::runtime_error(std::string("Non-finite value for ") + what + ": " + raw);
        }
        return converted;
    }

    for (const auto& u : units) {
        if (unit_suffix_matches(unit, u.suffix)) {
            const double converted = value * u.to_base;
            if (!std::isfinite(converted)) {
                throw std::runtime_error(std::string("Non-finite value for ") + what + ": " + raw);
            }
            return converted;
        }
    }

    throw std::runtime_error(std::string("Unknown unit '") + unit + "' for " + what);
}

}  // namespace

namespace YamlHelpers {

double parse_quantity_node(
        const YAML::Node& node,
        const std::vector<UnitSpec>& units,
        double default_unit_to_base,
        const char* what) {

    if (!node) {
        throw std::runtime_error(std::string("Missing value for ") + what);
    }

    if (node.IsScalar()) {
        const std::string raw = node.as<std::string>();
        return parse_quantity_string(raw, units, default_unit_to_base, what);
    }

    if (node.IsNull()) {
        throw std::runtime_error(std::string("Null value for ") + what);
    }

    throw std::runtime_error(std::string("Invalid node type for ") + what + ": " + kind(node));
}

double read_quantity_required(
        const YAML::Node& parent,
        const char* key,
        const std::vector<UnitSpec>& units,
        double default_unit_to_base,
        const char* what) {

    YAML::Node n = child_required(parent, key);
    return parse_quantity_node(n, units, default_unit_to_base, what);
}

const std::vector<UnitSpec>& VoltageUnits() {
    static const std::vector<UnitSpec> k = {
        {"V", 1.0},
        {"mV", 1e-3},
        {"uV", 1e-6},
        {"nV", 1e-9},
        {"kV", 1e3},
    };
    return k;
}

const std::vector<UnitSpec>& CurrentUnits() {
    static const std::vector<UnitSpec> k = {
        {"A", 1.0},
        {"mA", 1e-3},
        {"uA", 1e-6},
        {"nA", 1e-9},
        {"pA", 1e-12},
    };
    return k;
}

const std::vector<UnitSpec>& TimeUnits() {
    static const std::vector<UnitSpec> k = {
        {"s", 1.0},
        {"ms", 1e-3},
        {"us", 1e-6},
        {"ns", 1e-9},
        {"ps", 1e-12},
        {"fs", 1e-15},
    };
    return k;
}

const std::vector<UnitSpec>& CapacitanceUnits() {
    static const std::vector<UnitSpec> k = {
        {"F", 1.0},
        {"mF", 1e-3},
        {"uF", 1e-6},
        {"nF", 1e-9},
        {"pF", 1e-12},
        {"fF", 1e-15},
    };
    return k;
}

const std::vector<UnitSpec>& ResistanceUnits() {
    static const std::vector<UnitSpec> k = {
        {"ohm", 1.0},
        {"kohm", 1e3},
        {"Mohm", 1e6},
        {"Gohm", 1e9},
    };
    return k;
}

const std::vector<UnitSpec>& PowerUnits() {
    static const std::vector<UnitSpec> k = {
        {"W", 1.0},
        {"mW", 1e-3},
        {"uW", 1e-6},
        {"nW", 1e-9},
        {"pW", 1e-12},
    };
    return k;
}

const std::vector<UnitSpec>& EnergyUnits() {
    static const std::vector<UnitSpec> k = {
        {"J", 1.0},
        {"mJ", 1e-3},
        {"uJ", 1e-6},
        {"nJ", 1e-9},
        {"pJ", 1e-12},
        {"fJ", 1e-15},
    };
    return k;
}

const std::vector<UnitSpec>& TemperatureUnits() {
    static const std::vector<UnitSpec> k = {
        {"K", 1.0},
    };
    return k;
}

const std::vector<UnitSpec>& DataSizeUnits() {
    static const std::vector<UnitSpec> k = {
        {"B", 1.0},
        {"b", 1.0},
        {"Byte", 1.0},
        {"byte", 1.0},
        {"KB", 1024.0},
        {"kB", 1024.0},
        {"kb", 1024.0},
        {"MB", 1024.0 * 1024.0},
        {"mB", 1024.0 * 1024.0},
        {"mb", 1024.0 * 1024.0},
        {"GB", 1024.0 * 1024.0 * 1024.0},
        {"gB", 1024.0 * 1024.0 * 1024.0},
        {"gb", 1024.0 * 1024.0 * 1024.0},
    };
    return k;
}

const std::vector<UnitSpec>& BitUnits() {
    static const std::vector<UnitSpec> k = {
        {"bit", 1.0},
        {"bits", 1.0},
    };
    return k;
}

const std::vector<UnitSpec>& LengthUnits() {
    static const std::vector<UnitSpec> k = {
        {"m", 1.0},
        {"cm", 1e-2},
        {"mm", 1e-3},
        {"um", 1e-6},
        {"nm", 1e-9},
    };
    return k;
}

const std::vector<UnitSpec>& FeatureUnits() {
    static const std::vector<UnitSpec> k = {
        {"F", 1.0},
    };
    return k;
}

const std::vector<UnitSpec>& FeatureAreaUnits() {
    static const std::vector<UnitSpec> k = {
        {"F^2", 1.0},
    };
    return k;
}

}  // namespace YamlHelpers
