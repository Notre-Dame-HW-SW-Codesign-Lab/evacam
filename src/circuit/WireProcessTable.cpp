#include "WireProcessTable.h"

#include <array>

#include "constant.h"

namespace {

constexpr double kF22 = 22e-9;
constexpr double kF32 = 32e-9;
constexpr double kF45 = 45e-9;
constexpr double kF65 = 65e-9;
constexpr double kF90 = 90e-9;
constexpr double kF120 = 120e-9;
constexpr double kF200 = 200e-9;
constexpr double kCu = COPPER_RESISTIVITY;

struct WireProcessEntry {
    WireType wireType;
    WireProcessSpec spec;
};

struct ProcessNodeSpec {
    int maxFeatureSizeInNano;
    std::array<WireProcessEntry, 7> entries;
};

const ProcessNodeSpec kProcessNodes[] = {
    {
        22,
        {{
            {local_aggressive, {kF22, 0.00e-6, 2.55, 2 * kF22, 1.9, 1.9 * kF22, 6.0e-8}},
            {local_conservative, {kF22, 0.0021e-6, 3, 2 * kF22, 1.9, 1.9 * kF22, 6.0e-8}},
            {semi_aggressive, {kF22, 0.00e-6, 2.55, 4 * kF22, 1.9, 2 * 1.9 * kF22, 6.0e-8}},
            {semi_conservative, {kF22, 0.0021e-6, 3, 4 * kF22, 1.9, 2 * 1.9 * kF22, 6.0e-8}},
            {global_aggressive, {kF22, 0.00e-6, 2.55, 8 * kF22, 2.34, 0.42e-6 * 22 / 32, 3.0e-8}},
            {global_conservative, {kF22, 0.0063e-6, 3, 8 * kF22, 2.34, 0.385e-6 * 22 / 32, 3.0e-8}},
            {dram_wordline, {kF22, 0e-6, 0, 2 * kF22, 0, 0e-6, kCu}},
        }},
    },
    {
        32,
        {{
            {local_aggressive, {kF32, 0.00e-6, 2.82, 2 * kF32, 1.8, 1.8 * kF32, 5.0e-8}},
            {local_conservative, {kF32, 0.0026e-6, 3.16, 2 * kF32, 1.8, 1.8 * kF32, 5.0e-8}},
            {semi_aggressive, {kF32, 0.00e-6, 2.82, 4 * kF32, 1.9, 2 * 1.9 * kF32, 5.0e-8}},
            {semi_conservative, {kF32, 0.0026e-6, 3.16, 4 * kF32, 1.9, 2 * 1.9 * kF32, 5.0e-8}},
            {global_aggressive, {kF32, 0.00e-6, 2.82, 8 * kF32, 2.34, 0.42e-6, 2.5e-8}},
            {global_conservative, {kF32, 0.0078e-6, 3.16, 8 * kF32, 2.34, 0.385e-6, 2.5e-8}},
            {dram_wordline, {kF32, 0e-6, 0, 2 * kF32, 0, 0e-6, kCu}},
        }},
    },
    {
        45,
        {{
            {local_aggressive, {kF45, 0.00e-6, 2.6, 0.102e-6, 1.8, 0.0918e-6, 4.08e-8}},
            {local_conservative, {kF45, 0.0033e-6, 2.9, 0.102e-6, 1.8, 0.0918e-6, 4.08e-8}},
            {semi_aggressive, {kF45, 0.00e-6, 2.6, 4 * kF45, 1.8, 2 * 1.8 * kF45, 4.08e-8}},
            {semi_conservative, {kF45, 0.0033e-6, 2.9, 4 * kF45, 1.8, 2 * 1.8 * kF45, 4.08e-8}},
            {global_aggressive, {kF45, 0.00e-6, 2.6, 8 * kF45, 2.34, 0.63e-6, 2.06e-8}},
            {global_conservative, {kF45, 0.01e-6, 2.9, 8 * kF45, 2.34, 0.55e-6, 2.06e-8}},
            {dram_wordline, {kF45, 0e-6, 0, 2 * kF45, 0, 0e-6, kCu}},
        }},
    },
    {
        65,
        {{
            {local_aggressive, {kF65, 0.00e-6, 2.303, 2.5 * kF65, 2.7, 0.405e-6, kCu}},
            {local_conservative, {kF65, 0.006e-6, 2.734, 2.5 * kF65, 2.0, 0.405e-6, kCu}},
            {semi_aggressive, {kF65, 0.00e-6, 2.303, 4 * kF65, 2.7, 0.405e-6, kCu}},
            {semi_conservative, {kF65, 0.006e-6, 2.734, 4 * kF65, 2.0, 0.405e-6, kCu}},
            {global_aggressive, {kF65, 0.00e-6, 2.303, 8 * kF65, 2.8, 0.81e-6, kCu}},
            {global_conservative, {kF65, 0.006e-6, 2.734, 8 * kF65, 2.2, 0.77e-6, kCu}},
            {dram_wordline, {kF65, 0e-6, 0, 2 * kF65, 0, 0e-6, kCu}},
        }},
    },
    {
        90,
        {{
            {local_aggressive, {kF90, 0.01e-6, 2.709, 2.5 * kF90, 2.4, 0.48e-6, kCu}},
            {local_conservative, {kF90, 0.008e-6, 3.038, 2.5 * kF90, 2.0, 0.48e-6, kCu}},
            {semi_aggressive, {kF90, 0.01e-6, 2.709, 4 * kF90, 2.4, 0.48e-6, kCu}},
            {semi_conservative, {kF90, 0.008e-6, 3.038, 4 * kF90, 2.0, 0.48e-6, kCu}},
            {global_aggressive, {kF90, 0.01e-6, 2.709, 8 * kF90, 2.7, 0.96e-6, kCu}},
            {global_conservative, {kF90, 0.008e-6, 3.038, 8 * kF90, 2.2, 1.1e-6, kCu}},
            {dram_wordline, {kF90, 0e-6, 0, 2 * kF90, 0, 0e-6, kCu}},
        }},
    },
    {
        120,
        {{
            {local_aggressive, {kF120, 0.012e-6, 3.3, 240e-9, 1.6, 0.48e-6, kCu}},
            {local_conservative, {kF120, 0.01e-6, 3.6, 240e-9, 1.4, 0.48e-6, kCu}},
            {semi_aggressive, {kF120, 0.012e-6, 3.3, 320e-9, 1.7, 0.48e-6, kCu}},
            {semi_conservative, {kF120, 0.01e-6, 3.6, 320e-9, 1.5, 0.48e-6, kCu}},
            {global_aggressive, {kF120, 0.012e-6, 3.3, 475e-9, 2.1, 0.96e-6, kCu}},
            {global_conservative, {kF120, 0.01e-6, 3.6, 475e-9, 1.9, 1.1e-6, kCu}},
            {dram_wordline, {kF120, 0e-6, 0, 2 * kF120, 0, 0e-6, kCu}},
        }},
    },
    {
        200,
        {{
            {local_aggressive, {kF200, 0.016e-6, 3.75, 0.45e-6, 2.4, 1e-6, kCu}},
            {local_conservative, {
                kF200, 0.016e-6 * 0.8, 3.75 * 3.038 / 2.709,
                0.45e-6, 1.2, 1e-6, kCu,
            }},
            {semi_aggressive, {kF200, 0.016e-6, 3.75, 0.575e-6, 2.1, 1e-6, kCu}},
            {semi_conservative, {
                kF200, 0.016e-6 * 0.8, 3.75 * 3.038 / 2.709,
                0.575e-6, 2.1 * 2.0 / 2.4, 1e-6, kCu,
            }},
            {global_aggressive, {kF200, 0.016e-6, 3.75, 0.945e-6, 2.1, 2e-6, kCu}},
            {global_conservative, {
                kF200, 0.016e-6 * 0.8, 3.75 * 3.038 / 2.709,
                0.945e-6, 2.1 * 2.2 / 2.7, 2.2e-6, kCu,
            }},
            {dram_wordline, {kF200, 0e-6, 0, 2 * kF200, 0, 0e-6, kCu}},
        }},
    },
};

}  // namespace

bool FindWireProcessSpec(
        int featureSizeInNano,
        WireType wireType,
        WireProcessSpec *spec) {
    for (const auto &node : kProcessNodes) {
        if (featureSizeInNano > node.maxFeatureSizeInNano) {
            continue;
        }

        for (const auto &entry : node.entries) {
            if (entry.wireType == wireType) {
                *spec = entry.spec;
                return true;
            }
        }

        *spec = node.entries.back().spec;
        return true;
    }

    return false;
}
