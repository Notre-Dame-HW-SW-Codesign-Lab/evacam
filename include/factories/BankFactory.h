#ifndef FACTORIES_BANKFACTORY_H_
#define FACTORIES_BANKFACTORY_H_

#include <memory>

#include "typedef.h"

class EvaCamConfig;
class Bank;
class Wire;

class BankFactory {
    public:
        static std::shared_ptr<Bank> CreateBank(const std::shared_ptr<EvaCamConfig> &config);
        static void InitializeBank(const std::shared_ptr<EvaCamConfig> &config,
                const std::shared_ptr<Bank> &bank, int numRowMat, int numColumnMat,
                long long capacityBits, long blockSizeBits, int associativityValue, int numRowPerSet,
                int numActiveMatPerRow, int numActiveMatPerColumn, int muxSenseAmp, int muxOutputLev1,
                int muxOutputLev2, int numRowSubarray, int numColumnSubarray,
                int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn,
                BufferDesignTarget areaOptimizationLevel, MemoryType memoryType,
                const std::shared_ptr<Wire> &localWire, const std::shared_ptr<Wire> &globalWire,
                const CAM_Opt &camOpt);
};

#endif /* FACTORIES_BANKFACTORY_H_ */
