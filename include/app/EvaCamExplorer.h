#ifndef EVACAM_EXPLORER_H_
#define EVACAM_EXPLORER_H_

#include <atomic>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CandidateSpec.h"
#include "EvaCamConfig.h"
#include "Wire.h"
#include "config/OutputFileLock.h"

class Result;
class Bank;

struct EvaCamExplorationResult {
    long long numSolution = 0;
    std::vector<std::shared_ptr<Result>> bestResults;
    std::string explorationCsvPath;
    CandidateAccounting candidateAccounting;
};

// Instance-local hooks used to force and observe difficult scheduler paths in
// concurrency tests. Production callers should leave these empty. Callbacks
// can run concurrently and therefore must be thread-safe.
struct EvaCamExplorerTestHooks {
    std::function<void(std::size_t)> workerStarted;
    std::function<void()> schedulerPoint;
    std::function<void(std::size_t)> beforeGeometry;
    std::function<void(std::size_t)> workerFinished;
};

class EvaCamExplorer {
    public:
        EvaCamExplorer(std::shared_ptr<EvaCamConfig> config, int numThreads);
        EvaCamExplorer(std::shared_ptr<EvaCamConfig> config, int numThreads,
                EvaCamExplorerTestHooks testHooks);
        EvaCamExplorationResult Run();

    private:
        void InitializeExploration();
        void InitializeBestResults();
        void InitializeWireCandidates();
        std::vector<std::shared_ptr<Result>> CreateBestResultsBuffer() const;
        void OpenExplorationCsv();
        void RunExplorationPass();
        void PrintSolutionCount();
        bool NeedsEvaluatedCandidates() const;
        void EvaluateGeometry(int numRowMat, int numColumnMat, int numRowSubarray,
                std::vector<std::shared_ptr<Result>> &bestResults,
                long long &numSolutions,
                std::ostream *csvStream,
                std::vector<EvaluatedCandidate> *evaluatedCandidates,
                CandidateAccounting &accounting);
        void RunConstrainedExploration();
        void RunPrunedExploration();
        void ResetBestResults();
        CandidateSpec MakeCandidateSpec(int numRowMat, int numColumnMat, int numRowSubarray,
                int numColumnSubarray, int numActiveMatPerRow, int numActiveMatPerColumn,
                int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn, int muxSenseAmp,
                int muxOutputLev1, int muxOutputLev2, int areaOptimizationLevel,
                const Wire &localWire, const Wire &globalWire, int rowDriverOptLevel,
                int priorityOptLevel, int bitSerialWidth) const;
        std::shared_ptr<Bank> BuildBank(const CandidateSpec &candidate) const;
        const Wire &FindWireCandidate(const std::vector<Wire> &wires,
                const WireSpec &spec) const;
        std::shared_ptr<Result> MakeResult(const std::shared_ptr<Bank> &bank,
                const Wire &localWire,
                const Wire &globalWire) const;
        bool IsValidCandidate(const std::shared_ptr<Bank> &bank) const;
        bool MeetsConstraints(const CandidateMetrics &metrics,
                const ResultLimits &limits) const;
        void RestoreMetrics(const CandidateMetrics &metrics, Bank &bank) const;
        void ValidateCapacityOrThrow(const std::shared_ptr<Bank> &bank) const;
        void UpdateBestResults(std::vector<std::shared_ptr<Result>> &bestResults,
                const std::shared_ptr<Result> &result) const;
        void UpdateBestResult(const std::shared_ptr<Result> &bestResult,
                const std::shared_ptr<Result> &candidate) const;
        void MergeBestResults(const std::vector<std::shared_ptr<Result>> &bestResults);
        void FlushExplorationCsvBuffer(const std::string &buffer);
        void MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result,
                const CandidateSpec &candidate, std::ostream &stream) const;

        std::shared_ptr<EvaCamConfig> config_;
        int numThreads_ = 1;
        bool outputEnabled_ = true;
        EvaCamExplorerTestHooks testHooks_;
        std::vector<std::shared_ptr<Result>> bestResults_;
        std::vector<Wire> localWireCandidates_;
        std::vector<Wire> globalWireCandidates_;
        std::optional<OutputFileLock> explorationCsvLock_;
        std::string explorationCsvPath_;
        long long numSolution_ = 0;
        CandidateAccounting candidateAccounting_;
        std::vector<EvaluatedCandidate> evaluatedCandidates_;
        long long capacityBits_ = 0;
        long blockSizeBits_ = 0;
        bool fixedOuterGeometry_ = false;
        std::vector<int> numRowMatValues_;
        std::vector<int> numColumnMatValues_;
        std::vector<int> numRowSubarrayValues_;
        std::atomic<bool> runStarted_{false};
};

#endif /* EVACAM_EXPLORER_H_ */
