#ifndef MODEL_NANDCAMBANK_H_
#define MODEL_NANDCAMBANK_H_

#include "Bank.h"

// NAND blocks hold complete keys. Active counts describe concurrent blocks,
// rather than the word partitions used by the parallel CAM bank models.
class NandCamBank : public Bank {
    public:
        void Initialize(int numRowMat, int numColumnMat, long long capacity,
                long blockSize, int numActiveMatPerRow, int numActiveMatPerColumn,
                int muxSenseAmp, bool internalSenseAmp, int muxOutputLev1,
                int muxOutputLev2, int numRowSubarray, int numColumnSubarray,
                int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn,
                BufferDesignTarget areaOptimizationLevel, CAMType camType,
                SearchFunction searchFunction, std::shared_ptr<EvaCamConfig> config,
                const Wire &localWire, const Wire &globalWire,
                const CAM_Opt &camOpt) override;
        void CalculateArea() override;
        void CalculateRC() override;
        void CalculateLatencyAndPower() override;

    private:
        void BuildRoutes();
        long long totalBlocks = 0;
        long long blockRounds = 0;
        double routeDelay = 0;
        double routeQueryEnergy = 0;
        double routeResultEnergy = 0;
        double routeLeakage = 0;
        double routeArea = 0;
        double programRouteEnergy = 0;
        double eraseRouteEnergy = 0;
};

#endif  // MODEL_NANDCAMBANK_H_
