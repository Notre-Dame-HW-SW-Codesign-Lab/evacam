/*
 * Inherit SubArray.cpp from NVsim_origin
 * Modification: (1) different components (2) RC calculation on ML (3) provide sensing constraints
 * (4) result based on input and miss-rate patterns
 */

#ifndef CAM_SUBARRAY_H_
#define CAM_SUBARRAY_H_

#include "FunctionUnit.h"
#include "Precharger.h"
#include "CAM_SenseAmp.h"
#include "CAM_DataBuffer.h"
#include "CAM_LevelShifter.h"
#include "CAM_InputEncoder.h"
#include "CAM_OutputAccumulator.h"
#include "CAM_PriorityEncoder.h"
#include "RowDecoder.h"
#include "Mux.h"
#include "Wire.h"
#include "typedef.h"
#include "CAM_Line.h"
#include "MemCell.h"
#include "EvaCamConfig.h"
#include "EvaCAMMatchResult.h"
#include "model/VariationSampler.h"
#include <random>
#include <vector>

struct CAMResistanceSample {
    double mlWireRes = 0;
    double cellResOn = 0;
    double cellResOff = 0;
    double accessRes = 0;
    double matchRes = 0;
    double accessResOff = 0;
    double matchResOff = 0;
    bool hasAggregateMatchlineRes = false;
    double oneMissEffectiveCellRes = 0;
    double allMatchEffectiveCellRes = 0;
};

struct CamSubArrayTestAccessor;

struct CAMMetricStats {
    bool available = false;
    double nominal = 0;
    double sample = 0;
    double mean = 0;
    double stddev = 0;
    double min = 0;
    double max = 0;
    double p95 = 0;
};

struct CAMVariationSummary {
    bool enabled = false;
    std::string mode = "nominal";
    int samples = 0;
    CAMMetricStats matchlineDelay;
    CAMMetricStats searchLatency;
    CAMMetricStats searchDynamicEnergy;
    CAMMetricStats senseMargin;
    CAMMetricStats referenceDelay;
};

struct CAMVariationSample {
    int sample = 0;
    std::string cornerLabel;
    std::string memoryDeviceResOnCorner = "nominal";
    std::string memoryDeviceResOffCorner = "nominal";
    double matchlineDelay = 0;
    double searchLatency = 0;
    double searchDynamicEnergy = 0;
    double senseMargin = 0;
    double referDelay = 0;
};

class CAM_SubArray: public FunctionUnit {
    public:
        CAM_SubArray() {
            initialized = false;
            invalid = false;
            latencyCalculated = false;
            // TODO: Figure out how to calculate this if it doesn't go in the correct if statement
            matchlineDelay = 0;
            matchlineWireRes = 0;
            nominalMatchlineWireRes = 0;
            nominalResCellAccess = 0;
            nominalResMatchTran = 0;
            nominalResMemCellOn = 0;
            nominalResCellAccessOff = 0;
            nominalResMatchTranOff = 0;
            nominalResMemCellOff = 0;
            voltageMemCellOn = 0;    // TODO: Actually calculate this somewhere
            WriteDriverArea = 0;     // TODO: This is calculate but says unitialized sometimes
        }
        CAM_SubArray(const CAM_SubArray&) = delete;
        CAM_SubArray& operator=(const CAM_SubArray&) = delete;
        CAM_SubArray(CAM_SubArray&&) noexcept = default;
        CAM_SubArray& operator=(CAM_SubArray&&) noexcept = default;
        virtual ~CAM_SubArray() = default;

        /* Functions */
        void PrintProperty();
        void Initialize(long long _numRow, long long _numColumn, bool _split,
                int _muxSenseAmp, bool _internalSenseAmp, int _muxOutputLev1, int _muxOutputLev2,
                BufferDesignTarget _RowDecMergeOptLevel, BufferDesignTarget _RowDriverOptLevel,
                bool _withInputEnc, TypeOfInputEncoder _typeInputEnc, bool _customInputEnc,
                TypeOfSenseAmp _typeSenseAmp, bool _customSenseAmp, bool _withWriteDriver,
                bool _withOutputAcc, bool _withPriorityEnc, BufferDesignTarget _PriorityOptLevel,
                bool _withInputBuf, bool _withOutputBuf, CAMType _camType, SearchFunction _searchFunction, 
                bool _withVariation, std::shared_ptr<EvaCamConfig> _config, 
                const Wire &_localWire, const CAM_Opt &_CAM_opt
                );

        void ReadCustomDesign(char* _fileInputEnc, char* _fileSenseAmp);
        void CalculateArea();
        void CalculateLatency(double _rampInput);
        void CalculatePower();
        EvaCAMMatchResult EvaluateBinaryMatch(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult EvaluateBinaryMatchByMismatches(int mismatchCount) const;
        CAMResistanceSample BuildNominalResistanceSample() const;
        CAMResistanceSample BuildResistanceSample(unsigned int sampleIndex = 0) const;
        CAMResistanceSample BuildCellMonteCarloResistanceSample(unsigned int sampleIndex) const;
        CAMResistanceSample BuildCornerResistanceSample(unsigned int cornerIndex) const;
        CAMResistanceSample BuildVariationResistanceSample(unsigned int sampleIndex) const;
        double SampleVariationResistance(double nominal, double stdevFrac, unsigned int streamOffset, unsigned int sampleIndex) const;
        double SampleCellVariationResistance(double nominal, double stdevFrac, unsigned int streamOffset,
                unsigned int sampleIndex, unsigned int cellIndex) const;
        void UpdateVariationTimingSummary();
        void UpdateVariationPowerSummary();
        double SampleCellReadEnergy(const CAMResistanceSample &sample, double sampleMatchlineDelay) const;

    private:
        friend struct CamSubArrayTestAccessor;
        int CountMismatches(const std::vector<int> &stored, const std::vector<int> &query) const;
        double EffectiveMatchlineCellResistance(int mismatches, double cellResOn, double cellResOff) const;
        double EffectiveMcamStateResistance(double stateResistance, double baseStateResistance) const;
        std::vector<double> EffectiveMcamStateResistances() const;
        double MeanSquaredSearchVoltage(int rowPortIndex) const;
        double CalculateSearchlineDriveEnergy() const;
        double McamStateTau(double effectiveStateResistance, double mlWireRes) const;
        std::vector<double> McamStateTaus(const std::vector<double> &effectiveStateResistances) const;
        double McamStateDelay(double stateTau, double *ramp) const;
        std::vector<double> McamStateDelays(const std::vector<double> &stateTaus);
        void CalculateSearchPathLatenciesAfterMatchline();
        double MatchlineDischargeTau(double effectiveCellRes, double mlWireRes) const;
        double MatchlineEffectiveResistance(const CAMResistanceSample &sample, int mismatches) const;
        double MatchlineAllMatchTau(const CAMResistanceSample &sample) const;
        double MatchlineAllMatchTau(double cellResOff, double mlWireRes) const;
        double MatchlineSenseMargin(double tauAllMatch, double tauOneMiss, double senseTime) const;
        double MatchlineBeta(double effectiveCellRes, int activeDischargePaths = 1) const;
        double MatchlineHorowitzDelay(double tau, double effectiveCellRes, double *ramp,
                int activeDischargePaths = 1) const;

    public:
        /* Properties */
        std::unique_ptr<CAM_DataBuffer> inputBuf;
        std::unique_ptr<CAM_DataBuffer> outputBuf;
        std::unique_ptr<CAM_LevelShifter> inputLS;
        std::unique_ptr<CAM_LevelShifter> outputLS;
        std::unique_ptr<CAM_InputEncoder> inputEnc;
        std::unique_ptr<RowDecoder> RowDecMergeNand; /* Last level NAND and driver for wordline predecoder */

        std::vector<std::unique_ptr<RowDecoder>> RowDriver; /* NAND and driver for WL/SL/etc.; there may be multiple, e.g. ISSCC15 3T1R */

        std::unique_ptr<Precharger> precharger;

        std::unique_ptr<RowDecoder> ColDecMergeNand;
        std::vector<std::unique_ptr<RowDecoder>> WriteDriver;
        std::vector<std::unique_ptr<Mux>> ColMux;	/*  mux for ml/bl, there are could be multiple or single, e.g., JSSC 2T2R */

        std::unique_ptr<CAM_SenseAmp>	senseAmp;

        std::unique_ptr<RowDecoder>	senseAmpMuxLev1Nand;
        std::unique_ptr<Mux>			senseAmpMuxLev1;
        std::unique_ptr<RowDecoder>	senseAmpMuxLev2Nand;
        std::unique_ptr<Mux>			senseAmpMuxLev2;

        std::unique_ptr<CAM_OutputAccumulator> outputAcc;
        std::unique_ptr<CAM_PriorityEncoder> priorityEnc;

        CAMType camType; /* For CAM type specification */
        SearchFunction searchFunction; /* For search function specifcation */


        bool initialized;	/* Initialization flag */
        bool invalid;		/* Indicate that the current configuration is not valid, pass down to all the sub-components */
        bool latencyCalculated; /* CalculatePower depends on latency-derived fields. */

        // structure
        long long numRow;			/* Number of rows, number of words*/
        long long numColumn;		/* Number of columns, word size */
        int muxSenseAmp;		/* How many bitlines connect to one sense amplifier */
        int muxOutputLev1;		/* How many sense amplifiers connect to one output bit, level-1 */
        int muxOutputLev2;		/* How many sense amplifiers connect to one output bit, level-2 */
        long long numSenseAmp;	/* Number of sense amplifiers */
        long long numInputEnc;

        double lenRow;
        double lenCol;
        int indexMatchline;
        int indexBitline;
        int numBitline;
        std::vector<CAM_Line> Row;
        std::vector<CAM_Line> Col;


        // for input and output buffer
        bool withInputBuf;
        bool withOutputBuf;


        // for write driver
        bool withWriteDriver;
        double WriteDriverArea;
        double WriteDriverLatency;
        double WriteDriverDyn;
        double WriteDriverLeakage;

        // for input encoder
        bool withInputEnc;
        TypeOfInputEncoder typeInputEnc;
        bool customInputEnc;


        // for row drivers
        BufferDesignTarget DecMergeOptLevel;
        bool RowDecMergeInv;
        BufferDesignTarget DriverOptLevel;


        // for precharger
        double voltagePrecharge;

        // for sense amplifier
        bool internalSenseAmp; 	/* Indicate whether sense amp is within subarray */
        bool voltageSense;		/* Whether the sense amplifier is voltage-sensing */
        double senseVoltage;	/* Minimum sensible voltage */
        TypeOfSenseAmp typeSenseAmp;
        bool customSenseAmp;
        double senseMargin;
        double referDelay;
        double volMatchDrop;
        double volAllMissDrop;
        double resTotalCell;
        double capTotalCell;

        double columnDecoderLatency;	/* The worst-case mux latency, Unit: s */
        double bitlineDelayOn;  /* Bitline delay of LRS, Unit: s */
        double bitlineDelayOff; /* Bitline delay of HRS, Unit: s */
        double matchlineDelayOn;  /* matchline delay of LRS, Unit: s */
        double matchlineDelayOff; /* matchline delay of HRS, Unit: s */
        double resEquivalentOn;          /* resInSerialForSenseAmp in parallel with resMemCellOn, Unit: ohm */
        double resEquivalentOff;          /* resInSerialForSenseAmp in parallel with resMemCellOn, Unit: ohm */
        double resInSerialForSenseAmp; /* Serial resistance of voltage-in voltage sensing as a voltage divider, Unit: ohm */
        double bitlineDelay;	/* Bitline delay, Unit: s */
        double matchlineDelay;	/* Matchline delay, Unit: s */
        double matchlineDelayForApprox[128];
        double searchLatencyForApprox[128];
        int MaxDetectCellNumber; 

        // for accumulator
        bool withOutputAcc;

        // for variation model
        bool withVariation;
        /* Random number generator for variation distribution*/
        std::mt19937 variDistribution;

        // for priority encoder
        bool withPriorityEnc;
        BufferDesignTarget PriorityOptLevel;

        // for cell  ??
        double resMemCellOff;  /* HRS resistance, Unit: ohm */
        double resMemCellOn;   /* LRS resistance, Unit: ohm */
        double voltageMemCellOff; /* Voltage drop on HRS during read operation, Unit: V */
        double voltageMemCellOn;   /* Voltage drop on LRS druing read operation, Unit: V */
        double resCellAccess; /* Resistance of access device, Unit: ohm */
        double capCellAccess; /* Capacitance of access device, Unit: ohm */
        double resMatchTran;
        double capMatchTran;
        double nominalResCellAccess;
        double nominalResMatchTran;
        double nominalResMemCellOff;
        double nominalResMemCellOn;
        double nominalResCellAccessOff;
        double nominalResMatchTranOff;
        double nominalMatchlineWireRes;
        double matchlineWireRes;
        CAMResistanceSample sampledResistance;
        CAMVariationSummary variationSummary;
        std::vector<CAMVariationSample> variationSamples;

        double decoderLatency;
        double bitlineRamp;
        double matchlineRamp;
        double chargeLatency;
        double searchLatency;
        double searchDynamicEnergy;
        double searchlineDriveDynamicEnergy;
        double rampOutput;
        double resetEnergyPerBit;
        double setEnergyPerBit;
        double senseAmpLatency;

        double searchAverage;

        //Array size vs. Accuracy tradeoff
        double ArrayWidth;
        double tau;
        // double MatchlineSenseMargin;
        double temptau;
        double resmiss;
        double totalcapcell;

        // useless items
        bool split;			/* NOT USED YET for NVSIM: Whether the row decoder is at the middle of subarrays */
        double lenWordline;	/* Length of wordlines, Unit: m */
        double lenBitline;	/* Length of bitlines, Unit: m */
        double capWordline;	/* Wordline capacitance, Unit: F */
        double capBitline;	/* Bitline capacitance, Unit: F */
        double resWordline;	/* Wordline resistance, Unit: ohm */
        double resBitline;	/* Bitline resistance, Unit: ohm */
        double maxCurrentBL;
        std::unique_ptr<RowDecoder>	rowDecoder;
        std::unique_ptr<RowDecoder>	bitlineMuxDecoder;
        std::unique_ptr<RowDecoder>	senseAmpMuxLev1Decoder;
        std::unique_ptr<RowDecoder>	senseAmpMuxLev2Decoder;
        std::unique_ptr<Mux>		bitlineMux;

        Wire localWire;
        CAM_Opt CAM_opt;
};

#endif /* CAM_SUBARRAY_H_ */
