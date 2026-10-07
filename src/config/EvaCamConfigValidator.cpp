#include "config/EvaCamConfigValidator.h"

#include <stdexcept>

#include "EvaCamConfig.h"
#include "input/SenseAmpYamlLoader.h"

void EvaCamConfigValidator::Validate(EvaCamConfig &config) {
    if (config.technology.cell->memCellType == DRAM)
        throw std::runtime_error("[ERROR] DRAM model is still under development");

    if (config.technology.cell->memCellType == eDRAM)
        throw std::runtime_error("[ERROR] Embedded DRAM model is still under development");

    if (config.technology.cell->memCellType == MLCNAND)
        throw std::runtime_error("[ERROR] MLC NAND flash model is still under development");

    const auto& cell = *config.technology.cell;
    if (config.peripherals.inverterTripDecision) {
        if (config.peripherals.fileSenseAmp.empty() || config.peripherals.customSenseAmp)
            throw std::runtime_error("inverter_threshold requires an analytical_inverter sense amplifier");
        const auto model = YamlHelpers::ReadSenseAmpModelFromYaml(config.peripherals.fileSenseAmp);
        if (model.model != "analytical_inverter")
            throw std::runtime_error("inverter_threshold requires an analytical_inverter sense amplifier");
        const auto &tech = *config.technology.tech;
        config.peripherals.decisionThreshold = CamInverterTripVoltage(tech.vdd(), tech.vth(),
                tech.effectiveHoleMobility() * model.pSenseWidth
                / (tech.effectiveElectronMobility() * model.nSenseWidth));
    }
    if (config.peripherals.keeperMidpointDecision) {
        if (config.peripherals.matchlineCircuit != "diode_keeper")
            throw std::runtime_error("keeper_midpoint requires diode_keeper circuit");
        config.peripherals.decisionThreshold = config.technology.tech->vth()
                + (config.peripherals.keeperHighClamp + config.peripherals.keeperLowClamp) / 2;
    }
    const auto &timing = config.peripherals;
    if (!timing.fileSenseAmp.empty()
            && YamlHelpers::ReadSenseAmpModelFromYaml(timing.fileSenseAmp).model == "analytical_inverter"
            && timing.decisionMode != CamDecisionMode::FixedThreshold)
        throw std::runtime_error("analytical_inverter requires a fixed or inverter-derived voltage threshold");
    if (timing.matchlineCircuit != "legacy") {
        if (timing.decisionMode != CamDecisionMode::FixedThreshold || !timing.explicitSearchTiming
                || cell.fefetGate || timing.prechargeVoltage > config.technology.tech->vdd()
                || timing.decisionThreshold >= timing.prechargeVoltage)
            throw std::runtime_error("sensing.circuit requires voltage_threshold, explicit search_timing, and threshold < precharge <= Vdd");
        if ((timing.matchlineCircuit == "clamped_keeper" || timing.matchlineCircuit == "diode_keeper") && (timing.bitsPerDischargePath != 1
                || timing.decisionThreshold <= timing.keeperLowClamp))
            throw std::runtime_error("clamped_keeper requires one bit per path and threshold above low clamp");
        if (timing.matchlineCircuit == "direct_nvm" && cell.memCellType != FEFETRAM
                && cell.memCellType != PCRAM && cell.memCellType != memristor)
            throw std::runtime_error("direct_nvm requires FeFET, PCM or ReRAM cells");
        if (timing.bitsPerDischargePath == 2 && (!timing.withInputEnc || timing.customInputEnc
                || config.input.wordWidth % 2 != 0))
            throw std::runtime_error("Two-bit discharge paths require an even word width and the built-in two-bit encoder");
    }
    if (timing.matchlineCircuit == "diode_keeper" && (timing.keeperHighClamp + config.technology.tech->vth() >= timing.prechargeVoltage
            || timing.decisionThreshold <= timing.keeperLowClamp + config.technology.tech->vth()))
        throw std::runtime_error("diode_keeper requires source+Vth below precharge and decision above low source+Vth");
    if (timing.decisionMode != CamDecisionMode::LegacyHorowitz || timing.explicitSearchTiming) {
        if (cell.nandString || cell.camType != TCAM || config.input.searchFunction != EX
                || config.variation.enabled || cell.withVariation) {
            throw std::runtime_error("Analytical decision/search_timing requires nominal exact non-NAND TCAM");
        }
        if (timing.noPrechargeInc) {
            throw std::runtime_error("Analytical decision/search_timing cannot use exclude_precharge_latency");
        }
        if (timing.decisionMode == CamDecisionMode::FixedThreshold
                && timing.decisionThreshold >= config.technology.tech->vdd()) {
            throw std::runtime_error("sensing.decision.threshold must be below precharge Vdd");
        }
    }
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
