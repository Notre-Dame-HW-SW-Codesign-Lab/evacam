#include "config/EvaCamConfig.h"
#include "config/EvaCamYamlLoader.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char *kCellPath = "tests/tmp_top_level_cell.yaml";
const char *kConfigPath = "tests/tmp_top_level_config.yaml";

void WriteMinimalCellFile() {
    std::ofstream out(kCellPath);
    out <<
        "cell:\n"
        "  name: TestCell\n"
        "  type: SRAM\n"
        "  cam_type: TCAM\n"
        "  process_node: 45nm\n"
        "  area: 300F^2\n"
        "  aspect_ratio: 2.0\n"
        "access_device:\n"
        "  type: CMOS\n"
        "  cmos_width: 2F\n"
        "resistance:\n"
        "  'on': 1kohm\n"
        "  'off': 1Mohm\n"
        "read:\n"
        "  mode: voltage\n"
        "  voltage: 1V\n"
        "  current: 5uA\n"
        "  power: 2uW\n"
        "  energy: 10fJ\n"
        "  min_sense_voltage: 70mV\n"
        "write:\n"
        "  set:\n"
        "    mode: voltage\n"
        "    voltage: 4V\n"
        "    current: 1uA\n"
        "    pulse: 10ns\n"
        "    energy: 2pJ\n"
        "  reset:\n"
        "    mode: current\n"
        "    voltage: 3V\n"
        "    current: 2uA\n"
        "    pulse: 20ns\n"
        "    energy: 3pJ\n"
        "match:\n"
        "  cmos_width: 3F\n"
        "ports:\n"
        "  row:\n"
        "    0:\n"
        "      type: searchline\n"
        "      cmos_region: gate\n"
        "      num_cmos: 1\n"
        "      cmos_width: 1F\n"
        "      is_nmos: true\n"
        "      wire_width: 1F\n"
        "      voltages:\n"
        "        set_lrs: 4V\n"
        "        set_mrs: 4V\n"
        "        reset: 4V\n"
        "        search0: 1V\n"
        "        search1: 1V\n"
        "  column:\n"
        "    0:\n"
        "      type: matchline\n"
        "      cmos_region: drain\n"
        "      num_cmos: 1\n"
        "      cmos_width: 1F\n"
        "      is_nmos: true\n"
        "      wire_width: 1F\n"
        "      voltages:\n"
        "        set_lrs: 1V\n"
        "        set_mrs: 1V\n"
        "        reset: 1V\n"
        "        search0: 0V\n"
        "        search1: 1V\n";
}

void WriteBaseTopLevelConfig(const std::string &extra = "") {
    std::ofstream out(kConfigPath);
    out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_top_level_cell.yaml\n"
        "  capacity: 1KB\n"
        "  word_width: 64bits\n"
        "routing:\n"
        "  type: H-tree\n"
        "peripherals:\n"
        "  write_driver: false\n"
        "  input:\n"
        "    buffer: false\n"
        "    encoder: false\n"
        "    custom_encoder: false\n"
        "  output:\n"
        "    buffer: false\n"
        "    priority_encoder: false\n"
        "    accumulator: false\n"
        "sensing:\n"
        "  internal: true\n"
        "  custom_sense_amp: false\n"
        "  amplifier_type: nvsim_vol\n"
        "optimization:\n"
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n"
        "wires:\n"
        "  local:\n"
        "    type: LocalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: false\n"
        "  global:\n"
        "    type: GlobalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: false\n";
    if (!extra.empty()) {
        out << extra;
    }
}

void WriteExplicitSubarrayConfig(const std::string &memoryCapacity,
        const std::string &wordWidth,
        const std::string &optimizationTarget,
        const std::string &organizationExtra = "",
        const std::string &optimizationExtra = "",
        const std::string &extraSection = "") {
    std::ofstream out(kConfigPath);
    out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_top_level_cell.yaml\n";
    if (!memoryCapacity.empty()) {
        out << "  capacity: " << memoryCapacity << "\n";
    }
    out <<
        "  word_width: " << wordWidth << "\n"
        "routing:\n"
        "  type: H-tree\n"
        "peripherals:\n"
        "  write_driver: false\n"
        "  input:\n"
        "    buffer: false\n"
        "    encoder: false\n"
        "    custom_encoder: false\n"
        "  output:\n"
        "    buffer: false\n"
        "    priority_encoder: false\n"
        "    accumulator: false\n"
        "sensing:\n"
        "  internal: true\n"
        "  custom_sense_amp: false\n"
        "  amplifier_type: nvsim_vol\n"
        "optimization:\n"
        "  target: " << optimizationTarget << "\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n";
    if (!optimizationExtra.empty()) {
        out << optimizationExtra;
    }
    out <<
        "wires:\n"
        "  local:\n"
        "    type: LocalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: false\n"
        "  global:\n"
        "    type: GlobalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: false\n"
        "organization:\n";
    if (!organizationExtra.empty()) {
        out << organizationExtra;
    } else {
        out <<
            "  banks:\n"
            "    total: [1, 1]\n"
            "    active: [1, 1]\n"
            "  mats:\n"
            "    total: [1, 1]\n"
            "    active: [1, 1]\n";
    }
    out <<
        "  subarray:\n"
        "    dimensions: [64, 64]\n"
        "  mux:\n"
        "    sense_amp: 1\n"
        "    output_level1: 1\n"
        "    output_level2: 1\n";
    if (!extraSection.empty()) {
        out << extraSection;
    }
}

bool LoadThrowsWithMessage(const std::string &expectedSubstring) {
    try {
        EvaCamConfig config;
        EvaCamYamlLoader::Load(kConfigPath, config);
    } catch (const std::runtime_error &error) {
        return std::string(error.what()).find(expectedSubstring) != std::string::npos;
    }
    return false;
}

bool LoadThrows() {
    try {
        EvaCamConfig config;
        EvaCamYamlLoader::Load(kConfigPath, config);
    } catch (const std::runtime_error &) {
        return true;
    }
    return false;
}

void TestMinimalTopLevelConfigParses() {
    WriteBaseTopLevelConfig();

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);

    assert(config.input.processNode == 45);
    assert(config.input.capacity == 1024);
    assert(config.input.wordWidth == 64);
    assert(config.input.routingMode == h_tree);
    assert(config.peripherals.withWriteDriver == false);
    assert(config.peripherals.typeSenseAmp == nvsim_voltage_sense);
}

void TestOptionalSectionsCanBeOmitted() {
    WriteBaseTopLevelConfig();

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);

    assert(config.peripherals.addCapOnML == 0.0);
    assert(config.constraints.enabled == false);
    assert(config.runtimeSizing.realCapacity == 0);
}

void TestMissingRequiredTopLevelSectionThrows() {
    std::ofstream out(kConfigPath);
    out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n";

    assert(LoadThrows());
}

void TestInvalidUnitThrows() {
    WriteBaseTopLevelConfig(
        "matchline:\n"
        "  additional_cap: 1XB\n");

    assert(LoadThrowsWithMessage("Unknown unit"));
}

void TestInvalidEnumThrows() {
    WriteBaseTopLevelConfig(
        "cache:\n"
        "  access_mode: invalid_mode\n");

    assert(LoadThrowsWithMessage("Invalid value for 'access_mode'"));
}

void TestOrganizationAliasParses() {
    WriteBaseTopLevelConfig(
        "array:\n"
        "  banks:\n"
        "    total: [2, 4]\n"
        "    active: [1, 2]\n"
        "  mats:\n"
        "    total: [4, 8]\n"
        "    active: [2, 4]\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);

    assert(config.exploration.geometry.numRowMat.Min() == 2);
    assert(config.exploration.geometry.numColumnMat.Min() == 4);
    assert(config.exploration.geometry.numRowSubarray.Min() == 4);
    assert(config.exploration.geometry.numColumnSubarray.Min() == 8);
}

void TestCactiAssumptionNormalizesGeometry() {
    WriteBaseTopLevelConfig(
        "advanced:\n"
        "  use_cacti_assumption: true\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);

    assert(config.useCactiAssumption == true);
    assert(config.exploration.useCactiAssumption == true);
    assert(config.exploration.geometry.numActiveMatPerRow.Min()
            == config.exploration.geometry.numColumnMat.Max());
    assert(config.exploration.geometry.numActiveMatPerColumn.Min() == 1);
    assert(config.exploration.geometry.numRowSubarray.Min() == 2);
    assert(config.exploration.geometry.numColumnSubarray.Min() == 2);
}

void TestAutoCapacityRequiresFixedSubarrayDimensions() {
    {
        std::ofstream out(kConfigPath);
        out <<
            "design:\n"
            "  target: CAM\n"
            "  search_function: EX\n"
            "  process_node: 45nm\n"
            "  device_roadmap: HP\n"
            "  temperature: 300K\n"
            "memory:\n"
            "  cell_file: tests/tmp_top_level_cell.yaml\n"
            "  capacity: auto\n"
            "  word_width: 64bits\n"
            "routing:\n"
            "  type: H-tree\n"
            "peripherals:\n"
            "  write_driver: false\n"
            "  input:\n"
            "    buffer: false\n"
            "    encoder: false\n"
            "    custom_encoder: false\n"
            "  output:\n"
            "    buffer: false\n"
            "    priority_encoder: false\n"
            "    accumulator: false\n"
            "sensing:\n"
            "  internal: true\n"
            "  custom_sense_amp: false\n"
            "  amplifier_type: nvsim_vol\n"
            "optimization:\n"
            "  target: ReadLatency\n"
            "  buffer_design: latency\n"
            "  row_driver: latency\n"
            "  priority_encoder: latency\n"
            "wires:\n"
            "  local:\n"
            "    type: LocalAggressive\n"
            "    repeater: RepeatedOpt\n"
            "    low_swing: false\n"
            "  global:\n"
            "    type: GlobalAggressive\n"
            "    repeater: RepeatedOpt\n"
            "    low_swing: false\n";
    }

    assert(LoadThrowsWithMessage("memory.capacity is required unless organization.subarray.dimensions is supplied"));
}

void TestNonPowerOfTwoWordWidthRequiresRealCapacity() {
    {
        std::ofstream out(kConfigPath);
        out <<
            "design:\n"
            "  target: CAM\n"
            "  search_function: EX\n"
            "  process_node: 45nm\n"
            "  device_roadmap: HP\n"
            "  temperature: 300K\n"
            "memory:\n"
            "  cell_file: tests/tmp_top_level_cell.yaml\n"
            "  capacity: 1KB\n"
            "  word_width: 96bits\n"
            "routing:\n"
            "  type: H-tree\n"
            "peripherals:\n"
            "  write_driver: false\n"
            "  input:\n"
            "    buffer: false\n"
            "    encoder: false\n"
            "    custom_encoder: false\n"
            "  output:\n"
            "    buffer: false\n"
            "    priority_encoder: false\n"
            "    accumulator: false\n"
            "sensing:\n"
            "  internal: true\n"
            "  custom_sense_amp: false\n"
            "  amplifier_type: nvsim_vol\n"
            "optimization:\n"
            "  target: ReadLatency\n"
            "  buffer_design: latency\n"
            "  row_driver: latency\n"
            "  priority_encoder: latency\n"
            "wires:\n"
            "  local:\n"
            "    type: LocalAggressive\n"
            "    repeater: RepeatedOpt\n"
            "    low_swing: false\n"
            "  global:\n"
            "    type: GlobalAggressive\n"
            "    repeater: RepeatedOpt\n"
            "    low_swing: false\n";
    }

    assert(LoadThrowsWithMessage("non-power-of-two word_width requires extra.real_capacity"));
}

void TestFixedSubarrayDimensionsRejectExplorationTarget() {
    WriteExplicitSubarrayConfig("", "64bits", "Exploration");
    assert(LoadThrowsWithMessage("organization.subarray.dimensions is only supported for fixed non-DSE configs"));
}

void TestFixedSubarrayDimensionsRejectDeepExploration() {
    WriteExplicitSubarrayConfig("", "64bits", "ReadLatency", "", "  deep_exploration: true\n");
    assert(LoadThrowsWithMessage("organization.subarray.dimensions is only supported for fixed non-DSE configs"));
}

}  // namespace

int main() {
    WriteMinimalCellFile();
    TestMinimalTopLevelConfigParses();
    TestOptionalSectionsCanBeOmitted();
    TestMissingRequiredTopLevelSectionThrows();
    TestInvalidUnitThrows();
    TestInvalidEnumThrows();
    TestOrganizationAliasParses();
    TestCactiAssumptionNormalizesGeometry();
    TestAutoCapacityRequiresFixedSubarrayDimensions();
    TestNonPowerOfTwoWordWidthRequiresRealCapacity();
    TestFixedSubarrayDimensionsRejectExplorationTarget();
    TestFixedSubarrayDimensionsRejectDeepExploration();
    std::cout << "TopLevelConfigParser tests passed" << std::endl;
    return 0;
}
