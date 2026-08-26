#include "EvaCamExplorer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Bank.h"
#include "EvaCamConfig.h"
#include "Logger.h"
#include "ParetoPruner.h"
#include "Result.h"
#include "Wire.h"
#include "config/EvaCamConfigPrinter.h"
#include "config/EvaCamConfigValidator.h"
#include "config/DerivedValueHelpers.h"
#include "config/OutputPathBuilder.h"
#include "factories/BankFactory.h"
#include "factories/WireFactory.h"

EvaCamExplorer::EvaCamExplorer(std::shared_ptr<EvaCamConfig> config, int numThreads)
    : EvaCamExplorer(std::move(config), numThreads, {}) {
}

EvaCamExplorer::EvaCamExplorer(std::shared_ptr<EvaCamConfig> config, int numThreads,
        EvaCamExplorerTestHooks testHooks)
    : config_(std::move(config)),
      numThreads_(numThreads > 0 ? numThreads : 1),
      outputEnabled_(config_ && config_->logger.IsOutputEnabled()),
      testHooks_(std::move(testHooks)) {
}

EvaCamExplorationResult EvaCamExplorer::Run() {
    bool expected = false;
    if (!runStarted_.compare_exchange_strong(expected, true,
            std::memory_order_acq_rel, std::memory_order_acquire)) {
        throw std::logic_error(
                "EvaCamExplorer::Run() may only be called once per instance.");
    }

    if (outputEnabled_) {
        std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
        config_->technology.cell->PrintCell();
    }
    InitializeExploration();
    RunExplorationPass();
    if (config_->exploration.pruningEnabled) {
        RunPrunedExploration();
        PrintSolutionCount();
    } else {
        PrintSolutionCount();
        RunConstrainedExploration();
    }

    if (!config_->exploration.pruningEnabled
            && (!config_->constraints.enabled
                || config_->input.optimizationTarget == full_exploration)) {
        candidateAccounting_.constraintPassingCandidates =
            candidateAccounting_.validCandidates;
        candidateAccounting_.retainedCandidates = candidateAccounting_.validCandidates;
    }
    if (!candidateAccounting_.HasConsistentEnumerationCounts()
            || !candidateAccounting_.HasConsistentModelCounts()
            || !candidateAccounting_.HasConsistentFilteringCounts()) {
        throw std::logic_error("Candidate accounting invariants failed.");
    }
    auto accountingLog = config_->logger.Verbose();
    accountingLog
        << "Candidate accounting: raw=" << candidateAccounting_.rawCandidates
        << ", structural_rejections=" << candidateAccounting_.structurallyRejected
        << ", duplicates=" << candidateAccounting_.duplicateCandidates
        << ", modeled=" << candidateAccounting_.modeledCandidates
        << ", invalid=" << candidateAccounting_.invalidCandidates
        << ", valid=" << candidateAccounting_.validCandidates
        << ", constraint_rejections=" << candidateAccounting_.constraintRejected;
    if (config_->exploration.pruningEnabled) {
        accountingLog
        << ", constraint_passing=" << candidateAccounting_.constraintPassingCandidates
        << ", pruning_rejections=" << candidateAccounting_.pruningRejectedCandidates;
    }
    accountingLog
        << ", retained=" << candidateAccounting_.retainedCandidates
        << ", reconstructions=" << candidateAccounting_.reconstructionEvaluations;

    EvaCamExplorationResult result;
    result.numSolution = numSolution_;
    result.bestResults = bestResults_;
    result.explorationCsvPath = explorationCsvPath_;
    result.candidateAccounting = candidateAccounting_;
    return result;
}

void EvaCamExplorer::InitializeExploration() {
    EvaCamConfigValidator::Validate(*config_);
    InitializeBestResults();
    InitializeWireCandidates();

    if (outputEnabled_) {
        std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
        EvaCamConfigPrinter::Print(*config_);
    }

    capacityBits_ = DerivedValueHelpers::EffectiveCapacityBits(config_->input);
    blockSizeBits_ = DerivedValueHelpers::EffectiveBlockSizeBits(config_->input);

    const auto &resolved = config_->resolvedExploration;
    fixedOuterGeometry_ = DerivedValueHelpers::HasFixedOuterGeometry(resolved);
    numRowMatValues_ = resolved.geometry.numRowMatValues;
    numColumnMatValues_ = resolved.geometry.numColumnMatValues;
    numRowSubarrayValues_ = resolved.geometry.numRowSubarrayValues;

    OpenExplorationCsv();
}

void EvaCamExplorer::InitializeBestResults() {
    bestResults_ = CreateBestResultsBuffer();
}

void EvaCamExplorer::InitializeWireCandidates() {
    const auto &wires = config_->resolvedExploration.wires;
    for (int wireType : wires.localWireTypeValues) {
        for (int repeaterType : wires.localWireRepeaterTypeValues) {
            for (int isLowSwing : wires.isLocalWireLowSwingValues) {
                if ((WireRepeaterType)repeaterType != repeated_none && (bool)isLowSwing) {
                    continue;
                }
                Wire wire = WireFactory::CreateDefaultLocalWire(config_);
                wire.Initialize(config_->input.processNode, (WireType)wireType,
                        (WireRepeaterType)repeaterType, config_->input.temperature,
                        (bool)isLowSwing, config_);
                localWireCandidates_.push_back(wire);
            }
        }
    }

    for (int wireType : wires.globalWireTypeValues) {
        for (int repeaterType : wires.globalWireRepeaterTypeValues) {
            for (int isLowSwing : wires.isGlobalWireLowSwingValues) {
                if ((WireRepeaterType)repeaterType != repeated_none && (bool)isLowSwing) {
                    continue;
                }
                Wire wire = WireFactory::CreateDefaultGlobalWire(config_);
                wire.Initialize(config_->input.processNode, (WireType)wireType,
                        (WireRepeaterType)repeaterType, config_->input.temperature,
                        (bool)isLowSwing, config_);
                globalWireCandidates_.push_back(wire);
            }
        }
    }

    if (localWireCandidates_.empty() || globalWireCandidates_.empty()) {
        throw std::runtime_error("Exploration wire domains contain no valid combinations.");
    }
}

std::vector<std::shared_ptr<Result>> EvaCamExplorer::CreateBestResultsBuffer() const {
    std::vector<std::shared_ptr<Result>> bestResults((int)full_exploration);
    for (int i = 0; i < (int)full_exploration; i++) {
        bestResults[i] = std::make_shared<Result>();
        bestResults[i]->Initialize(config_);
        bestResults[i]->optimizationTarget = (OptimizationTarget)i;
    }
    return bestResults;
}

void EvaCamExplorer::OpenExplorationCsv() {
    if (!DerivedValueHelpers::IsFullExploration(config_->input)) {
        return;
    }

    explorationCsvPath_ = OutputPathBuilder::ExplorationCsvPath(config_->input, config_->technology);
    explorationCsvLock_.emplace(OutputFileLock::Acquire(explorationCsvPath_));
    std::filesystem::path csvPath(explorationCsvPath_);
    if (!csvPath.parent_path().empty()) {
        std::filesystem::create_directories(csvPath.parent_path());
    }
    std::ofstream clearFile(explorationCsvPath_.c_str(), std::ofstream::trunc);
    if (!clearFile) {
        throw std::runtime_error("Failed to open exploration CSV file: " + explorationCsvPath_);
    }
}

void EvaCamExplorer::RunExplorationPass() {
    const long long total = (long long)numRowMatValues_.size()
        * (long long)numColumnMatValues_.size()
        * (long long)numRowSubarrayValues_.size();
    std::atomic<long long> loopsComplete = 0;

    if (fixedOuterGeometry_) {
        config_->logger.Verbose() << "Fixed bank/mat/subarray sizes detected; bypassing outer geometry loops.";
        std::ostringstream fixedCsv;
        CandidateAccounting accounting;
        std::vector<EvaluatedCandidate> evaluatedCandidates;
        EvaluateGeometry(numRowMatValues_.front(), numColumnMatValues_.front(), numRowSubarrayValues_.front(),
                bestResults_, numSolution_, config_->exploration.pruningEnabled ? nullptr : &fixedCsv,
                NeedsEvaluatedCandidates() ? &evaluatedCandidates : nullptr,
                accounting);
        candidateAccounting_ += accounting;
        evaluatedCandidates_.insert(evaluatedCandidates_.end(),
                evaluatedCandidates.begin(), evaluatedCandidates.end());
        if (!config_->exploration.pruningEnabled) {
            FlushExplorationCsvBuffer(fixedCsv.str());
        }
        return;
    }

    std::atomic<bool> progressDone = false;
    std::atomic<bool> workerFailed = false;
    std::exception_ptr workerException;
    std::mutex workerExceptionMutex;
    const auto captureWorkerException = [&]() {
        {
            std::lock_guard<std::mutex> exceptionLock(workerExceptionMutex);
            if (!workerException) {
                workerException = std::current_exception();
            }
        }
        workerFailed.store(true, std::memory_order_release);
    };
    const std::size_t totalOuterGeometries = static_cast<std::size_t>(total);
    const std::size_t workerCount = std::min(
            static_cast<std::size_t>(numThreads_), totalOuterGeometries);
    std::atomic<std::size_t> nextOuterIndex = 0;
    std::mutex workerStartMutex;
    std::condition_variable workerStartCondition;
    bool workersMayStart = false;
    std::vector<std::vector<std::shared_ptr<Result>>> threadBestResults(
            workerCount);
    std::vector<long long> threadNumSolutions(workerCount, 0);
    std::vector<CandidateAccounting> threadAccounting(workerCount);
    std::vector<std::string> outerCsvBuffers(totalOuterGeometries);
    std::vector<std::vector<EvaluatedCandidate>> outerEvaluatedCandidates(totalOuterGeometries);
    // These vectors never resize while workers run. Each worker owns one
    // thread slot, and nextOuterIndex gives it exclusive ownership of one
    // outer-geometry slot. The main thread reads them only after every join.
    std::vector<std::thread> workers;
    workers.reserve(workerCount);
    std::thread progressThread;
    if (total > 1 && outputEnabled_) {
        progressThread = std::thread([&]() {
            try {
                long long lastReportedPercentage = -1;
                while (!progressDone.load(std::memory_order_acquire)) {
                    const long long percentage =
                        loopsComplete.load(std::memory_order_relaxed) * 100 / total;
                    if (percentage > lastReportedPercentage) {
                        std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
                        std::cout << "\rProgress: " << percentage << "%" << std::flush;
                        lastReportedPercentage = percentage;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } catch (...) {
                captureWorkerException();
            }
        });
    }

    try {
        for (std::size_t threadIndex = 0; threadIndex < workerCount; ++threadIndex) {
            workers.emplace_back([&, threadIndex]() {
                try {
                    if (testHooks_.workerStarted) {
                        testHooks_.workerStarted(threadIndex);
                    }
                    if (testHooks_.schedulerPoint) {
                        testHooks_.schedulerPoint();
                    }
                    {
                        std::unique_lock<std::mutex> startLock(workerStartMutex);
                        workerStartCondition.wait(startLock,
                                [&workersMayStart]() { return workersMayStart; });
                    }
                    if (testHooks_.schedulerPoint) {
                        testHooks_.schedulerPoint();
                    }
                    auto localBestResults = CreateBestResultsBuffer();
                    long long localNumSolutions = 0;
                    CandidateAccounting localAccounting;

                    while (!workerFailed.load(std::memory_order_acquire)) {
                        if (testHooks_.schedulerPoint) {
                            testHooks_.schedulerPoint();
                        }
                        const std::size_t outerIndex =
                            nextOuterIndex.fetch_add(1, std::memory_order_relaxed);
                        if (testHooks_.schedulerPoint) {
                            testHooks_.schedulerPoint();
                        }
                        if (outerIndex >= totalOuterGeometries) {
                            break;
                        }
                        if (testHooks_.beforeGeometry) {
                            testHooks_.beforeGeometry(outerIndex);
                        }

                        const std::size_t rowSubarrayIndex =
                            outerIndex % numRowSubarrayValues_.size();
                        const std::size_t remainingIndex =
                            outerIndex / numRowSubarrayValues_.size();
                        const std::size_t columnMatIndex =
                            remainingIndex % numColumnMatValues_.size();
                        const std::size_t rowMatIndex =
                            remainingIndex / numColumnMatValues_.size();
                        std::ostringstream csv;
                        std::vector<EvaluatedCandidate> evaluatedCandidates;
                        EvaluateGeometry(numRowMatValues_[rowMatIndex],
                                numColumnMatValues_[columnMatIndex],
                                numRowSubarrayValues_[rowSubarrayIndex],
                                localBestResults, localNumSolutions,
                                config_->exploration.pruningEnabled ? nullptr : &csv,
                                NeedsEvaluatedCandidates() ? &evaluatedCandidates : nullptr,
                                localAccounting);
                        if (testHooks_.schedulerPoint) {
                            testHooks_.schedulerPoint();
                        }
                        if (!config_->exploration.pruningEnabled) {
                            outerCsvBuffers[outerIndex] = csv.str();
                        }
                        outerEvaluatedCandidates[outerIndex] =
                            std::move(evaluatedCandidates);
                        loopsComplete.fetch_add(1, std::memory_order_relaxed);
                        if (testHooks_.schedulerPoint) {
                            testHooks_.schedulerPoint();
                        }
                    }

                    threadBestResults[threadIndex] = std::move(localBestResults);
                    threadNumSolutions[threadIndex] = localNumSolutions;
                    threadAccounting[threadIndex] = localAccounting;
                } catch (...) {
                    captureWorkerException();
                }
                try {
                    if (testHooks_.workerFinished) {
                        testHooks_.workerFinished(threadIndex);
                    }
                } catch (...) {
                    captureWorkerException();
                }
            });
        }
        {
            std::lock_guard<std::mutex> startLock(workerStartMutex);
            workersMayStart = true;
            workerStartCondition.notify_all();
        }
    } catch (...) {
        workerFailed.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> startLock(workerStartMutex);
            workersMayStart = true;
            workerStartCondition.notify_all();
        }
        for (std::thread &worker : workers) {
            worker.join();
        }
        progressDone.store(true, std::memory_order_release);
        if (progressThread.joinable()) {
            progressThread.join();
        }
        throw;
    }

    for (std::thread &worker : workers) {
        worker.join();
    }
    progressDone.store(true, std::memory_order_release);
    if (progressThread.joinable()) {
        progressThread.join();
        if (!workerException) {
            std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
            std::cout << "\rProgress: 100%" << std::endl;
        }
    }

    if (workerException) {
        std::rethrow_exception(workerException);
    }

    for (std::size_t threadIndex = 0; threadIndex < threadBestResults.size(); ++threadIndex) {
        numSolution_ += threadNumSolutions[threadIndex];
        candidateAccounting_ += threadAccounting[threadIndex];
        if (!threadBestResults[threadIndex].empty()) {
            MergeBestResults(threadBestResults[threadIndex]);
        }
    }
    for (std::size_t outerIndex = 0; outerIndex < totalOuterGeometries; ++outerIndex) {
        evaluatedCandidates_.insert(evaluatedCandidates_.end(),
                outerEvaluatedCandidates[outerIndex].begin(),
                outerEvaluatedCandidates[outerIndex].end());
        if (!config_->exploration.pruningEnabled) {
            FlushExplorationCsvBuffer(outerCsvBuffers[outerIndex]);
        }
    }
}

void EvaCamExplorer::PrintSolutionCount() {
    if (!outputEnabled_ || numSolution_ <= 0) {
        return;
    }
    std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
    std::cout << std::endl;
    std::cout << "*** There are " << numSolution_ << " Solutions. ***" << std::endl;
}

bool EvaCamExplorer::NeedsEvaluatedCandidates() const {
    return config_->exploration.pruningEnabled
        || (config_->constraints.enabled
            && config_->input.optimizationTarget != full_exploration);
}

void EvaCamExplorer::EvaluateGeometry(int numRowMat, int numColumnMat, int numRowSubarray,
        std::vector<std::shared_ptr<Result>> &bestResults,
        long long &numSolutions,
        std::ostream *csvStream,
        std::vector<EvaluatedCandidate> *evaluatedCandidates,
        CandidateAccounting &accounting) {
    const auto numActiveMatPerRowValues = config_->exploration.ActiveMatPerRowValues(numColumnMat);
    const auto numActiveMatPerColumnValues = config_->exploration.ActiveMatPerColumnValues(numRowMat);
    const auto &resolved = config_->resolvedExploration;
    const auto &numColumnSubarrayValues = resolved.geometry.numColumnSubarrayValues;
    const auto &muxSenseAmpValues = resolved.geometry.muxSenseAmpValues;
    const auto &muxOutputLev1Values = resolved.geometry.muxOutputLev1Values;
    const auto &muxOutputLev2Values = resolved.geometry.muxOutputLev2Values;
    const auto &areaOptimizationLevels = resolved.cam.areaOptimizationLevelValues;
    const auto &rowDriverOptimizationLevels = resolved.cam.rowDriverOptLevelValues;
    const auto &priorityOptimizationLevels = resolved.cam.priorityOptLevelValues;
    const auto &bitSerialWidthValues = resolved.cam.bitSerialWidthValues;
    std::unordered_set<CandidateSpec, CandidateSpecHash> seenCandidates;

    for (const Wire &localWire : localWireCandidates_)
        for (const Wire &globalWire : globalWireCandidates_)
            for (int numActiveMatPerRow : numActiveMatPerRowValues)
                for (int numActiveMatPerColumn : numActiveMatPerColumnValues)
                    for (int numColumnSubarray : numColumnSubarrayValues)
                        for (int numActiveSubarrayPerRow : config_->exploration.ActiveSubarrayPerRowValues(numColumnSubarray))
                            for (int numActiveSubarrayPerColumn : config_->exploration.ActiveSubarrayPerColumnValues(numRowSubarray))
                                for (int muxSenseAmp : muxSenseAmpValues)
                                    for (int muxOutputLev1 : muxOutputLev1Values)
                                        for (int muxOutputLev2 : muxOutputLev2Values)
                                            for (int areaOptimizationLevel : areaOptimizationLevels)
                                                for (int rowDriverOptLevel : rowDriverOptimizationLevels)
                                                    for (int priorityOptLevel : priorityOptimizationLevels)
                                                        for (int bitSerialWidth : bitSerialWidthValues) {
                                                        accounting.rawCandidates++;
                                                        if (!config_->exploration.IsValidPartitioning(blockSizeBits_,
                                                                    numActiveMatPerRow,
                                                                    numActiveMatPerColumn,
                                                                    numActiveSubarrayPerRow,
                                                                    numActiveSubarrayPerColumn)) {
                                                            accounting.structurallyRejected++;
                                                            continue;
                                                        }

                                                        const int effectivePriorityOptLevel =
                                                            config_->peripherals.withPriorityEnc
                                                                ? priorityOptLevel : latency_first;
                                                        const CandidateSpec candidate = MakeCandidateSpec(
                                                                numRowMat, numColumnMat, numRowSubarray,
                                                                numColumnSubarray, numActiveMatPerRow,
                                                                numActiveMatPerColumn, numActiveSubarrayPerRow,
                                                                numActiveSubarrayPerColumn, muxSenseAmp,
                                                                muxOutputLev1, muxOutputLev2,
                                                                areaOptimizationLevel, localWire, globalWire,
                                                                rowDriverOptLevel, effectivePriorityOptLevel,
                                                                bitSerialWidth);
                                                        if (!seenCandidates.insert(candidate).second) {
                                                            accounting.duplicateCandidates++;
                                                            continue;
                                                        }
                                                        accounting.modeledCandidates++;
                                                        const auto dataBank = BuildBank(candidate);

                                                        if (!IsValidCandidate(dataBank)) {
                                                            accounting.invalidCandidates++;
                                                            continue;
                                                        }
                                                        accounting.validCandidates++;

                                                        ValidateCapacityOrThrow(dataBank);
                                                        numSolutions++;

                                                        const auto tempResult = MakeResult(dataBank, localWire, globalWire);
                                                        UpdateBestResults(bestResults, tempResult);
                                                        if (evaluatedCandidates) {
                                                            evaluatedCandidates->push_back(
                                                                    {candidate, CandidateMetrics::FromBank(*dataBank)});
                                                        }
                                                        if (csvStream) {
                                                            MaybeWriteExplorationCsv(tempResult, candidate, *csvStream);
                                                        }
                                                    }
}

void EvaCamExplorer::RunConstrainedExploration() {
    if (config_->input.optimizationTarget == full_exploration || !config_->constraints.enabled) {
        return;
    }

    const ResultLimits constraintLimits = config_->BuildResultLimits(bestResults_);
    config_->ApplyResultLimits(constraintLimits, bestResults_);

    numSolution_ = 0;
    std::array<const EvaluatedCandidate *, static_cast<std::size_t>(full_exploration)> winners{};
    for (const EvaluatedCandidate &candidate : evaluatedCandidates_) {
        if (!MeetsConstraints(candidate.metrics, constraintLimits)) {
            candidateAccounting_.constraintRejected++;
            continue;
        }

        numSolution_++;
        candidateAccounting_.constraintPassingCandidates++;
        candidateAccounting_.retainedCandidates++;
        for (std::size_t index = 0; index < winners.size(); ++index) {
            const EvaluatedCandidate *winner = winners[index];
            if (!winner
                    || candidate.metrics.objectiveValues[index]
                        < winner->metrics.objectiveValues[index]
                    || (candidate.metrics.objectiveValues[index]
                            == winner->metrics.objectiveValues[index]
                        && candidate.spec < winner->spec)) {
                winners[index] = &candidate;
            }
        }
    }

    std::vector<std::pair<CandidateSpec, std::shared_ptr<Bank>>> reconstructed;
    for (std::size_t index = 0; index < winners.size(); ++index) {
        if (!winners[index]) {
            continue;
        }
        const auto existing = std::find_if(reconstructed.begin(), reconstructed.end(),
                [&winners, index](const auto &entry) {
                    return entry.first == winners[index]->spec;
                });
        std::shared_ptr<Bank> bank;
        if (existing != reconstructed.end()) {
            bank = existing->second;
        } else {
            bank = BuildBank(winners[index]->spec);
            candidateAccounting_.reconstructionEvaluations++;
            if (!IsValidCandidate(bank)) {
                throw std::runtime_error(
                        "A valid candidate became invalid while reconstructing constrained results.");
            }
            RestoreMetrics(winners[index]->metrics, *bank);
            reconstructed.emplace_back(winners[index]->spec, bank);
        }
        bestResults_[index]->bank = bank;
        bestResults_[index]->localWire = bank->localWire;
        bestResults_[index]->globalWire = bank->globalWire;
    }
    PrintSolutionCount();
}

void EvaCamExplorer::RunPrunedExploration() {
    std::vector<EvaluatedCandidate> constraintPassing;
    constraintPassing.reserve(evaluatedCandidates_.size());

    if (config_->constraints.enabled) {
        const ResultLimits constraintLimits = config_->BuildResultLimits(bestResults_);
        config_->ApplyResultLimits(constraintLimits, bestResults_);
        for (EvaluatedCandidate &candidate : evaluatedCandidates_) {
            if (!MeetsConstraints(candidate.metrics, constraintLimits)) {
                candidateAccounting_.constraintRejected++;
                continue;
            }
            constraintPassing.push_back(std::move(candidate));
        }
    } else {
        ResetBestResults();
        constraintPassing = std::move(evaluatedCandidates_);
    }

    candidateAccounting_.constraintPassingCandidates =
        static_cast<long long>(constraintPassing.size());
    ParetoPruningResult pruning = ParetoPruner::Prune(std::move(constraintPassing));
    candidateAccounting_.pruningRejectedCandidates =
        static_cast<long long>(pruning.rejectedCandidates);
    candidateAccounting_.retainedCandidates =
        static_cast<long long>(pruning.retainedCandidates.size());
    numSolution_ = candidateAccounting_.retainedCandidates;

    std::ostringstream csv;
    for (const EvaluatedCandidate &candidate : pruning.retainedCandidates) {
        const auto bank = BuildBank(candidate.spec);
        candidateAccounting_.reconstructionEvaluations++;
        if (!IsValidCandidate(bank)) {
            throw std::runtime_error(
                    "A Pareto-retained candidate became invalid while reconstructing results.");
        }
        ValidateCapacityOrThrow(bank);
        if (!(CandidateSpec::FromBank(*bank) == candidate.spec)) {
            throw std::runtime_error(
                    "A Pareto-retained candidate changed identity while reconstructing results.");
        }
        RestoreMetrics(candidate.metrics, *bank);
        const auto result = MakeResult(bank, bank->localWire, bank->globalWire);
        UpdateBestResults(bestResults_, result);
        MaybeWriteExplorationCsv(result, candidate.spec, csv);
    }
    FlushExplorationCsvBuffer(csv.str());
}

void EvaCamExplorer::ResetBestResults() {
    for (const std::shared_ptr<Result> &result : bestResults_) {
        result->reset();
    }
}

CandidateSpec EvaCamExplorer::MakeCandidateSpec(int numRowMat, int numColumnMat,
        int numRowSubarray, int numColumnSubarray, int numActiveMatPerRow,
        int numActiveMatPerColumn, int numActiveSubarrayPerRow,
        int numActiveSubarrayPerColumn, int muxSenseAmp, int muxOutputLev1,
        int muxOutputLev2, int areaOptimizationLevel, const Wire &localWire,
        const Wire &globalWire, int rowDriverOptLevel, int priorityOptLevel,
        int bitSerialWidth) const {
    CandidateSpec candidate;
    candidate.numRowMat = numRowMat;
    candidate.numColumnMat = numColumnMat;
    candidate.numActiveMatPerRow = numActiveMatPerRow;
    candidate.numActiveMatPerColumn = numActiveMatPerColumn;
    candidate.numRowSubarray = numRowSubarray;
    candidate.numColumnSubarray = numColumnSubarray;
    candidate.numActiveSubarrayPerRow = numActiveSubarrayPerRow;
    candidate.numActiveSubarrayPerColumn = numActiveSubarrayPerColumn;
    candidate.muxSenseAmp = muxSenseAmp;
    candidate.muxOutputLev1 = muxOutputLev1;
    candidate.muxOutputLev2 = muxOutputLev2;
    candidate.areaOptimizationLevel = areaOptimizationLevel;
    candidate.rowDriverOptimizationLevel = rowDriverOptLevel;
    candidate.priorityOptimizationLevel = priorityOptLevel;
    candidate.bitSerialWidth = bitSerialWidth;
    candidate.localWire = {localWire.wireType,
        localWire.wireRepeaterType, localWire.isLowSwing};
    candidate.globalWire = {globalWire.wireType,
        globalWire.wireRepeaterType, globalWire.isLowSwing};
    return candidate;
}

std::shared_ptr<Bank> EvaCamExplorer::BuildBank(const CandidateSpec &candidate) const {
    const Wire &localWire = FindWireCandidate(localWireCandidates_, candidate.localWire);
    const Wire &globalWire = FindWireCandidate(globalWireCandidates_, candidate.globalWire);
    CAM_Opt camOpt{};
    camOpt.RowDriver = candidate.rowDriverOptimizationLevel;
    camOpt.Proirity = candidate.priorityOptimizationLevel;
    camOpt.BitSerialWidth = candidate.bitSerialWidth;
    const auto bank = BankFactory::CreateBank(*config_);
    BankFactory::InitializeBank(config_, bank, candidate.numRowMat,
            candidate.numColumnMat, capacityBits_, blockSizeBits_,
            candidate.numActiveMatPerRow, candidate.numActiveMatPerColumn,
            candidate.muxSenseAmp, candidate.muxOutputLev1, candidate.muxOutputLev2,
            candidate.numRowSubarray, candidate.numColumnSubarray,
            candidate.numActiveSubarrayPerRow, candidate.numActiveSubarrayPerColumn,
            static_cast<BufferDesignTarget>(candidate.areaOptimizationLevel),
            localWire, globalWire, camOpt);
    return bank;
}

const Wire &EvaCamExplorer::FindWireCandidate(const std::vector<Wire> &wires,
        const WireSpec &spec) const {
    const auto match = std::find_if(wires.begin(), wires.end(), [&spec](const Wire &wire) {
        return wire.wireType == spec.type
            && wire.wireRepeaterType == spec.repeaterType
            && wire.isLowSwing == static_cast<bool>(spec.isLowSwing);
    });
    if (match == wires.end()) {
        throw std::logic_error("Candidate references a wire outside the resolved exploration space.");
    }
    return *match;
}

std::shared_ptr<Result> EvaCamExplorer::MakeResult(const std::shared_ptr<Bank> &bank,
        const Wire &localWire,
        const Wire &globalWire) const {
    auto result = std::make_shared<Result>();
    result->Initialize(config_);
    result->bank = bank;
    result->localWire = localWire;
    result->globalWire = globalWire;
    return result;
}

bool EvaCamExplorer::IsValidCandidate(const std::shared_ptr<Bank> &bank) const {
    return !bank->invalid && !bank->mat->subarray->invalid;
}

bool EvaCamExplorer::MeetsConstraints(const CandidateMetrics &metrics,
        const ResultLimits &limits) const {
    return metrics.readLatency <= limits.readLatency
        && metrics.writeLatency <= limits.writeLatency
        && metrics.readDynamicEnergy <= limits.readDynamicEnergy
        && metrics.writeDynamicEnergy <= limits.writeDynamicEnergy
        && metrics.leakage <= limits.leakage
        && metrics.area <= limits.area
        && metrics.readEdp <= limits.readEdp
        && metrics.writeEdp <= limits.writeEdp;
}

void EvaCamExplorer::RestoreMetrics(const CandidateMetrics &metrics, Bank &bank) const {
    bank.readLatency = metrics.readLatency;
    bank.writeLatency = metrics.writeLatency;
    bank.readDynamicEnergy = metrics.readDynamicEnergy;
    bank.writeDynamicEnergy = metrics.writeDynamicEnergy;
    bank.area = metrics.area;
    bank.leakage = metrics.leakage;
    bank.searchLatency = metrics.objectiveValues[search_latency_optimized];
    bank.searchDynamicEnergy = metrics.objectiveValues[search_energy_optimized];
}

void EvaCamExplorer::ValidateCapacityOrThrow(const std::shared_ptr<Bank> &bank) const {
    if ((long long)bank->mat->subarray->numColumn * bank->mat->subarray->numRow
            * bank->numColumnMat * bank->numRowMat * bank->numColumnSubarray
            * bank->numRowSubarray == capacityBits_) {
        return;
    }

    if (outputEnabled_) {
        std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
        std::cout << "numcolumn x numrow x numcolumnmat x numrowmat x numcolumnsubarry x numrowsubarray"
            << bank->mat->subarray->ConfiguredColumns() << ": "
            << bank->mat->subarray->ConfiguredRows()
            << ": " << bank->numColumnMat << ": " << bank->numRowMat << ": "
            << bank->numColumnSubarray << ": " << bank->numRowSubarray << std::endl;
        std::cout << "1 Bank = " << bank->numRowMat << "x"
            << bank->numColumnMat << " Mats" << std::endl;
        std::cout << "Activation - " << bank->numActiveMatPerColumn
            << "x" << bank->numActiveMatPerRow << " Mats" << std::endl;
        std::cout << "1 Mat  = " << bank->numRowSubarray
            << "x" << bank->numColumnSubarray << " Subarrays" << std::endl;
        std::cout << "Activation - " << bank->numActiveSubarrayPerColumn
            << "x" << bank->numActiveSubarrayPerRow << " Subarrays" << std::endl;
        std::cout << "Mux Degree - " << bank->muxSenseAmp << " x "
            << bank->muxOutputLev1 << " x " << bank->muxOutputLev2 << std::endl;
    }
    throw std::runtime_error("ERROR: DATA capacity violation. Shouldn't happen");
}

void EvaCamExplorer::UpdateBestResults(std::vector<std::shared_ptr<Result>> &bestResults,
        const std::shared_ptr<Result> &result) const {
    for (int i = 0; i < (int)full_exploration; i++) {
        UpdateBestResult(bestResults[i], result);
    }
}

void EvaCamExplorer::UpdateBestResult(const std::shared_ptr<Result> &bestResult,
        const std::shared_ptr<Result> &candidate) const {
    if (!bestResult || !bestResult->bank || !candidate || !candidate->bank
            || !candidate->bank->initialized) {
        return;
    }

    const std::size_t target = static_cast<std::size_t>(bestResult->optimizationTarget);
    const double candidateValue = CandidateMetrics::FromBank(*candidate->bank)
        .objectiveValues[target];
    bool shouldUpdate = !bestResult->bank->initialized;
    if (!shouldUpdate) {
        const double bestValue = CandidateMetrics::FromBank(*bestResult->bank)
            .objectiveValues[target];
        shouldUpdate = candidateValue < bestValue
            || (candidateValue == bestValue
                && CandidateSpec::FromBank(*candidate->bank)
                    < CandidateSpec::FromBank(*bestResult->bank));
    }
    if (shouldUpdate) {
        bestResult->bank = candidate->bank;
        bestResult->localWire = candidate->localWire;
        bestResult->globalWire = candidate->globalWire;
    }
}

void EvaCamExplorer::MergeBestResults(const std::vector<std::shared_ptr<Result>> &bestResults) {
    for (int i = 0; i < (int)full_exploration; i++) {
        UpdateBestResult(bestResults_[i], bestResults[i]);
    }
}

void EvaCamExplorer::FlushExplorationCsvBuffer(const std::string &buffer) {
    if (buffer.empty() || explorationCsvPath_.empty()) {
        return;
    }

    std::ofstream csv(explorationCsvPath_.c_str(), std::ofstream::app);
    if (!csv) {
        throw std::runtime_error("Failed to append exploration CSV file: " + explorationCsvPath_);
    }
    csv << buffer;
}

void EvaCamExplorer::MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result,
        const CandidateSpec &candidate, std::ostream &stream) const {
    if (!result || !DerivedValueHelpers::ShouldWriteExplorationCsv(config_->input)) {
        return;
    }

    result->printToCsvFile(stream);
    stream << candidate.StableId() << ","
        << candidate.rowDriverOptimizationLevel << ","
        << candidate.priorityOptimizationLevel << ","
        << candidate.bitSerialWidth << ",";
    stream << std::endl;
}
