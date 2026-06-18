#ifndef INPUT_YAMLUNITPARSERS_H_
#define INPUT_YAMLUNITPARSERS_H_

#include <yaml.h>

#include <vector>

namespace YamlHelpers {

struct UnitSpec {
    const char* suffix;
    double to_base;
};

double parse_quantity_node(
        const YAML::Node& node,
        const std::vector<UnitSpec>& units,
        double default_unit_to_base,
        const char* what);

double read_quantity_required(
        const YAML::Node& parent,
        const char* key,
        const std::vector<UnitSpec>& units,
        double default_unit_to_base,
        const char* what);

const std::vector<UnitSpec>& VoltageUnits();
const std::vector<UnitSpec>& McamCenterVoltageUnits();
const std::vector<UnitSpec>& CurrentUnits();
const std::vector<UnitSpec>& TimeUnits();
const std::vector<UnitSpec>& CapacitanceUnits();
const std::vector<UnitSpec>& ResistanceUnits();
const std::vector<UnitSpec>& PowerUnits();
const std::vector<UnitSpec>& EnergyUnits();
const std::vector<UnitSpec>& TemperatureUnits();
const std::vector<UnitSpec>& DataSizeUnits();
const std::vector<UnitSpec>& BitUnits();
const std::vector<UnitSpec>& LengthUnits();
const std::vector<UnitSpec>& FeatureUnits();
const std::vector<UnitSpec>& FeatureAreaUnits();

}  // namespace YamlHelpers

#endif  // INPUT_YAMLUNITPARSERS_H_
