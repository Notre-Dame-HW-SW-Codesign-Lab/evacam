#ifndef BANKWITHHTREE_H_
#define BANKWITHHTREE_H_

#include "Bank.h"

class BankWithHtree: public Bank {
    public:
        BankWithHtree();
        BankWithHtree(const BankWithHtree&) = delete;
        BankWithHtree& operator=(const BankWithHtree&) = delete;
        BankWithHtree(BankWithHtree&&) noexcept = default;
        BankWithHtree& operator=(BankWithHtree&&) noexcept = default;
        ~BankWithHtree() override = default;
        void Initialize(int _numRowMat, int _numColumnMat, long long _capacity,
                long _blockSize, int _associativity, int _numRowPerSet, int _numActiveMatPerRow,
                int _numActiveMatPerColumn, int _muxSenseAmp, bool _internalSenseAmp, 
                int _muxOutputLev1, int _muxOutputLev2, int _numRowSubarray, int _numColumnSubarray,
                int _numActiveSubarrayPerRow, int _numActiveSubarrayPerColumn,
                BufferDesignTarget _areaOptimizationLevel, MemoryType _memoryType, CAMType _camType, 
                SearchFunction _searchFunction, std::shared_ptr<EvaCamConfig> _config,
                const Wire &_localWire, const Wire &_globalWire,
                const CAM_Opt &_CAM_opt);

        void CalculateArea();
        void CalculateRC();
        void CalculateLatencyAndPower();

        int numAddressBit;		/* Number of bank address bits */
        int numDataDistributeBit;	/* Number of bank mem_data bits (these bits will be distributed along with the address) */
        int numDataBroadcastBit;	/* Number of bank mem_data bits (these bits will be broadcasted at every node) */

        int levelHorizontal;			/* The number of horizontal levels */
        int levelVertical;				/* The number of vertical levels */

        std::vector<int> numHorizontalAddressBitToRoute;  /* The number of horizontal bits to route on level x */
        std::vector<int> numHorizontalDataDistributeBitToRoute;	/* The number of horizontal mem_data-in bits to route on level x */
        std::vector<int> numHorizontalDataBroadcastBitToRoute;		/* The number of horizontal mem_data-out bits to route on level x */
        std::vector<int> numHorizontalWire;        /* The number of horizontal wire tiers on level x */
        std::vector<int> numSumHorizontalWire;     /* The number of total horizontal wire groups on level x */
        std::vector<int> numActiveHorizontalWire;  /* The number of active horizontal wire groups on level x */
        std::vector<double> lengthHorizontalWire;	/* The length of horizontal wires on level x, Unit: m */
        std::vector<int> numVerticalAddressBitToRoute;	/* The number of vertical address bits to route on level x */
        std::vector<int> numVerticalDataDistributeBitToRoute;	/* The number of vertical mem_data-in bits to route on level x */
        std::vector<int> numVerticalDataBroadcastBitToRoute;	/* The number of vertical mem_data-out bits to route on level x */
        std::vector<int> numVerticalWire;          /* The number of vertical wire tiers on level x */
        std::vector<int> numSumVerticalWire;       /* The number of total vertical wire groups on level x */
        std::vector<int> numActiveVerticalWire;    /* The number of active vertical wire groups on level x */
        std::vector<double> lengthVerticalWire;	/* The length of vertical wires on level x, Unit: m */
};

#endif /* BANKWITHHTREE_H_ */
