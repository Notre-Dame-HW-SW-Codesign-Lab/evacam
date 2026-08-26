#include <stdexcept>

#include "factories/BankFactory.h"

#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "EvaCamConfig.h"

std::shared_ptr<Bank> BankFactory::CreateBank(const EvaCamConfig &config) {
    switch (config.input.routingMode) {
        case h_tree:
            return std::make_shared<BankWithHtree>();
        case non_h_tree:
            return std::make_shared<BankWithoutHtree>();
        default:
            throw std::invalid_argument("Unsupported bank routing mode");
    }
}

void BankFactory::InitializeBank(const std::shared_ptr<EvaCamConfig> &config,
        const std::shared_ptr<Bank> &bank, int numRowMat, int numColumnMat,
        long long modelCapacityCells, long physicalColumnsPerWord,
        int numActiveMatPerRow, int numActiveMatPerColumn, int muxSenseAmp, int muxOutputLev1,
        int muxOutputLev2, int numRowSubarray, int numColumnSubarray,
        int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn,
        BufferDesignTarget areaOptimizationLevel,
        const Wire &localWire, const Wire &globalWire,
        const CAM_Opt &camOpt) {
    bank->Initialize(numRowMat, numColumnMat, modelCapacityCells, physicalColumnsPerWord,
            numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp, config->input.internalSensing,
            muxOutputLev1, muxOutputLev2, numRowSubarray, numColumnSubarray,
            numActiveSubarrayPerRow, numActiveSubarrayPerColumn, areaOptimizationLevel,
            config->technology.cell->camType, config->input.searchFunction,
            config, localWire, globalWire, camOpt);
}
