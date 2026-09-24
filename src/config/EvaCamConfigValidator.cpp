#include "config/EvaCamConfigValidator.h"

#include <stdexcept>

#include "EvaCamConfig.h"

void EvaCamConfigValidator::Validate(EvaCamConfig &config) {
    if (config.technology.cell->memCellType == DRAM)
        throw std::runtime_error("[ERROR] DRAM model is still under development");

    if (config.technology.cell->memCellType == eDRAM)
        throw std::runtime_error("[ERROR] Embedded DRAM model is still under development");

    if (config.technology.cell->memCellType == MLCNAND)
        throw std::runtime_error("[ERROR] MLC NAND flash model is still under development");

    const auto& cell = *config.technology.cell;
    if (cell.memCellType == SLCNAND || cell.memCellType == NAND3D || cell.nandString
            || cell.nand.configured || cell.nand3d.configured) {
        const bool planar = cell.memCellType == SLCNAND && cell.nand.configured && !cell.nand3d.configured;
        const bool vertical = cell.memCellType == NAND3D && cell.nand3d.configured
                && cell.nand3d.electrical.configured && !cell.nand.configured;
        if (!cell.nandString || (!planar && !vertical)) {
            throw std::runtime_error("NAND CAM requires SLCNAND/memory_device.nand or NAND3D/memory_device.nand3d with cell.topology: nand_string");
        }
        if (cell.camType != TCAM || config.input.searchFunction != EX) {
            throw std::runtime_error("NAND CAM supports TCAM exact search (EX) with wildcards only");
        }
        const auto& peripherals = config.peripherals;
        if (peripherals.withInputEnc || peripherals.customInputEnc || peripherals.withInputBuffer
                || peripherals.withOutputAcc || peripherals.withPriorityEnc || peripherals.withOutputBuffer
                || peripherals.withWriteDriver) {
            throw std::runtime_error("NAND CAM requires generic peripherals to be disabled; configure NAND query, wordline_driver, sense, and page_buffer costs");
        }
        if (peripherals.customSenseAmp || !peripherals.fileCustomSA.empty()
                || !peripherals.fileSenseAmp.empty() || peripherals.typeSenseAmp != discharge) {
            throw std::runtime_error("NAND CAM requires sensing_mode: discharge and NAND sensing parameters; generic sense amplifiers are not supported");
        }
        if (peripherals.noPrechargeInc || peripherals.scaledVoltage != 0
                || peripherals.addCapOnML != 0 || config.input.hasCamWidthMatchTran) {
            throw std::runtime_error("NAND CAM does not support generic precharge, scaled_voltage, or matchline overrides");
        }
        if (config.variation.enabled || config.variation.mode != "nominal" || cell.withVariation) {
            throw std::runtime_error("NAND CAM variation is not supported");
        }
        if (!config.runtimeSizing.hasFixedSubarrayDimensions) {
            throw std::runtime_error("NAND CAM requires fixed organization.subarray.dimensions");
        }
        if (config.input.optimizationTarget == full_exploration || config.exploration.deepExploration
                || config.requestDeepExploration || config.constraints.enabled) {
            throw std::runtime_error("NAND CAM full/deep exploration and design constraints are not supported");
        }
        if (config.input.optimizationTarget == read_latency_optimized
                || config.input.optimizationTarget == read_energy_optimized
                || config.input.optimizationTarget == read_edp_optimized) {
            throw std::runtime_error("NAND CAM conventional read optimization is not supported; select SearchLatency, SearchDynamicEnergy, SearchEDP, Area, LeakagePower, or a write objective");
        }
    }

    if (!config.input.internalSensing)
        throw std::runtime_error("[ERROR] CAM bank routing requires internal sensing in this version.");
}
