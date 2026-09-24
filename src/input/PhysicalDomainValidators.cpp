#include "input/PhysicalDomainValidators.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#include "MemCell.h"
#include "Technology.h"
#include "input/YamlNodeHelpers.h"

namespace {

void ValidateOptionalPositive(double value, const char* what) {
    YamlHelpers::require_finite(value, what);
    if (value != 0) {
        YamlHelpers::require_positive(value, what);
    }
}

void ValidateCurrentArray(const std::array<double, 101>& values, const char* what) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        YamlHelpers::require_positive(
                values[i], std::string(what) + "[" + std::to_string(i) + "]");
    }
}

void ValidateNandTopology(const MemCell& cell) {
    if (cell.camType != TCAM) {
        throw std::runtime_error("NAND string topology requires cam_type: TCAM");
    }
    if (cell.camNumRow != 0 || cell.camNumCol != 0 || cell.accessType != none_access) {
        throw std::runtime_error("NAND string topology does not use generic CAM ports or access devices");
    }
    if (cell.withVariation || cell.hasVariationSeed || cell.hasVariationSamples
            || cell.variationMode != "nominal" || !cell.variationLutFile.empty()) {
        throw std::runtime_error("NAND string variation is not supported");
    }
}

void ValidateNandElectrical(const NandDeviceSpec& spec, const std::string& prefix, const char* model) {
    if (spec.model != model) {
        throw std::runtime_error(prefix + ".model must be " + model);
    }
    if (spec.calibrationStatus != "synthetic" && spec.calibrationStatus != "uncalibrated"
            && spec.calibrationStatus != "calibrated") {
        throw std::runtime_error(prefix + ".calibration_status must be synthetic, uncalibrated, or calibrated");
    }
    if (spec.source.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::runtime_error(prefix + ".source must describe the parameter provenance");
    }
    for (const auto& field : {
            std::pair<double, const char*>{spec.resistanceReadOn, "resistance.read_on"},
            {spec.resistancePass, "resistance.pass"}, {spec.resistanceOff, "resistance.off"},
            {spec.resistanceSelect, "resistance.select"}, {spec.capacitanceGate, "capacitance.gate"},
            {spec.capacitanceBitline, "capacitance.bitline"}, {spec.voltagePrecharge, "bias.precharge"},
            {spec.decisionTime, "sensing.decision_time"}, {spec.minSenseMargin, "sensing.min_margin"},
            {spec.precharge.latency, "precharge.latency"},
            {spec.programPage.latency, "program_page.latency"},
            {spec.programPage.energy, "program_page.energy"},
            {spec.eraseBlock.latency, "erase_block.latency"},
            {spec.eraseBlock.energy, "erase_block.energy"}, {spec.supplyEfficiency, "supply_efficiency"}}) {
        YamlHelpers::require_positive(field.first, prefix + "." + std::string(field.second));
    }
    for (const auto& field : {
            std::pair<double, const char*>{spec.capacitanceInternal, "capacitance.internal"},
            {spec.capacitanceSource, "capacitance.source"}, {spec.capacitanceSelect, "capacitance.select"},
            {spec.voltageRead, "bias.read"}, {spec.voltagePass, "bias.pass"},
            {spec.referenceVoltage, "sensing.reference_voltage"}, {spec.senseOffset, "sensing.offset"}}) {
        YamlHelpers::require_non_negative(field.first, prefix + "." + std::string(field.second));
    }
    YamlHelpers::require_finite(spec.thresholdLow, prefix + ".threshold.low");
    YamlHelpers::require_finite(spec.thresholdHigh, prefix + ".threshold.high");
    if (!(spec.thresholdLow < spec.voltageRead && spec.voltageRead < spec.thresholdHigh
            && spec.thresholdHigh < spec.voltagePass)) {
        throw std::runtime_error("NAND threshold/bias ordering requires low < read < high < pass");
    }
    if (!(spec.resistancePass <= spec.resistanceReadOn && spec.resistanceReadOn < spec.resistanceOff)) {
        throw std::runtime_error("NAND resistance ordering requires pass <= read_on < off");
    }
    if (spec.supplyEfficiency > 1) {
        throw std::runtime_error(prefix + ".supply_efficiency must be in (0, 1]");
    }
    if (spec.referenceVoltage >= spec.voltagePrecharge
            || spec.minSenseMargin >= spec.voltagePrecharge / 2
            || spec.senseOffset >= spec.voltagePrecharge / 2) {
        throw std::runtime_error("NAND sensing reference/margin/offset must fit the precharge voltage range");
    }
    for (const auto& field : {
            std::pair<const NandPeripheralSpec*, const char*>{&spec.wordlineDriver, "wordline_driver"},
            {&spec.sense, "sense"}, {&spec.pageBuffer, "page_buffer"}}) {
        const std::string fieldPath = prefix + "." + std::string(field.second);
        YamlHelpers::require_positive(field.first->area, fieldPath + ".area");
        YamlHelpers::require_non_negative(field.first->latency, fieldPath + ".latency");
        YamlHelpers::require_non_negative(field.first->energy, fieldPath + ".energy");
        YamlHelpers::require_non_negative(field.first->leakage, fieldPath + ".leakage");
    }
    for (const auto& field : {
            std::pair<const NandOperationSpec*, const char*>{&spec.query, "query"},
            {&spec.setup, "setup"}, {&spec.precharge, "precharge"}, {&spec.recovery, "recovery"}}) {
        const std::string fieldPath = prefix + "." + std::string(field.second);
        YamlHelpers::require_non_negative(field.first->latency, fieldPath + ".latency");
        YamlHelpers::require_non_negative(field.first->energy, fieldPath + ".energy");
    }
}

void ValidateNand(const MemCell& cell) {
    if (cell.memCellType != SLCNAND || !cell.nandString || !cell.nand.configured || cell.nand3d.configured) {
        throw std::runtime_error("NAND CAM requires type: SLCNAND, cell.topology: nand_string, and memory_device.nand");
    }
    ValidateNandTopology(cell);
    ValidateNandElectrical(cell.nand, "memory_device.nand", "analytical_rc");
}

void ValidateNand3d(const MemCell& cell) {
    const auto& spec = cell.nand3d;
    if (cell.memCellType != NAND3D || !cell.nandString || !spec.configured
            || !spec.electrical.configured || cell.nand.configured) {
        throw std::runtime_error("NAND3D CAM requires type: NAND3D, cell.topology: nand_string, and memory_device.nand3d only");
    }
    ValidateNandTopology(cell);
    ValidateNandElectrical(spec.electrical, "memory_device.nand3d", "transient_rc");
    if (spec.storageMode != "SLC") {
        throw std::runtime_error("memory_device.nand3d.storage_mode must be SLC");
    }
    if (spec.storageLayers < 4 || spec.storageLayers > 4096 || spec.dummyLayers < 0
            || spec.dummyLayers > 4096 - spec.storageLayers) {
        throw std::runtime_error("NAND3D stack requires at least 4 storage layers, nonnegative dummy layers, and at most 4096 total layers");
    }
    if (spec.stringRows <= 0 || spec.stringColumns < 8 || spec.stringColumns % 8 != 0
            || spec.stringRows > 1048576 / spec.stringColumns) {
        throw std::runtime_error("NAND3D layout requires positive string_rows, byte-aligned string_columns >= 8, and at most 1048576 strings");
    }
    if (spec.peripheralPlacement != "beside" && spec.peripheralPlacement != "under_array") {
        throw std::runtime_error("memory_device.nand3d.layout.peripheral_placement must be beside or under_array");
    }
    for (const auto& field : {std::pair<double, const char*>{spec.holePitchX, "layout.hole_pitch_x"},
            {spec.holePitchY, "layout.hole_pitch_y"}, {spec.layerPitch, "layout.layer_pitch"},
            {spec.staircaseStepWidth, "layout.staircase_step_width"},
            {spec.staircaseContactLength, "layout.staircase_contact_length"},
            {spec.prechargeDriverResistance, "precharge_driver_resistance"},
            {spec.solverMaxStep, "solver.max_step"}, {spec.solverTolerance, "solver.tolerance"},
            {spec.electrical.capacitanceInternal, "capacitance.internal"},
            {spec.electrical.capacitanceSource, "capacitance.source"}}) {
        YamlHelpers::require_positive(field.first, "memory_device.nand3d." + std::string(field.second));
    }
    YamlHelpers::require_non_negative(spec.isolationWidth, "memory_device.nand3d.layout.isolation_width");
    if (spec.solverTolerance > 1e-3 || spec.solverMaxSteps < 100) {
        throw std::runtime_error("NAND3D solver requires tolerance <= 1mV and max_steps >= 100");
    }
    const double stringWidth = spec.stringColumns * spec.holePitchX;
    const double stringHeight = spec.stringRows * spec.holePitchY;
    const double stackHeight = (spec.storageLayers + spec.dummyLayers + 2.0) * spec.layerPitch;
    const double staircaseWidth = (spec.storageLayers + spec.dummyLayers + 2.0) * spec.staircaseStepWidth;
    const double width = stringWidth + staircaseWidth + 2 * spec.isolationWidth;
    const double height = std::max(stringHeight, spec.staircaseContactLength) + 2 * spec.isolationWidth;
    for (double dimension : {width, height, stackHeight, width * height}) {
        YamlHelpers::require_positive(dimension, "NAND3D derived layout dimension/area");
    }
}

}  // namespace

namespace PhysicalDomainValidators {

void ValidateMemCell(const MemCell& cell) {
    YamlHelpers::require_positive(cell.processNode, "cell.layout.cell_process_node");
    if (cell.memCellType == NAND3D || cell.nand3d.configured) {
        ValidateNand3d(cell);
        return;
    }
    YamlHelpers::require_positive(cell.area, "cell.layout.area");
    YamlHelpers::require_positive(cell.aspectRatio, "cell.layout.aspect_ratio");
    YamlHelpers::require_positive(
            cell.heightInFeatureSize, "cell.layout derived height");
    YamlHelpers::require_positive(
            cell.widthInFeatureSize, "cell.layout derived width");

    if (cell.nandString || cell.nand.configured || cell.memCellType == SLCNAND) {
        ValidateNand(cell);
        return;
    }

    if (cell.memCellType != SRAM) {
        YamlHelpers::require_positive(cell.resistanceOn, "memory_device.resistance.on");
        YamlHelpers::require_positive(cell.resistanceOff, "memory_device.resistance.off");
        if (cell.resistanceOff < cell.resistanceOn) {
            throw std::runtime_error(
                    "[Input] Error: memory_device.resistance.off must be greater than or "
                    "equal to memory_device.resistance.on.");
        }
    } else {
        YamlHelpers::require_non_negative(cell.resistanceOn, "memory_device.resistance.on");
        YamlHelpers::require_non_negative(cell.resistanceOff, "memory_device.resistance.off");
    }

    YamlHelpers::require_non_negative(
            cell.capacitanceOn, "memory_device.capacitance.on");
    YamlHelpers::require_non_negative(
            cell.capacitanceOff, "memory_device.capacitance.off");
    YamlHelpers::require_non_negative(cell.readVoltage, "memory_device.read.voltage");
    YamlHelpers::require_non_negative(cell.readCurrent, "memory_device.read.current");
    YamlHelpers::require_non_negative(cell.readPower, "memory_device.read.power");
    YamlHelpers::require_non_negative(cell.readEnergy, "memory_device.read.energy");
    if (cell.camType == MCAM) {
        YamlHelpers::require_non_negative(
                cell.minSenseVoltage, "memory_device.read.min_sense_voltage");
    } else {
        YamlHelpers::require_positive(
                cell.minSenseVoltage, "memory_device.read.min_sense_voltage");
    }
    YamlHelpers::require_positive(
            cell.wordlineBoostRatio, "memory_device.read.wordline_boost_ratio");

    // Programming voltage/current signs may encode polarity. Their magnitudes are
    // therefore checked through the selected operation rather than forcing > 0.
    YamlHelpers::require_finite(cell.setVoltage, "memory_device.write.set.voltage");
    YamlHelpers::require_finite(cell.setCurrent, "memory_device.write.set.current");
    YamlHelpers::require_finite(cell.resetVoltage, "memory_device.write.reset.voltage");
    YamlHelpers::require_finite(cell.resetCurrent, "memory_device.write.reset.current");
    YamlHelpers::require_non_negative(cell.setEnergy, "memory_device.write.set.energy");
    YamlHelpers::require_non_negative(cell.resetEnergy, "memory_device.write.reset.energy");
    if (cell.memCellType != SRAM) {
        YamlHelpers::require_positive(cell.setPulse, "memory_device.write.set.pulse");
        YamlHelpers::require_positive(cell.resetPulse, "memory_device.write.reset.pulse");
        if (cell.setEnergy == 0 && cell.setVoltage == 0 && cell.setCurrent == 0) {
            throw std::runtime_error(
                    "[Input] Error: memory_device.write.set must provide a non-zero "
                    "voltage, current, or energy.");
        }
        if (cell.resetEnergy == 0 && cell.resetVoltage == 0 && cell.resetCurrent == 0) {
            throw std::runtime_error(
                    "[Input] Error: memory_device.write.reset must provide a non-zero "
                    "voltage, current, or energy.");
        }
    } else {
        YamlHelpers::require_non_negative(cell.setPulse, "memory_device.write.set.pulse");
        YamlHelpers::require_non_negative(cell.resetPulse, "memory_device.write.reset.pulse");
    }

    ValidateOptionalPositive(cell.widthAccessCMOS, "cell.access_device.cmos_width");
    ValidateOptionalPositive(cell.widthSOIDevice, "memory_device.device.soi_width");
    YamlHelpers::require_positive(
            cell.widthSRAMCellNMOS, "memory_device.sram.nmos_width");
    YamlHelpers::require_positive(
            cell.widthSRAMCellPMOS, "memory_device.sram.pmos_width");
    YamlHelpers::require_non_negative(
            cell.voltageDropAccessDevice, "cell.access_device.voltage_drop");
    YamlHelpers::require_non_negative(
            cell.leakageCurrentAccessDevice, "cell.access_device.leakage_current");
    YamlHelpers::require_positive(
            cell.gateOxThicknessFactor, "memory_device.device.gate_ox_thickness_factor");

    for (const auto& entry : {
             std::pair<double, const char*>{cell.resistanceOnAtSetVoltage,
                     "memory_device.resistance.at_set.on"},
             {cell.resistanceOffAtSetVoltage, "memory_device.resistance.at_set.off"},
             {cell.resistanceOnAtResetVoltage, "memory_device.resistance.at_reset.on"},
             {cell.resistanceOffAtResetVoltage, "memory_device.resistance.at_reset.off"},
             {cell.resistanceOnAtReadVoltage, "memory_device.resistance.at_read.on"},
             {cell.resistanceOffAtReadVoltage, "memory_device.resistance.at_read.off"},
             {cell.resistanceOnAtHalfReadVoltage, "memory_device.resistance.at_half_read.on"},
             {cell.resistanceOffAtHalfReadVoltage, "memory_device.resistance.at_half_read.off"},
             {cell.resistanceOnAtHalfResetVoltage,
                     "memory_device.resistance.at_half_reset.on"}}) {
        ValidateOptionalPositive(entry.first, entry.second);
    }

    YamlHelpers::require_non_negative(
            cell.resistanceOnVariation,
            "memory_device.variation.memory_device_resistance_on_stdev");
    YamlHelpers::require_non_negative(
            cell.resistanceOffVariation,
            "memory_device.variation.memory_device_resistance_off_stdev");
    YamlHelpers::require_non_negative(
            cell.resistanceOnMaxVariation,
            "memory_device.variation.memory_device_resistance_on_max_var");
    YamlHelpers::require_non_negative(
            cell.resistanceOffMaxVariation,
            "memory_device.variation.memory_device_resistance_off_max_var");
    for (int state = 0; state < cell.numResistanceState; state++) {
        YamlHelpers::require_non_negative(
                cell.resStateVariation[state], "mcam.state_variation");
    }
    if (cell.hasVariationSamples && cell.variationSamples <= 0) {
        throw std::runtime_error(
                "[Input] Error: memory_device.variation.samples must be positive; got "
                + std::to_string(cell.variationSamples) + ".");
    }

    ValidateOptionalPositive(cell.flashEraseTime, "memory_device.flash.erase_time");
    ValidateOptionalPositive(cell.flashProgramTime, "memory_device.flash.program_time");
    if (cell.gateCouplingRatio != 0) {
        YamlHelpers::require_range(
                cell.gateCouplingRatio, 0.0, 1.0,
                "memory_device.flash.gate_coupling_ratio");
    }
}

void ValidateTechnology(const Technology& technology) {
    if (!technology.initialized()) {
        throw std::runtime_error(
                "[Input] Error: derived technology must be initialized before validation.");
    }
    YamlHelpers::require_positive(
            technology.featureSize(), "derived technology feature size");
    YamlHelpers::require_positive(technology.vdd(), "derived technology vdd");
    YamlHelpers::require_non_negative(technology.vth(), "derived technology vth");
    if (technology.useUpdatedLib() && 0.7 * technology.vdd() <= technology.vth()) {
        throw std::runtime_error(
                "[Input] Error: derived technology vth must be less than 0.7 * vdd "
                "for the updated transconductance model.");
    }
    YamlHelpers::require_positive(
            technology.phyGateLength(), "derived technology physical gate length");
    YamlHelpers::require_non_negative(
            technology.capIdealGate(), "derived technology ideal gate capacitance");
    YamlHelpers::require_non_negative(
            technology.capFringe(), "derived technology fringe capacitance");
    YamlHelpers::require_non_negative(
            technology.capJunction(), "derived technology junction capacitance");
    YamlHelpers::require_non_negative(
            technology.capOx(), "derived technology oxide capacitance");
    YamlHelpers::require_non_negative(
            technology.effectiveElectronMobility(),
            "derived technology electron mobility");
    YamlHelpers::require_non_negative(
            technology.effectiveHoleMobility(), "derived technology hole mobility");
    YamlHelpers::require_positive(
            technology.pnSizeRatio(), "derived technology pn size ratio");
    YamlHelpers::require_positive(
            technology.effectiveResistanceMultiplier(),
            "derived technology effective resistance multiplier");
    ValidateCurrentArray(technology.currentOnNmos(), "derived technology on_nmos current");
    ValidateCurrentArray(technology.currentOnPmos(), "derived technology on_pmos current");
    ValidateCurrentArray(technology.currentOffNmos(), "derived technology off_nmos current");
    ValidateCurrentArray(technology.currentOffPmos(), "derived technology off_pmos current");
}

}  // namespace PhysicalDomainValidators
