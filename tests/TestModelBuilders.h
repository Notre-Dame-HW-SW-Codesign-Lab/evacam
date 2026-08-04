#ifndef EVACAM_TESTS_TESTMODELBUILDERS_H_
#define EVACAM_TESTS_TESTMODELBUILDERS_H_

#include <memory>
#include <utility>

#include "CAM_BasicEncoder.h"
#include "CAM_DataBuffer.h"
#include "CAM_OutputAccumulator.h"
#include "EvaCamConfig.h"
#include "MemCell.h"
#include "Technology.h"
#include "TechnologySpec.h"
#include "Wire.h"

namespace TestModelBuilders {

inline TechnologySpec MakeTechnologySpec() {
    TechnologySpec spec{};
    spec.featureSizeInNano = 90;
    spec.featureSize = 90e-9;
    spec.roadmap = HP;
    spec.useUpdatedLib = false;
    spec.vdd = 1.2;
    spec.vth = 0.2;
    spec.vdsatNmos = 0.15;
    spec.vdsatPmos = 0.63;
    spec.phyGateLength = 37e-9;
    spec.capIdealGate = 6.38e-10;
    spec.capFringe = 2.5e-10;
    spec.capJunction = 1e-3;
    spec.capOverlap = 0;
    spec.capSidewall = 2.5e-10;
    spec.capDrainToChannel = 0.5e-10;
    spec.capOx = 0.0173;
    spec.buildInPotential = 0.9;
    spec.effectiveElectronMobility = 0.024343;
    spec.effectiveHoleMobility = 0.005832;
    spec.pnSizeRatio = 2.45;
    spec.effectiveResistanceMultiplier = 1.54;
    spec.currentGmNmos = 1000;
    spec.currentGmPmos = 600;
    spec.heightFin = 0;
    spec.widthFin = 0;
    spec.PitchFin = 0;
    spec.capPolywire = 0;

    for (std::size_t index = 0; index < spec.currentOnNmos.size(); index++) {
        spec.currentOnNmos[index] = 1050.0 - 13.0 * index;
        spec.currentOnPmos[index] = 639.0 - 10.0 * index;
        spec.currentOffNmos[index] = 0.05 + 0.02 * index;
        spec.currentOffPmos[index] = 0.052 + 0.0002 * index;
    }
    return spec;
}

inline std::shared_ptr<Technology> MakeTechnology() {
    auto technology = std::make_shared<Technology>();
    technology->InitializeFromSpec(MakeTechnologySpec());
    return technology;
}

inline std::shared_ptr<MemCell> MakeMemCell() {
    auto cell = std::make_shared<MemCell>();
    cell->memCellType = SRAM;
    cell->processNode = 90;
    cell->area = 120;
    cell->aspectRatio = 1;
    cell->widthInFeatureSize = 10;
    cell->heightInFeatureSize = 12;
    cell->resistanceOn = 1e4;
    cell->resistanceOff = 1e7;
    cell->capacitanceOn = 1e-15;
    cell->capacitanceOff = 1e-15;
    cell->readVoltage = 0.5;
    cell->readCurrent = 50e-6;
    cell->readEnergy = 1e-15;
    cell->resetVoltage = 1.2;
    cell->resetCurrent = 100e-6;
    cell->resetPulse = 10e-9;
    cell->setVoltage = 1.0;
    cell->setCurrent = 80e-6;
    cell->setPulse = 10e-9;
    cell->designTarget = CAM_chip;
    cell->vdd = 1.2;
    cell->camType = TCAM;
    cell->camNumRow = 0;
    cell->camNumCol = 0;
    return cell;
}

inline std::shared_ptr<EvaCamConfig> MakeEvaCamConfig() {
    auto config = std::make_shared<EvaCamConfig>();
    config->input.processNode = 90;
    config->input.deviceRoadmap = HP;
    config->input.temperature = 300;
    config->input.capacity = 1024;
    config->input.wordWidth = 64;
    config->technology.tech = MakeTechnology();
    config->technology.cell = MakeMemCell();
    return config;
}

inline Wire MakeWire(
        const std::shared_ptr<EvaCamConfig> &config,
        WireType wireType = local_aggressive,
        WireRepeaterType repeaterType = repeated_none,
        bool lowSwing = false) {
    Wire wire;
    wire.Initialize(
            config->input.processNode,
            wireType,
            repeaterType,
            config->input.temperature,
            lowSwing,
            config);
    return wire;
}

template <typename Component, typename Initializer>
std::unique_ptr<Component> MakeInitializedCamComponent(Initializer &&initializer) {
    auto component = std::make_unique<Component>();
    std::forward<Initializer>(initializer)(*component);
    return component;
}

inline std::unique_ptr<CAM_BasicEncoder> MakeCamBasicEncoder(
        const std::shared_ptr<EvaCamConfig> &config) {
    return MakeInitializedCamComponent<CAM_BasicEncoder>(
            [&config](CAM_BasicEncoder &component) {
                component.Initialize(8, 10e-15, 1e3, config);
            });
}

inline std::unique_ptr<CAM_DataBuffer> MakeCamDataBuffer(
        const std::shared_ptr<EvaCamConfig> &config) {
    return MakeInitializedCamComponent<CAM_DataBuffer>(
            [&config](CAM_DataBuffer &component) {
                component.Initialize(false, 10e-15, 1e3, config);
            });
}

inline std::unique_ptr<CAM_OutputAccumulator> MakeCamOutputAccumulator(
        const std::shared_ptr<EvaCamConfig> &config) {
    return MakeInitializedCamComponent<CAM_OutputAccumulator>(
            [&config](CAM_OutputAccumulator &component) {
                component.Initialize(10e-15, 1e3, config);
            });
}

}  // namespace TestModelBuilders

#endif  // EVACAM_TESTS_TESTMODELBUILDERS_H_
