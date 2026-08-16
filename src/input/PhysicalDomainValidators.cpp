#include "input/PhysicalDomainValidators.h"

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

}  // namespace

namespace PhysicalDomainValidators {

void ValidateMemCell(const MemCell& cell) {
    YamlHelpers::require_positive(cell.processNode, "cell.layout.cell_process_node");
    YamlHelpers::require_positive(cell.area, "cell.layout.area");
    YamlHelpers::require_positive(cell.aspectRatio, "cell.layout.aspect_ratio");
    YamlHelpers::require_positive(
            cell.heightInFeatureSize, "cell.layout derived height");
    YamlHelpers::require_positive(
            cell.widthInFeatureSize, "cell.layout derived width");

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
