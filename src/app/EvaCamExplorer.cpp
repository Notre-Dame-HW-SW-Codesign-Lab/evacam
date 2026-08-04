#include "EvaCamExplorer.h"

#include <omp.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "Bank.h"
#include "Result.h"
#include "EvaCamConfig.h"
#include "Logger.h"
#include "Wire.h"
#include "config/EvaCamConfigPrinter.h"
#include "config/EvaCamConfigValidator.h"
#include "config/DerivedValueHelpers.h"
#include "config/OutputPathBuilder.h"
#include "factories/BankFactory.h"
#include "factories/WireFactory.h"

EvaCamExplorer::EvaCamExplorer(std::shared_ptr<EvaCamConfig> config, int numThreads)
    : config_(std::move(config)),
      numThreads_(numThreads > 0 ? numThreads : 1) {
}

EvaCamExplorationResult EvaCamExplorer::Run() {
    {
        std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
        config_->technology.cell->PrintCell();
    }
    InitializeExploration();
    RunExplorationPass(nullptr);
    PrintSolutionCount();
    RunConstrainedExploration();

    EvaCamExplorationResult result;
    {
        std::lock_guard<std::mutex> solutionsLock(numSolutionsMutex_);
        result.numSolution = numSolution_;
    }
    {
        std::lock_guard<std::mutex> resultsLock(bestResultsMutex_);
        result.bestResults = bestResults_;
    }
    result.explorationCsvPath = explorationCsvPath_;
    return result;
}

void EvaCamExplorer::InitializeExploration() {
    EvaCamConfigValidator::Validate(*config_);
    InitializeBestResults();
    InitializeWireCandidates();

    {
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

void EvaCamExplorer::RunExplorationPass(const ResultLimits *limits) {
    const long long total = (long long)numRowMatValues_.size()
        * (long long)numColumnMatValues_.size()
        * (long long)numRowSubarrayValues_.size();
    std::atomic<long long> loopsComplete = 0;

    if (fixedOuterGeometry_) {
        config_->logger.Verbose() << "Fixed bank/mat/subarray sizes detected; bypassing outer geometry loops.";
        std::ostringstream fixedCsv;
        EvaluateGeometry(numRowMatValues_.front(), numColumnMatValues_.front(), numRowSubarrayValues_.front(),
                bestResults_, numSolution_, &fixedCsv, limits);
        FlushExplorationCsvBuffer(fixedCsv.str());
        return;
    }

    std::atomic<bool> progressDone = false;
    std::thread progressThread;
    if (total > 1) {
        progressThread = std::thread([&]() {
            long long lastReportedPercentage = -1;
            while (!progressDone.load(std::memory_order_relaxed)) {
                const long long percentage = loopsComplete.load(std::memory_order_relaxed) * 100 / total;
                if (percentage > lastReportedPercentage) {
                    std::lock_guard<std::mutex> progressLock(progressMutex_);
                    std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
                    std::cout << "\rProgress: " << percentage << "%" << std::flush;
                    lastReportedPercentage = percentage;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

#pragma omp parallel num_threads(numThreads_)
    {
        auto threadBestResults = CreateBestResultsBuffer();
        long long threadNumSolutions = 0;
        std::ostringstream threadCsv;

#pragma omp for collapse(3)
        for (int i = 0; i < (int)numRowMatValues_.size(); i++) {
            for (int j = 0; j < (int)numColumnMatValues_.size(); j++) {
                for (int k = 0; k < (int)numRowSubarrayValues_.size(); k++) {
                    loopsComplete.fetch_add(1, std::memory_order_relaxed);
                    EvaluateGeometry(numRowMatValues_[i], numColumnMatValues_[j], numRowSubarrayValues_[k],
                            threadBestResults, threadNumSolutions, &threadCsv, limits);
                }
            }
        }

        {
            std::lock_guard<std::mutex> solutionsLock(numSolutionsMutex_);
            numSolution_ += threadNumSolutions;
        }

        {
            std::lock_guard<std::mutex> resultsLock(bestResultsMutex_);
            MergeBestResults(threadBestResults);
        }

        FlushExplorationCsvBuffer(threadCsv.str());
    }

    progressDone.store(true, std::memory_order_relaxed);
    if (progressThread.joinable()) {
        progressThread.join();
        std::lock_guard<std::mutex> progressLock(progressMutex_);
        std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
        std::cout << "\rProgress: 100%" << std::endl;
    }
}

void EvaCamExplorer::PrintSolutionCount() {
    std::lock_guard<std::mutex> solutionsLock(numSolutionsMutex_);
    if (numSolution_ <= 0) {
        return;
    }
    std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
    std::cout << std::endl;
    std::cout << "*** There are " << numSolution_ << " Solutions. ***" << std::endl;
}

void EvaCamExplorer::EvaluateGeometry(int numRowMat, int numColumnMat, int numRowSubarray,
        std::vector<std::shared_ptr<Result>> &bestResults,
        long long &numSolutions,
        std::ostream *csvStream,
        const ResultLimits *limits) {
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
                                                        if (!config_->exploration.IsValidPartitioning(blockSizeBits_,
                                                                    numActiveMatPerRow,
                                                                    numActiveMatPerColumn,
                                                                    numActiveSubarrayPerRow,
                                                                    numActiveSubarrayPerColumn)) {
                                                            continue;
                                                        }

                                                        CAM_Opt iterCamOpt{};
                                                        iterCamOpt.RowDriver = rowDriverOptLevel;
                                                        iterCamOpt.Proirity = priorityOptLevel;
                                                        iterCamOpt.BitSerialWidth = bitSerialWidth;

                                                        const auto dataBank = BuildBank(numRowMat, numColumnMat, numRowSubarray,
                                                                numColumnSubarray, numActiveMatPerRow,
                                                                numActiveMatPerColumn, numActiveSubarrayPerRow,
                                                                numActiveSubarrayPerColumn, muxSenseAmp, muxOutputLev1,
                                                                muxOutputLev2,
                                                                (BufferDesignTarget)areaOptimizationLevel,
                                                                localWire, globalWire, iterCamOpt);

                                                        if (!IsValidCandidate(dataBank)
                                                                || !MeetsConstraints(dataBank, limits)) {
                                                            continue;
                                                        }

                                                        ValidateCapacityOrThrow(dataBank);
                                                        numSolutions++;

                                                        const auto tempResult = MakeResult(dataBank, localWire, globalWire);
                                                        UpdateBestResults(bestResults, tempResult);
                                                        if (csvStream) {
                                                            MaybeWriteExplorationCsv(tempResult, *csvStream);
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

    RunExplorationPass(&constraintLimits);
    PrintSolutionCount();
}

std::shared_ptr<Bank> EvaCamExplorer::BuildBank(int numRowMat, int numColumnMat, int numRowSubarray,
        int numColumnSubarray, int numActiveMatPerRow, int numActiveMatPerColumn,
        int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn, int muxSenseAmp,
        int muxOutputLev1, int muxOutputLev2,
        BufferDesignTarget areaOptimizationLevel, const Wire &localWire,
        const Wire &globalWire, const CAM_Opt &camOpt) const {
    const auto bank = BankFactory::CreateBank(*config_);
    BankFactory::InitializeBank(config_, bank, numRowMat, numColumnMat, capacityBits_, blockSizeBits_,
            numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
            muxOutputLev1, muxOutputLev2, numRowSubarray, numColumnSubarray,
            numActiveSubarrayPerRow, numActiveSubarrayPerColumn, areaOptimizationLevel,
            localWire, globalWire, camOpt);
    return bank;
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

bool EvaCamExplorer::MeetsConstraints(const std::shared_ptr<Bank> &bank,
        const ResultLimits *limits) const {
    return limits == nullptr
        || (bank->readLatency <= limits->readLatency
            && bank->writeLatency <= limits->writeLatency
            && bank->readDynamicEnergy <= limits->readDynamicEnergy
            && bank->writeDynamicEnergy <= limits->writeDynamicEnergy
            && bank->leakage <= limits->leakage
            && bank->area <= limits->area
            && bank->readLatency * bank->readDynamicEnergy <= limits->readEdp
            && bank->writeLatency * bank->writeDynamicEnergy <= limits->writeEdp);
}

void EvaCamExplorer::ValidateCapacityOrThrow(const std::shared_ptr<Bank> &bank) const {
    if ((long long)bank->mat->subarray->numColumn * bank->mat->subarray->numRow
            * bank->numColumnMat * bank->numRowMat * bank->numColumnSubarray
            * bank->numRowSubarray == capacityBits_) {
        return;
    }

    {
        std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
        std::cout << "numcolumn x numrow x numcolumnmat x numrowmat x numcolumnsubarry x numrowsubarray"
            << bank->mat->subarray->numColumn << ": " << bank->mat->subarray->numRow
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
        bestResults[i]->compareAndUpdate(result);
    }
}

void EvaCamExplorer::MergeBestResults(const std::vector<std::shared_ptr<Result>> &bestResults) {
    for (int i = 0; i < (int)full_exploration; i++) {
        bestResults_[i]->compareAndUpdate(bestResults[i]);
    }
}

void EvaCamExplorer::FlushExplorationCsvBuffer(const std::string &buffer) {
    if (buffer.empty() || explorationCsvPath_.empty()) {
        return;
    }

    std::lock_guard<std::mutex> csvLock(explorationCsvMutex_);
    std::ofstream csv(explorationCsvPath_.c_str(), std::ofstream::app);
    if (!csv) {
        throw std::runtime_error("Failed to append exploration CSV file: " + explorationCsvPath_);
    }
    csv << buffer;
}

void EvaCamExplorer::MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result, std::ostream &stream) const {
    if (!result || !DerivedValueHelpers::ShouldWriteExplorationCsv(config_->input)) {
        return;
    }

    result->printToCsvFile(stream);
    stream << std::endl;
}
