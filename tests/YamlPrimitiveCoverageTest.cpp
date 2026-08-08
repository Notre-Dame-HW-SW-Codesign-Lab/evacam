#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace {

void TestParserMutexIsProcessWideAndRecursive() {
    std::recursive_mutex &first = YamlHelpers::ParserMutex();
    std::recursive_mutex &second = YamlHelpers::ParserMutex();
    assert(&first == &second);
    std::lock_guard<std::recursive_mutex> outer(first);
    std::lock_guard<std::recursive_mutex> inner(second);
}

void ExpectThrows(const std::function<void()>& action, const std::string& text) {
    try {
        action();
        assert(false && "Expected runtime_error");
    } catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(text) != std::string::npos);
    }
}

void ExpectNear(double actual, double expected) {
    assert(std::fabs(actual - expected) <= 1e-12 * std::max(1.0, std::fabs(expected)));
}

void TestNodeKindsChildrenAndIndices() {
    const YAML::Node root = YAML::Load("map: {value: 7}\nsequence: [one, two]\nnull: null\nscalar: text\n");
    assert(YamlHelpers::kind(root["missing"]) == "missing");
    // yaml-cpp treats an explicit null as false, so this helper intentionally
    // classifies both a missing child and an explicit null as "missing".
    assert(YamlHelpers::kind(root["null"]) == "missing");
    assert(YamlHelpers::kind(root["scalar"]) == "scalar");
    assert(YamlHelpers::kind(root["sequence"]) == "sequence");
    assert(YamlHelpers::kind(root["map"]) == "map");
    assert(YamlHelpers::child_required(root, "map")["value"].as<int>() == 7);
    assert(!YamlHelpers::child_optional(root, "absent"));
    assert(YamlHelpers::child_optional(YAML::Load("[]"), "value").IsNull());
    assert(YamlHelpers::child_required_index(root["sequence"], 1, "items").as<std::string>() == "two");
    ExpectThrows([] { YamlHelpers::child_required(YAML::Load("{}")["x"], "x"); }, "Parent node missing");
    ExpectThrows([&] { YamlHelpers::child_required(root["sequence"], "x"); }, "expected map");
    ExpectThrows([&] { YamlHelpers::child_required(root, "absent"); }, "Missing key: absent");
    ExpectThrows([] { YamlHelpers::child_required_index(YAML::Load("{}")["x"], 0, "items"); }, "Parent node missing");
    ExpectThrows([&] { YamlHelpers::child_required_index(root["map"], 0, "items"); }, "expected sequence");
    ExpectThrows([&] { YamlHelpers::child_required_index(root["sequence"], 2, "items"); }, "Index out of range");
}

void TestBooleanKeysSchemaSuffixesAndUnknownKeys() {
    const YAML::Node boolKeys = YAML::Load("true: yes\nfalse: no\n");
    assert(YamlHelpers::child_optional_bool_key(boolKeys, true).as<std::string>() == "yes");
    assert(YamlHelpers::child_optional_bool_key(boolKeys, false).as<std::string>() == "no");
    assert(YamlHelpers::child_optional_bool_key(YAML::Load("[]"), true).IsNull());
    assert(YamlHelpers::is_yaml_file("A.YAML"));
    assert(YamlHelpers::is_yaml_file("a.YmL"));
    assert(!YamlHelpers::is_yaml_file("yaml"));
    assert(!YamlHelpers::is_yaml_file("file.txt"));
    const YAML::Node canonical = YAML::Load("schema: cell\n");
    const YAML::Node versioned = YAML::Load("schema: evacam.cell.v2\n");
    assert(YamlHelpers::schema_matches(canonical, "cell"));
    assert(YamlHelpers::schema_matches(versioned, "cell"));
    assert(!YamlHelpers::schema_matches(YAML::Load("schema: [cell]\n"), "cell"));
    assert(!YamlHelpers::schema_matches(YAML::Load("{}"), "cell"));
    YamlHelpers::require_schema(canonical, "cell", "cell file");
    ExpectThrows([] { YamlHelpers::require_schema(YAML::Load("schema: wrong"), "cell", "cell file"); }, "cell file schema must be cell");
    YamlHelpers::reject_unknown_keys(YAML::Load("{known: value}"), {"known"}, "model");
    YamlHelpers::reject_unknown_keys(YAML::Load("[]"), {"known"}, "model");
    ExpectThrows([] { YamlHelpers::reject_unknown_keys(YAML::Load("{typo: value}"), {"known"}, "model"); }, "unknown key 'model.typo'");
    ExpectThrows([] { YamlHelpers::reject_unknown_keys(YAML::Load("? [a, b]\n: value\n"), {"known"}, "model"); }, "non-scalar key");
}

void TestReadTemplatesAndValidationTemplates() {
    const YAML::Node root = YAML::Load("integer: 4\ntext: hello\nfinite: 2.5\nsequence: [8]\n");
    assert(YamlHelpers::value_string(42) == "42");
    assert(YamlHelpers::require_finite(4, "integer") == 4);
    assert(YamlHelpers::require_positive(1.0, "positive") == 1.0);
    assert(YamlHelpers::require_non_negative(0.0, "non-negative") == 0.0);
    assert(YamlHelpers::require_non_zero(-1, "non-zero") == -1);
    assert(YamlHelpers::require_range(4, 4, 4, "range") == 4);
    ExpectThrows([] { YamlHelpers::require_finite(std::numeric_limits<double>::infinity(), "value"); }, "Non-finite value");
    ExpectThrows([] { YamlHelpers::require_positive(-1, "value"); }, "must be positive");
    ExpectThrows([] { YamlHelpers::require_non_negative(-1, "value"); }, "must be non-negative");
    ExpectThrows([] { YamlHelpers::require_non_zero(0, "value"); }, "must be non-zero");
    ExpectThrows([] { YamlHelpers::require_range(3, 4, 5, "value"); }, "between 4 and 5");
    assert(YamlHelpers::read_required<int>(root, "integer") == 4);
    assert(YamlHelpers::read_scalar_required<std::string>(root["text"], "text") == "hello");
    assert(YamlHelpers::read_required_index<int>(root["sequence"], 0, "sequence[0]") == 8);
    assert(YamlHelpers::read_optional<int>(root, "missing", 9) == 9);
    ExpectThrows([&] { YamlHelpers::read_required<int>(root, "missing"); }, "Missing key");
    ExpectThrows([] { YamlHelpers::read_scalar_required<int>(YAML::Load("{}")["missing"], "number"); }, "Missing node");
    ExpectThrows([&] { YamlHelpers::read_required<int>(root, "text"); }, "Bad conversion");
    ExpectThrows([&] { YamlHelpers::read_required_index<int>(root["sequence"], 1, "sequence[1]"); }, "Index out of range");
    ExpectThrows([&] { YamlHelpers::read_optional<int>(root, "text", 0); }, "Bad conversion");
    assert(YamlHelpers::checked_integer<int>(4.0 + 5e-10, "count") == 4);
    ExpectThrows([] { YamlHelpers::checked_integer<int>(4.1, "count"); }, "whole number");
    ExpectThrows([] { YamlHelpers::checked_integer<std::int8_t>(128.0, "count"); }, "outside the supported integer range");
    ExpectThrows([] { YamlHelpers::checked_integer<int>(std::numeric_limits<double>::quiet_NaN(), "count"); }, "Non-finite");
}

template <typename Enum>
void TestEveryEnumMapping(const char* label) {
    const auto& mapping = YamlHelpers::EnumTraits<Enum>::mapping();
    assert(!mapping.empty());
    for (const auto& item : mapping) {
        const YAML::Node root = YAML::Load(std::string("value: ") + item.first + "\n");
        assert(YamlHelpers::read_enum_required<Enum>(root, "value") == item.second);
    }
    ExpectThrows([&] { YamlHelpers::read_enum_required<Enum>(YAML::Load("value: invalid"), "value"); }, "Invalid value");
    (void)label;
}

enum class SmallEnum { Alpha, Beta };

void TestEnumMappingsCaseBehaviorAndInitializerList() {
    TestEveryEnumMapping<MemCellType>("MemCellType"); TestEveryEnumMapping<CellAccessType>("CellAccessType");
    TestEveryEnumMapping<DeviceRoadmap>("DeviceRoadmap"); TestEveryEnumMapping<WireType>("WireType");
    TestEveryEnumMapping<WireRepeaterType>("WireRepeaterType"); TestEveryEnumMapping<BufferDesignTarget>("BufferDesignTarget");
    TestEveryEnumMapping<CAMType>("CAMType"); TestEveryEnumMapping<SearchFunction>("SearchFunction");
    TestEveryEnumMapping<RoutingMode>("RoutingMode"); TestEveryEnumMapping<WriteScheme>("WriteScheme");
    TestEveryEnumMapping<DesignTarget>("DesignTarget"); TestEveryEnumMapping<OptimizationTarget>("OptimizationTarget");
    TestEveryEnumMapping<TypeOfInputEncoder>("TypeOfInputEncoder"); TestEveryEnumMapping<TypeOfSenseAmp>("TypeOfSenseAmp");
    TestEveryEnumMapping<CAM_PortType>("CAM_PortType"); TestEveryEnumMapping<CAM_CmosRegion>("CAM_CmosRegion");
    const YAML::Node upper = YAML::Load("value: ALPHA\n");
    assert(YamlHelpers::read_enum_required<SmallEnum>(upper, "value", {{"alpha", SmallEnum::Alpha}}, false) == SmallEnum::Alpha);
    ExpectThrows([&] { YamlHelpers::read_enum_required<SmallEnum>(upper, "value", {{"alpha", SmallEnum::Alpha}}, true); }, "Invalid value");
}

void TestAllUnitTablesAndUnitlessPolicy() {
    const std::vector<std::pair<const std::vector<YamlHelpers::UnitSpec>*, double>> tables = {
        {&YamlHelpers::VoltageUnits(), 2.0}, {&YamlHelpers::McamCenterVoltageUnits(), 2.0}, {&YamlHelpers::CurrentUnits(), 2.0},
        {&YamlHelpers::TimeUnits(), 2.0}, {&YamlHelpers::CapacitanceUnits(), 2.0}, {&YamlHelpers::ResistanceUnits(), 2.0},
        {&YamlHelpers::PowerUnits(), 2.0}, {&YamlHelpers::EnergyUnits(), 2.0}, {&YamlHelpers::TemperatureUnits(), 2.0},
        {&YamlHelpers::DataSizeUnits(), 2.0}, {&YamlHelpers::BitUnits(), 2.0}, {&YamlHelpers::LengthUnits(), 2.0},
        {&YamlHelpers::FeatureUnits(), 2.0}, {&YamlHelpers::FeatureAreaUnits(), 2.0}};
    for (const auto& table : tables) {
        assert(!table.first->empty());
        for (const auto& unit : *table.first) {
            ExpectNear(YamlHelpers::parse_quantity_node(YAML::Load(std::string("2") + unit.suffix), *table.first, 7.0, "quantity"), 2.0 * unit.to_base);
        }
        ExpectNear(YamlHelpers::parse_quantity_node(YAML::Load("2"), *table.first, table.second, "quantity"), 4.0);
    }
}

void TestQuantityPrivateParserBehaviorAndErrors() {
    ExpectNear(YamlHelpers::parse_quantity_node(YAML::Load("  2.5 mV  "), YamlHelpers::VoltageUnits(), 1.0, "voltage"), .0025);
    ExpectNear(YamlHelpers::parse_quantity_node(YAML::Load("3Kohm"), YamlHelpers::ResistanceUnits(), 1.0, "resistance"), 3000.0);
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("3MV"), YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Unknown unit");
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("3ohm"), YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Unknown unit");
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("nanV"), YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Non-finite");
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("''"), YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Empty value");
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("letters"), YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Invalid numeric");
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("[1]"), YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Invalid node type");
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("null"), YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Null value");
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("{}")["missing"], YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Missing value");
    ExpectThrows([] { YamlHelpers::parse_quantity_node(YAML::Load("1e308kV"), YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Non-finite");
    ExpectThrows([] { YamlHelpers::read_quantity_required(YAML::Load("{}"), "voltage", YamlHelpers::VoltageUnits(), 1.0, "voltage"); }, "Missing key");
}

}  // namespace

int main() {
    TestParserMutexIsProcessWideAndRecursive();
    TestNodeKindsChildrenAndIndices();
    TestBooleanKeysSchemaSuffixesAndUnknownKeys();
    TestReadTemplatesAndValidationTemplates();
    TestEnumMappingsCaseBehaviorAndInitializerList();
    TestAllUnitTablesAndUnitlessPolicy();
    TestQuantityPrivateParserBehaviorAndErrors();
    std::cout << "Yaml primitive coverage tests passed" << std::endl;
    return 0;
}
