#ifndef BANKWITHHTREE_H_
#define BANKWITHHTREE_H_

#include "Bank.h"

class BankWithHtree: public Bank {
    public:
        struct HtreeLevel {
            int addressBits = 0;
            int dataDistributeBits = 0;
            int dataBroadcastBits = 0;
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
                BufferDesignTarget _areaOptimizationLevel, MemoryType _memoryType,
                CAMType _camType, 
                SearchFunction _searchFunction, std::shared_ptr<EvaCamConfig> _config,
                const Wire &_localWire, const Wire &_globalWire,
                const CAM_Opt &_CAM_opt);

        void CalculateArea();
        void CalculateRC();
        void CalculateLatencyAndPower();

        int numAddressBit;		/* Number of bank address bits */
        /* Number of bank mem_data bits distributed along with the address */
        int numDataDistributeBit;
        /* Number of bank mem_data bits broadcasted at every node */
        int numDataBroadcastBit;

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
            int dataDistributeBitsToRoute = 0;
            int dataBroadcastBitsToRoute = 0;
            int horizontalWireTier = 1;
            int verticalWireTier = 1;
        };

        struct MatRouting {
            long blockSize = 0;
            int numWay = 1;
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
                int totalBits, int beta);
        bool HasRoutableBits(const RoutingState &state) const;
        RoutingState CreateInitialRoutingState() const;
        bool InitializeFirstHorizontalLevel(RoutingState &state);
        bool ReduceExtraHorizontalLevels(RoutingState &state);
        bool ReduceExtraVerticalLevels(RoutingState &state);
        bool ReducePairedHorizontalAndVerticalLevels(RoutingState &state);
        bool FinalizeMatRoutingBits(RoutingState &state);
        bool DetermineMatRouting(const RoutingState &state, MatRouting &matRouting);
};

#endif /* BANKWITHHTREE_H_ */
