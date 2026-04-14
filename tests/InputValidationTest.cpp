#include "config/EvaCamConfig.h"
#include "config/EvaCamYamlLoader.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char *kCellPath = "tests/tmp_input_validation_cell.yaml";
const char *kConfigPath = "tests/tmp_input_validation_config.yaml";

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

void WriteConfig(const std::string &memoryBlock,
        const std::string &optimizationBlock,
        const std::string &organizationBlock = "",
        const std::string &extraBlock = "") {
    std::ofstream out(kConfigPath);
    out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_input_validation_cell.yaml\n"
        << memoryBlock <<
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
        << optimizationBlock <<
        "wires:\n"
        "  local:\n"
        "    type: LocalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: false\n"
        "  global:\n"
        "    type: GlobalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: false\n";
    if (!organizationBlock.empty()) {
        out << organizationBlock;
    }
    if (!extraBlock.empty()) {
        out << extraBlock;
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
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestMissingRequiredTopLevelKeyThrows() {
    WriteConfig(
        "  capacity: 1KB\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrows());
}

void TestFixedSubarrayDimensionsDeriveCapacity() {
    WriteConfig(
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n",
        "organization:\n"
        "  banks:\n"
        "    total: [1, 1]\n"
        "    active: [1, 1]\n"
        "  mats:\n"
        "    total: [1, 1]\n"
        "    active: [1, 1]\n"
        "  subarray:\n"
        "    dimensions: [64, 64]\n"
        "  mux:\n"
        "    sense_amp: 1\n"
        "    output_level1: 1\n"
        "    output_level2: 1\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);
    assert(config.input.capacity == 512);
    assert(config.runtimeSizing.hasFixedSubarrayDimensions == true);
}

void TestAutoCapacityWithoutFixedSubarrayDimensionsThrows() {
    WriteConfig(
        "  capacity: auto\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("memory.capacity is required unless organization.subarray.dimensions is supplied"));
}

void TestFixedSubarrayDimensionsRejectExploration() {
    WriteConfig(
        "  word_width: 64bits\n",
        "  target: Exploration\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n",
        "organization:\n"
        "  banks:\n"
        "    total: [1, 1]\n"
        "    active: [1, 1]\n"
        "  mats:\n"
        "    total: [1, 1]\n"
        "    active: [1, 1]\n"
        "  subarray:\n"
        "    dimensions: [64, 64]\n"
        "  mux:\n"
        "    sense_amp: 1\n"
        "    output_level1: 1\n"
        "    output_level2: 1\n");

    assert(LoadThrowsWithMessage("organization.subarray.dimensions is only supported for fixed non-DSE configs"));
}

void TestNonPowerOfTwoWordWidthRequiresRealCapacity() {
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 96bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("non-power-of-two word_width requires extra.real_capacity"));
}

void TestNonPowerOfTwoWordWidthParsesWithRealCapacity() {
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 96bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n",
        "",
        "extra:\n"
        "  real_capacity: 1152B\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);
    assert(config.runtimeSizing.realCapacity == 1152);
}

}  // namespace

int main() {
    WriteMinimalCellFile();
    TestMissingRequiredTopLevelKeyThrows();
    TestFixedSubarrayDimensionsDeriveCapacity();
    TestAutoCapacityWithoutFixedSubarrayDimensionsThrows();
    TestFixedSubarrayDimensionsRejectExploration();
    TestNonPowerOfTwoWordWidthRequiresRealCapacity();
    TestNonPowerOfTwoWordWidthParsesWithRealCapacity();
    std::cout << "InputValidation tests passed" << std::endl;
    return 0;
}
