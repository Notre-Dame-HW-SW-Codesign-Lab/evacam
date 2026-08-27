#include "config/EvaCamConfig.h"
#include "config/EvaCamYamlLoader.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char *kCellPath = "tests/tmp_top_level_cell_config.yaml";
const char *kConfigPath = "tests/tmp_top_level.config.yaml";
const char *kArchitecturePath = "tests/tmp_top_level.architecture.yaml";
const char *kMemoryDevicePath = "tests/tmp_top_level.memory_device.yaml";
const char *kSensingPath = "tests/tmp_top_level.sensing.yaml";

void WriteMemoryDeviceFile(const std::string &type = "SRAM") {
    std::ofstream out(kMemoryDevicePath);
    out <<
        "schema: memory_device\n"
        "name: TestDevice\n"
        "type: " << type << "\n"
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
}

void WriteMinimalCellFile() {
    WriteMemoryDeviceFile();

    std::ofstream out(kCellPath);
    out <<
        "schema: cell\n"
        "name: TestCell\n"
        "cam_type: TCAM\n"
        "memory_device: ./tmp_top_level.memory_device.yaml\n"
        "layout:\n"
        "  cell_process_node: 45nm\n"
        "  area: 300F^2\n"
        "  aspect_ratio: 2.0\n"
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

void WriteSensingFile(const std::string &extra = "") {
    std::ofstream out(kSensingPath);
    out <<
        "schema: sensing\n"
        "internal: true\n"
        "sense_amplifier: ../config/lib/sense_amp/nvsim_vol.sense_amp.yaml\n";
    out << extra;
}

void WriteArchitectureFile(const std::string &memoryCapacity = "1KB",
        const std::string &wordWidth = "64bits",
        const std::string &organization = "",
        const std::string &extra = "",
        const std::string &physicalCapacity = "",
        const std::string &temperature = "300K") {
    WriteSensingFile();

    std::ofstream out(kArchitecturePath);
    out <<
        "schema: architecture\n"
        "design:\n"
        "  target: CAM\n"
        "  search_function: EX\n"
        "  system_process_node: 45nm\n"
        "  device_roadmap: HP\n"
        "  temperature: " << temperature << "\n"
        "memory:\n";
    if (!memoryCapacity.empty()) {
        out << "  capacity: " << memoryCapacity << "\n";
    }
    if (!physicalCapacity.empty()) {
        out << "  physical_capacity: " << physicalCapacity << "\n";
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
        "    encoder_type: encoding_two_bit\n"
        "  output:\n"
        "    buffer: false\n"
        "    priority_encoder: false\n"
        "    accumulator: false\n"
        "wires:\n"
        "  local:\n"
        "    type: LocalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: false\n"
        "  global:\n"
        "    type: GlobalAggressive\n"
        "    repeater: RepeatedOpt\n"
        "    low_swing: false\n"
        "sensing: ./tmp_top_level.sensing.yaml\n";
    if (!organization.empty()) {
        out << organization;
    }
    if (!extra.empty()) {
        out << extra;
    }
}

void WriteRunConfig(const std::string &optimizationTarget = "ReadLatency",
        const std::string &extra = "") {
    std::ofstream out(kConfigPath);
    out <<
        "schema: config\n"
        "name: tmp_top_level\n"
        "architecture: ./tmp_top_level.architecture.yaml\n"
        "cell: ./tmp_top_level_cell_config.yaml\n"
        "technology: ../config/lib/technology/cmos.legacy.yaml\n"
        "optimization:\n"
        "  target: " << optimizationTarget << "\n"
        "  buffer_design: latency\n"
        "  row_driver: latency\n"
        "  priority_encoder: latency\n";
    out << extra;
}

void WriteBaseTopLevelConfig(const std::string &architectureExtra = "",
        const std::string &runExtra = "") {
    WriteMinimalCellFile();
    WriteArchitectureFile("1KB", "64bits", "", architectureExtra);
    WriteRunConfig("ReadLatency", runExtra);
}

void WriteExplicitSubarrayConfig(const std::string &memoryCapacity,
        const std::string &wordWidth,
        const std::string &optimizationTarget,
        const std::string &organizationExtra = "",
        const std::string &optimizationExtra = "",
        const std::string &runExtra = "") {
    WriteMinimalCellFile();
    std::string organization =
        "organization:\n";
    if (!organizationExtra.empty()) {
        organization += organizationExtra;
    } else {
        organization +=
            "  banks:\n"
            "    total: [1, 1]\n"
            "    active: [1, 1]\n"
            "  mats:\n"
            "    total: [1, 1]\n"
            "    active: [1, 1]\n";
    }
    organization +=
        "  subarray:\n"
        "    dimensions: [64, 64]\n"
        "  mux:\n"
        "    sense_amp: 1\n"
        "    output_level1: 1\n"
        "    output_level2: 1\n";
    WriteArchitectureFile(memoryCapacity, wordWidth, organization);

    std::string extra = optimizationExtra + runExtra;
    WriteRunConfig(optimizationTarget, extra);
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

void TestSplitConfigParsesAndMapsMovedFields() {
    WriteMinimalCellFile();
    WriteArchitectureFile("1KB", "64bits",
        "organization:\n"
        "  bit_serial_width: 64bits\n"
        "physical_limits:\n"
        "  max_nmos_size: 12F\n"
        "  max_driver_current: 2uA\n",
        "", "1KB");
    WriteSensingFile(
            "worst_case_sense_margin: 30mV\n"
            "strict_sense_margin: true\n");
    WriteRunConfig("ReadLatency",
        "design_constraints:\n"
        "  enabled: true\n"
        "  area: 0.5\n"
        "exploration:\n"
        "  use_cacti_assumption: true\n"
        "  enable_pruning: false\n"
        "modeling:\n"
        "  exclude_precharge_latency: true\n"
        "  include_leakage: true\n"
        "  scaled_voltage: 0.9\n"
        "output:\n"
        "  results: results/split.yaml\n"
        "  exploration_csv_prefix: results/split_points\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);

    assert(config.input.fileMemCell.find("tests/tmp_top_level_cell_config.yaml") != std::string::npos);
    assert(config.runtimeSizing.realCapacity == 1024);
    assert(config.input.maxNmosSize == 12);
    assert(config.input.maxDriverCurrent == 2e-6);
    assert(config.peripherals.matchlineSenseMargin == 30e-3);
    assert(config.peripherals.strictSenseMargin);
    assert(config.peripherals.typeInputEnc == encoding_two_bit);
    assert(config.constraints.enabled);
    assert(config.constraints.area == 0.5);
    assert(config.useCactiAssumption);
    assert(!config.exploration.pruningEnabled);
    assert(config.peripherals.noPrechargeInc);
    assert(config.peripherals.includeLeakage);
    assert(config.peripherals.scaledVoltage == 0.9);
    assert(config.input.outputYamlFileName == "results/split.yaml");
    assert(config.input.outputFilePrefix == "results/split_points");
}

void TestRunConfigRejectsArchitectureFields() {
    WriteBaseTopLevelConfig("", "memory:\n  capacity: 1KB\n");
    assert(LoadThrowsWithMessage("field owned by the other config: memory"));
}

void TestArchitectureRejectsRunFields() {
    WriteBaseTopLevelConfig("optimization:\n  target: ReadLatency\n");
    assert(LoadThrowsWithMessage("field owned by the other config: optimization"));
}

void TestRunConfigRequiresReferences() {
    WriteMinimalCellFile();
    WriteArchitectureFile();
    {
        std::ofstream out(kConfigPath);
        out <<
            "schema: config\n"
            "name: tmp_top_level\n"
            "architecture: ./tmp_top_level.architecture.yaml\n"
            "technology: ../config/lib/technology/cmos.legacy.yaml\n"
            "optimization:\n"
            "  target: ReadLatency\n";
    }
    assert(LoadThrowsWithMessage("Missing key: cell"));
}

void TestSensingFileActivatesSenseAmpModel() {
    WriteBaseTopLevelConfig();

    EvaCamConfig config;
    EvaCamYamlLoader::Load(kConfigPath, config);
    assert(config.peripherals.customSenseAmp == false);
    assert(config.peripherals.fileSenseAmp.find("config/lib/sense_amp/nvsim_vol.sense_amp.yaml")
            != std::string::npos);
}

void TestMissingRequiredTopLevelSectionThrows() {
    std::ofstream out(kConfigPath);
    out <<
        "schema: config\n"
        "name: missing_refs\n";

    assert(LoadThrows());
}

void TestInvalidUnitThrows() {
    WriteBaseTopLevelConfig(
        "matchline:\n"
        "  additional_cap: 1XB\n");

    assert(LoadThrowsWithMessage("Unknown unit"));
}

void TestRunConfigRejectsNonFiniteValues() {
    for (const char *value : {"nan", "inf", "1e999"}) {
        WriteBaseTopLevelConfig("",
                std::string("design_constraints:\n  read_latency: ") + value + "s\n");
        assert(LoadThrowsWithMessage(
                "Non-finite value for constraints.read_latency"));
    }
}

void TestArchitectureRejectsNonFiniteValues() {
    for (const char *value : {"nan", "inf", "1e999"}) {
        WriteMinimalCellFile();
        WriteArchitectureFile(
                "1KB", "64bits", "", "", "", std::string(value) + "K");
        WriteRunConfig();
        assert(LoadThrowsWithMessage("Non-finite value for design.temperature"));
    }
}

void TestSensingRejectsNonFiniteValues() {
    for (const char *value : {"nan", "inf", "1e999"}) {
        WriteBaseTopLevelConfig();
        WriteSensingFile(
                std::string("worst_case_sense_margin: ") + value + "V\n");
        assert(LoadThrowsWithMessage(
                "Non-finite value for extra.worst_case_sense_margin"));
    }
}

void TestUnknownConfigKeysThrow() {
    WriteBaseTopLevelConfig();
    {
        std::ofstream out(kConfigPath, std::ios::app);
        out << "unknown_run_option: true\n";
    }
    assert(LoadThrowsWithMessage("unknown key 'run config.unknown_run_option'"));

    WriteBaseTopLevelConfig();
    {
        std::ofstream out(kArchitecturePath, std::ios::app);
        out << "unknown_architecture_option: true\n";
    }
    assert(LoadThrowsWithMessage("unknown key 'architecture config.unknown_architecture_option'"));

    WriteBaseTopLevelConfig();
    {
        std::ofstream out(kArchitecturePath, std::ios::app);
        out << "physical_limits:\n  unknown_limit: 1F\n";
    }
    assert(LoadThrowsWithMessage("unknown key 'physical_limits.unknown_limit'"));

    WriteBaseTopLevelConfig();
    {
        std::ofstream out(kSensingPath, std::ios::app);
        out << "unknown_sensing_option: true\n";
    }
    assert(LoadThrowsWithMessage("unknown key 'sensing.unknown_sensing_option'"));
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

    assert(LoadThrowsWithMessage("field owned by the other config: array"));
}

void TestCactiAssumptionNormalizesGeometry() {
    WriteBaseTopLevelConfig("", 
        "exploration:\n"
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
    WriteMinimalCellFile();
    WriteArchitectureFile("auto", "64bits");
    WriteRunConfig();

    assert(LoadThrowsWithMessage("memory.capacity is required unless organization.subarray.dimensions is supplied"));
}

void TestNonPowerOfTwoWordWidthRequiresRealCapacity() {
    WriteMinimalCellFile();
    WriteArchitectureFile("1KB", "96bits");
    WriteRunConfig();

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
    TestMinimalTopLevelConfigParses();
    TestOptionalSectionsCanBeOmitted();
    TestSplitConfigParsesAndMapsMovedFields();
    TestRunConfigRejectsArchitectureFields();
    TestArchitectureRejectsRunFields();
    TestRunConfigRequiresReferences();
    TestSensingFileActivatesSenseAmpModel();
    TestMissingRequiredTopLevelSectionThrows();
    TestInvalidUnitThrows();
    TestRunConfigRejectsNonFiniteValues();
    TestArchitectureRejectsNonFiniteValues();
    TestSensingRejectsNonFiniteValues();
    TestUnknownConfigKeysThrow();
    TestOrganizationAliasParses();
    TestCactiAssumptionNormalizesGeometry();
    TestAutoCapacityRequiresFixedSubarrayDimensions();
    TestNonPowerOfTwoWordWidthRequiresRealCapacity();
    TestFixedSubarrayDimensionsRejectExplorationTarget();
    TestFixedSubarrayDimensionsRejectDeepExploration();
    std::cout << "TopLevelConfigParser tests passed" << std::endl;
    return 0;
}
