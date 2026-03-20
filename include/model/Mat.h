#ifndef MAT_H_
#define MAT_H_

#include "FunctionUnit.h"
#include "SubArray.h"
#include "PredecodeBlock.h"
#include "typedef.h"
#include "Comparator.h"
#include "CAM_SubArray.h"
#include "Wire.h"


class Mat: public FunctionUnit {
    public:
        Mat() {
            initialized = false;
            invalid = false;
        }
        Mat(const Mat&) {}
        virtual ~Mat() {}

        /* Functions */
        void PrintProperty();
        void Initialize(int _numRowSubarray, int _numColumnSubarray, int _numAddressBit, long _numDataBit,
                int _numWay, int _numRowPerSet, bool _split, int _numActiveSubarrayPerRow, 
                int _numActiveSubarrayPerColumn, int _muxSenseAmp, bool _internalSenseAmp, 
                int _muxOutputLev1, int _muxOutputLev2, BufferDesignTarget _areaOptimizationLevel, 
                MemoryType _memoryType, CAMType _camType, SearchFunction _searchFunction, 
                std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire,
                std::shared_ptr<CAM_Opt> _CAM_opt);
        void CalculateArea();
        void CalculateRC();
        void CalculateLatency(double _rampInput);
        void CalculatePower();
        Mat & operator=(const Mat &);

        /* Properties */
        bool initialized;	/* Initialization flag */
        bool invalid;		/* Indicate that the current configuration is not valid, pass down to all the sub-components */
        bool internalSenseAmp;
        int numRowSubarray;		/* Number of subarray rows in a mat */
        int numColumnSubarray;	/* Number of subarray columns in a mat */
        int numAddressBit;		/* Number of mat address bits */
        long numDataBit;		/* Number of mat mem_data bits */
        int numWay;				/* Number of cache ways distributed to this mat, non-cache it is 1 */
        int numRowPerSet;		/* For cache design, the number of wordlines which a set is partitioned into */
        bool split;			/* Whether the row decoder is at the middle of subarrays */
        int numActiveSubarrayPerRow;	/* For different access types */
        int numActiveSubarrayPerColumn;	/* For different access types */
        int muxSenseAmp;	/* How many bitlines connect to one sense amplifier */
        int muxOutputLev1;	/* How many sense amplifiers connect to one output bit, level-1 */
        int muxOutputLev2;	/* How many sense amplifiers connect to one output bit, level-2 */
        BufferDesignTarget areaOptimizationLevel;
        MemoryType memoryType;


        CAMType camType; /* For CAM type specification */
        SearchFunction searchFunction; /* For search function specifcation */

        double predecoderLatency;	/* The maximum latency of all the predecoder blocks, Unit: s */
        double areaAllPredecoderBlocks;

        std::shared_ptr<CAM_SubArray> subarray;
        std::shared_ptr<PredecodeBlock> rowPredecoderBlock1;
        std::shared_ptr<PredecodeBlock> rowPredecoderBlock2;
        std::shared_ptr<PredecodeBlock> bitlineMuxPredecoderBlock1;
        std::shared_ptr<PredecodeBlock> bitlineMuxPredecoderBlock2;
        std::shared_ptr<PredecodeBlock> senseAmpMuxLev1PredecoderBlock1;
        std::shared_ptr<PredecodeBlock> senseAmpMuxLev1PredecoderBlock2;
        std::shared_ptr<PredecodeBlock> senseAmpMuxLev2PredecoderBlock1;
        std::shared_ptr<PredecodeBlock> senseAmpMuxLev2PredecoderBlock2;

        std::shared_ptr<Wire> localWire;
        std::shared_ptr<CAM_Opt> CAM_opt;

        Comparator comparator;
};

#endif /* MAT_H_ */
