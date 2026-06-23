#include "config/VariationConfigBuilder.h"

#include <chrono>
#include <stdexcept>

#include "MemCell.h"

namespace {

uint32_t DefaultVariationSeed() {
    const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return static_cast<uint32_t>(ticks);
}

void ValidateCornerVariation(const char *fieldName, double value) {
    if (value < 0.0) {
        throw std::runtime_error(std::string("[Input] Error: variation.") + fieldName
                + " must be non-negative.");
    }
}

int CornerDimensionCount(const VariationConfig &variation) {
    int count = 0;
    if (variation.memoryDeviceResOnMaxVar > 0.0)
        count++;
    if (variation.memoryDeviceResOffMaxVar > 0.0)
        count++;
    return count;
}

}  // namespace

VariationConfig VariationConfigBuilder::FromCell(const MemCell &cell) {
    VariationConfig variation;
    variation.enabled = cell.withVariation;
    variation.hasUserSeed = cell.hasVariationSeed;
    variation.hasUserSamples = cell.hasVariationSamples;
    variation.seed = cell.hasVariationSeed ? cell.variationSeed : DefaultVariationSeed();
    variation.mode = cell.variationMode;
    variation.lutFile = cell.variationLutFile;
    variation.samples = cell.variationSamples;
    variation.memoryDeviceResOnStdev = cell.resistanceOnVariation;
    variation.memoryDeviceResOffStdev = cell.resistanceOffVariation;
    variation.memoryDeviceResOnMaxVar = cell.resistanceOnMaxVariation;
    variation.memoryDeviceResOffMaxVar = cell.resistanceOffMaxVariation;

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
    } else if (variation.mode == "corner") {
        ValidateCornerVariation("memory_device_resistance_on_max_var", variation.memoryDeviceResOnMaxVar);
        ValidateCornerVariation("memory_device_resistance_off_max_var", variation.memoryDeviceResOffMaxVar);

        if (variation.memoryDeviceResOnMaxVar >= 1.0
                || variation.memoryDeviceResOffMaxVar >= 1.0) {
            throw std::runtime_error("[Input] Error: corner variation max values must keep low-corner resistances positive.");
        }

        const int dimensions = CornerDimensionCount(variation);
        if (dimensions == 0) {
            throw std::runtime_error("[Input] Error: corner variation requires at least one positive *_max_var field.");
        }
        variation.samples = 1 << dimensions;
    } else {
        throw std::runtime_error("[Input] Error: variation.mode must be 'single_point', 'monte_carlo', or 'corner'.");
    }

    return variation;
}
