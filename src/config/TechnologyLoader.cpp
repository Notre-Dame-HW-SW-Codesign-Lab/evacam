#include "config/TechnologyLoader.h"

#include <chrono>
#include <memory>
#include <stdexcept>

#include "MemCell.h"
#include "Technology.h"
#include "TechnologyTables.h"

namespace {

std::shared_ptr<Technology> LoadTech(const InputConfig &input, const PeripheralConfig &peripherals) {
    auto tech = std::make_shared<Technology>();
    tech->Initialize(input.processNode, input.deviceRoadmap, peripherals.useUpdatedLib);

    auto techHigh = std::make_shared<Technology>();
    double alpha = 0;

    if (input.processNode > 200) {
        throw std::runtime_error("[Error] Technology node above 200nm not supported.");
    } else if (input.processNode > 120) {
        techHigh->Initialize(200, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 120.0) / 60;
    } else if (input.processNode > 90) {
        techHigh->Initialize(120, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 90.0) / 30;
    } else if (input.processNode > 65) {
        techHigh->Initialize(90, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 65.0) / 25;
    } else if (input.processNode > 45) {
        techHigh->Initialize(65, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 45.0) / 20;
    } else if (input.processNode > 32) {
        techHigh->Initialize(45, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 32.0) / 13;
    } else if (input.processNode > 22) {
        techHigh->Initialize(32, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 22.0) / 10;
    } else if (input.processNode > 14) {
        techHigh->Initialize(22, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 14.0) / 8;
    } else if (input.processNode > 10) {
        techHigh->Initialize(14, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 10.0) / 4;
    } else if (input.processNode > 7) {
        techHigh->Initialize(10, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = (input.processNode - 10.0) / 3;
    } else if (input.processNode == 7) {
        techHigh->Initialize(7, input.deviceRoadmap, peripherals.useUpdatedLib);
        alpha = 1;
    } else {
        throw std::runtime_error("[Error]: Technology node below 7nm is not supported.");
    }

    tech->InterpolateWith(techHigh, alpha);
    return tech;
}

std::shared_ptr<MemCell> LoadCell(const InputConfig &input, const std::shared_ptr<Technology> &tech) {
    auto cell = std::make_shared<MemCell>();
    cell->ReadCellFromFile(input.fileMemCell, input.designTarget, tech->vdd());
    return cell;
}

std::shared_ptr<Technology> LoadFefetTech(const InputConfig &input, const PeripheralConfig &peripherals) {
    auto fefetTech = std::make_shared<Technology>();
    if (FindTechnologySpec(input.processNode, FEFET, peripherals.useUpdatedLib)) {
        fefetTech->Initialize(input.processNode, FEFET, peripherals.useUpdatedLib);
        return fefetTech;
    }

    if (!peripherals.useUpdatedLib && FindTechnologySpec(input.processNode, FEFET, true)) {
        fefetTech->Initialize(input.processNode, FEFET, true);
        return fefetTech;
    }

    if (FindTechnologySpec(input.processNode, input.deviceRoadmap, peripherals.useUpdatedLib)) {
        fefetTech->Initialize(input.processNode, input.deviceRoadmap, peripherals.useUpdatedLib);
        return fefetTech;
    }

    throw std::runtime_error("[Technology] Unsupported FeFET technology configuration.");
}

uint32_t DefaultVariationSeed() {
    const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return static_cast<uint32_t>(ticks);
}

}  // namespace

TechnologyContext TechnologyLoader::Load(
        const InputConfig &input,
        const PeripheralConfig &peripherals,
        VariationConfig *variation) {
    TechnologyContext technology;
    technology.tech = LoadTech(input, peripherals);
    technology.cell = LoadCell(input, technology.tech);
    technology.fefetTech = LoadFefetTech(input, peripherals);
    if (variation) {
        variation->enabled = technology.cell->withVariation;
        variation->seed = technology.cell->hasVariationSeed
                ? technology.cell->variationSeed
                : DefaultVariationSeed();
        variation->mode = technology.cell->variationMode;
        variation->samples = technology.cell->variationSamples;
        variation->memoryDeviceResOnStdev = technology.cell->resistanceOnVariation;
        variation->memoryDeviceResOffStdev = technology.cell->resistanceOffVariation;
        variation->mlWireResStdev = technology.cell->matchlineWireResistanceVariation;
        variation->deviceAccessResStdev = technology.cell->deviceAccessResistanceVariation;
        variation->deviceMatchResStdev = technology.cell->deviceMatchResistanceVariation;

        if (!variation->enabled) {
            variation->mode = "nominal";
            variation->samples = 1;
        } else if (variation->mode == "nominal") {
            throw std::runtime_error("[Input] Error: variation.mode cannot be 'nominal'; disable variation instead.");
        } else if (variation->mode == "single_point") {
            variation->samples = 1;
        } else if (variation->mode == "monte_carlo") {
            if (variation->samples <= 1) {
                throw std::runtime_error("[Input] Error: variation.samples must be > 1 for monte_carlo mode.");
            }
        } else {
            throw std::runtime_error("[Input] Error: variation.mode must be 'single_point' or 'monte_carlo'.");
        }
    }
    return technology;
}
