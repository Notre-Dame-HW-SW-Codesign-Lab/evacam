#ifndef BANKWITHHTREE_H_
#define BANKWITHHTREE_H_

#include "Bank.h"

class BankWithHtree: public Bank {
    public:
        struct HtreeLevel {
            int addressBits = 0;
            int dataBits = 0;
            int wireGroups = 0;
            int totalWireGroups = 0;
            int activeWireGroups = 0;
            double length = 0;
        };

        BankWithHtree();
        BankWithHtree(const BankWithHtree&) = delete;
        BankWithHtree& operator=(const BankWithHtree&) = delete;
        BankWithHtree(BankWithHtree&&) noexcept = default;
        BankWithHtree& operator=(BankWithHtree&&) noexcept = default;
        ~BankWithHtree() override = default;
        void Initialize(int _numRowMat, int _numColumnMat, long long _capacity,
                long _blockSize, int _numActiveMatPerRow,
                int _numActiveMatPerColumn, int _muxSenseAmp, bool _internalSenseAmp, 
                int _muxOutputLev1, int _muxOutputLev2, int _numRowSubarray, int _numColumnSubarray,
                int _numActiveSubarrayPerRow, int _numActiveSubarrayPerColumn,
                BufferDesignTarget _areaOptimizationLevel, CAMType _camType,
                SearchFunction _searchFunction, std::shared_ptr<EvaCamConfig> _config,
                const Wire &_localWire, const Wire &_globalWire,
                const CAM_Opt &_CAM_opt);

        void CalculateArea();
        void CalculateRC();
        void CalculateLatencyAndPower();

        int numAddressBit;		/* Number of bank address bits */
        int numDataBit;		/* Number of bank data bits routed with the address */

        int levelHorizontal;			/* The number of horizontal levels */
        int levelVertical;				/* The number of vertical levels */

        std::vector<HtreeLevel> horizontalLevels;
        std::vector<HtreeLevel> verticalLevels;

    private:
        struct RoutingState {
            int horizontalLevelsRemaining = 0;
            int verticalLevelsRemaining = 0;
            int activeRowsRemaining = 0;
            int activeColumnsRemaining = 0;
            int addressBitsToRoute = 0;
            int dataBitsToRoute = 0;
            int horizontalWireTier = 1;
            int verticalWireTier = 1;
        };

        struct WireAreaModel {
            int sharingWidth = 1;
            double effectivePitch = 0;
        };

        void MarkInvalid(const char *reason);
        void MarkInvalidInitialized(const char *reason);
        int TotalHorizontalBits(int level) const;
        int TotalVerticalBits(int level) const;
        WireAreaModel GetWireAreaModel() const;
        void AccumulateHtreeLevelLatencyAndPower(const HtreeLevel &level,
                int totalBits);
        bool HasRoutableBits(const RoutingState &state) const;
        RoutingState CreateInitialRoutingState() const;
        bool InitializeFirstHorizontalLevel(RoutingState &state);
        bool ReduceExtraHorizontalLevels(RoutingState &state);
        bool ReduceExtraVerticalLevels(RoutingState &state);
        bool ReducePairedHorizontalAndVerticalLevels(RoutingState &state);
        bool FinalizeMatRoutingBits(RoutingState &state);
};

#endif /* BANKWITHHTREE_H_ */
