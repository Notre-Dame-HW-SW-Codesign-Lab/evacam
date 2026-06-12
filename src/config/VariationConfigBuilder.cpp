#include "config/VariationConfigBuilder.h"

#include <chrono>
#include <stdexcept>

#include "MemCell.h"

namespace {

uint32_t DefaultVariationSeed() {
    const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return static_cast<uint32_t>(ticks);
}

void ValidateBoundaryVariation(const char *fieldName, double value) {
    if (value < 0.0) {
        throw std::runtime_error(std::string("[Input] Error: variation.") + fieldName
                + " must be non-negative.");
    }
}

int BoundaryDimensionCount(const VariationConfig &variation) {
    int count = 0;
    if (variation.mlWireResMaxVar > 0.0)
        count++;
    if (variation.deviceAccessResMaxVar > 0.0)
        count++;
    if (variation.deviceAccessResMaxVar > 0.0)
        count++;
    if (variation.memoryDeviceResOnMaxVar + variation.deviceMatchResMaxVar > 0.0)
        count++;
    if (variation.memoryDeviceResOffMaxVar + variation.deviceMatchResMaxVar > 0.0)
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
    variation.mlWireResStdev = cell.matchlineWireResistanceVariation;
    variation.deviceAccessResStdev = cell.deviceAccessResistanceVariation;
    variation.deviceMatchResStdev = cell.deviceMatchResistanceVariation;
    variation.memoryDeviceResOnMaxVar = cell.resistanceOnMaxVariation;
    variation.memoryDeviceResOffMaxVar = cell.resistanceOffMaxVariation;
    variation.mlWireResMaxVar = cell.matchlineWireResistanceMaxVariation;
    variation.deviceAccessResMaxVar = cell.deviceAccessResistanceMaxVariation;
    variation.deviceMatchResMaxVar = cell.deviceMatchResistanceMaxVariation;

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
    } else if (variation.mode == "boundary") {
        ValidateBoundaryVariation("memory_device_resistance_on_max_var", variation.memoryDeviceResOnMaxVar);
        ValidateBoundaryVariation("memory_device_resistance_off_max_var", variation.memoryDeviceResOffMaxVar);
        ValidateBoundaryVariation("matchline_wire_resistance_max_var", variation.mlWireResMaxVar);
        ValidateBoundaryVariation("device_access_resistance_max_var", variation.deviceAccessResMaxVar);
        ValidateBoundaryVariation("device_match_resistance_max_var", variation.deviceMatchResMaxVar);

        const double matchOnMaxVar = variation.memoryDeviceResOnMaxVar + variation.deviceMatchResMaxVar;
        const double matchOffMaxVar = variation.memoryDeviceResOffMaxVar + variation.deviceMatchResMaxVar;
        if (variation.mlWireResMaxVar >= 1.0
                || variation.deviceAccessResMaxVar >= 1.0
                || matchOnMaxVar >= 1.0
                || matchOffMaxVar >= 1.0) {
            throw std::runtime_error("[Input] Error: boundary variation max values must keep low-corner resistances positive.");
        }

        const int dimensions = BoundaryDimensionCount(variation);
        if (dimensions == 0) {
            throw std::runtime_error("[Input] Error: boundary variation requires at least one positive *_max_var field.");
        }
        variation.samples = 1 << dimensions;
    } else {
        throw std::runtime_error("[Input] Error: variation.mode must be 'single_point', 'monte_carlo', or 'boundary'.");
    }

    return variation;
}
