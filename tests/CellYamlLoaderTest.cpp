#include "MemCell.h"
#include "input/MemoryDeviceYamlLoader.h"

#include <yaml.h>

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

const char *kCellPath = "tests/tmp_cell_loader_cell_config.yaml";
const char *kMemoryDevicePath = "tests/tmp_cell_loader.memory_device.yaml";
const char *kMissingFieldPath = "tests/tmp_cell_loader_missing.yaml";

void WriteMinimalCellFile(const char *path, const std::string &extra = "", int numCmos = 1) {
    {
        std::ofstream out(kMemoryDevicePath);
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
        if (!extra.empty()) {
            out << extra;
        }
    }

    std::ofstream out(path);
    out <<
        "schema: cell\n"
        "name: TestCell\n"
        "cam_type: TCAM\n"
        "memory_device: ./tmp_cell_loader.memory_device.yaml\n"
        "access_device:\n"
        "  type: cmos\n"
        "  cmos_width: 2F\n"
        "  voltage_drop: 100mV\n"
        "  leakage_current: 5nA\n"
        "layout:\n"
        "  cell_process_node: 45nm\n"
        "  area: 300F^2\n"
        "  aspect_ratio: 2.0\n"
        "ports:\n"
        "  row:\n"
        "    0:\n"
        "      type: searchline\n"
        "      cmos_region: gate\n"
        "      num_cmos: " << numCmos << "\n"
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
        "      num_cmos: " << numCmos << "\n"
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

bool LoadCellThrows(const char *path) {
    try {
        MemCell cell;
        cell.ReadCellFromFile(path, CAM_chip, 1.0);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool LoadCellThrowsWithMessage(const char *path, const std::string& expected) {
    try {
        MemCell cell;
        cell.ReadCellFromFile(path, CAM_chip, 1.0);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

void TestMinimalCellParses() {
    WriteMinimalCellFile(kCellPath);

    MemCell cell;
    cell.ReadCellFromFile(kCellPath, CAM_chip, 1.0);

    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-18; };
    assert(cell.memCellType == SRAM);
    assert(cell.processNode == 45);
    assert(cell.accessType == CMOS_access);
    assert(near(cell.widthAccessCMOS, 2.0));
    assert(near(cell.voltageDropAccessDevice, 0.1));
    assert(near(cell.leakageCurrentAccessDevice, 5e-9));
    assert(near(cell.area, 300.0));
    assert(near(cell.aspectRatio, 2.0));
    assert(near(cell.resistanceOn, 1e3));
    assert(near(cell.resistanceOff, 1e6));
    assert(cell.readMode == true);
    assert(near(cell.readVoltage, 1.0));
    assert(near(cell.minSenseVoltage, 0.07));
    assert(cell.resetMode == false);
    assert(cell.camNumRow == 1);
    assert(cell.camNumCol == 1);
}

void TestVariationDefaultsWhenOmitted() {
    WriteMinimalCellFile(kCellPath);

    MemCell cell;
    cell.ReadCellFromFile(kCellPath, CAM_chip, 1.0);

    assert(cell.withVariation == false);
    assert(cell.hasVariationSeed == false);
    assert(cell.hasVariationSamples == false);
    assert(cell.variationSeed == 0u);
    assert(cell.variationMode == "nominal");
    assert(cell.variationLutFile.empty());
    assert(cell.variationSamples == 1);
    assert(cell.resistanceOnVariation == 0.0);
    assert(cell.resistanceOffVariation == 0.0);
    assert(cell.resistanceOnMaxVariation == 0.0);
    assert(cell.resistanceOffMaxVariation == 0.0);
    assert(cell.hasMcamSearchlineVoltages == false);
    assert(cell.hasMcamCenterVoltage == false);
}

void TestVariationSectionParses() {
    WriteMinimalCellFile(
        kCellPath,
        "variation:\n"
        "  seed: 12345\n"
        "  mode: monte_carlo\n"
        "  lut_file: variation_lut.csv\n"
        "  samples: 7\n"
        "  memory_device_resistance_on_stdev: 5%\n"
        "  memory_device_resistance_off_stdev: 7%\n"
        "  memory_device_resistance_on_max_var: 11%\n"
        "  memory_device_resistance_off_max_var: 13%\n");

    MemCell cell;
    cell.ReadCellFromFile(kCellPath, CAM_chip, 1.0);

    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-18; };
    assert(cell.withVariation == true);
    assert(cell.hasVariationSeed == true);
    assert(cell.hasVariationSamples == true);
    assert(cell.variationSeed == 12345u);
    assert(cell.variationMode == "monte_carlo");
    assert(cell.variationLutFile == "variation_lut.csv");
    assert(cell.variationSamples == 7);
    assert(near(cell.resistanceOnVariation, 0.05));
    assert(near(cell.resistanceOffVariation, 0.07));
    assert(near(cell.resistanceOnMaxVariation, 0.11));
    assert(near(cell.resistanceOffMaxVariation, 0.13));
}

void TestMcamVoltagesParse() {
    WriteMinimalCellFile(
        kCellPath,
        "mcam:\n"
        "  num_resistance_state: 4\n"
        "  resistance_state: [1Mohm, 500kohm, 100kohm, 50kohm]\n"
        "  ml_precharge_voltage:\n"
        "    0: 1V\n"
        "    1: 900mV\n"
        "    2: 0.75V\n"
        "    3: 600mV\n"
        "  searchline_voltage: [0.4V, 500mV, 0.6V, 700mV]\n"
        "  center_voltage: 840mV\n");

    MemCell cell;
    cell.ReadCellFromFile(kCellPath, CAM_chip, 1.0);

    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-12; };
    assert(near(cell.mlPrechargeVoltage[0], 1.0));
    assert(near(cell.mlPrechargeVoltage[1], 0.9));
    assert(near(cell.mlPrechargeVoltage[2], 0.75));
    assert(near(cell.mlPrechargeVoltage[3], 0.6));
    assert(near(cell.searchlineVoltage[0], 0.4));
    assert(near(cell.searchlineVoltage[1], 0.5));
    assert(near(cell.searchlineVoltage[2], 0.6));
    assert(near(cell.searchlineVoltage[3], 0.7));
    assert(near(cell.centerVoltage, 0.84));
    assert(cell.hasMcamSearchlineVoltages == true);
    assert(cell.hasMcamCenterVoltage == true);

    WriteMinimalCellFile(
        kCellPath,
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500kohm]\n"
        "  center_voltage: 1.25V\n");

    MemCell cellWithVolts;
    cellWithVolts.ReadCellFromFile(kCellPath, CAM_chip, 1.0);
    assert(near(cellWithVolts.centerVoltage, 1.25));

    WriteMinimalCellFile(
        kCellPath,
        "mcam:\n"
        "  num_resistance_state: 2\n"
        "  resistance_state: [1Mohm, 500kohm]\n"
        "  center_voltage: 840uV\n");
    assert(LoadCellThrows(kCellPath));
}

void TestMissingRequiredCellFieldThrows() {
    {
        std::ofstream out(kMissingFieldPath);
        out <<
            "schema: cell\n"
            "name: TestCell\n"
            "cam_type: TCAM\n"
            "memory_device: ./tmp_cell_loader.memory_device.yaml\n"
            "layout:\n"
            "  cell_process_node: 45nm\n"
            "  area: 300F^2\n";
    }

    assert(LoadCellThrows(kMissingFieldPath));
}

void TestElectricalPortRequiresPositiveCmosCount() {
    WriteMinimalCellFile(kCellPath, "", 0);
    assert(LoadCellThrows(kCellPath));
}

void TestUnknownCellKeyThrows() {
    WriteMinimalCellFile(kCellPath);
    std::ofstream out(kCellPath, std::ios::app);
    out << "layuot: {}\n";
    out.close();

    assert(LoadCellThrowsWithMessage(kCellPath, "unknown key 'cell.layuot'"));
}

void TestUnknownMemoryDeviceKeyThrows() {
    WriteMinimalCellFile(kCellPath, "min_sense_voltge: 70mV\n");
    assert(LoadCellThrowsWithMessage(
            kCellPath, "unknown key 'memory_device.min_sense_voltge'"));
}

void TestLegacyMemoryDeviceLoaderStillParses() {
    MemCell cell;
    YamlHelpers::ReadMemoryDeviceFromYaml(
            cell, "config/2FeFET_TCAM_var/2FeFET_TCAM_var.memory_device.yaml");
    assert(cell.memCellType == FEFETRAM);
    assert(cell.processNode == 45);
    assert(std::fabs(cell.resistanceOn - 10000.0) < 1e-12);
}

void WriteYaml(const char* path, const YAML::Node& root) {
    std::ofstream output(path);
    output << root;
}

void TestPhysicalDomainsThrowDescriptiveErrors() {
    WriteMinimalCellFile(kCellPath);
    YAML::Node device = YAML::LoadFile(kMemoryDevicePath);
    device["type"] = "MRAM";
    device["resistance"]["on"] = "0ohm";
    WriteYaml(kMemoryDevicePath, device);
    assert(LoadCellThrowsWithMessage(
            kCellPath, "memory_device.resistance.on must be positive; got 0"));

    WriteMinimalCellFile(kCellPath);
    device = YAML::LoadFile(kMemoryDevicePath);
    device["capacitance"]["on"] = "-1fF";
    WriteYaml(kMemoryDevicePath, device);
    assert(LoadCellThrowsWithMessage(
            kCellPath, "memory_device.capacitance.on must be non-negative"));

    WriteMinimalCellFile(kCellPath);
    device = YAML::LoadFile(kMemoryDevicePath);
    device["write"]["set"]["pulse"] = "-1ns";
    WriteYaml(kMemoryDevicePath, device);
    assert(LoadCellThrowsWithMessage(
            kCellPath, "memory_device.write.set.pulse must be non-negative"));

    WriteMinimalCellFile(kCellPath);
    YAML::Node cell = YAML::LoadFile(kCellPath);
    cell["layout"]["area"] = "0F^2";
    WriteYaml(kCellPath, cell);
    assert(LoadCellThrowsWithMessage(kCellPath, "cell.layout.area must be positive"));
}

void TestSignedProgrammingPolarityIsAccepted() {
    WriteMinimalCellFile(kCellPath);
    YAML::Node device = YAML::LoadFile(kMemoryDevicePath);
    device["type"] = "MRAM";
    device["write"]["set"]["voltage"] = "-4V";
    device["write"]["reset"]["current"] = "-2uA";
    WriteYaml(kMemoryDevicePath, device);

    MemCell cell;
    cell.ReadCellFromFile(kCellPath, CAM_chip, 1.0);
    assert(cell.setVoltage == -4.0);
    assert(cell.resetCurrent == -2e-6);
}

}  // namespace

int main() {
    TestMinimalCellParses();
    TestVariationDefaultsWhenOmitted();
    TestVariationSectionParses();
    TestMcamVoltagesParse();
    TestMissingRequiredCellFieldThrows();
    TestElectricalPortRequiresPositiveCmosCount();
    TestUnknownCellKeyThrows();
    TestUnknownMemoryDeviceKeyThrows();
    TestLegacyMemoryDeviceLoaderStillParses();
    TestPhysicalDomainsThrowDescriptiveErrors();
    TestSignedProgrammingPolarityIsAccepted();
    std::cout << "CellYamlLoader tests passed" << std::endl;
    return 0;
}
