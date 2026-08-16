#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "MemCell.h"

#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::StreamCapture;
using TestSupport::TemporaryDirectory;

MemCell MakeEnergyCell(MemCellType type) {
    MemCell cell;
    cell.memCellType = type;
    cell.vdd = 1.2;
    cell.resistanceOn = 100;
    cell.resistanceOff = 1000;
    cell.resistanceOnAtSetVoltage = 50;
    cell.resistanceOnAtResetVoltage = 40;
    cell.voltageDropAccessDevice = 0.2;
    cell.resetVoltage = -2.0;
    cell.resetCurrent = -3.0;
    cell.resetPulse = 5.0;
    cell.setVoltage = -3.0;
    cell.setCurrent = -4.0;
    cell.setPulse = 7.0;
    return cell;
}

void TestConstructorEstablishesDocumentedDefaults() {
    MemCell cell;
    assert(cell.memCellType == PCRAM);
    AssertNear(cell.area, 0);
    AssertNear(cell.resistanceOn, 0);
    AssertNear(cell.resistanceOff, 0);
    assert(cell.readMode);
    assert(cell.resetMode);
    assert(cell.setMode);
    AssertNear(cell.minSenseVoltage, 0.08);
    AssertNear(cell.wordlineBoostRatio, 1.0);
    assert(cell.accessType == CMOS_access);
    assert(!cell.isNVMdischarge);
    AssertNear(cell.gateOxThicknessFactor, 2.0);
    AssertNear(cell.widthSRAMCellNMOS, 2.08);
    AssertNear(cell.widthSRAMCellPMOS, 1.23);
    assert(!cell.withVariation);
    assert(cell.variationMode == "nominal");
    assert(cell.monteCarloGranularity == "cell");
    assert(cell.variationSamples == 1);
    assert(!cell.hasMcamStateVariations);
    assert(!cell.hasMcamPrechargeVoltages);
    assert(!cell.hasMcamSearchlineVoltages);
    for (int index = 0; index < 64; ++index) {
        AssertNear(cell.ResistanceState[index], 0);
        AssertNear(cell.mlPrechargeVoltage[index], 0);
        AssertNear(cell.searchlineVoltage[index], 0);
        AssertNear(cell.resStateVariation[index], 0);
    }
}

void TestCopyConstructionPreservesCellState() {
    MemCell original;
    original.memCellType = memristor;
    original.processNode = 32;
    original.resistanceOn = 125;
    original.readVoltage = 0.7;
    original.variationMode = "monte_carlo";
    original.ResistanceState[3] = 450;

    MemCell copy(original);
    assert(copy.memCellType == memristor);
    assert(copy.processNode == 32);
    AssertNear(copy.resistanceOn, 125);
    AssertNear(copy.readVoltage, 0.7);
    assert(copy.variationMode == "monte_carlo");
    AssertNear(copy.ResistanceState[3], 450);

    original.ResistanceState[3] = 900;
    AssertNear(copy.ResistanceState[3], 450);
}

void TestReadCellFromFileDelegatesToV2YamlLoader() {
    TemporaryDirectory directory("evacam-memcell");
    const auto deviceFile = directory.WriteFile("device.yaml", R"yaml(
schema: memory_device
name: unit-device
type: memristor
resistance:
  'on': 400ohm
  'off': 300kohm
read:
  mode: current
  current: 50uA
write:
  set: {mode: current, current: 150uA, pulse: 10ns}
  reset: {mode: current, current: 160uA, pulse: 11ns}
)yaml");
    const auto cellFile = directory.WriteFile("cell.yaml", R"yaml(
schema: cell
name: unit-cell
cam_type: TCAM
memory_device: device.yaml
access_device: {type: cmos, cmos_width: 12F, voltage_drop: 0.15V}
layout: {cell_process_node: 40nm, area: 157F^2, aspect_ratio: 2}
ports: {row: {}, column: {}}
)yaml");

    MemCell cell;
    cell.ReadCellFromFile(cellFile.string(), CAM_chip, 0.9);
    assert(cell.designTarget == CAM_chip);
    AssertNear(cell.vdd, 0.9);
    assert(cell.memCellType == memristor);
    assert(cell.camType == TCAM);
    assert(cell.accessType == CMOS_access);
    assert(cell.processNode == 40);
    AssertNear(cell.area, 157);
    AssertNear(cell.resistanceOn, 400);
    AssertNear(cell.resistanceOff, 300e3);
    assert(!cell.readMode);
    AssertNear(cell.readCurrent, 50e-6);
    AssertNear(cell.setPulse, 10e-9);
    AssertNear(cell.resetPulse, 11e-9);
    (void)deviceFile;
}

void TestReadCellFromFileRejectsUnsupportedAndInvalidInputs() {
    TemporaryDirectory directory("evacam-memcell-invalid");
    const auto textFile = directory.WriteFile("cell.txt", "not yaml");
    MemCell cell;
    AssertThrows<std::runtime_error>(
            [&] { cell.ReadCellFromFile(textFile.string(), CAM_chip, 1.1); },
            "Only YAML cell files are supported");
    assert(cell.designTarget == CAM_chip);
    AssertNear(cell.vdd, 1.1);

    const auto invalidYaml = directory.WriteFile("cell.yaml", "schema: cell\n");
    AssertThrows<std::runtime_error>(
            [&] { cell.ReadCellFromFile(invalidYaml.string(), CAM_chip, 1.0); },
            "layout");
}

void TestCalculateReadPowerCoversModesAndDisabledOperation() {
    MemCell cell;
    cell.vdd = 1.2;
    cell.readMode = true;
    cell.readVoltage = 0;
    cell.readCurrent = 2e-6;
    AssertNear(cell.CalculateReadPower(), 2.4e-6);

    cell.readVoltage = 0.6;
    cell.readCurrent = 0;
    cell.resistanceOn = 100;
    cell.resistanceOff = 900;
    cell.voltageDropAccessDevice = 0.1;
    AssertNear(cell.CalculateReadPower(), 1.2 * 0.5 / (100 + 300));

    cell.readMode = false;
    AssertNear(cell.CalculateReadPower(), 1.2 * 0.5 / 100);

    cell.readMode = true;
    cell.readVoltage = 0.6;
    cell.readCurrent = 1e-6;
    assert(cell.CalculateReadPower() == -1.0);
    cell.readPower = 1e-6;
    assert(cell.CalculateReadPower() == -1.0);
}

void TestCalculateWriteEnergyCoversTypesModesAndDisabledOperations() {
    {
        MemCell cell = MakeEnergyCell(memristor);
        cell.accessType = none_access;
        cell.CalculateWriteEnergy();
        AssertNear(cell.resetEnergy, 2 * 1.8 / 40 * 5);
        AssertNear(cell.setEnergy, 3 * 2.8 / 50 * 7);
    }
    {
        MemCell cell = MakeEnergyCell(memristor);
        cell.accessType = CMOS_access;
        cell.CalculateWriteEnergy();
        AssertNear(cell.resetEnergy, 2 * 1.8 / 100 * 5);
        AssertNear(cell.setEnergy, 3 * 2.8 / 100 * 7);
    }
    for (MemCellType type : {PCRAM, MRAM}) {
        MemCell cell = MakeEnergyCell(type);
        cell.CalculateWriteEnergy();
        AssertNear(cell.resetEnergy, 2 * 1.8 / 100 * 5);
        AssertNear(cell.setEnergy, 3 * 2.8 / 100 * 7);
    }
    for (MemCellType type : {FBRAM, FEFETRAM}) {
        MemCell cell = MakeEnergyCell(type);
        cell.CalculateWriteEnergy();
        AssertNear(cell.resetEnergy, 2 * 3 * 5);
        AssertNear(cell.setEnergy, 3 * 4 * 7);
    }
    {
        MemCell cell = MakeEnergyCell(MRAM);
        cell.resetMode = false;
        cell.setMode = false;
        cell.resetVoltage = 0;
        cell.CalculateWriteEnergy();
        AssertNear(cell.resetEnergy, 1.2 * 3 * 5);
        AssertNear(cell.setEnergy, 1.2 * 4 * 7);
    }
    {
        MemCell cell = MakeEnergyCell(MRAM);
        cell.resetMode = false;
        cell.setMode = false;
        cell.resetVoltage = 2;
        cell.CalculateWriteEnergy();
        AssertNear(cell.resetEnergy, 2 * 3 * 5);
        AssertNear(cell.setEnergy, 3 * 4 * 7);
    }
    {
        MemCell cell = MakeEnergyCell(MRAM);
        cell.resetEnergy = 17;
        cell.setEnergy = 19;
        cell.CalculateWriteEnergy();
        AssertNear(cell.resetEnergy, 17);
        AssertNear(cell.setEnergy, 19);
    }
}

void TestGetMemristanceInterpolatesEndpointsAndReportsInvalidCells() {
    MemCell cell;
    cell.memCellType = memristor;
    cell.readVoltage = 1;
    cell.resistanceOnAtReadVoltage = 100;
    cell.resistanceOnAtHalfReadVoltage = 200;
    AssertNear(cell.GetMemristance(1), 100);
    AssertNear(cell.GetMemristance(0.5), 200);
    AssertNear(cell.GetMemristance(0.75), 150);

    cell.memCellType = PCRAM;
    StreamCapture capture(std::cout);
    assert(cell.GetMemristance(1) == -1);
    capture.Stop();
    Require(capture.Text().find("non-memristor") != std::string::npos,
            "non-memristor warning was not printed");

    MemCell uninitializedMemristor;
    uninitializedMemristor.memCellType = memristor;
    assert(std::isnan(uninitializedMemristor.GetMemristance(1)));
}

void TestPrintCellReportsRepresentativeProperties() {
    MemCell cell;
    cell.memCellType = SRAM;
    cell.area = 6;
    cell.widthAccessCMOS = 3;
    StreamCapture capture(std::cout);
    cell.PrintCell();
    cell.memCellType = PCRAM;
    cell.resistanceOn = 2000;
    cell.resistanceOff = 2e6;
    cell.readMode = false;
    cell.accessType = diode_access;
    cell.PrintCell();
    cell.memCellType = SLCNAND;
    cell.flashProgramTime = 1e-6;
    cell.PrintCell();
    cell.memCellType = static_cast<MemCellType>(99);
    cell.PrintCell();
    capture.Stop();
    const std::string output = capture.Text();
    Require(output.find("Memory Cell: SRAM") != std::string::npos, "missing SRAM output");
    Require(output.find("SRAM Cell Access Transistor Width") != std::string::npos, "missing SRAM property");
    Require(output.find("Memory Cell: PCRAM") != std::string::npos, "missing PCRAM output");
    Require(output.find("Read Mode: Current-Sensing") != std::string::npos, "missing read mode");
    Require(output.find("Access Type: Diode") != std::string::npos, "missing access type");
    Require(output.find("Single-Level Cell NAND Flash") != std::string::npos, "missing NAND output");
    Require(output.find("Memory Cell: Unknown") != std::string::npos, "missing unknown output");
    Require(output.find("===========   For CAM") != std::string::npos, "missing CAM output");
}

}  // namespace

int main() {
    TestConstructorEstablishesDocumentedDefaults();
    TestCopyConstructionPreservesCellState();
    TestReadCellFromFileDelegatesToV2YamlLoader();
    TestReadCellFromFileRejectsUnsupportedAndInvalidInputs();
    TestCalculateReadPowerCoversModesAndDisabledOperation();
    TestCalculateWriteEnergyCoversTypesModesAndDisabledOperations();
    TestGetMemristanceInterpolatesEndpointsAndReportsInvalidCells();
    TestPrintCellReportsRepresentativeProperties();
    return 0;
}
