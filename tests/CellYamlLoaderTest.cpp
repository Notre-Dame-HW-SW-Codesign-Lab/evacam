#include "MemCell.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

const char *kCellPath = "tests/tmp_cell_loader_cell_config.yaml";
const char *kMissingFieldPath = "tests/tmp_cell_loader_missing.yaml";

void WriteMinimalCellFile(const char *path, const std::string &extra = "") {
    std::ofstream out(path);
    out <<
        "cell:\n"
        "  name: TestCell\n"
        "  type: SRAM\n"
        "  cam_type: TCAM\n"
        "  cell_process_node: 45nm\n"
        "  area: 300F^2\n"
        "  aspect_ratio: 2.0\n"
        "access_device:\n"
        "  type: CMOS\n"
        "  cmos_width: 2F\n"
        "  voltage_drop: 150mV\n"
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
    if (!extra.empty()) {
        out << extra;
    }
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

void TestMinimalCellParses() {
    WriteMinimalCellFile(kCellPath);

    MemCell cell;
    cell.ReadCellFromFile(kCellPath, CAM_chip, 1.0);

    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-18; };
    assert(cell.memCellType == SRAM);
    assert(cell.processNode == 45);
    assert(near(cell.area, 300.0));
    assert(near(cell.aspectRatio, 2.0));
    assert(near(cell.widthAccessCMOS, 2.0));
    assert(near(cell.voltageDropAccessDevice, 0.15));
    assert(near(cell.resistanceOn, 1e3));
    assert(near(cell.resistanceOff, 1e6));
    assert(cell.readMode == true);
    assert(near(cell.readVoltage, 1.0));
    assert(near(cell.minSenseVoltage, 0.07));
    assert(cell.resetMode == false);
    assert(near(cell.camWidthMatchTran, 3.0));
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
    assert(cell.matchlineWireResistanceVariation == 0.0);
    assert(cell.deviceAccessResistanceVariation == 0.0);
    assert(cell.deviceMatchResistanceVariation == 0.0);
    assert(cell.resistanceOnMaxVariation == 0.0);
    assert(cell.resistanceOffMaxVariation == 0.0);
    assert(cell.matchlineWireResistanceMaxVariation == 0.0);
    assert(cell.deviceAccessResistanceMaxVariation == 0.0);
    assert(cell.deviceMatchResistanceMaxVariation == 0.0);
}

void TestVariationSectionParses() {
    WriteMinimalCellFile(
        kCellPath,
        "variation:\n"
        "  with_variation: true\n"
        "  seed: 12345\n"
        "  mode: monte_carlo\n"
        "  lut_file: variation_lut.csv\n"
        "  samples: 7\n"
        "  memory_device_resistance_on_stdev: 5%\n"
        "  memory_device_resistance_off_stdev: 7%\n"
        "  matchline_wire_resistance_stdev: 3%\n"
        "  device_access_resistance_stdev: 2%\n"
        "  device_match_resistance_stdev: 4%\n"
        "  memory_device_resistance_on_max_var: 11%\n"
        "  memory_device_resistance_off_max_var: 13%\n"
        "  matchline_wire_resistance_max_var: 17%\n"
        "  device_access_resistance_max_var: 19%\n"
        "  device_match_resistance_max_var: 23%\n");

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
    assert(near(cell.matchlineWireResistanceVariation, 0.03));
    assert(near(cell.deviceAccessResistanceVariation, 0.02));
    assert(near(cell.deviceMatchResistanceVariation, 0.04));
    assert(near(cell.resistanceOnMaxVariation, 0.11));
    assert(near(cell.resistanceOffMaxVariation, 0.13));
    assert(near(cell.matchlineWireResistanceMaxVariation, 0.17));
    assert(near(cell.deviceAccessResistanceMaxVariation, 0.19));
    assert(near(cell.deviceMatchResistanceMaxVariation, 0.23));
}

void TestMcamMlPrechargeVoltageParses() {
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
        "  searchline_voltage: [0.4V, 500mV, 0.6V, 700mV]\n");

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
}

void TestMissingRequiredCellFieldThrows() {
    {
        std::ofstream out(kMissingFieldPath);
        out <<
            "cell:\n"
            "  name: TestCell\n"
            "  type: SRAM\n"
            "  cell_process_node: 45nm\n"
            "  area: 300F^2\n";
    }

    assert(LoadCellThrows(kMissingFieldPath));
}

}  // namespace

int main() {
    TestMinimalCellParses();
    TestVariationDefaultsWhenOmitted();
    TestVariationSectionParses();
    TestMcamMlPrechargeVoltageParses();
    TestMissingRequiredCellFieldThrows();
    std::cout << "CellYamlLoader tests passed" << std::endl;
    return 0;
}
