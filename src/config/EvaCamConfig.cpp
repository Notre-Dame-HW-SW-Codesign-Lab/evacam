#include "EvaCamConfig.h"
#include "Result.h"
#include "config/EvaCamYamlLoader.h"
#include "config/TechnologyLoader.h"

#include <array>
#include <stdexcept>
//#include <magic_enum.hpp>

namespace {
}

void EvaCamConfig::SetDeepExploration(bool enabled) {
    exploration = ExplorationSpec::Default();
    if (enabled) {
        exploration.ApplyDeepExplorationDefaults();
    } else {
        exploration.deepExploration = false;
        exploration.geometry.numRowMat = IntValueDomain::PowersOfTwo(1, 4);
        exploration.geometry.numColumnMat = IntValueDomain::PowersOfTwo(1, 4);
        exploration.geometry.numRowSubarray = IntValueDomain::PowersOfTwo(1, 8);
        exploration.geometry.numColumnSubarray = IntValueDomain::PowersOfTwo(1, 8);
        exploration.geometry.muxSenseAmp = IntValueDomain::PowersOfTwo(1, 32);
        exploration.geometry.muxOutputLev1 = IntValueDomain::PowersOfTwo(1, 32);
        exploration.geometry.muxOutputLev2 = IntValueDomain::PowersOfTwo(1, 32);
        exploration.wires.localWireType = IntValueDomain::Sequential(local_aggressive, semi_conservative);
        exploration.wires.globalWireType = IntValueDomain::Sequential(semi_aggressive, global_conservative);
        exploration.wires.localWireRepeaterType = IntValueDomain::Sequential(repeated_none, repeated_50);
        exploration.wires.globalWireRepeaterType = IntValueDomain::Sequential(repeated_none, repeated_50);
    }

    resolvedExploration = ExplorationSpaceResolver::Resolve(exploration);
}

ResultLimits EvaCamConfig::BuildResultLimits(const std::vector<std::shared_ptr<Result>> &bestResults) const {
    constexpr std::array<OptimizationTarget, 8> requiredTargets = {
        read_latency_optimized,
        write_latency_optimized,
        read_energy_optimized,
        write_energy_optimized,
        read_edp_optimized,
        write_edp_optimized,
        leakage_optimized,
        area_optimized,
    };
    for (OptimizationTarget target : requiredTargets) {
        const std::size_t index = static_cast<std::size_t>(target);
        if (index >= bestResults.size() || !bestResults[index]
                || !bestResults[index]->bank) {
            throw std::invalid_argument(
                    "BuildResultLimits requires a populated result and bank for every constraint target.");
        }
    }

    ResultLimits limits{};
    limits.readLatency = bestResults[read_latency_optimized]->bank->readLatency * (constraints.readLatency + 1);
    limits.writeLatency = bestResults[write_latency_optimized]->bank->writeLatency * (constraints.writeLatency + 1);
    limits.readDynamicEnergy = bestResults[read_energy_optimized]->bank->readDynamicEnergy
        * (constraints.readDynamicEnergy + 1);
    limits.writeDynamicEnergy = bestResults[write_energy_optimized]->bank->writeDynamicEnergy
        * (constraints.writeDynamicEnergy + 1);
    limits.leakage = bestResults[leakage_optimized]->bank->leakage * (constraints.leakage + 1);
    limits.area = bestResults[area_optimized]->bank->area * (constraints.area + 1);
    limits.readEdp = bestResults[read_edp_optimized]->bank->readLatency
        * bestResults[read_edp_optimized]->bank->readDynamicEnergy * (constraints.readEdp + 1);
    limits.writeEdp = bestResults[write_edp_optimized]->bank->writeLatency
        * bestResults[write_edp_optimized]->bank->writeDynamicEnergy * (constraints.writeEdp + 1);
    return limits;
}

void EvaCamConfig::ApplyResultLimits(const ResultLimits &limits,
        const std::vector<std::shared_ptr<Result>> &results) const {
    for (const auto &result : results) {
        if (!result || !result->bank) {
            throw std::invalid_argument(
                    "ApplyResultLimits requires every result to contain a bank.");
        }
    }
    for (const auto &result : results) {
        result->reset();
        result->limitReadLatency = limits.readLatency;
        result->limitWriteLatency = limits.writeLatency;
        result->limitReadDynamicEnergy = limits.readDynamicEnergy;
        result->limitWriteDynamicEnergy = limits.writeDynamicEnergy;
        result->limitReadEdp = limits.readEdp;
        result->limitWriteEdp = limits.writeEdp;
        result->limitArea = limits.area;
        result->limitLeakage = limits.leakage;
    }
}

void EvaCamConfig::ReadConfigFromFile(const std::string &inputFile) {
    EvaCamYamlLoader::Load(inputFile, *this);
    technology = TechnologyLoader::Load(input, peripherals, &variation);
    if (variation.enabled && variation.mode == "corner") {
        if (variation.hasUserSamples) {
            logger.Verbose() << "[Input] Warning: variation.samples is ignored because corner mode is deterministic.";
        }
        if (variation.hasUserSeed) {
            logger.Verbose() << "[Input] Warning: variation.seed is ignored because corner mode is deterministic.";
        }
    }
}
