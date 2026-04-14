#include "config/VariationConfigBuilder.h"

#include <chrono>
#include <stdexcept>

#include "MemCell.h"

namespace {

uint32_t DefaultVariationSeed() {
    const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return static_cast<uint32_t>(ticks);
}

}  // namespace

VariationConfig VariationConfigBuilder::FromCell(const MemCell &cell) {
    VariationConfig variation;
    variation.enabled = cell.withVariation;
    variation.seed = cell.hasVariationSeed ? cell.variationSeed : DefaultVariationSeed();
    variation.mode = cell.variationMode;
    variation.samples = cell.variationSamples;
    variation.memoryDeviceResOnStdev = cell.resistanceOnVariation;
    variation.memoryDeviceResOffStdev = cell.resistanceOffVariation;
    variation.mlWireResStdev = cell.matchlineWireResistanceVariation;
    variation.deviceAccessResStdev = cell.deviceAccessResistanceVariation;
    variation.deviceMatchResStdev = cell.deviceMatchResistanceVariation;

    if (!variation.enabled) {
        variation.mode = "nominal";
        variation.samples = 1;
    } else if (variation.mode == "nominal") {
        throw std::runtime_error("[Input] Error: variation.mode cannot be 'nominal'; disable variation instead.");
    } else if (variation.mode == "single_point") {
        variation.samples = 1;
    } else if (variation.mode == "monte_carlo") {
        if (variation.samples <= 1) {
            throw std::runtime_error("[Input] Error: variation.samples must be > 1 for monte_carlo mode.");
        }
    } else {
        throw std::runtime_error("[Input] Error: variation.mode must be 'single_point' or 'monte_carlo'.");
    }

    return variation;
}
