#ifndef EVACAM_EXPLORER_H_
#define EVACAM_EXPLORER_H_

#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "EvaCamConfig.h"

class CAM_Result;
class Result;
class Bank;
class Wire;
struct CAM_Opt;

struct EvaCamExplorationResult {
    long long numSolution = 0;
    std::vector<std::shared_ptr<CAM_Result>> bestResults;
    std::string explorationCsvPath;
};

class EvaCamExplorer {
    public:
        explicit EvaCamExplorer(std::shared_ptr<EvaCamConfig> config);
        EvaCamExplorationResult Run();

    private:
        void InitializeExploration();
        void InitializeBestResults();
        std::vector<std::shared_ptr<CAM_Result>> CreateBestResultsBuffer() const;
        void OpenExplorationCsv();
        void RunPrimaryExploration();
        void EvaluateGeometry(int numRowMat, int numColumnMat, int numRowSubarray,
                std::vector<std::shared_ptr<CAM_Result>> &bestResults,
                long long &numSolutions,
                std::ostream *csvStream,
                const std::shared_ptr<Wire> &localWire,
                const std::shared_ptr<Wire> &globalWire);
        void RefineWires();
        void RefineLocalWires();
        void RefineGlobalWires();
        std::shared_ptr<Result> ReevaluateBestResultWithWires(int optimizationIndex,
                const std::shared_ptr<Wire> &localWire,
                const std::shared_ptr<Wire> &globalWire);
        void BuildPruningResults();
        void RunConstrainedExploration();
        void EvaluateConstrainedGeometry(int numRowMat, int numColumnMat, int numRowSubarray);
        std::shared_ptr<Bank> BuildBank(int numRowMat, int numColumnMat, int numRowSubarray,
                int numColumnSubarray, int numActiveMatPerRow, int numActiveMatPerColumn,
                int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn, int muxSenseAmp,
                int muxOutputLev1, int muxOutputLev2, int numRowPerSet,
                BufferDesignTarget areaOptimizationLevel, const std::shared_ptr<Wire> &localWire,
                const std::shared_ptr<Wire> &globalWire, const std::shared_ptr<CAM_Opt> &camOpt) const;
        std::shared_ptr<Result> MakeResult(const std::shared_ptr<Bank> &bank,
                const std::shared_ptr<Wire> &localWire,
                const std::shared_ptr<Wire> &globalWire,
                bool cloneBank) const;
        bool IsValidCandidate(const std::shared_ptr<Bank> &bank) const;
        void ValidateCapacityOrThrow(const std::shared_ptr<Bank> &bank) const;
        void UpdateBestResults(const std::shared_ptr<Result> &result);
        void UpdateBestResults(std::vector<std::shared_ptr<CAM_Result>> &bestResults,
                const std::shared_ptr<Result> &result) const;
        void MergeBestResults(const std::vector<std::shared_ptr<CAM_Result>> &bestResults);
        void FlushExplorationCsvBuffer(const std::string &buffer);
        void MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result);
        void MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result, std::ostream &stream) const;

        std::shared_ptr<EvaCamConfig> config_;
        std::vector<std::shared_ptr<CAM_Result>> bestResults_;
        std::shared_ptr<Wire> localWire_;
        std::shared_ptr<Wire> globalWire_;
        std::shared_ptr<CAM_Opt> camOpt_;
        std::string explorationCsvPath_;
        long long numSolution_ = 0;
        long long capacityBits_ = 0;
        long blockSizeBits_ = 0;
        int associativity_ = 0;
        bool fixedOuterGeometry_ = false;
        std::vector<int> numRowMatValues_;
        std::vector<int> numColumnMatValues_;
        std::vector<int> numRowSubarrayValues_;
        bool hasConstraintLimits_ = false;
        ResultLimits constraintLimits_{};
        std::mutex bestResultsMutex_;
        std::mutex explorationCsvMutex_;
        std::mutex numSolutionsMutex_;
        std::mutex progressMutex_;
};

#endif /* EVACAM_EXPLORER_H_ */
