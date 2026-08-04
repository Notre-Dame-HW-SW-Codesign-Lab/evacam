#include "MemCell.h"
#include "input/CellYamlLoader.h"
#include "input/MemoryDeviceYamlLoader.h"

#include "TestSupport.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::TemporaryDirectory;

MemCell LoadCell(const std::filesystem::path& path) {
    MemCell cell;
    YamlHelpers::ReadMemCellFromYaml(cell, path.string());
    return cell;
}

MemCell LoadDevice(const std::filesystem::path& path) {
    MemCell cell;
    YamlHelpers::ReadMemoryDeviceFromYaml(cell, path.string());
    return cell;
}

std::string DeviceBody() {
    return
        "schema: evacam.memory_device.v2\n"
        "name: branch-device\n"
        "type: MRAM\n"
        "resistance:\n"
        "  on: 1kohm\n"
        "  off: 1Mohm\n"
        "  at_set: {on: 2kohm, off: 2Mohm}\n"
        "  at_reset: {on: 3kohm, off: 3Mohm}\n"
        "  at_read: {on: 4kohm, off: 4Mohm}\n"
        "  at_half_read: {on: 5kohm, off: 5Mohm}\n"
        "  at_half_reset: {on: 6kohm}\n"
        "capacitance: {on: 2fF, off: 3fF}\n"
        "device: {gate_ox_thickness_factor: 1.5, soi_width: 3F}\n"
        "read:\n"
        "  mode: CURRENT\n"
        "  voltage: 0.7V\n"
        "  current: 8uA\n"
        "  power: 9uW\n"
        "  energy: 10fJ\n"
        "  min_sense_voltage: 11mV\n"
        "  wordline_boost_ratio: 1.2\n"
        "  read_floating: true\n"
        "write:\n"
        "  set: {mode: current, voltage: -1V, current: 2uA, pulse: 3ns, energy: 4pJ}\n"
        "  reset: {mode: VOLTAGE, voltage: -5V, current: 6uA, pulse: 7ns, energy: 8pJ}\n"
        "sram: {nmos_width: 4F, pmos_width: 5F}\n"
        "flash: {erase_voltage: 9V, program_voltage: 10V, pass_voltage: 11V, erase_time: 12us, program_time: 13us, gate_coupling_ratio: 0.4}\n"
        "variation:\n"
        "  with_variation: false\n"
        "  seed: 42\n"
        "  mode: corner\n"
        "  monte_carlo_granularity: effective\n"
        "  lut_file: lut.csv\n"
        "  samples: 9\n"
        "  memory_device_resistance_on_stdev: ' 12 % '\n"
        "  memory_device_resistance_off_stdev: 0.13\n"
        "  memory_device_resistance_on_max_var: 14%\n"
        "  memory_device_resistance_off_max_var: 0.15\n"
        "mcam:\n"
        "  num_resistance_state: 3\n"
        "  resistance_state: {0: 10kohm, 2: 30kohm}\n"
        "  state_variation: {0: 5%, 2: 0.07}\n"
        "  ml_precharge_voltage: {0: 1V, 2: 800mV}\n"
        "  searchline_voltage: {0: 0.2V, 2: 0.4V}\n"
        "  center_voltage: 0.3V\n";
}

std::string V2Cell(const std::string& deviceRef, const std::string& extra = "") {
    return
        "schema: evacam.cell.v2\n"
        "name: MCAM branch cell\n"
        "cam_type: MCAM\n"
        "memory_device: " + deviceRef + "\n"
        "access_device: {type: cmos, cmos_width: 2F, voltage_drop: 30mV, leakage_current: 4nA}\n"
        "layout: {cell_process_node: 45nm, area: 20F^2, aspect_ratio: 2}\n"
        "ports:\n"
        "  row:\n"
        "    0: {type: searchline, cmos_region: gate, num_cmos: 2, cmos_width: 3F, is_nmos: true, leak: true, is_nvm_discharge: true, wire_width: 1F, voltages: {set_lrs: 1V, set_mrs: 2V, reset: 3V, search0: 4V, search1: 5V}}\n"
        "  column:\n"
        "    1: {type: matchline, cmos_region: drain, num_cmos: 1, cmos_width: 4F, is_nmos: false, wire_width: 2F}\n" + extra;
}

std::string ReplaceOnce(std::string text, const std::string& from, const std::string& to) {
    const size_t position = text.find(from);
    Require(position != std::string::npos, "fixture replacement target must exist");
    text.replace(position, from.size(), to);
    return text;
}

void TestReadMemCellFromYamlV2ResolvesRelativeDeviceAndParsesEverySection() {
    TemporaryDirectory temp("cell-loader-v2");
    const auto device = temp.WriteFile("nested/device.yaml", DeviceBody());
    const auto cellFile = temp.WriteFile("nested/cell.yaml", V2Cell("device.yaml"));

    const auto original = std::filesystem::current_path();
    std::filesystem::current_path(temp.Path().parent_path());
    MemCell cell = LoadCell(cellFile);
    std::filesystem::current_path(original);

    Require(cell.memCellType == MRAM, "v2 referenced device type");
    Require(cell.processNode == 45 && cell.camType == MCAM, "v2 layout and CAM type");
    AssertNear(cell.resistanceOn, 1e3); AssertNear(cell.resistanceOff, 1e6);
    AssertNear(cell.resistanceOnAtSetVoltage, 2e3); AssertNear(cell.resistanceOffAtHalfReadVoltage, 5e6);
    AssertNear(cell.resistanceOnAtHalfResetVoltage, 6e3);
    AssertNear(cell.capacitanceOn, 2e-15); AssertNear(cell.widthSOIDevice, 3);
    Require(!cell.readMode && !cell.setMode && cell.resetMode, "read/write modes");
    AssertNear(cell.readCurrent, 8e-6); AssertNear(cell.setPulse, 3e-9); AssertNear(cell.resetEnergy, 8e-12);
    Require(cell.readFloating && cell.withVariation == false && cell.hasVariationSeed && cell.hasVariationSamples,
            "optional read and variation fields");
    Require(cell.variationSeed == 42 && cell.variationSamples == 9, "variation seed and samples");
    AssertNear(cell.resistanceOnVariation, .12); AssertNear(cell.resistanceOffMaxVariation, .15);
    Require(cell.numResistanceState == 3 && cell.hasMcamSearchlineVoltages && cell.hasMcamCenterVoltage,
            "MCAM flags");
    AssertNear(cell.ResistanceState[2], 3e4); AssertNear(cell.resStateVariation[0], .05);
    AssertNear(cell.mlPrechargeVoltage[2], .8); AssertNear(cell.searchlineVoltage[2], .4);
    Require(cell.camNumRow == 1 && cell.camNumCol == 2, "sparse port indices preserve extent");
    Require(cell.camPort[0][0].ConnectedRegion == gate && cell.camPort[1][1].ConnectedRegion == drain,
            "v2 direct port connection");
    Require(cell.camPort[0][0].leak && cell.camPort[0][0].isNVMdischarge && !cell.camPort[1][1].isNMOS,
            "port flags");
    AssertNear(cell.camPort[0][0].volSearch1, 5); AssertNear(cell.widthAccessCMOS, 2);
}

void TestReadMemCellFromYamlRejectsUnsupportedLegacyCellShape() {
    TemporaryDirectory temp("cell-loader-legacy");
    const auto path = temp.WriteFile("legacy.yaml",
        "schema: legacy-cell\n"
        "cell: {name: acam, type: SRAM, cell_process_node: 32nm, area: 12F^2, aspect_ratio: 3}\n"
        "resistance: {true: 7kohm, false: 8Mohm}\n"
        "match: {cmos_width: 9F, is_nvm_discharge: true}\n"
        "ports:\n"
        "  row:\n"
        "    0: {type: searchline, connection: {kind: memory_terminal, terminal: SOURCE}, wire_width: 1F}\n"
        "  column:\n"
        "    0: {type: matchline, connection: {kind: access_terminal, terminal: Drain}, num_cmos: 2, is_nmos: false, cmos_width: 2F, wire_width: 1F}\n");
    AssertThrows<std::runtime_error>([&] { LoadCell(path); }, "cell config schema must be cell");
}

void TestReadMemoryDeviceFromYamlLegacyCellAndSections() {
    TemporaryDirectory temp("memory-loader");
    const auto path = temp.WriteFile("device.yaml",
        "type: PCRAM\n"
        "cell: {type: FEFETRAM, cell_process_node: 28nm, area: 16F^2, aspect_ratio: 4}\n"
        "resistance: {on: 2kohm, off: 3Mohm}\n"
        "read: {mode: voltage, voltage: 0.6V, current: 2uA, power: 3uW, energy: 4fJ, min_sense_voltage: 5mV}\n"
        "write: {set: {voltage: 1V, current: 2uA, pulse: 3ns, energy: 4pJ}, reset: {voltage: 5V, current: 6uA, pulse: 7ns, energy: 8pJ}}\n"
        "mcam: {center_voltage: 0.7V}\n");
    MemCell cell = LoadDevice(path);
    Require(cell.memCellType == FEFETRAM && cell.processNode == 28 && cell.readMode, "legacy memory device cell override");
    AssertNear(cell.area, 16); AssertNear(cell.readEnergy, 4e-15); AssertNear(cell.resetPulse, 7e-9);
    Require(cell.hasMcamCenterVoltage, "legacy memory device MCAM field"); AssertNear(cell.centerVoltage, .7);
}

void TestLoadersRejectSchemasKeysAndUnsupportedForms() {
    TemporaryDirectory temp("cell-loader-errors");
    const auto device = temp.WriteFile("device.yaml", DeviceBody());
    const auto v2 = temp.WriteFile("cell.yaml", V2Cell("device.yaml"));
    (void)device;
    AssertThrows<std::runtime_error>([&] { LoadCell(temp.WriteFile("bad-access.yaml",
            ReplaceOnce(V2Cell("device.yaml"),
                    "access_device: {type: cmos, cmos_width: 2F, voltage_drop: 30mV, leakage_current: 4nA}",
                    "access_device: device.yaml"))); }, "access_device must be an inline mapping");
    AssertThrows<std::runtime_error>([&] { LoadCell(temp.WriteFile("bad-top.yaml", V2Cell("device.yaml") + "read: {}\n")); }, "does not allow top-level read");
    AssertThrows<std::runtime_error>([&] { LoadCell(temp.WriteFile("bad-key.yaml", V2Cell("device.yaml") + "typo: 1\n")); }, "unknown key 'cell.typo'");
    AssertThrows<std::runtime_error>([&] { LoadCell(temp.WriteFile("bad-port.yaml",
            ReplaceOnce(V2Cell("device.yaml"), "cmos_region: gate",
                    "connection: {kind: memory_terminal, terminal: gate}"))); }, "connection is not allowed");
    AssertThrows<std::runtime_error>([&] { LoadCell(temp.WriteFile("bad-dram.yaml", V2Cell("device.yaml") + "dram: {}\n")); }, "dram is not supported");
    AssertThrows<std::runtime_error>([&] { LoadDevice(temp.WriteFile("bad-device.yaml", "type: SRAM\nunknown: 1\n")); }, "unknown key 'memory_device.unknown'");
    temp.WriteFile("bad-device.yaml", ReplaceOnce(DeviceBody(),
            "memory_device_resistance_on_stdev: ' 12 % '",
            "memory_device_resistance_on_stdev: ' '") );
    AssertThrows<std::runtime_error>([&] { LoadCell(temp.WriteFile("bad-variation.yaml",
            V2Cell("bad-device.yaml"))); }, "Empty value for variation.memory_device_resistance_on_stdev");
    (void)v2;
}

}  // namespace

int main() {
    TestReadMemCellFromYamlV2ResolvesRelativeDeviceAndParsesEverySection();
    TestReadMemCellFromYamlRejectsUnsupportedLegacyCellShape();
    TestReadMemoryDeviceFromYamlLegacyCellAndSections();
    TestLoadersRejectSchemasKeysAndUnsupportedForms();
    std::cout << "Cell and memory loader branch tests passed\n";
    return 0;
}
