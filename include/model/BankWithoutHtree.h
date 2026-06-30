#ifndef BANKWITHOUTHTREE_H_
#define BANKWITHOUTHTREE_H_

#include "Bank.h"
#include "Mat.h"
#include "typedef.h"

class BankWithoutHtree: public Bank {
    public:
        BankWithoutHtree() {
            initialized = false;
            invalid = false;
        }
        BankWithoutHtree(const BankWithoutHtree&) = delete;
        BankWithoutHtree& operator=(const BankWithoutHtree&) = delete;
        BankWithoutHtree(BankWithoutHtree&&) noexcept = default;
        BankWithoutHtree& operator=(BankWithoutHtree&&) noexcept = default;
        virtual ~BankWithoutHtree() = default;

        /* Functions */
        void Initialize(int _numRowMat, int _numColumnMat, long long _capacity,
                long _blockSize, int _numActiveMatPerRow,
                int _numActiveMatPerColumn, int _muxSenseAmp, bool _internalSenseAmp, int _muxOutputLev1, 
                int _muxOutputLev2,
                int _numRowSubarray, int _numColumnSubarray,
                int _numActiveSubarrayPerRow, int _numActiveSubarrayPerColumn,
                BufferDesignTarget _areaOptimizationLevel, CAMType _camType,
                SearchFunction _searchFunction, std::shared_ptr<EvaCamConfig> _config,
                const Wire &_localWire, const Wire &_globalWire,
                const CAM_Opt &_CAM_opt);

        void CalculateArea();
        void CalculateRC();
        void CalculateLatencyAndPower();

        int numAddressBit;		   /* Number of bank address bits */
        int numAddressBitRouteToMat;  /* Number of address bits routed to mat */
        int numDataBitRouteToMat;   /* Number of data bits routed to mat */

        std::unique_ptr<Mux>	globalBitlineMux;
        std::unique_ptr<SenseAmp> globalSenseAmp;
};

#endif /* BANKWITHOUTHTREE_H_ */
