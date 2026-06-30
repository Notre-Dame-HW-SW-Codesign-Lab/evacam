#ifndef FACTORIES_BANKFACTORY_H_
#define FACTORIES_BANKFACTORY_H_

#include <memory>

#include "typedef.h"

class EvaCamConfig;
class Bank;
class Wire;

class BankFactory {
    public:
        static std::shared_ptr<Bank> CreateBank(const EvaCamConfig &config);
        static void InitializeBank(const std::shared_ptr<EvaCamConfig> &config,
                const std::shared_ptr<Bank> &bank, int numRowMat, int numColumnMat,
                long long capacityBits, long blockSizeBits,
                int numActiveMatPerRow, int numActiveMatPerColumn, int muxSenseAmp, int muxOutputLev1,
                int muxOutputLev2, int numRowSubarray, int numColumnSubarray,
                int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn,
                BufferDesignTarget areaOptimizationLevel,
                const Wire &localWire, const Wire &globalWire,
                const CAM_Opt &camOpt);
};

#endif /* FACTORIES_BANKFACTORY_H_ */
