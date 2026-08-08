#include "config/ConfigSectionReaders.h"

#include <cassert>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

#include "EvaCamConfig.h"
#include "TestSupport.h"

namespace {

void AssertFixed(const IntValueDomain &domain, int value) {
    assert(domain.IsFixed());
    assert(domain.Min() == value);
    assert(domain.Max() == value);
}

void TestReadDesignSection() {
    EvaCamConfig config;
    const YAML::Node root = YAML::Load(
            "design:\n"
            "  target: CAM\n"
            "  search_function: BE\n"
            "  system_process_node: 90nm\n"
            "  device_roadmap: LSTP\n"
            "  temperature: 325K\n"
            "  ignored_by_reader: true\n");

    ConfigSectionReaders::ReadDesignSection(root, config);

    assert(config.input.designTarget == CAM_chip);
    assert(config.input.searchFunction == BE);
    assert(config.input.processNode == 90);
    assert(config.input.deviceRoadmap == LSTP);
    assert(config.input.temperature == 325);

    TestSupport::AssertThrows<std::runtime_error>([&] {
        ConfigSectionReaders::ReadDesignSection(YAML::Load("design: {}\n"), config);
    }, "Missing key: target");
}

void TestReadMemorySection() {
    EvaCamConfig automatic;
    ConfigSectionReaders::ReadMemorySection(YAML::Load(
            "memory:\n"
            "  cell_file: cell.yaml\n"
            "  capacity: auto\n"
            "  word_width: 64bit\n"), automatic);
    assert(automatic.input.fileMemCell == "cell.yaml");
    assert(automatic.input.capacity == 0);
    assert(automatic.runtimeSizing.hasExplicitCapacity);
    assert(automatic.runtimeSizing.capacityIsAuto);
    AssertFixed(automatic.exploration.cam.bitSerialWidth, 64);

    EvaCamConfig omitted;
    ConfigSectionReaders::ReadMemorySection(YAML::Load(
            "memory:\n"
            "  cell_file: cell.yaml\n"
            "  word_width: 32\n"), omitted);
    assert(!omitted.runtimeSizing.hasExplicitCapacity);
    assert(!omitted.runtimeSizing.capacityIsAuto);
    assert(omitted.input.capacity == 0);

    EvaCamConfig explicitCapacity;
    ConfigSectionReaders::ReadMemorySection(YAML::Load(
            "memory:\n"
            "  cell_file: cell.yaml\n"
            "  capacity: 2KB\n"
            "  word_width: 32bit\n"
            "  ignored_by_reader: true\n"), explicitCapacity);
    assert(explicitCapacity.runtimeSizing.hasExplicitCapacity);
    assert(!explicitCapacity.runtimeSizing.capacityIsAuto);
    assert(explicitCapacity.input.capacity == 2 * 1024);

    TestSupport::AssertThrows<std::runtime_error>([&] {
        ConfigSectionReaders::ReadMemorySection(YAML::Load("memory: {}\n"), omitted);
    }, "Missing key: cell_file");
}

void TestReadRoutingSection() {
    EvaCamConfig config;
    ConfigSectionReaders::ReadRoutingSection(YAML::Load(
            "routing:\n"
            "  type: H-tree\n"
            "  ignored_by_reader: true\n"), config);
    assert(config.input.routingMode == h_tree);

    ConfigSectionReaders::ReadRoutingSection(
            YAML::Load("routing:\n  type: non_h_tree\n"), config);
    assert(config.input.routingMode == non_h_tree);
}

void TestReadPeripheralSection() {
    EvaCamConfig config;
    config.input.wordWidth = 128;
    ConfigSectionReaders::ReadPeripheralSection(YAML::Load(
            "peripherals:\n"
            "  write_driver: true\n"
            "  input:\n"
            "    buffer: true\n"
            "    encoder: true\n"
            "    custom_encoder: false\n"
            "  output:\n"
            "    buffer: true\n"
            "    priority_encoder: false\n"
            "    accumulator: false\n"
            "  ignored_by_reader: true\n"), config);
    assert(config.peripherals.withWriteDriver);
    assert(config.peripherals.withInputBuffer);
    assert(config.peripherals.withInputEnc);
    assert(!config.peripherals.customInputEnc);
    assert(config.peripherals.withOutputBuffer);
    assert(!config.peripherals.withPriorityEnc);
    assert(!config.peripherals.withOutputAcc);
    AssertFixed(config.exploration.cam.bitSerialWidth, 128);

    TestSupport::AssertThrows<std::runtime_error>([&] {
        ConfigSectionReaders::ReadPeripheralSection(YAML::Load("peripherals:\n  write_driver: true\n"), config);
    }, "Missing key: input");
}

void TestReadSensingSection() {
    EvaCamConfig config;
    ConfigSectionReaders::ReadSensingSection(YAML::Load(
            "sensing:\n"
            "  internal: false\n"
            "  custom_sense_amp: true\n"
            "  sensing_mode: nvsim_cur\n"
            "  ignored_by_reader: true\n"), config);
    assert(!config.input.internalSensing);
    assert(config.peripherals.customSenseAmp);
    assert(config.peripherals.typeSenseAmp == nvsim_current_sense);

    TestSupport::AssertThrows<std::runtime_error>([&] {
        ConfigSectionReaders::ReadSensingSection(YAML::Load("sensing:\n  internal: true\n"), config);
    }, "Missing key: custom_sense_amp");
}

void TestReadOptimizationSection() {
    EvaCamConfig config;
    ConfigSectionReaders::ReadOptimizationSection(YAML::Load(
            "optimization:\n"
            "  target: Area\n"
            "  buffer_design: balance\n"
            "  row_driver: latency\n"
            "  priority_encoder: area\n"
            "  ignored_by_reader: true\n"), config);
    assert(config.input.optimizationTarget == area_optimized);
    assert(!config.requestDeepExploration);
    AssertFixed(config.exploration.cam.areaOptimizationLevel, latency_area_trade_off);
    AssertFixed(config.exploration.cam.rowDriverOptLevel, latency_first);
    AssertFixed(config.exploration.cam.priorityOptLevel, area_first);

    TestSupport::AssertThrows<std::runtime_error>([&] {
        ConfigSectionReaders::ReadOptimizationSection(YAML::Load("optimization:\n  target: Area\n"), config);
    }, "Missing key: buffer_design");
}

void TestReadWireSection() {
    EvaCamConfig config;
    ConfigSectionReaders::ReadWireSection(YAML::Load(
            "wires:\n"
            "  local:\n"
            "    type: LocalAggressive\n"
            "    repeater: RepeatedOpt\n"
            "    low_swing: true\n"
            "  global:\n"
            "    type: GlobalConservative\n"
            "    repeater: RepeatedNone\n"
            "    low_swing: false\n"
            "  ignored_by_reader: true\n"), config);
    AssertFixed(config.exploration.wires.localWireType, local_aggressive);
    AssertFixed(config.exploration.wires.localWireRepeaterType, repeated_opt);
    AssertFixed(config.exploration.wires.isLocalWireLowSwing, true);
    AssertFixed(config.exploration.wires.globalWireType, global_conservative);
    AssertFixed(config.exploration.wires.globalWireRepeaterType, repeated_none);
    AssertFixed(config.exploration.wires.isGlobalWireLowSwing, false);

    TestSupport::AssertThrows<std::runtime_error>([&] {
        ConfigSectionReaders::ReadWireSection(YAML::Load("wires:\n  local: {}\n"), config);
    }, "Missing key: type");
}

void TestReadOrganizationSection() {
    EvaCamConfig absent;
    ConfigSectionReaders::ReadOrganizationSection(YAML::Load("unknown: value\n"), absent);
    assert(!absent.runtimeSizing.hasFixedSubarrayDimensions);

    EvaCamConfig config;
    ConfigSectionReaders::ReadOrganizationSection(YAML::Load(
            "array:\n"
            "  banks:\n"
            "    total: [2, 4]\n"
            "    active: [1, 2]\n"
            "  mats:\n"
            "    total: [4, 8]\n"
            "    active: [2, 4]\n"
            "  subarray:\n"
            "    dimensions: [64, 128]\n"
            "  mux:\n"
            "    sense_amp: 2\n"
            "    output_level1: 4\n"
            "    output_level2: 8\n"
            "  ignored_by_reader: true\n"), config);
    AssertFixed(config.exploration.geometry.numRowMat, 2);
    AssertFixed(config.exploration.geometry.numColumnMat, 4);
    AssertFixed(config.exploration.geometry.numActiveMatPerColumn, 1);
    AssertFixed(config.exploration.geometry.numActiveMatPerRow, 2);
    AssertFixed(config.exploration.geometry.numRowSubarray, 4);
    AssertFixed(config.exploration.geometry.numColumnSubarray, 8);
    AssertFixed(config.exploration.geometry.numActiveSubarrayPerColumn, 2);
    AssertFixed(config.exploration.geometry.numActiveSubarrayPerRow, 4);
    AssertFixed(config.exploration.geometry.numRow, 64);
    AssertFixed(config.exploration.geometry.numColumn, 128);
    AssertFixed(config.exploration.geometry.muxSenseAmp, 2);
    AssertFixed(config.exploration.geometry.muxOutputLev1, 4);
    AssertFixed(config.exploration.geometry.muxOutputLev2, 8);
    assert(config.runtimeSizing.hasFixedSubarrayDimensions);
    assert(config.runtimeSizing.fixedSubarrayRows == 64);
    assert(config.runtimeSizing.fixedSubarrayColumns == 128);

    TestSupport::AssertThrows<std::runtime_error>([&] {
        ConfigSectionReaders::ReadOrganizationSection(YAML::Load(
                "organization:\n"
                "  banks:\n"
                "    total: [1, 1]\n"
                "    active: [1, 1]\n"
                "  mats:\n"
                "    total: [1, 1]\n"
                "    active: [1, 1]\n"
                "  subarray:\n"
                "    dimensions: [64]\n"), config);
    }, "organization.subarray.dimensions");
}

void TestReadMatchlineSection() {
    EvaCamConfig omitted;
    ConfigSectionReaders::ReadMatchlineSection(YAML::Load("unknown: value\n"), omitted);
    assert(omitted.peripherals.addCapOnML == 0);

    EvaCamConfig config;
    ConfigSectionReaders::ReadMatchlineSection(YAML::Load(
            "matchline:\n"
            "  additional_cap: 2fF\n"
            "  match_transistor:\n"
            "    cmos_width: 4F\n"
            "  ignored_by_reader: true\n"), config);
    TestSupport::AssertNear(config.peripherals.addCapOnML, 2e-15);
    TestSupport::AssertNear(config.input.camWidthMatchTran, 4.0);
    assert(config.input.hasCamWidthMatchTran);
}

void TestReadConstraintSection() {
    EvaCamConfig absent;
    ConfigSectionReaders::ReadConstraintSection(YAML::Load("unknown: value\n"), absent);
    assert(!absent.constraints.enabled);

    EvaCamConfig config;
    ConfigSectionReaders::ReadConstraintSection(YAML::Load(
            "constraints:\n"
            "  read_latency: 5ns\n"
            "  area: 12.5\n"
            "  ignored_by_reader: true\n"), config);
    assert(config.constraints.enabled);
    TestSupport::AssertNear(config.constraints.readLatency, 5e-9);
    TestSupport::AssertNear(config.constraints.area, 12.5);

    EvaCamConfig explicitEnabled;
    ConfigSectionReaders::ReadConstraintSection(YAML::Load("constraints:\n  enabled: true\n"), explicitEnabled);
    assert(explicitEnabled.constraints.enabled);
}

void TestReadAdvancedSection() {
    EvaCamConfig absent;
    ConfigSectionReaders::ReadAdvancedSection(YAML::Load("unknown: value\n"), absent);
    assert(!absent.useCactiAssumption);

    EvaCamConfig config;
    ConfigSectionReaders::ReadAdvancedSection(YAML::Load(
            "advanced:\n"
            "  max_nmos_size: 6F\n"
            "  use_cacti_assumption: true\n"
            "  enable_pruning: true\n"
            "  bit_serial_width: 16bit\n"
            "  input_encoder_type: encoding_two_bit\n"
            "  custom_sa_input_file: custom.yaml\n"
            "  sense_amp_input_file: sense.yaml\n"
            "  use_updated_lib: true\n"
            "  exclude_precharge_latency: true\n"
            "  include_leakage: true\n"
            "  scaled_voltage: 0.8\n"
            "  ignored_by_reader: true\n"), config);
    TestSupport::AssertNear(config.input.maxNmosSize, 6.0);
    assert(config.useCactiAssumption);
    assert(config.exploration.useCactiAssumption);
    assert(config.exploration.pruningEnabled);
    AssertFixed(config.exploration.cam.bitSerialWidth, 16);
    assert(config.peripherals.typeInputEnc == encoding_two_bit);
    assert(config.peripherals.fileCustomSA == "custom.yaml");
    assert(config.peripherals.fileSenseAmp == "sense.yaml");
    assert(config.peripherals.useUpdatedLib);
    assert(config.peripherals.noPrechargeInc);
    assert(config.peripherals.includeLeakage);
    TestSupport::AssertNear(config.peripherals.scaledVoltage, 0.8);
}

void TestReadFlashSection() {
    EvaCamConfig absent;
    ConfigSectionReaders::ReadFlashSection(YAML::Load("unknown: value\n"), absent);
    assert(absent.input.pageSize == 0);

    EvaCamConfig config;
    ConfigSectionReaders::ReadFlashSection(YAML::Load(
            "flash:\n"
            "  page_size: 2KB\n"
            "  block_size: 1MB\n"
            "  ignored_by_reader: true\n"), config);
    assert(config.input.pageSize == 2 * 1024 * 8);
    assert(config.input.flashBlockSize == 1024 * 1024 * 8);
}

void TestReadExtraSection() {
    EvaCamConfig config;
    ConfigSectionReaders::ReadExtraSection(YAML::Load(
            "extra:\n"
            "  output_file_prefix: output/custom\n"
            "  output_yaml_file: output.yaml\n"
            "  technology_file: technology.yaml\n"
            "  worst_case_sense_margin: 25mV\n"
            "  max_driver_current: 2mA\n"
            "  real_capacity: 3KB\n"
            "  RealCapacity (MB): 9\n"
            "  ignored_by_reader: true\n"), config);
    assert(config.input.outputFilePrefix == "output/custom");
    assert(config.input.outputYamlFileName == "output.yaml");
    assert(config.input.fileTechnology == "technology.yaml");
    TestSupport::AssertNear(config.peripherals.matchlineSenseMargin, 25e-3);
    TestSupport::AssertNear(config.input.maxDriverCurrent, 2e-3);
    assert(config.runtimeSizing.realCapacity == 3 * 1024);

    EvaCamConfig legacy;
    ConfigSectionReaders::ReadExtraSection(YAML::Load("extra:\n  RealCapacity (KB): 4\n"), legacy);
    assert(legacy.runtimeSizing.realCapacity == 4 * 1024);

    TestSupport::AssertThrows<std::runtime_error>([&] {
        ConfigSectionReaders::ReadExtraSection(YAML::Load("extra:\n  real_capacity: 1.5B\n"), legacy);
    }, "extra.real_capacity");
}

}  // namespace

int main() {
    TestReadDesignSection();
    TestReadMemorySection();
    TestReadRoutingSection();
    TestReadPeripheralSection();
    TestReadSensingSection();
    TestReadOptimizationSection();
    TestReadWireSection();
    TestReadOrganizationSection();
    TestReadMatchlineSection();
    TestReadConstraintSection();
    TestReadAdvancedSection();
    TestReadFlashSection();
    TestReadExtraSection();
    return 0;
}
