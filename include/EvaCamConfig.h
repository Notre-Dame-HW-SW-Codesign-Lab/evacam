#ifndef EVACAMCONFIG_H_
#define EVACAMCONFIG_H_

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <stdint.h>
#include <vector>

#include "Logger.h"
#include "typedef.h"
#include "Technology.h"
#include "MemCell.h"

class Wire;
class Result;
class Bank;
struct CAM_Opt;

struct ResultLimits {
    double readLatency;
    double writeLatency;
    double readDynamicEnergy;
    double writeDynamicEnergy;
    double readEdp;
    double writeEdp;
    double area;
    double leakage;
};

class EvaCamConfig : public std::enable_shared_from_this<EvaCamConfig> {
    public:
        EvaCamConfig();
        EvaCamConfig(const EvaCamConfig&) = delete;
        virtual ~EvaCamConfig() {}

        /* Functions */
        void ReadConfigFromFile(const std::string & inputFile);
        void ValidateSupportedConfiguration();
        void TechSetup();
        void MemCellSetup();
        void FEFETTechSetup();
        void SetDeepExploration(bool enabled);
        std::string DefaultResultsYamlPath(const std::string &inputFile) const;
        std::string ExplorationCsvPath() const;
        std::shared_ptr<Wire> CreateDefaultLocalWire() const;
        std::shared_ptr<Wire> CreateDefaultGlobalWire() const;
        long long EffectiveCapacityBits() const;
        long EffectiveBlockSizeBits() const;
        int EffectiveAssociativity() const;
        bool HasFixedOuterGeometry() const;
        std::vector<int> NumRowMatValues() const;
        std::vector<int> NumColumnMatValues() const;
        std::vector<int> NumRowSubarrayValues() const;
        std::vector<int> NumActiveMatPerRowValues(int numColumnMat) const;
        std::vector<int> NumActiveMatPerColumnValues(int numRowMat) const;
        std::vector<int> NumColumnSubarrayValues() const;
        std::vector<int> NumActiveSubarrayPerRowValues(int numColumnSubarray) const;
        std::vector<int> NumActiveSubarrayPerColumnValues(int numRowSubarray) const;
        std::vector<int> MuxSenseAmpValues() const;
        std::vector<int> MuxOutputLev1Values() const;
        std::vector<int> MuxOutputLev2Values() const;
        std::vector<int> NumRowPerSetValues() const;
        std::vector<int> AreaOptimizationLevels() const;
        std::vector<int> RowDriverOptimizationLevels() const;
        std::vector<int> PriorityOptimizationLevels() const;
        std::vector<int> BitSerialWidthValues() const;
        bool IsValidPartitioning(long blockSizeBits, int numActiveMatPerRow, int numActiveMatPerColumn,
                int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn) const;
        std::shared_ptr<Bank> CreateBank() const;
        void InitializeBank(const std::shared_ptr<Bank> &bank, int numRowMat, int numColumnMat,
                long long capacityBits, long blockSizeBits, int associativityValue, int numRowPerSet,
                int numActiveMatPerRow, int numActiveMatPerColumn, int muxSenseAmp, int muxOutputLev1,
                int muxOutputLev2, int numRowSubarray, int numColumnSubarray,
                int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn,
                BufferDesignTarget areaOptimizationLevel, MemoryType memoryType,
                const std::shared_ptr<Wire> &localWire, const std::shared_ptr<Wire> &globalWire,
                const std::shared_ptr<CAM_Opt> &camOpt) const;
        bool IsFullExploration() const;
        bool ShouldWriteExplorationCsv() const;
        bool IsPruningEnabledForExploration() const;
        ResultLimits BuildResultLimits(const std::vector<std::shared_ptr<Result>> &bestResults) const;
        void ApplyResultLimits(const ResultLimits &limits, const std::vector<std::shared_ptr<Result>> &results) const;

        void PrintConfig();


        /* Properties */
        Logger logger;
        std::shared_ptr<Technology> tech;
        std::shared_ptr<Technology> FEFET_tech;
        std::shared_ptr<MemCell> cell;
        DesignTarget designTarget;		/* Cache, RAM, or CAM */
        SearchFunction searchFunction;  /* Exact match, best match, or threshold match */
        OptimizationTarget optimizationTarget;	/* Either read latency, write latency, read energy, write energy, leakage, or area */
        int processNode;				/* Process node (nm) */
        double sensemargin;
        int64_t capacity;				/* Memory/cache capacity, Unit: Byte */
        long wordWidth;					/* The width of each input/output word, Unit: bit */
        DeviceRoadmap deviceRoadmap;	/* ITRS roadmap: HP, LSTP, or LOP */
        std::string fileMemCell;				/* Input file name of memory cell type */
        int temperature;				/* The ambient temperature, Unit: K */
        double maxDriverCurrent;        /* The maximum driving current that the wordline/bitline driver can provide */
        WriteScheme writeScheme;		/* The write scheme */
        double readLatencyConstraint;	/* The allowed variation to the best read latency */
        double writeLatencyConstraint;	/* The allowed variation to the best write latency */
        double readDynamicEnergyConstraint;		/* The allowed variation to the best read dynamic energy */
        double writeDynamicEnergyConstraint;	/* The allowed variation to the best write dynamic energy */
        double leakageConstraint;		/* The allowed variation to the best leakage energy */
        double areaConstraint;			/* The allowed variation to the best leakage energy */
        double readEdpConstraint;		/* The allowed variation to the best read EDP */
        double writeEdpConstraint;		/* The allowed variation to the best write EDP */
        bool isConstraintApplied;		/* If any design constraint is applied */
        bool isPruningEnabled;			/* Whether to prune the results during the exploration */
        bool useCactiAssumption;		/* Use the CACTI assumptions on the array organization */

        int associativity;				/* Associativity, for cache design only */
        CacheAccessMode cacheAccessMode;	/* Access mode (for cache only) : normal, sequential, fast */

        long pageSize;					/* Unit: bit, For DRAM and NAND flash memory only */
        long flashBlockSize;				/* Unit: bit, For NAND flash memory only */


        bool isNVMdischarge; //undeclared variable


        RoutingMode routingMode;
        bool internalSensing;

        double maxNmosSize;				/* Default value is MAX_NMOS_SIZE in constant.h, however, user might change it, Unit: F */
        double MatchlineSenseMargin;  /* The worst-case sense margin (WC-SM) of matchline */

        std::string outputFilePrefix;

        int minNumRowMat;
        int maxNumRowMat;
        int minNumColumnMat;
        int maxNumColumnMat;
        int minNumActiveMatPerRow;
        int maxNumActiveMatPerRow;
        int minNumActiveMatPerColumn;
        int maxNumActiveMatPerColumn;
        int minNumRowSubarray;
        int maxNumRowSubarray;
        ///////////////////////limit the subarrary size can not be too small
        int minNumRow;
        int maxNumRow;
        int minNumColumn;
        int maxNumColumn;
        ///////////////////////////
        int minNumColumnSubarray;
        int maxNumColumnSubarray;
        int minNumActiveSubarrayPerRow;
        int maxNumActiveSubarrayPerRow;
        int minNumActiveSubarrayPerColumn;
        int maxNumActiveSubarrayPerColumn;
        int minNumActivePerColumn;
        int maxNumActivePerColumn;	
        int minMuxSenseAmp;
        int maxMuxSenseAmp;
        int minMuxOutputLev1;
        int maxMuxOutputLev1;
        int minMuxOutputLev2;
        int maxMuxOutputLev2;
        int minNumRowPerSet;
        int maxNumRowPerSet;
        int minAreaOptimizationLevel;	/* This one is actually OptPriority type */
        int maxAreaOptimizationLevel;	/* This one is actually OptPriority type */
        int minLocalWireType;			/* This one is actually WireType type */
        int maxLocalWireType;			/* This one is actually WireType type */
        int minGlobalWireType;			/* This one is actually WireType type */
        int maxGlobalWireType;			/* This one is actually WireType type */
        int minLocalWireRepeaterType;		/* This one is actually WireRepeaterType type */
        int maxLocalWireRepeaterType;		/* This one is actually WireRepeaterType type */
        int minGlobalWireRepeaterType;		/* This one is actually WireRepeaterType type */
        int maxGlobalWireRepeaterType;		/* This one is actually WireRepeaterType type */
        int minIsLocalWireLowSwing;		/* This one is actually boolean */
        int maxIsLocalWireLowSwing;		/* This one is actually boolean */
        int minIsGlobalWireLowSwing;		/* This one is actually boolean */
        int maxIsGlobalWireLowSwing;		/* This one is actually boolean */


        ///////////////////////////////////////////////////////////////////////////
        int minRowDriverOptLevel;
        int maxRowDriverOptLevel;
        bool withInputEnc;
        TypeOfInputEncoder typeInputEnc;
        bool customInputEnc;
        TypeOfSenseAmp typeSenseAmp;
        bool customSenseAmp;
        bool withOutputAcc;
        bool withPriorityEnc;
        int minPriorityOptLevel;
        int maxPriorityOptLevel;
        bool withWriteDriver;
        bool withInputBuffer;
        bool withOutputBuffer;

        ///////////////////////////////////////////////////////////////////////////
        // for single mat debug
        int numAddressBit;
        int numDataBit;
        int64_t realCapacity;
        bool NoPrechargeInc;
        bool IncludeLeakge;
        double scaledVoltage;
        std::string fileCustomSA;
        bool UseUpdatedLib;
        int minBitSerialWidth;
        int maxBitSerialWidth;

        double AddCapOnML;
};

#endif /* EVACAMCONFIG_H_ */
