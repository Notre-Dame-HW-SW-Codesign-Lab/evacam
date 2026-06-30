#include "config/EvaCamConfig.h"
#include "config/EvaCamYamlLoader.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char *kCellPath = "tests/tmp_input_validation_cell_config.yaml";
const char *kConfigPath = "tests/tmp_input_validation_system_config.yaml";
const char *kCustomSaPath = "tests/tmp_input_validation_custom_sa.yaml";

void WriteMinimalCellFile(const std::string &cellType = "SRAM",
        const std::string &camType = "TCAM",
        const std::string &columnType = "matchline",
        const std::string &columnRegion = "drain",
        const std::string &extraCellBlock = "",
        const std::string &additionalRowPorts = "") {
    std::ofstream out(kCellPath);
    out <<
        "cell:\n"
        "  name: TestCell\n"
        "  type: " << cellType << "\n"
        "  cam_type: " << camType << "\n"
        "  system_process_node: 45nm\n"
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
        << extraCellBlock <<
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
        << additionalRowPorts <<
        "  column:\n"
        "    0:\n"
        "      type: " << columnType << "\n"
        "      cmos_region: " << columnRegion << "\n"
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

void WriteInvalidCustomSenseAmpFile() {
    std::ofstream out(kCustomSaPath);
    out <<
        "custom_sense_amp:\n"
        "  height: 10F\n"
        "  width: 4F\n";
}

void WriteCellFileWithoutRowPorts() {
    std::ofstream out(kCellPath);
    out <<
        "cell:\n"
        "  name: TestCell\n"
        "  type: SRAM\n"
        "  cam_type: TCAM\n"
        "  system_process_node: 45nm\n"
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

void WriteCellFileWithoutColumnPorts() {
    std::ofstream out(kCellPath);
    out <<
        "cell:\n"
        "  name: TestCell\n"
        "  type: SRAM\n"
        "  cam_type: TCAM\n"
        "  system_process_node: 45nm\n"
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
        "        search1: 1V\n";
}

void WriteConfig(const std::string &memoryBlock,
        const std::string &optimizationBlock,
        const std::string &organizationBlock = "",
        const std::string &extraBlock = "",
        const std::string &advancedBlock = "",
        const std::string &designTarget = "CAM",
        const std::string &routingType = "H-tree") {
    std::ofstream out(kConfigPath);
    out <<
        "design:\n"
        "  target: " << designTarget << "\n"
        "  search_function: EX\n"
        "  system_process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_input_validation_cell_config.yaml\n"
        << memoryBlock <<
        "routing:\n"
        "  type: " << routingType << "\n"
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
    if (!advancedBlock.empty()) {
        out << advancedBlock;
    }
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

void TestNonCamDesignTargetsThrow() {
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n",
        "", "", "", "cache");
    assert(LoadThrowsWithMessage("Invalid value for 'target': cache"));

    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n",
        "", "", "", "RAM");
    assert(LoadThrowsWithMessage("Invalid value for 'target': RAM"));
}

void TestNonHTreeRoutingThrows() {
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n",
        "", "", "", "CAM", "non_h_tree");

    assert(LoadThrowsWithMessage("non H-tree is under development"));
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

void TestUnsupportedCamMemCellTypeThrows() {
    WriteMinimalCellFile("FBRAM");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("memory.cell.type is not supported for CAM modeling"));
    WriteMinimalCellFile();
}

void TestExternalSensingRejectsNonSramCam() {
    WriteMinimalCellFile("MRAM");
    {
        std::ofstream out(kConfigPath);
        out <<
            "design:\n"
            "  target: CAM\n"
            "  search_function: EX\n"
            "  system_process_node: 45nm\n"
            "  device_roadmap: HP\n"
            "  temperature: 300K\n"
            "memory:\n"
            "  cell_file: tests/tmp_input_validation_cell_config.yaml\n"
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
            "  internal: false\n"
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

    assert(LoadThrowsWithMessage("external sensing is only supported for SRAM CAM cells"));
    WriteMinimalCellFile();
}

void TestMissingCamRowPortsThrows() {
    WriteCellFileWithoutRowPorts();
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("cell.ports.row must define at least one CAM row port"));
    WriteMinimalCellFile();
}

void TestMissingCamColumnPortsThrows() {
    WriteCellFileWithoutColumnPorts();
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("cell.ports.column must define at least one CAM column port"));
    WriteMinimalCellFile();
}

void TestAcamUnsupportedThrows() {
    WriteMinimalCellFile("SRAM", "ACAM");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("ACAM is not supported"));
    WriteMinimalCellFile();
}

void TestMcamRequiresFefetramThrows() {
    WriteMinimalCellFile("SRAM", "MCAM");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("only 2FeFET MCAM design has limited support"));
    WriteMinimalCellFile();
}

void TestMcamRequiresResistanceStatesThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("mcam"));
    WriteMinimalCellFile();
}

void TestMcamResistanceStateCountMismatchThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 4\n"
        "  resistance_state: [1Mohm, 500Kohm]\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("must define every configured resistance state"));
    WriteMinimalCellFile();
}

void TestMcamRequiresAtLeastTwoResistanceStatesThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 1\n"
        "  resistance_state: [1Mohm]\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("mcam.num_resistance_state must be between 2 and 64"));
    WriteMinimalCellFile();
}

void TestMcamResistanceStatesMustBePositiveThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 0ohm]\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("mcam.resistance_state values must be positive"));
    WriteMinimalCellFile();
}

void TestMcamMlPrechargeVoltageCountMismatchThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 3\n"
        "  resistance_state: [1Mohm, 500Kohm, 250Kohm]\n"
        "  ml_precharge_voltage: [1V, 900mV]\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("mcam.ml_precharge_voltage must define every configured resistance state"));
    WriteMinimalCellFile();
}

void TestMcamMlPrechargeVoltagesMustBeNonNegativeThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  ml_precharge_voltage: [1V, -1mV]\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("mcam.ml_precharge_voltage values must be non-negative"));
    WriteMinimalCellFile();
}

void TestMcamSearchlineVoltageCountMismatchThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 3\n"
        "  resistance_state: [1Mohm, 500Kohm, 250Kohm]\n"
        "  searchline_voltage: [1V, 900mV]\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("mcam.searchline_voltage must define every configured resistance state"));
    WriteMinimalCellFile();
}

void TestMcamSearchlineVoltagesMustBeNonNegativeThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  searchline_voltage: [1V, -1mV]\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("mcam.searchline_voltage values must be non-negative"));
    WriteMinimalCellFile();
}

void TestMcamCenterVoltageMustBeNonNegativeThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  center_voltage: -1mV\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("mcam.center_voltage must be non-negative"));
    WriteMinimalCellFile();
}

void TestMcamSearchlineVoltageMappingRequiresBothInputs() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  searchline_voltage: [1V, 900mV]\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage(
            "mcam.searchline_voltage and mcam.center_voltage must be provided together"));
    WriteMinimalCellFile();
}

void TestMcamSearchlineVoltageMappingRequiresTwoSearchlines() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  searchline_voltage: [1V, 900mV]\n"
        "  center_voltage: 600mV\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("requires exactly two searchline row ports"));
    WriteMinimalCellFile();
}

void TestMcamComplementarySearchlineVoltageMustBeNonNegative() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  searchline_voltage: [1V, 900mV]\n"
        "  center_voltage: 400mV\n",
        "    1:\n"
        "      type: searchline\n"
        "      cmos_region: gate\n"
        "      num_cmos: 1\n"
        "      cmos_width: 1F\n"
        "      is_nmos: true\n"
        "      wire_width: 1F\n");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage(
            "MCAM derived complementary searchline voltage must be non-negative"));
    WriteMinimalCellFile();
}

void TestMatchlineGateConnectionThrows() {
    WriteMinimalCellFile("SRAM", "TCAM", "matchline", "gate");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("matchline connection cannot use cmos_region gate"));
    WriteMinimalCellFile();
}

void TestBitlineOnlyColumnRequiresMatchline() {
    WriteMinimalCellFile("SRAM", "TCAM", "Bitline", "drain");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("must define at least one CAM matchline port"));
    WriteMinimalCellFile();
}

void TestMissingMatchlineColumnThrows() {
    WriteMinimalCellFile("SRAM", "TCAM", "dataline", "drain");
    WriteConfig(
        "  capacity: 1KB\n"
        "  word_width: 64bits\n",
        "  target: ReadLatency\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n");

    assert(LoadThrowsWithMessage("must define at least one CAM matchline port"));
    WriteMinimalCellFile();
}

void TestUnsupportedSenseAmpTypeThrows() {
    {
        std::ofstream out(kConfigPath);
        out <<
            "design:\n"
            "  target: CAM\n"
            "  search_function: EX\n"
            "  system_process_node: 45nm\n"
            "  device_roadmap: HP\n"
            "  temperature: 300K\n"
            "memory:\n"
            "  cell_file: tests/tmp_input_validation_cell_config.yaml\n"
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
            "  amplifier_type: self_clock\n"
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

    assert(LoadThrowsWithMessage("sensing.amplifier_type is not supported"));
}

void TestMissingCustomSenseAmpFileThrows() {
    {
        std::ofstream out(kConfigPath);
        out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  system_process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_input_validation_cell_config.yaml\n"
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
        "  custom_sense_amp: true\n"
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
        "    low_swing: false\n"
            "advanced:\n"
            "  custom_sa_input_file: tests/does_not_exist_sa.yaml\n";
    }

    assert(LoadThrowsWithMessage("custom sense amp file cannot be found"));
}

void TestInvalidCustomSenseAmpFileThrows() {
    WriteInvalidCustomSenseAmpFile();
    {
        std::ofstream out(kConfigPath);
        out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  system_process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_input_validation_cell_config.yaml\n"
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
        "  custom_sense_amp: true\n"
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
        "    low_swing: false\n"
            "advanced:\n"
            "  custom_sa_input_file: tests/tmp_input_validation_custom_sa.yaml\n";
    }

    assert(LoadThrowsWithMessage("custom sense amp file is missing required fields"));
}

void TestCustomInputEncoderUnsupportedThrows() {
    {
        std::ofstream out(kConfigPath);
        out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  system_process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_input_validation_cell_config.yaml\n"
        "  capacity: 1KB\n"
        "  word_width: 64bits\n"
        "routing:\n"
        "  type: H-tree\n"
        "peripherals:\n"
        "  write_driver: false\n"
        "  input:\n"
        "    buffer: false\n"
        "    encoder: true\n"
        "    custom_encoder: true\n"
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

    assert(LoadThrowsWithMessage("custom input encoder is not supported"));
}

void TestLocalLowSwingWithRepeaterThrows() {
    {
        std::ofstream out(kConfigPath);
        out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  system_process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_input_validation_cell_config.yaml\n"
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
        "    low_swing: true\n"
            "  global:\n"
            "    type: GlobalAggressive\n"
            "    repeater: RepeatedOpt\n"
            "    low_swing: false\n";
    }

    assert(LoadThrowsWithMessage("wires.local.low_swing is not supported with repeaters"));
}

void TestGlobalLowSwingWithRepeaterThrows() {
    {
        std::ofstream out(kConfigPath);
        out <<
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  system_process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        "  cell_file: tests/tmp_input_validation_cell_config.yaml\n"
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
            "    low_swing: true\n";
    }

    assert(LoadThrowsWithMessage("wires.global.low_swing is not supported with repeaters"));
}

}  // namespace

int main() {
    WriteMinimalCellFile();
    TestNonCamDesignTargetsThrow();
    TestNonHTreeRoutingThrows();
    TestMissingRequiredTopLevelKeyThrows();
    TestFixedSubarrayDimensionsDeriveCapacity();
    TestAutoCapacityWithoutFixedSubarrayDimensionsThrows();
    TestFixedSubarrayDimensionsRejectExploration();
    TestNonPowerOfTwoWordWidthRequiresRealCapacity();
    TestNonPowerOfTwoWordWidthParsesWithRealCapacity();
    TestUnsupportedCamMemCellTypeThrows();
    TestExternalSensingRejectsNonSramCam();
    TestMissingCamRowPortsThrows();
    TestMissingCamColumnPortsThrows();
    TestAcamUnsupportedThrows();
    TestMcamRequiresFefetramThrows();
    TestMcamRequiresResistanceStatesThrows();
    TestMcamResistanceStateCountMismatchThrows();
    TestMcamRequiresAtLeastTwoResistanceStatesThrows();
    TestMcamResistanceStatesMustBePositiveThrows();
    TestMcamMlPrechargeVoltageCountMismatchThrows();
    TestMcamMlPrechargeVoltagesMustBeNonNegativeThrows();
    TestMcamSearchlineVoltageCountMismatchThrows();
    TestMcamSearchlineVoltagesMustBeNonNegativeThrows();
    TestMcamCenterVoltageMustBeNonNegativeThrows();
    TestMcamSearchlineVoltageMappingRequiresBothInputs();
    TestMcamSearchlineVoltageMappingRequiresTwoSearchlines();
    TestMcamComplementarySearchlineVoltageMustBeNonNegative();
    TestMatchlineGateConnectionThrows();
    TestBitlineOnlyColumnRequiresMatchline();
    TestMissingMatchlineColumnThrows();
    TestUnsupportedSenseAmpTypeThrows();
    TestMissingCustomSenseAmpFileThrows();
    TestInvalidCustomSenseAmpFileThrows();
    TestCustomInputEncoderUnsupportedThrows();
    TestLocalLowSwingWithRepeaterThrows();
    TestGlobalLowSwingWithRepeaterThrows();
    std::cout << "InputValidation tests passed" << std::endl;
    return 0;
}
