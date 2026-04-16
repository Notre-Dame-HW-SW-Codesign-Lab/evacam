#include "factories/BankFactory.h"

#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "EvaCamConfig.h"

std::shared_ptr<Bank> BankFactory::CreateBank(const std::shared_ptr<EvaCamConfig> &config) {
    if (config->input.routingMode == h_tree) {
        return std::make_shared<BankWithHtree>();
    }
    return std::make_shared<BankWithoutHtree>();
}

void BankFactory::InitializeBank(const std::shared_ptr<EvaCamConfig> &config,
        const std::shared_ptr<Bank> &bank, int numRowMat, int numColumnMat,
        long long capacityBits, long blockSizeBits, int associativityValue, int numRowPerSet,
        int numActiveMatPerRow, int numActiveMatPerColumn, int muxSenseAmp, int muxOutputLev1,
        int muxOutputLev2, int numRowSubarray, int numColumnSubarray,
        int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn,
        BufferDesignTarget areaOptimizationLevel, MemoryType memoryType,
        const Wire &localWire, const Wire &globalWire,
        const CAM_Opt &camOpt) {
    bank->Initialize(numRowMat, numColumnMat, capacityBits, blockSizeBits, associativityValue, numRowPerSet,
            numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp, config->input.internalSensing,
            muxOutputLev1, muxOutputLev2, numRowSubarray, numColumnSubarray,
            numActiveSubarrayPerRow, numActiveSubarrayPerColumn, areaOptimizationLevel,
            memoryType, config->technology.cell->camType, config->input.searchFunction,
            config, localWire, globalWire, camOpt);
}
