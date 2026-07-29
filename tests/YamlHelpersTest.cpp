#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"
#include "config/EvaCamConfig.h"
#include "config/EvaCamYamlLoader.h"
#include "config/TechnologyLoader.h"
#include "config/VariationConfigBuilder.h"
#include "MemCell.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace YamlHelpers;

enum class MyEnum { Foo, Bar };

static void test_basic_read() {
    const YAML::Node n = YAML::Load("root:\n  value: 42\n  text: hello\n  flag: true\n");
    auto root = YamlHelpers::child_required(n, "root");
    int value = YamlHelpers::read_required<int>(root, "value");
    std::string text = YamlHelpers::read_required<std::string>(root, "text");
    bool flag = YamlHelpers::read_required<bool>(root, "flag");

    assert(value == 42);
    assert(text == "hello");
    assert(flag == true);
}

static void test_enum_read() {
    const YAML::Node n = YAML::Load(
        "root:\n"
        "  mode: Bar\n"
        "  mem: SRAM\n"
        "  access: CMOS\n"
        "  target: CAM\n"
        "  road: HP\n"
        "  wire: LocalAggressive\n"
        "  repeater: RepeatedOpt\n"
        "  buffer: latency\n"
        "  cam: TCAM\n"
        "  search: EX\n"
        "  route: H-tree\n"
        "  write: Normal\n"
        "  opt: ReadLatency\n"
        "  encoder: encoding_two_bit\n"
        "  sense: nvsim_vol\n"
        "  port: Matchline\n"
        "  region: drain\n");
    auto root = YamlHelpers::child_required(n, "root");
    auto v = YamlHelpers::read_enum_required<MyEnum>(
        root,
        "mode",
        {{"Foo", MyEnum::Foo}, {"Bar", MyEnum::Bar}},
        true);
    assert(v == MyEnum::Bar);

    auto mem = YamlHelpers::read_enum_required<MemCellType>(root, "mem");
    auto access = YamlHelpers::read_enum_required<CellAccessType>(root, "access");
    auto target = YamlHelpers::read_enum_required<DesignTarget>(root, "target");
    auto road = YamlHelpers::read_enum_required<DeviceRoadmap>(root, "road");
    auto wire = YamlHelpers::read_enum_required<WireType>(root, "wire");
    auto repeater = YamlHelpers::read_enum_required<WireRepeaterType>(root, "repeater");
    auto buffer = YamlHelpers::read_enum_required<BufferDesignTarget>(root, "buffer");
    auto cam = YamlHelpers::read_enum_required<CAMType>(root, "cam");
    auto search = YamlHelpers::read_enum_required<SearchFunction>(root, "search");
    auto route = YamlHelpers::read_enum_required<RoutingMode>(root, "route");
    auto write = YamlHelpers::read_enum_required<WriteScheme>(root, "write");
    auto opt = YamlHelpers::read_enum_required<OptimizationTarget>(root, "opt");
    auto encoder = YamlHelpers::read_enum_required<TypeOfInputEncoder>(root, "encoder");
    auto sense = YamlHelpers::read_enum_required<TypeOfSenseAmp>(root, "sense");
    auto port = YamlHelpers::read_enum_required<CAM_PortType>(root, "port");
    auto region = YamlHelpers::read_enum_required<CAM_CmosRegion>(root, "region");

    assert(mem == SRAM);
    assert(access == CMOS_access);
    assert(target == CAM_chip);
    assert(road == HP);
    assert(wire == local_aggressive);
    assert(repeater == repeated_opt);
    assert(buffer == latency_first);
    assert(cam == TCAM);
    assert(search == EX);
    assert(route == h_tree);
    assert(write == normal_write);
    assert(opt == read_latency_optimized);
    assert(encoder == encoding_two_bit);
    assert(sense == nvsim_voltage_sense);
    assert(port == Matchline);
    assert(region == drain);
}

static void test_bcam_alias() {
    const YAML::Node n = YAML::Load(
        "root:\n"
        "  cam: BCAM\n");
    auto root = YamlHelpers::child_required(n, "root");
    auto cam = YamlHelpers::read_enum_required<CAMType>(root, "cam");
    assert(cam == TCAM);
}

static void test_units() {
    const YAML::Node n = YAML::Load(
        "root:\n"
        "  voltage: 4V\n"
        "  current: 500uA\n"
        "  delay: 10ns\n"
        "  energy: 2pJ\n"
        "  cap: 5fF\n"
        "  resistance: 854.7Kohm\n"
        "  temp: 350K\n"
        "  cap_bytes: 2KB\n"
        "  width_bits: 128bit\n");

    auto root = YamlHelpers::child_required(n, "root");

    double v = YamlHelpers::read_quantity_required(root, "voltage", VoltageUnits(), 1.0, "voltage");
    double i = YamlHelpers::read_quantity_required(root, "current", CurrentUnits(), 1.0, "current");
    double t = YamlHelpers::read_quantity_required(root, "delay", TimeUnits(), 1.0, "delay");
    double e = YamlHelpers::read_quantity_required(root, "energy", EnergyUnits(), 1.0, "energy");
    double c = YamlHelpers::read_quantity_required(root, "cap", CapacitanceUnits(), 1.0, "capacitance");
    double r = YamlHelpers::read_quantity_required(root, "resistance", ResistanceUnits(), 1.0, "resistance");
    double temp = YamlHelpers::read_quantity_required(root, "temp", TemperatureUnits(), 1.0, "temperature");
    double cap = YamlHelpers::read_quantity_required(root, "cap_bytes", DataSizeUnits(), 1.0, "capacity");
    double bits = YamlHelpers::read_quantity_required(root, "width_bits", BitUnits(), 1.0, "word_width");

    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-18; };
    assert(near(v, 4.0));
    assert(near(i, 500e-6));
    assert(near(t, 10e-9));
    assert(near(e, 2e-12));
    assert(near(c, 5e-15));
    assert(near(r, 854.7e3));
    assert(near(temp, 350.0));
    assert(near(cap, 2048.0));
    assert(near(bits, 128.0));

    bool rejectedUppercaseMilli = false;
    try {
        const YAML::Node bad = YAML::Load("root:\n  voltage: 4MV\n");
        auto badRoot = YamlHelpers::child_required(bad, "root");
        YamlHelpers::read_quantity_required(badRoot, "voltage", VoltageUnits(), 1.0, "voltage");
    } catch (const std::runtime_error&) {
        rejectedUppercaseMilli = true;
    }
    assert(rejectedUppercaseMilli);
}

static void test_non_finite_numbers_are_rejected() {
    const YAML::Node quantities = YAML::Load(
        "root:\n"
        "  nan: nanV\n"
        "  infinity: infV\n"
        "  overflow: 1e309V\n");
    const auto quantityRoot = YamlHelpers::child_required(quantities, "root");

    for (const char *key : {"nan", "infinity", "overflow"}) {
        try {
            (void)YamlHelpers::read_quantity_required(
                    quantityRoot, key, VoltageUnits(), 1.0, key);
            assert(false && "Expected non-finite quantity to throw");
        } catch (const std::runtime_error&) {
            // expected
        }
    }

    const YAML::Node scalars = YAML::Load(
        "root:\n"
        "  nan: .nan\n"
        "  infinity: .inf\n");
    const auto scalarRoot = YamlHelpers::child_required(scalars, "root");
    for (const char *key : {"nan", "infinity"}) {
        try {
            (void)YamlHelpers::read_required<double>(scalarRoot, key);
            assert(false && "Expected non-finite scalar to throw");
        } catch (const std::runtime_error&) {
            // expected
        }
    }
}

static void write_v2_cell_fixture(const std::string &variationBlock = "") {
    {
        std::ofstream out("tests/tmp_yaml_helpers.memory_device.yaml");
        out <<
            "schema: memory_device\n"
            "name: TestDevice\n"
            "type: SRAM\n"
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
        out << variationBlock;
    }

    {
        std::ofstream out("tests/tmp_yaml_helpers.cell.yaml");
        out <<
            "schema: cell\n"
            "name: TestCell\n"
            "cam_type: TCAM\n"
            "memory_device: ./tmp_yaml_helpers.memory_device.yaml\n"
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
}

static void write_v2_loader_config(const char *cfgPath,
        const std::string &memoryCapacity,
        const std::string &wordWidth,
        const std::string &organizationSection = "",
        const std::string &optimizationExtra = "",
        const std::string &runExtra = "",
        const std::string &variationBlock = "",
        const std::string &physicalCapacity = "") {
    write_v2_cell_fixture(variationBlock);

    {
        std::ofstream out("tests/tmp_yaml_helpers.sensing.yaml");
        out <<
            "schema: sensing\n"
            "internal: true\n"
            "sense_amplifier: ../config/lib/sense_amp/nvsim_vol.sense_amp.yaml\n";
    }

    {
        std::ofstream out("tests/tmp_yaml_helpers.architecture.yaml");
        out <<
            "schema: architecture\n"
            "design:\n"
            "  target: CAM\n"
            "  search_function: EX\n"
            "  system_process_node: 45nm\n"
            "  device_roadmap: HP\n"
            "  temperature: 300K\n"
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
            "sensing: ./tmp_yaml_helpers.sensing.yaml\n";
        if (!organizationSection.empty()) {
            out << organizationSection;
        }
    }

    {
        std::ofstream out(cfgPath);
        out <<
            "schema: config\n"
            "name: tmp_yaml_helpers\n"
            "architecture: ./tmp_yaml_helpers.architecture.yaml\n"
            "cell: ./tmp_yaml_helpers.cell.yaml\n"
            "technology: ../config/lib/technology/cmos.legacy.yaml\n"
            "optimization:\n"
            "  target: ReadLatency\n"
            "  buffer_design: latency\n"
            "  row_driver: latency\n"
            "  priority_encoder: latency\n";
        if (!optimizationExtra.empty()) {
            out << optimizationExtra;
        }
        if (!runExtra.empty()) {
            out << runExtra;
        }
    }
}

static void test_organization_section() {
    const char *cfgPath = "tests/tmp_organization_system_config.yaml";

    write_v2_loader_config(cfgPath, "1KB", "64bits",
        "organization:\n"
        "  banks:\n"
        "    total: [2, 4]\n"
        "    active: [1, 2]\n"
        "  mats:\n"
        "    total: [4, 8]\n"
        "    active: [2, 4]\n"
        "  mux:\n"
        "    sense_amp: 2\n"
        "    output_level1: 4\n"
        "    output_level2: 8\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(cfgPath, config);

    assert(config.exploration.geometry.numRowMat.Min() == 2);
    assert(config.exploration.geometry.numColumnMat.Min() == 4);
    assert(config.exploration.geometry.numActiveMatPerColumn.Min() == 1);
    assert(config.exploration.geometry.numActiveMatPerRow.Min() == 2);
    assert(config.exploration.geometry.numRowSubarray.Min() == 4);
    assert(config.exploration.geometry.numColumnSubarray.Min() == 8);
    assert(config.exploration.geometry.numActiveSubarrayPerColumn.Min() == 2);
    assert(config.exploration.geometry.numActiveSubarrayPerRow.Min() == 4);
    assert(config.exploration.geometry.muxSenseAmp.Min() == 2);
    assert(config.exploration.geometry.muxOutputLev1.Min() == 4);
    assert(config.exploration.geometry.muxOutputLev2.Min() == 8);
}

static void write_explicit_subarray_config(const char *cfgPath,
        const std::string &memoryCapacity,
        const std::string &subarrayDimensions,
        const std::string &organizationPrefix = "",
        const std::string &optimizationExtra = "",
        const std::string &extraSection = "") {
    std::string organization = "organization:\n";
    if (!organizationPrefix.empty()) {
        organization += organizationPrefix;
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
        "    dimensions: " + subarrayDimensions + "\n"
        "  mux:\n"
        "    sense_amp: 1\n"
        "    output_level1: 1\n"
        "    output_level2: 1\n";

    std::string physicalCapacity;
    std::string runExtra = extraSection;
    if (extraSection == "extra:\n  real_capacity: 512B\n") {
        physicalCapacity = "512B";
        runExtra.clear();
    }

    write_v2_loader_config(cfgPath, memoryCapacity, "64bits", organization,
            optimizationExtra, runExtra, "", physicalCapacity);
}

static bool load_config_throws(const char *cfgPath) {
    try {
        EvaCamConfig config;
        EvaCamYamlLoader::Load(cfgPath, config);
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

static void test_explicit_subarray_dimensions() {
    const char *cfgPath = "tests/tmp_explicit_subarray_system_config.yaml";

    write_explicit_subarray_config(cfgPath, "", "[64, 64]");
    {
        EvaCamConfig config;
        EvaCamYamlLoader::Load(cfgPath, config);
        assert(config.input.capacity == 512);
        assert(config.runtimeSizing.hasFixedSubarrayDimensions == true);
        assert(config.runtimeSizing.fixedSubarrayRows == 64);
        assert(config.runtimeSizing.fixedSubarrayColumns == 64);
        assert(config.exploration.geometry.numRow.Values() == std::vector<int>({64}));
        assert(config.exploration.geometry.numColumn.Values() == std::vector<int>({64}));
    }

    write_explicit_subarray_config(cfgPath, "auto", "[64, 64]");
    {
        EvaCamConfig config;
        EvaCamYamlLoader::Load(cfgPath, config);
        assert(config.input.capacity == 512);
        assert(config.runtimeSizing.capacityIsAuto == true);
    }

    write_explicit_subarray_config(cfgPath, "512B", "[64, 64]");
    {
        EvaCamConfig config;
        EvaCamYamlLoader::Load(cfgPath, config);
        assert(config.input.capacity == 512);
    }

    write_explicit_subarray_config(cfgPath, "1KB", "[64, 64]");
    assert(load_config_throws(cfgPath));

    write_explicit_subarray_config(cfgPath, "", "[64, 72]");
    {
        EvaCamConfig config;
        EvaCamYamlLoader::Load(cfgPath, config);
        assert(config.input.capacity == 576);
        assert(config.runtimeSizing.fixedSubarrayColumns == 72);
        assert(config.exploration.geometry.numColumn.Values() == std::vector<int>({72}));
    }

    write_explicit_subarray_config(cfgPath, "", "[8, 64]");
    assert(load_config_throws(cfgPath));

    write_explicit_subarray_config(cfgPath, "", "[64]");
    assert(load_config_throws(cfgPath));

    write_explicit_subarray_config(cfgPath, "", "[64, 64, 64]");
    assert(load_config_throws(cfgPath));

    write_explicit_subarray_config(cfgPath, "", "[64, 64]", "", "  deep_exploration: true\n");
    assert(load_config_throws(cfgPath));

    write_explicit_subarray_config(cfgPath, "", "[64, 64]", "", "", "extra:\n  real_capacity: 1KB\n");
    assert(load_config_throws(cfgPath));

    write_explicit_subarray_config(cfgPath, "", "[64, 64]", "", "", "extra:\n  real_capacity: 512B\n");
    {
        EvaCamConfig config;
        EvaCamYamlLoader::Load(cfgPath, config);
        assert(config.runtimeSizing.realCapacity == 512);
    }

    write_explicit_subarray_config(cfgPath, "", "[64, 64]",
            "  banks:\n"
            "    total: [1, 1]\n"
            "    active: [1, 1]\n");
    assert(load_config_throws(cfgPath));

    write_explicit_subarray_config(cfgPath, "autoB", "[64, 64]");
    assert(load_config_throws(cfgPath));
}

static void test_invalid_inputs() {
    // Invalid unit should throw
    const YAML::Node bad_unit = YAML::Load("root:\n  voltage: 1XB\n");
    auto root1 = YamlHelpers::child_required(bad_unit, "root");
    try {
        (void)YamlHelpers::read_quantity_required(root1, "voltage", VoltageUnits(), 1.0, "voltage");
        assert(false && "Expected invalid unit to throw");
    } catch (const std::runtime_error&) {
        // expected
    }

    // Invalid enum should throw
    const YAML::Node bad_enum = YAML::Load("root:\n  target: NOT_A_TARGET\n");
    auto root2 = YamlHelpers::child_required(bad_enum, "root");
    try {
        (void)YamlHelpers::read_enum_required<DesignTarget>(root2, "target");
        assert(false && "Expected invalid enum to throw");
    } catch (const std::runtime_error&) {
        // expected
    }

    const YAML::Node cache_target = YAML::Load("root:\n  target: cache\n");
    auto root3 = YamlHelpers::child_required(cache_target, "root");
    try {
        (void)YamlHelpers::read_enum_required<DesignTarget>(root3, "target");
        assert(false && "Expected cache target to throw");
    } catch (const std::runtime_error&) {
        // expected
    }

    const YAML::Node ram_target = YAML::Load("root:\n  target: RAM\n");
    auto root4 = YamlHelpers::child_required(ram_target, "root");
    try {
        (void)YamlHelpers::read_enum_required<DesignTarget>(root4, "target");
        assert(false && "Expected RAM target to throw");
    } catch (const std::runtime_error&) {
        // expected
    }
}

static void test_memcell_yaml() {
    write_v2_cell_fixture();

    MemCell cell;
    cell.ReadCellFromFile("tests/tmp_yaml_helpers.cell.yaml", CAM_chip, 1.0);

    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-18; };

    assert(cell.memCellType == SRAM);
    assert(cell.processNode == 45);
    assert(near(cell.area, 300.0));
    assert(near(cell.aspectRatio, 2.0));

    assert(near(cell.resistanceOn, 1e3));
    assert(near(cell.resistanceOff, 1e6));

    assert(cell.readMode == true);
    assert(near(cell.readVoltage, 1.0));
    assert(near(cell.readCurrent, 5e-6));
    assert(near(cell.readPower, 2e-6));
    assert(near(cell.readEnergy, 10e-15));
    assert(near(cell.minSenseVoltage, 0.07));

    assert(cell.setMode == true);
    assert(near(cell.setVoltage, 4.0));
    assert(near(cell.setCurrent, 1e-6));
    assert(near(cell.setPulse, 10e-9));
    assert(near(cell.setEnergy, 2e-12));

    assert(cell.resetMode == false);
    assert(near(cell.resetVoltage, 3.0));
    assert(near(cell.resetCurrent, 2e-6));
    assert(near(cell.resetPulse, 20e-9));
    assert(near(cell.resetEnergy, 3e-12));

    assert(cell.camNumRow == 1);
    assert(cell.camNumCol == 1);
    assert(near(cell.camPort[0][0].volSetLRS, 4.0));
    assert(near(cell.camPort[1][0].volSearch1, 1.0));
}

static void test_memcell_variation_yaml() {
    write_v2_cell_fixture(
            "variation:\n"
            "  seed: 12345\n"
            "  mode: monte_carlo\n"
            "  monte_carlo_granularity: effective\n"
            "  lut_file: variation_lut.csv\n"
            "  samples: 7\n"
            "  memory_device_resistance_on_stdev: 5%\n"
            "  memory_device_resistance_off_stdev: 7%\n"
            "  memory_device_resistance_on_max_var: 11%\n"
            "  memory_device_resistance_off_max_var: 13%\n");

    MemCell cell;
    cell.ReadCellFromFile("tests/tmp_yaml_helpers.cell.yaml", CAM_chip, 1.0);

    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-18; };

    assert(cell.withVariation == true);
    assert(cell.hasVariationSeed == true);
    assert(cell.hasVariationSamples == true);
    assert(cell.variationSeed == 12345u);
    assert(cell.variationMode == "monte_carlo");
    assert(cell.monteCarloGranularity == "effective");
    assert(cell.variationLutFile == "variation_lut.csv");
    assert(cell.variationSamples == 7);
    assert(near(cell.resistanceOnVariation, 0.05));
    assert(near(cell.resistanceOffVariation, 0.07));
    assert(near(cell.resistanceOnMaxVariation, 0.11));
    assert(near(cell.resistanceOffMaxVariation, 0.13));
}

static void test_cell_variation_drives_runtime_config() {
    const char* cfgPath = "tests/tmp_variation_system_config.yaml";

    write_v2_loader_config(cfgPath, "1KB", "8bit", "", "", "",
        "variation:\n"
        "  mode: monte_carlo\n"
        "  lut_file: tests/tmp_variation_lut.csv\n"
        "  samples: 17\n"
        "  memory_device_resistance_on_stdev: 5%\n"
        "  memory_device_resistance_off_stdev: 8%\n");

    EvaCamConfig config;
    EvaCamYamlLoader::Load(cfgPath, config);

    assert(config.variation.enabled == false);
    assert(config.variation.seed == 0u);
    assert(config.variation.mode == "nominal");
    assert(config.variation.lutFile.empty());
    assert(config.variation.samples == 1);

    InputConfig input = config.input;
    PeripheralConfig peripherals = config.peripherals;
    VariationConfig variation = config.variation;
    TechnologyContext technology = TechnologyLoader::Load(input, peripherals, &variation);

    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-18; };
    assert(technology.cell != nullptr);
    assert(variation.enabled == true);
    assert(variation.seed != 0u);
    assert(variation.mode == "monte_carlo");
    assert(variation.monteCarloGranularity == "cell");
    assert(variation.lutFile == "tests/tmp_variation_lut.csv");
    assert(variation.samples == 17);
    assert(near(variation.memoryDeviceResOnStdev, 0.05));
    assert(near(variation.memoryDeviceResOffStdev, 0.08));
}

static void test_variation_builder_rejects_invalid_mode_and_samples() {
    MemCell cell;
    cell.withVariation = true;

    cell.variationMode = "nominal";
    try {
        (void)VariationConfigBuilder::FromCell(cell);
        assert(false && "Expected nominal variation mode to throw");
    } catch (const std::runtime_error&) {
    }

    cell.variationMode = "monte_carlo";
    cell.variationSamples = 1;
    try {
        (void)VariationConfigBuilder::FromCell(cell);
        assert(false && "Expected monte_carlo samples <= 1 to throw");
    } catch (const std::runtime_error&) {
    }

    cell.variationSamples = 9;
    cell.monteCarloGranularity = "invalid";
    try {
        (void)VariationConfigBuilder::FromCell(cell);
        assert(false && "Expected invalid monte_carlo_granularity to throw");
    } catch (const std::runtime_error&) {
    }
    cell.monteCarloGranularity = "cell";

    cell.variationMode = "invalid_mode";
    cell.variationSamples = 9;
    try {
        (void)VariationConfigBuilder::FromCell(cell);
        assert(false && "Expected invalid variation mode to throw");
    } catch (const std::runtime_error&) {
    }

    cell.variationMode = "corner";
    cell.variationSamples = 1;
    try {
        (void)VariationConfigBuilder::FromCell(cell);
        assert(false && "Expected corner with no max-var fields to throw");
    } catch (const std::runtime_error&) {
    }

    cell.resistanceOnMaxVariation = 0.05;
    VariationConfig corner = VariationConfigBuilder::FromCell(cell);
    assert(corner.enabled == true);
    assert(corner.mode == "corner");
    assert(corner.samples == 2);
    assert(std::fabs(corner.memoryDeviceResOnMaxVar - 0.05) < 1e-18);

    cell.resistanceOnMaxVariation = 0.96;
    cell.resistanceOffMaxVariation = 1.0;
    try {
        (void)VariationConfigBuilder::FromCell(cell);
        assert(false && "Expected corner max-var >= 1 to throw");
    } catch (const std::runtime_error&) {
    }

    cell.withVariation = false;
    cell.variationMode = "monte_carlo";
    cell.variationSamples = 99;
    VariationConfig disabled = VariationConfigBuilder::FromCell(cell);
    assert(disabled.enabled == false);
    assert(disabled.mode == "nominal");
    assert(disabled.samples == 1);
}

int main() {
    test_basic_read();
    test_enum_read();
    test_bcam_alias();
    test_units();
    test_non_finite_numbers_are_rejected();
    test_memcell_yaml();
    test_organization_section();
    test_explicit_subarray_dimensions();
    test_invalid_inputs();
    test_memcell_variation_yaml();
    test_cell_variation_drives_runtime_config();
    test_variation_builder_rejects_invalid_mode_and_samples();
    std::cout << "YamlHelpers tests passed" << std::endl;
    return 0;
}
