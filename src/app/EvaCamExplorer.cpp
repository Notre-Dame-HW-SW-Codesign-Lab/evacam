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
    RunPrimaryExploration();
    RefineWires();
    BuildPruningResults();
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
    localWire_ = WireFactory::CreateDefaultLocalWire(config_);
    globalWire_ = WireFactory::CreateDefaultGlobalWire(config_);
    camOpt_ = {};

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

void EvaCamExplorer::RunPrimaryExploration() {
    const long long total = (long long)numRowMatValues_.size()
        * (long long)numColumnMatValues_.size()
        * (long long)numRowSubarrayValues_.size();
    std::atomic<long long> loopsComplete = 0;

    if (fixedOuterGeometry_) {
        config_->logger.Verbose() << "Fixed bank/mat/subarray sizes detected; bypassing outer geometry loops.";
        std::ostringstream fixedCsv;
        EvaluateGeometry(numRowMatValues_.front(), numColumnMatValues_.front(), numRowSubarrayValues_.front(),
                bestResults_, numSolution_, &fixedCsv, localWire_, globalWire_);
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
        auto threadLocalWire = WireFactory::CreateDefaultLocalWire(config_);
        auto threadGlobalWire = WireFactory::CreateDefaultGlobalWire(config_);
        auto threadBestResults = CreateBestResultsBuffer();
        long long threadNumSolutions = 0;
        std::ostringstream threadCsv;

#pragma omp for collapse(3)
        for (int i = 0; i < (int)numRowMatValues_.size(); i++) {
            for (int j = 0; j < (int)numColumnMatValues_.size(); j++) {
                for (int k = 0; k < (int)numRowSubarrayValues_.size(); k++) {
                    loopsComplete.fetch_add(1, std::memory_order_relaxed);
                    EvaluateGeometry(numRowMatValues_[i], numColumnMatValues_[j], numRowSubarrayValues_[k],
                            threadBestResults, threadNumSolutions, &threadCsv, threadLocalWire, threadGlobalWire);
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

void EvaCamExplorer::EvaluateGeometry(int numRowMat, int numColumnMat, int numRowSubarray,
        std::vector<std::shared_ptr<Result>> &bestResults,
        long long &numSolutions,
        std::ostream *csvStream,
        const Wire &localWire,
        const Wire &globalWire) {
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

                                                        if (!IsValidCandidate(dataBank)) {
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

void EvaCamExplorer::RefineWires() {
    std::lock_guard<std::mutex> resultsLock(bestResultsMutex_);
    std::lock_guard<std::mutex> solutionsLock(numSolutionsMutex_);
    if (numSolution_ <= 0) {
        return;
    }

    {
        std::lock_guard<std::mutex> outputLock(Logger::OutputMutex());
        std::cout << std::endl;
        std::cout << "*** There are " << numSolution_ << " Solutions. ***" << std::endl;
    }

    RefineLocalWires();
    RefineGlobalWires();
}

void EvaCamExplorer::RefineLocalWires() {
    const auto &resolved = config_->resolvedExploration;
    for (int localWireType : resolved.wires.localWireTypeValues) {
        for (int localWireRepeaterType : resolved.wires.localWireRepeaterTypeValues) {
            for (int isLocalWireLowSwing : resolved.wires.isLocalWireLowSwingValues) {
                if ((WireRepeaterType)localWireRepeaterType != repeated_none
                        && (bool)isLocalWireLowSwing != false) {
                    continue;
                }

                localWire_.Initialize(config_->input.processNode,
                        (WireType)localWireType,
                        (WireRepeaterType)localWireRepeaterType,
                        config_->input.temperature,
                        (bool)isLocalWireLowSwing,
                        config_);

                std::shared_ptr<Result> tempResult;
                for (int i = 0; i < (int)full_exploration; i++) {
                    if (!bestResults_[i]->bank->initialized) {
                        continue;
                    }

                    globalWire_.Initialize(config_->input.processNode,
                            bestResults_[i]->globalWire.wireType,
                            bestResults_[i]->globalWire.wireRepeaterType,
                            config_->input.temperature,
                            bestResults_[i]->globalWire.isLowSwing,
                            config_);
                    tempResult = ReevaluateBestResultWithWires(i, localWire_, globalWire_);
                }

                MaybeWriteExplorationCsv(tempResult);
            }
        }
    }
}

void EvaCamExplorer::RefineGlobalWires() {
    const auto &resolved = config_->resolvedExploration;
    for (int globalWireType : resolved.wires.globalWireTypeValues) {
        for (int globalWireRepeaterType : resolved.wires.globalWireRepeaterTypeValues) {
            for (int isGlobalWireLowSwing : resolved.wires.isGlobalWireLowSwingValues) {
                if ((WireRepeaterType)globalWireRepeaterType != repeated_none
                        && (bool)isGlobalWireLowSwing != false) {
                    continue;
                }

                globalWire_.Initialize(config_->input.processNode,
                        (WireType)globalWireType,
                        (WireRepeaterType)globalWireRepeaterType,
                        config_->input.temperature,
                        (bool)isGlobalWireLowSwing,
                        config_);

                std::shared_ptr<Result> tempResult;
                for (int i = 0; i < (int)full_exploration; i++) {
                    if (!bestResults_[i]->bank->initialized) {
                        continue;
                    }

                    localWire_.Initialize(config_->input.processNode,
                            bestResults_[i]->localWire.wireType,
                            bestResults_[i]->localWire.wireRepeaterType,
                            config_->input.temperature,
                            bestResults_[i]->localWire.isLowSwing,
                            config_);
                    tempResult = ReevaluateBestResultWithWires(i, localWire_, globalWire_);
                }

                MaybeWriteExplorationCsv(tempResult);
            }
        }
    }
}

std::shared_ptr<Result> EvaCamExplorer::ReevaluateBestResultWithWires(int optimizationIndex,
        const Wire &localWire,
        const Wire &globalWire) {
    camOpt_.BitSerialWidth = bestResults_[optimizationIndex]->bank->numBitSerial;
    camOpt_.Proirity = bestResults_[optimizationIndex]->bank->mat->subarray->PriorityOptLevel;
    camOpt_.RowDriver = bestResults_[optimizationIndex]->bank->mat->subarray->DriverOptLevel;

    const auto trialBank = BuildBank(bestResults_[optimizationIndex]->bank->numRowMat,
            bestResults_[optimizationIndex]->bank->numColumnMat,
            bestResults_[optimizationIndex]->bank->numRowSubarray,
            bestResults_[optimizationIndex]->bank->numColumnSubarray,
            bestResults_[optimizationIndex]->bank->numActiveMatPerRow,
            bestResults_[optimizationIndex]->bank->numActiveMatPerColumn,
            bestResults_[optimizationIndex]->bank->numActiveSubarrayPerRow,
            bestResults_[optimizationIndex]->bank->numActiveSubarrayPerColumn,
            bestResults_[optimizationIndex]->bank->muxSenseAmp,
            bestResults_[optimizationIndex]->bank->muxOutputLev1,
            bestResults_[optimizationIndex]->bank->muxOutputLev2,
            bestResults_[optimizationIndex]->bank->areaOptimizationLevel,
            localWire,
            globalWire,
            camOpt_);

    bestResults_[optimizationIndex]->bank = trialBank;
    const auto tempResult = MakeResult(trialBank, localWire, globalWire);
    bestResults_[optimizationIndex]->compareAndUpdate(tempResult);
    return tempResult;
}

void EvaCamExplorer::BuildPruningResults() {
    if (!DerivedValueHelpers::IsPruningEnabledForExploration(config_->input, config_->constraints)) {
        return;
    }

    std::vector<std::vector<std::vector<std::unique_ptr<Result>>>> pruningResults;
    pruningResults.resize((int)full_exploration);
    for (auto &v1 : pruningResults) {
        v1.resize((int)full_exploration);
        for (auto &v2 : v1) {
            v2.resize(3);
            for (auto &res : v2) {
                res = std::make_unique<Result>();
                res->Initialize(config_);
            }
        }
    }

    for (int i = 0; i < (int)full_exploration; i++) {
        for (int j = 0; j < (int)full_exploration; j++) {
            for (int k = 0; k < 3; k++) {
                pruningResults[i][j][k]->optimizationTarget = (OptimizationTarget)i;
                pruningResults[i][j][k]->localWire = bestResults_[i]->localWire;
                pruningResults[i][j][k]->globalWire = bestResults_[i]->globalWire;
                switch ((OptimizationTarget)j) {
                    case read_latency_optimized:
                        pruningResults[i][j][k]->limitReadLatency = bestResults_[j]->bank->readLatency * (1 + (k + 1.0) / 10);
                        break;
                    case write_latency_optimized:
                        pruningResults[i][j][k]->limitWriteLatency = bestResults_[j]->bank->writeLatency * (1 + (k + 1.0) / 10);
                        break;
                    case read_energy_optimized:
                        pruningResults[i][j][k]->limitReadDynamicEnergy = bestResults_[j]->bank->readDynamicEnergy * (1 + (k + 1.0) / 10);
                        break;
                    case write_energy_optimized:
                        pruningResults[i][j][k]->limitWriteDynamicEnergy = bestResults_[j]->bank->writeDynamicEnergy * (1 + (k + 1.0) / 10);
                        break;
                    case read_edp_optimized:
                        pruningResults[i][j][k]->limitReadEdp = bestResults_[j]->bank->readLatency * bestResults_[j]->bank->readDynamicEnergy * (1 + (k + 1.0) / 10);
                        break;
                    case write_edp_optimized:
                        pruningResults[i][j][k]->limitWriteEdp = bestResults_[j]->bank->writeLatency * bestResults_[j]->bank->writeDynamicEnergy * (1 + (k + 1.0) / 10);
                        break;
                    case area_optimized:
                        pruningResults[i][j][k]->limitArea = bestResults_[j]->bank->area * (1 + (k + 1.0) / 10);
                        break;
                    case leakage_optimized:
                        pruningResults[i][j][k]->limitLeakage = bestResults_[j]->bank->leakage * (1 + (k + 1.0) / 10);
                        break;
                    default:
                        config_->logger.Verbose() << "Warning: should not happen";
                }
            }
        }
    }
}

void EvaCamExplorer::RunConstrainedExploration() {
    if (config_->input.optimizationTarget == full_exploration || !config_->constraints.enabled) {
        return;
    }

    constraintLimits_ = config_->BuildResultLimits(bestResults_);
    hasConstraintLimits_ = true;
    config_->ApplyResultLimits(constraintLimits_, bestResults_);

    numSolution_ = 0;
    localWire_ = WireFactory::CreateDefaultLocalWire(config_);
    globalWire_ = WireFactory::CreateDefaultGlobalWire(config_);

    if (fixedOuterGeometry_) {
        config_->logger.Verbose() << "Fixed bank/mat/subarray sizes detected; bypassing constrained outer geometry loops.";
        EvaluateConstrainedGeometry(numRowMatValues_.front(), numColumnMatValues_.front(), numRowSubarrayValues_.front());
        return;
    }

    for (int numRowMat : numRowMatValues_) {
        for (int numColumnMat : numColumnMatValues_) {
            for (int numRowSubarray : numRowSubarrayValues_) {
                EvaluateConstrainedGeometry(numRowMat, numColumnMat, numRowSubarray);
            }
        }
    }
}

void EvaCamExplorer::EvaluateConstrainedGeometry(int numRowMat, int numColumnMat, int numRowSubarray) {
    const auto numActiveMatPerRowValues = config_->exploration.ActiveMatPerRowValues(numColumnMat);
    const auto numActiveMatPerColumnValues = config_->exploration.ActiveMatPerColumnValues(numRowMat);
    const auto &resolved = config_->resolvedExploration;
    const auto &numColumnSubarrayValues = resolved.geometry.numColumnSubarrayValues;
    const auto &muxSenseAmpValues = resolved.geometry.muxSenseAmpValues;
    const auto &muxOutputLev1Values = resolved.geometry.muxOutputLev1Values;
    const auto &muxOutputLev2Values = resolved.geometry.muxOutputLev2Values;
    const auto &areaOptimizationLevels = resolved.cam.areaOptimizationLevelValues;

    for (int numActiveMatPerRow : numActiveMatPerRowValues)
        for (int numActiveMatPerColumn : numActiveMatPerColumnValues)
            for (int numColumnSubarray : numColumnSubarrayValues)
                for (int numActiveSubarrayPerRow : config_->exploration.ActiveSubarrayPerRowValues(numColumnSubarray))
                    for (int numActiveSubarrayPerColumn : config_->exploration.ActiveSubarrayPerColumnValues(numRowSubarray))
                        for (int muxSenseAmp : muxSenseAmpValues)
                            for (int muxOutputLev1 : muxOutputLev1Values)
                                for (int muxOutputLev2 : muxOutputLev2Values)
                                    for (int areaOptimizationLevel : areaOptimizationLevels) {
                                            if (!config_->exploration.IsValidPartitioning(blockSizeBits_,
                                                        numActiveMatPerRow,
                                                        numActiveMatPerColumn,
                                                        numActiveSubarrayPerRow,
                                                        numActiveSubarrayPerColumn)) {
                                                continue;
                                            }

                                            const auto dataBank = BuildBank(numRowMat, numColumnMat, numRowSubarray,
                                                    numColumnSubarray, numActiveMatPerRow,
                                                    numActiveMatPerColumn, numActiveSubarrayPerRow,
                                                    numActiveSubarrayPerColumn, muxSenseAmp, muxOutputLev1,
                                                    muxOutputLev2,
                                                    (BufferDesignTarget)areaOptimizationLevel,
                                                    localWire_, globalWire_, camOpt_);

                                            if (!dataBank->invalid
                                                    && dataBank->readLatency <= constraintLimits_.readLatency
                                                    && dataBank->writeLatency <= constraintLimits_.writeLatency
                                                    && dataBank->readDynamicEnergy <= constraintLimits_.readDynamicEnergy
                                                    && dataBank->writeDynamicEnergy <= constraintLimits_.writeDynamicEnergy
                                                    && dataBank->leakage <= constraintLimits_.leakage
                                                    && dataBank->area <= constraintLimits_.area
                                                    && dataBank->readLatency * dataBank->readDynamicEnergy <= constraintLimits_.readEdp
                                                    && dataBank->writeLatency * dataBank->writeDynamicEnergy <= constraintLimits_.writeEdp) {
                                                ValidateCapacityOrThrow(dataBank);
                                                numSolution_++;
                                                const auto tempResult = MakeResult(dataBank, localWire_, globalWire_);
                                                UpdateBestResults(tempResult);
                                            }
                                        }
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

void EvaCamExplorer::UpdateBestResults(const std::shared_ptr<Result> &result) {
    UpdateBestResults(bestResults_, result);
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

void EvaCamExplorer::MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result) {
    if (explorationCsvPath_.empty()) {
        return;
    }

    std::ostringstream csvBuffer;
    MaybeWriteExplorationCsv(result, csvBuffer);
    FlushExplorationCsvBuffer(csvBuffer.str());
}

void EvaCamExplorer::MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result, std::ostream &stream) const {
    if (!result || !DerivedValueHelpers::ShouldWriteExplorationCsv(config_->input, config_->constraints)) {
        return;
    }

    result->printToCsvFile(stream);
    stream << std::endl;
}
