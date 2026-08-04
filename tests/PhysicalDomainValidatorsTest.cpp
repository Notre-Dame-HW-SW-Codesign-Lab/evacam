#include "input/PhysicalDomainValidators.h"

#include <cassert>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "MemCell.h"
#include "Technology.h"
#include "TechnologySpec.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertThrows;

// Private-helper mapping: ValidateOptionalPositive is exercised by the optional
// cell fields below; ValidateCurrentArray is exercised by every current table.
MemCell MakeValidCell() {
    MemCell cell;
    cell.memCellType = SRAM;
    cell.processNode = 90;
    cell.area = 120;
    cell.aspectRatio = 1;
    cell.widthInFeatureSize = 10;
    cell.heightInFeatureSize = 12;
    cell.resistanceOn = 1e4;
    cell.resistanceOff = 1e7;
    cell.capacitanceOn = 1e-15;
    cell.capacitanceOff = 1e-15;
    cell.readVoltage = 0.5;
    cell.readCurrent = 50e-6;
    cell.minSenseVoltage = 0.08;
    cell.wordlineBoostRatio = 1.0;
    cell.readPower = 0;
    cell.setEnergy = 1e-12;
    cell.resetEnergy = 1e-12;
    cell.gateOxThicknessFactor = 2;
    cell.widthSRAMCellNMOS = 2;
    cell.widthSRAMCellPMOS = 1;
    return cell;
}

MemCell MakeValidNvmCell() {
    MemCell cell = MakeValidCell();
    cell.memCellType = memristor;
    cell.resistanceOn = 1;
    cell.resistanceOff = 2;
    cell.setPulse = 1e-9;
    cell.resetPulse = 1e-9;
    cell.setVoltage = -1;
    cell.resetCurrent = -1e-6;
    return cell;
}

Technology MakeValidTechnology(bool finFet = false) {
    TechnologySpec spec = TestModelBuilders::MakeTechnologySpec();
    if (finFet) {
        spec.heightFin = 30e-9;
        spec.widthFin = 8e-9;
        spec.PitchFin = 20e-9;
    }
    Technology technology;
    technology.InitializeFromSpec(spec);
    return technology;
}

void ExpectCellFailure(const MemCell &cell, const std::string &path) {
    AssertThrows<std::runtime_error>(
            [&] { PhysicalDomainValidators::ValidateMemCell(cell); }, path);
}

void TestValidateMemCellAcceptsExplicitSramBoundaries() {
    MemCell cell = MakeValidCell();
    cell.resistanceOn = 0;
    cell.resistanceOff = 0;
    cell.capacitanceOn = 0;
    cell.capacitanceOff = 0;
    cell.readVoltage = 0;
    cell.readCurrent = 0;
    cell.readPower = 0;
    cell.readEnergy = 0;
    cell.setVoltage = -1.0;  // Programming polarity is intentionally signed.
    cell.setCurrent = -1e-6;
    cell.resetVoltage = 1.0;
    cell.resetCurrent = -1e-6;
    cell.setPulse = 0;
    cell.resetPulse = 0;
    cell.setEnergy = 0;
    cell.resetEnergy = 0;
    cell.widthAccessCMOS = 0;
    cell.widthSOIDevice = 0;
    cell.voltageDropAccessDevice = 0;
    cell.leakageCurrentAccessDevice = 0;
    cell.flashEraseTime = 0;
    cell.flashProgramTime = 0;
    cell.gateCouplingRatio = 0;
    PhysicalDomainValidators::ValidateMemCell(cell);
}

void TestValidateMemCellChecksLayoutAndCommonFields() {
    struct Case { const char *path; double MemCell::*field; bool positive; };
    const Case cases[] = {
        {"cell.layout.area", &MemCell::area, true},
        {"cell.layout.aspect_ratio", &MemCell::aspectRatio, true},
        {"cell.layout derived height", &MemCell::heightInFeatureSize, true},
        {"cell.layout derived width", &MemCell::widthInFeatureSize, true},
        {"memory_device.capacitance.on", &MemCell::capacitanceOn, false},
        {"memory_device.capacitance.off", &MemCell::capacitanceOff, false},
        {"memory_device.read.voltage", &MemCell::readVoltage, false},
        {"memory_device.read.current", &MemCell::readCurrent, false},
        {"memory_device.read.power", &MemCell::readPower, false},
        {"memory_device.read.energy", &MemCell::readEnergy, false},
        {"memory_device.read.min_sense_voltage", &MemCell::minSenseVoltage, true},
        {"memory_device.read.wordline_boost_ratio", &MemCell::wordlineBoostRatio, true},
        {"memory_device.write.set.energy", &MemCell::setEnergy, false},
        {"memory_device.write.reset.energy", &MemCell::resetEnergy, false},
        {"cell.access_device.voltage_drop", &MemCell::voltageDropAccessDevice, false},
        {"cell.access_device.leakage_current", &MemCell::leakageCurrentAccessDevice, false},
        {"memory_device.sram.nmos_width", &MemCell::widthSRAMCellNMOS, true},
        {"memory_device.sram.pmos_width", &MemCell::widthSRAMCellPMOS, true},
        {"memory_device.device.gate_ox_thickness_factor", &MemCell::gateOxThicknessFactor, true},
        {"memory_device.variation.memory_device_resistance_on_stdev", &MemCell::resistanceOnVariation, false},
        {"memory_device.variation.memory_device_resistance_off_stdev", &MemCell::resistanceOffVariation, false},
        {"memory_device.variation.memory_device_resistance_on_max_var", &MemCell::resistanceOnMaxVariation, false},
        {"memory_device.variation.memory_device_resistance_off_max_var", &MemCell::resistanceOffMaxVariation, false},
    };
    MemCell processNode = MakeValidCell();
    processNode.processNode = 0;
    ExpectCellFailure(processNode, "cell.layout.cell_process_node");
    for (const Case &test : cases) {
        MemCell zero = MakeValidCell();
        zero.*(test.field) = 0;
        if (test.positive) ExpectCellFailure(zero, test.path);
        else PhysicalDomainValidators::ValidateMemCell(zero);

        MemCell negative = MakeValidCell();
        negative.*(test.field) = -1;
        ExpectCellFailure(negative, test.path);
        MemCell nonfinite = MakeValidCell();
        nonfinite.*(test.field) = std::numeric_limits<double>::infinity();
        ExpectCellFailure(nonfinite, test.path);
    }
}

void TestValidateMemCellChecksNvmResistanceProgrammingAndRelations() {
    MemCell valid = MakeValidNvmCell();
    PhysicalDomainValidators::ValidateMemCell(valid);

    struct Case { const char *path; double MemCell::*field; };
    const Case positive[] = {
        {"memory_device.resistance.on", &MemCell::resistanceOn},
        {"memory_device.resistance.off", &MemCell::resistanceOff},
        {"memory_device.write.set.pulse", &MemCell::setPulse},
        {"memory_device.write.reset.pulse", &MemCell::resetPulse},
    };
    for (const Case &test : positive) {
        MemCell zero = MakeValidNvmCell(); zero.*(test.field) = 0; ExpectCellFailure(zero, test.path);
        MemCell nonfinite = MakeValidNvmCell(); nonfinite.*(test.field) = std::numeric_limits<double>::quiet_NaN();
        ExpectCellFailure(nonfinite, test.path);
    }
    MemCell inverted = MakeValidNvmCell();
    inverted.resistanceOff = inverted.resistanceOn / 2;
    ExpectCellFailure(inverted, "resistance.off must be greater");
    for (double MemCell::*field : {&MemCell::setVoltage, &MemCell::setCurrent,
                 &MemCell::resetVoltage, &MemCell::resetCurrent}) {
        MemCell nonfinite = MakeValidNvmCell();
        nonfinite.*field = std::numeric_limits<double>::quiet_NaN();
        ExpectCellFailure(nonfinite, "memory_device.write");
    }
    MemCell noSetOperation = MakeValidNvmCell();
    noSetOperation.setEnergy = noSetOperation.setVoltage = noSetOperation.setCurrent = 0;
    ExpectCellFailure(noSetOperation, "write.set must provide");
    MemCell noResetOperation = MakeValidNvmCell();
    noResetOperation.resetEnergy = noResetOperation.resetVoltage = noResetOperation.resetCurrent = 0;
    ExpectCellFailure(noResetOperation, "write.reset must provide");
}

void TestValidateMemCellChecksOptionalResistanceAndFlashFields() {
    const std::pair<const char *, double MemCell::*> optional[] = {
        {"cell.access_device.cmos_width", &MemCell::widthAccessCMOS},
        {"memory_device.device.soi_width", &MemCell::widthSOIDevice},
        {"memory_device.resistance.at_set.on", &MemCell::resistanceOnAtSetVoltage},
        {"memory_device.resistance.at_set.off", &MemCell::resistanceOffAtSetVoltage},
        {"memory_device.resistance.at_reset.on", &MemCell::resistanceOnAtResetVoltage},
        {"memory_device.resistance.at_reset.off", &MemCell::resistanceOffAtResetVoltage},
        {"memory_device.resistance.at_read.on", &MemCell::resistanceOnAtReadVoltage},
        {"memory_device.resistance.at_read.off", &MemCell::resistanceOffAtReadVoltage},
        {"memory_device.resistance.at_half_read.on", &MemCell::resistanceOnAtHalfReadVoltage},
        {"memory_device.resistance.at_half_read.off", &MemCell::resistanceOffAtHalfReadVoltage},
        {"memory_device.resistance.at_half_reset.on", &MemCell::resistanceOnAtHalfResetVoltage},
        {"memory_device.flash.erase_time", &MemCell::flashEraseTime},
        {"memory_device.flash.program_time", &MemCell::flashProgramTime},
    };
    for (const auto &test : optional) {
        MemCell accepted = MakeValidCell(); accepted.*(test.second) = 0;
        PhysicalDomainValidators::ValidateMemCell(accepted);
        MemCell rejected = MakeValidCell(); rejected.*(test.second) = -1;
        ExpectCellFailure(rejected, test.first);
        MemCell nonfinite = MakeValidCell(); nonfinite.*(test.second) = std::numeric_limits<double>::infinity();
        ExpectCellFailure(nonfinite, test.first);
    }
    for (double value : {-0.1, 1.1, std::numeric_limits<double>::quiet_NaN()}) {
        MemCell cell = MakeValidCell(); cell.gateCouplingRatio = value;
        ExpectCellFailure(cell, "memory_device.flash.gate_coupling_ratio");
    }
    MemCell boundary = MakeValidCell(); boundary.gateCouplingRatio = 1;
    PhysicalDomainValidators::ValidateMemCell(boundary);
    MemCell samples = MakeValidCell(); samples.hasVariationSamples = true; samples.variationSamples = 0;
    ExpectCellFailure(samples, "memory_device.variation.samples");
}

void TestValidateTechnologyAcceptsPlanarAndFinFetFixtures() {
    PhysicalDomainValidators::ValidateTechnology(MakeValidTechnology());
    PhysicalDomainValidators::ValidateTechnology(MakeValidTechnology(true));
}

void TestValidateTechnologyChecksInitializationFieldsAndUpdatedThreshold() {
    Technology uninitialized;
    AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateTechnology(uninitialized); },
            "must be initialized");
    struct Case { const char *path; double TechnologySpec::*field; bool positive; };
    const Case cases[] = {
        {"derived technology feature size", &TechnologySpec::featureSize, true},
        {"derived technology vdd", &TechnologySpec::vdd, true},
        {"derived technology vth", &TechnologySpec::vth, false},
        {"derived technology physical gate length", &TechnologySpec::phyGateLength, true},
        {"derived technology ideal gate capacitance", &TechnologySpec::capIdealGate, false},
        {"derived technology fringe capacitance", &TechnologySpec::capFringe, false},
        {"derived technology junction capacitance", &TechnologySpec::capJunction, false},
        {"derived technology oxide capacitance", &TechnologySpec::capOx, false},
        {"derived technology electron mobility", &TechnologySpec::effectiveElectronMobility, false},
        {"derived technology hole mobility", &TechnologySpec::effectiveHoleMobility, false},
        {"derived technology pn size ratio", &TechnologySpec::pnSizeRatio, true},
        {"derived technology effective resistance multiplier", &TechnologySpec::effectiveResistanceMultiplier, true},
    };
    for (const Case &test : cases) {
        TechnologySpec spec = TestModelBuilders::MakeTechnologySpec(); spec.*(test.field) = 0;
        Technology zero; zero.InitializeFromSpec(spec);
        if (test.positive) AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateTechnology(zero); }, test.path);
        else PhysicalDomainValidators::ValidateTechnology(zero);
        spec = TestModelBuilders::MakeTechnologySpec(); spec.*(test.field) = -1;
        Technology negative; negative.InitializeFromSpec(spec);
        AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateTechnology(negative); }, test.path);
        spec = TestModelBuilders::MakeTechnologySpec(); spec.*(test.field) = std::numeric_limits<double>::infinity();
        Technology nonfinite; nonfinite.InitializeFromSpec(spec);
        AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateTechnology(nonfinite); }, test.path);
    }
    TechnologySpec updated = TestModelBuilders::MakeTechnologySpec();
    updated.useUpdatedLib = true; updated.vth = 0.7 * updated.vdd;
    Technology technology; technology.InitializeFromSpec(updated);
    AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateTechnology(technology); }, "less than 0.7");
}

void TestValidateTechnologyChecksEveryCurrentTable() {
    struct Case { const char *path; std::array<double, 11> TechnologySpec::*table; };
    const Case cases[] = {
        {"derived technology on_nmos current", &TechnologySpec::currentOnNmos},
        {"derived technology on_pmos current", &TechnologySpec::currentOnPmos},
        {"derived technology off_nmos current", &TechnologySpec::currentOffNmos},
        {"derived technology off_pmos current", &TechnologySpec::currentOffPmos},
    };
    for (const Case &test : cases) {
        for (std::size_t anchor : {std::size_t{0}, std::size_t{5}, std::size_t{10}}) {
            TechnologySpec spec = TestModelBuilders::MakeTechnologySpec(); (spec.*(test.table))[anchor] = 0;
            Technology zero; zero.InitializeFromSpec(spec);
            AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateTechnology(zero); }, test.path);
            spec = TestModelBuilders::MakeTechnologySpec(); (spec.*(test.table))[anchor] = std::numeric_limits<double>::infinity();
            Technology nonfinite; nonfinite.InitializeFromSpec(spec);
            AssertThrows<std::runtime_error>([&] { PhysicalDomainValidators::ValidateTechnology(nonfinite); }, test.path);
        }
    }
}

}  // namespace

int main() {
    TestValidateMemCellAcceptsExplicitSramBoundaries();
    TestValidateMemCellChecksLayoutAndCommonFields();
    TestValidateMemCellChecksNvmResistanceProgrammingAndRelations();
    TestValidateMemCellChecksOptionalResistanceAndFlashFields();
    TestValidateTechnologyAcceptsPlanarAndFinFetFixtures();
    TestValidateTechnologyChecksInitializationFieldsAndUpdatedThreshold();
    TestValidateTechnologyChecksEveryCurrentTable();
    return 0;
}
