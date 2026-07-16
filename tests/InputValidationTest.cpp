#include "config/EvaCamConfig.h"
#include "config/EvaCamYamlLoader.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char *kCellPath = "tests/tmp_input_validation.cell.yaml";
const char *kConfigPath = "tests/tmp_input_validation.config.yaml";
const char *kArchitecturePath = "tests/tmp_input_validation.architecture.yaml";
const char *kMemoryDevicePath = "tests/tmp_input_validation.memory_device.yaml";
const char *kSensingPath = "tests/tmp_input_validation.sensing.yaml";
const char *kCustomSaPath = "tests/tmp_input_validation_custom_sa.yaml";

void WriteMemoryDeviceFile(const std::string &cellType = "SRAM",
        const std::string &extraDeviceBlock = "") {
    std::ofstream out(kMemoryDevicePath);
    out <<
        "schema: memory_device\n"
        "name: TestDevice\n"
        "type: " << cellType << "\n"
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
        "    energy: 3pJ\n";
    out << extraDeviceBlock;
}

std::string RowPort(int index, const std::string &type = "searchline",
        const std::string &terminal = "gate") {
    return
        "    " + std::to_string(index) + ":\n"
        "      type: " + type + "\n"
        "      connection:\n"
        "        kind: memory_terminal\n"
        "        terminal: " + terminal + "\n"
        "      wire_width: 1F\n"
        "      voltages:\n"
        "        set_lrs: 4V\n"
        "        set_mrs: 4V\n"
        "        reset: 4V\n"
        "        search0: 1V\n"
        "        search1: 1V\n";
}

std::string ColumnPort(const std::string &type = "matchline",
        const std::string &terminal = "drain") {
    return
        "    0:\n"
        "      type: " + type + "\n"
        "      connection:\n"
        "        kind: memory_terminal\n"
        "        terminal: " + terminal + "\n"
        "      wire_width: 1F\n"
        "      voltages:\n"
        "        set_lrs: 1V\n"
        "        set_mrs: 1V\n"
        "        reset: 1V\n"
        "        search0: 0V\n"
        "        search1: 1V\n";
}

void WriteMinimalCellFile(const std::string &cellType = "SRAM",
        const std::string &camType = "TCAM",
        const std::string &columnType = "matchline",
        const std::string &columnTerminal = "drain",
        const std::string &extraDeviceBlock = "",
        const std::string &additionalRowPorts = "") {
    WriteMemoryDeviceFile(cellType, extraDeviceBlock);

    std::ofstream out(kCellPath);
    out <<
        "schema: cell\n"
        "name: TestCell\n"
        "cam_type: " << camType << "\n"
        "memory_device: ./tmp_input_validation.memory_device.yaml\n"
        "layout:\n"
        "  cell_process_node: 45nm\n"
        "  area: 300F^2\n"
        "  aspect_ratio: 2.0\n"
        "ports:\n"
        "  row:\n"
        << RowPort(0)
        << additionalRowPorts <<
        "  column:\n"
        << ColumnPort(columnType, columnTerminal);
}

void WriteCellFileWithoutRowPorts() {
    WriteMemoryDeviceFile();
    std::ofstream out(kCellPath);
    out <<
        "schema: cell\n"
        "name: TestCell\n"
        "cam_type: TCAM\n"
        "memory_device: ./tmp_input_validation.memory_device.yaml\n"
        "layout:\n"
        "  cell_process_node: 45nm\n"
        "  area: 300F^2\n"
        "  aspect_ratio: 2.0\n"
        "ports:\n"
        "  column:\n"
        << ColumnPort();
}

void WriteCellFileWithoutColumnPorts() {
    WriteMemoryDeviceFile();
    std::ofstream out(kCellPath);
    out <<
        "schema: cell\n"
        "name: TestCell\n"
        "cam_type: TCAM\n"
        "memory_device: ./tmp_input_validation.memory_device.yaml\n"
        "layout:\n"
        "  cell_process_node: 45nm\n"
        "  area: 300F^2\n"
        "  aspect_ratio: 2.0\n"
        "ports:\n"
        "  row:\n"
        << RowPort(0);
}

void WriteInvalidCustomSenseAmpFile() {
    std::ofstream out(kCustomSaPath);
    out <<
        "schema: sense_amp\n"
        "name: invalid_scalar\n"
        "model: scalar\n"
        "geometry:\n"
        "  height: 10F\n"
        "  width: 4F\n";
}

void WriteSensingFile(const std::string &body =
        "schema: sensing\n"
        "internal: true\n"
        "sense_amplifier: ../config/lib/sense_amp/nvsim_vol.sense_amp.yaml\n") {
    std::ofstream out(kSensingPath);
    out << body;
}

void WriteArchitecture(const std::string &memoryBlock,
        const std::string &organizationBlock = "",
        const std::string &designTarget = "CAM",
        const std::string &routingType = "H-tree",
        const std::string &peripheralInputExtra = "",
        const std::string &wireLocalLowSwing = "false",
        const std::string &wireGlobalLowSwing = "false") {
    WriteSensingFile();
    std::ofstream out(kArchitecturePath);
    out <<
        "schema: architecture\n"
        "design:\n"
        "  target: " << designTarget << "\n"
        "  search_function: EX\n"
        "  system_process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: 300K\n"
        "memory:\n"
        << memoryBlock <<
        "routing:\n"
        "  type: " << routingType << "\n"
        "peripherals:\n"
        "  write_driver: false\n"
        "  input:\n"
        "    buffer: false\n";
    if (peripheralInputExtra.empty()) {
        out <<
            "    encoder: false\n"
            "    custom_encoder: false\n";
    } else {
        out << peripheralInputExtra;
    }
    out <<
        "  output:\n"
        "    buffer: false\n"
        "    priority_encoder: false\n"
        "    accumulator: false\n"
        "wires:\n"
        "  local:\n"
        "    type: LocalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: " << wireLocalLowSwing << "\n"
        "  global:\n"
        "    type: GlobalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: " << wireGlobalLowSwing << "\n"
        "sensing: ./tmp_input_validation.sensing.yaml\n";
    if (!organizationBlock.empty()) {
        out << organizationBlock;
    }
}

void WriteConfig(const std::string &memoryBlock,
        const std::string &optimizationBlock,
        const std::string &organizationBlock = "",
        const std::string &extraBlock = "",
        const std::string &advancedBlock = "",
        const std::string &designTarget = "CAM",
        const std::string &routingType = "H-tree") {
    WriteArchitecture(memoryBlock, organizationBlock, designTarget, routingType);
    std::ofstream out(kConfigPath);
    out <<
        "schema: config\n"
        "name: tmp_input_validation\n"
        "architecture: ./tmp_input_validation.architecture.yaml\n"
        "cell: ./tmp_input_validation.cell.yaml\n"
        "technology: ../config/lib/technology/cmos.legacy.yaml\n"
        "optimization:\n"
        << optimizationBlock
        << advancedBlock
        << extraBlock;
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

const char *kReadLatencyOptimization =
    "  target: ReadLatency\n"
    "  buffer_design: latency\n"
    "  row_driver: latency\n"
    "  priority_encoder: latency\n";

void TestNonCamDesignTargetsThrow() {
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n",
            kReadLatencyOptimization, "", "", "", "cache");
    assert(LoadThrowsWithMessage("Invalid value for 'target': cache"));

    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n",
            kReadLatencyOptimization, "", "", "", "RAM");
    assert(LoadThrowsWithMessage("Invalid value for 'target': RAM"));
}

void TestNonHTreeRoutingThrows() {
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n",
            kReadLatencyOptimization, "", "", "", "CAM", "non_h_tree");
    assert(LoadThrowsWithMessage("non H-tree is under development"));
}

void TestMissingRequiredTopLevelKeyThrows() {
    WriteConfig("  capacity: 1KB\n", kReadLatencyOptimization);
    assert(LoadThrows());
}

void TestFixedSubarrayDimensionsDeriveCapacity() {
    WriteConfig("  word_width: 64bits\n", kReadLatencyOptimization,
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

void TestFixedSubarrayDimensionsAllowEightRows() {
    WriteConfig("  word_width: 8bits\n", kReadLatencyOptimization,
        "organization:\n"
        "  banks:\n"
        "    total: [1, 1]\n"
        "    active: [1, 1]\n"
        "  mats:\n"
        "    total: [1, 1]\n"
        "    active: [1, 1]\n"
        "  subarray:\n"
        "    dimensions: [8, 64]\n"
        "  mux:\n"
        "    sense_amp: 1\n"
        "    output_level1: 1\n"
        "    output_level2: 1\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);
    assert(config.input.capacity == 64);
}

void TestMcamFixedSubarrayDimensionsAllowIndependentRows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500kohm]\n");
    WriteConfig("  word_width: 64bits\n", kReadLatencyOptimization,
        "organization:\n"
        "  banks:\n"
        "    total: [1, 1]\n"
        "    active: [1, 1]\n"
        "  mats:\n"
        "    total: [1, 1]\n"
        "    active: [1, 1]\n"
        "  subarray:\n"
        "    dimensions: [10, 256]\n"
        "  mux:\n"
        "    sense_amp: 1\n"
        "    output_level1: 1\n"
        "    output_level2: 1\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);
    assert(config.input.capacity == 320);
    WriteMinimalCellFile();
}

void TestAutoCapacityWithoutFixedSubarrayDimensionsThrows() {
    WriteConfig("  capacity: auto\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("memory.capacity is required unless organization.subarray.dimensions is supplied"));
}

void TestFixedSubarrayDimensionsRejectExploration() {
    WriteConfig("  word_width: 64bits\n",
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
    WriteConfig("  capacity: 1KB\n  word_width: 96bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("non-power-of-two word_width requires extra.real_capacity"));
}

void TestNonPowerOfTwoWordWidthParsesWithRealCapacity() {
    WriteArchitecture("  capacity: 1KB\n  physical_capacity: 1152B\n  word_width: 96bits\n");
    {
        std::ofstream out(kConfigPath);
        out <<
            "schema: config\n"
            "name: tmp_input_validation\n"
            "architecture: ./tmp_input_validation.architecture.yaml\n"
            "cell: ./tmp_input_validation.cell.yaml\n"
            "technology: ../config/lib/technology/cmos.legacy.yaml\n"
            "optimization:\n"
            << kReadLatencyOptimization;
    }

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);
    assert(config.runtimeSizing.realCapacity == 1152);
}

void TestUnsupportedCamMemCellTypeThrows() {
    WriteMinimalCellFile("FBRAM");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("memory.cell.type is not supported for CAM modeling"));
    WriteMinimalCellFile();
}

void TestExternalSensingRejectsNonSramCam() {
    WriteMinimalCellFile("MRAM");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    WriteSensingFile(
        "schema: sensing\n"
        "internal: false\n"
        "sense_amplifier: ../config/lib/sense_amp/nvsim_vol.sense_amp.yaml\n");
    assert(LoadThrowsWithMessage("external sensing is only supported for SRAM CAM cells"));
    WriteMinimalCellFile();
}

void TestMissingCamRowPortsThrows() {
    WriteCellFileWithoutRowPorts();
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("cell.ports.row must define at least one CAM row port"));
    WriteMinimalCellFile();
}

void TestMissingCamColumnPortsThrows() {
    WriteCellFileWithoutColumnPorts();
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("cell.ports.column must define at least one CAM column port"));
    WriteMinimalCellFile();
}

void TestAcamUnsupportedThrows() {
    WriteMinimalCellFile("SRAM", "ACAM");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("ACAM is not supported"));
    WriteMinimalCellFile();
}

void TestMcamRequiresFefetramThrows() {
    WriteMinimalCellFile("SRAM", "MCAM");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("only 2FeFET MCAM design has limited support"));
    WriteMinimalCellFile();
}

void TestMcamRequiresResistanceStatesThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("mcam"));
    WriteMinimalCellFile();
}

void TestMcamResistanceStateCountMismatchThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 4\n"
        "  resistance_state: [1Mohm, 500Kohm]\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("must define every configured resistance state"));
    WriteMinimalCellFile();
}

void TestMcamRequiresAtLeastTwoResistanceStatesThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 1\n"
        "  resistance_state: [1Mohm]\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("mcam.num_resistance_state must be between 2 and 64"));
    WriteMinimalCellFile();
}

void TestMcamResistanceStatesMustBePositiveThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 0ohm]\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("mcam.resistance_state values must be positive"));
    WriteMinimalCellFile();
}

void TestMcamMlPrechargeVoltageCountMismatchThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 3\n"
        "  resistance_state: [1Mohm, 500Kohm, 250Kohm]\n"
        "  ml_precharge_voltage: [1V, 900mV]\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("mcam.ml_precharge_voltage must define every configured resistance state"));
    WriteMinimalCellFile();
}

void TestMcamMlPrechargeVoltagesMustBeNonNegativeThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  ml_precharge_voltage: [1V, -1mV]\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("mcam.ml_precharge_voltage values must be non-negative"));
    WriteMinimalCellFile();
}

void TestMcamSearchlineVoltageCountMismatchThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 3\n"
        "  resistance_state: [1Mohm, 500Kohm, 250Kohm]\n"
        "  searchline_voltage: [1V, 900mV]\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("mcam.searchline_voltage must define every configured resistance state"));
    WriteMinimalCellFile();
}

void TestMcamSearchlineVoltagesMustBeNonNegativeThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  searchline_voltage: [1V, -1mV]\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("mcam.searchline_voltage values must be non-negative"));
    WriteMinimalCellFile();
}

void TestMcamCenterVoltageMustBeNonNegativeThrows() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  center_voltage: -1mV\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("mcam.center_voltage must be non-negative"));
    WriteMinimalCellFile();
}

void TestMcamSearchlineVoltageMappingRequiresBothInputs() {
    WriteMinimalCellFile("FEFETRAM", "MCAM", "matchline", "drain",
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500Kohm]\n"
        "  searchline_voltage: [1V, 900mV]\n");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
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
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
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
        RowPort(1));
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage(
            "MCAM derived complementary searchline voltage must be non-negative"));
    WriteMinimalCellFile();
}

void TestMatchlineGateConnectionThrows() {
    WriteMinimalCellFile("SRAM", "TCAM", "matchline", "gate");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("matchline connection cannot use cmos_region gate"));
    WriteMinimalCellFile();
}

void TestBitlineOnlyColumnRequiresMatchline() {
    WriteMinimalCellFile("SRAM", "TCAM", "Bitline", "drain");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("must define at least one CAM matchline port"));
    WriteMinimalCellFile();
}

void TestMissingMatchlineColumnThrows() {
    WriteMinimalCellFile("SRAM", "TCAM", "dataline", "drain");
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    assert(LoadThrowsWithMessage("must define at least one CAM matchline port"));
    WriteMinimalCellFile();
}

void TestUnsupportedSenseAmpTypeThrows() {
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization);
    WriteSensingFile(
        "schema: sensing\n"
        "internal: true\n"
        "custom_sense_amp: false\n"
        "sensing_mode: self_clock\n");
    assert(LoadThrowsWithMessage("sensing.sensing_mode is not supported"));
}

void TestMissingCustomSenseAmpFileThrows() {
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization,
            "", "", "advanced:\n  custom_sa_input_file: tests/does_not_exist_sa.yaml\n");
    WriteSensingFile(
        "schema: sensing\n"
        "internal: true\n"
        "custom_sense_amp: true\n"
        "sensing_mode: nvsim_vol\n");
    assert(LoadThrows());
}

void TestInvalidCustomSenseAmpFileThrows() {
    WriteInvalidCustomSenseAmpFile();
    WriteConfig("  capacity: 1KB\n  word_width: 64bits\n", kReadLatencyOptimization,
            "", "", "advanced:\n  custom_sa_input_file: tests/tmp_input_validation_custom_sa.yaml\n");
    WriteSensingFile(
        "schema: sensing\n"
        "internal: true\n"
        "custom_sense_amp: true\n"
        "sensing_mode: nvsim_vol\n");
    assert(LoadThrows());
}

void TestCustomInputEncoderUnsupportedThrows() {
    WriteArchitecture("  capacity: 1KB\n  word_width: 64bits\n", "", "CAM", "H-tree",
        "    encoder: true\n"
        "    custom_encoder: true\n");
    {
        std::ofstream out(kConfigPath);
        out <<
            "schema: config\n"
            "name: tmp_input_validation\n"
            "architecture: ./tmp_input_validation.architecture.yaml\n"
            "cell: ./tmp_input_validation.cell.yaml\n"
            "technology: ../config/lib/technology/cmos.legacy.yaml\n"
            "optimization:\n" << kReadLatencyOptimization;
    }
    assert(LoadThrowsWithMessage("custom input encoder is not supported"));
}

void TestLocalLowSwingWithRepeaterThrows() {
    WriteArchitecture("  capacity: 1KB\n  word_width: 64bits\n", "", "CAM", "H-tree",
            "", "true", "false");
    {
        std::ofstream out(kConfigPath);
        out <<
            "schema: config\n"
            "name: tmp_input_validation\n"
            "architecture: ./tmp_input_validation.architecture.yaml\n"
            "cell: ./tmp_input_validation.cell.yaml\n"
            "technology: ../config/lib/technology/cmos.legacy.yaml\n"
            "optimization:\n" << kReadLatencyOptimization;
    }
    assert(LoadThrowsWithMessage("wires.local.low_swing is not supported with repeaters"));
}

void TestGlobalLowSwingWithRepeaterThrows() {
    WriteArchitecture("  capacity: 1KB\n  word_width: 64bits\n", "", "CAM", "H-tree",
            "", "false", "true");
    {
        std::ofstream out(kConfigPath);
        out <<
            "schema: config\n"
            "name: tmp_input_validation\n"
            "architecture: ./tmp_input_validation.architecture.yaml\n"
            "cell: ./tmp_input_validation.cell.yaml\n"
            "technology: ../config/lib/technology/cmos.legacy.yaml\n"
            "optimization:\n" << kReadLatencyOptimization;
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
    TestFixedSubarrayDimensionsAllowEightRows();
    TestMcamFixedSubarrayDimensionsAllowIndependentRows();
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
