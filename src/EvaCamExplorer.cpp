#include "../include/EvaCamExplorer.h"

#include <omp.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../include/Bank.h"
#include "../include/CAM_Result.h"
#include "../include/EvaCamConfig.h"
#include "../include/Result.h"
#include "../include/Wire.h"
#include "../include/macros.h"

namespace {

std::vector<std::shared_ptr<Result>> AsResults(
        const std::vector<std::shared_ptr<CAM_Result>> &bestResults) {
    std::vector<std::shared_ptr<Result>> results;
    results.reserve(bestResults.size());
    for (const auto &result : bestResults) {
        results.push_back(result);
    }
    return results;
}

}  // namespace

EvaCamExplorer::EvaCamExplorer(std::shared_ptr<EvaCamConfig> config)
    : config_(std::move(config)) {
}

EvaCamExplorationResult EvaCamExplorer::Run() {
    config_->cell->PrintCell();
    InitializeExploration();
    RunPrimaryExploration();
    RefineWires();
    BuildPruningResults();
    RunConstrainedExploration();

    if (explorationCsv_ && explorationCsv_->is_open()) {
        explorationCsv_->close();
    }

    EvaCamExplorationResult result;
    result.numSolution = numSolution_;
    result.bestResults = bestResults_;
    result.explorationCsvPath = explorationCsvPath_;
    return result;
}

void EvaCamExplorer::InitializeExploration() {
    config_->ValidateSupportedConfiguration();
    InitializeBestResults();
    localWire_ = config_->CreateDefaultLocalWire();
    globalWire_ = config_->CreateDefaultGlobalWire();
    camOpt_ = std::make_shared<CAM_Opt>();

    config_->PrintConfig();

    capacityBits_ = config_->EffectiveCapacityBits();
    blockSizeBits_ = config_->EffectiveBlockSizeBits();
    associativity_ = config_->EffectiveAssociativity();

    fixedOuterGeometry_ = config_->HasFixedOuterGeometry();
    numRowMatValues_ = config_->NumRowMatValues();
    numColumnMatValues_ = config_->NumColumnMatValues();
    numRowSubarrayValues_ = config_->NumRowSubarrayValues();

    OpenExplorationCsv();
}

void EvaCamExplorer::InitializeBestResults() {
    bestResults_ = CreateBestResultsBuffer();
}

std::vector<std::shared_ptr<CAM_Result>> EvaCamExplorer::CreateBestResultsBuffer() const {
    std::vector<std::shared_ptr<CAM_Result>> bestResults((int)full_exploration);
    for (int i = 0; i < (int)full_exploration; i++) {
        bestResults[i] = std::make_shared<CAM_Result>();
        bestResults[i]->Initialize(config_);
        bestResults[i]->optimizationTarget = (OptimizationTarget)i;
    }
    return bestResults;
}

void EvaCamExplorer::OpenExplorationCsv() {
    if (!config_->IsFullExploration()) {
        return;
    }

    explorationCsvPath_ = config_->ExplorationCsvPath();
    std::filesystem::path csvPath(explorationCsvPath_);
    if (!csvPath.parent_path().empty()) {
        std::filesystem::create_directories(csvPath.parent_path());
    }

    explorationCsv_ = std::make_unique<std::ofstream>();
    explorationCsv_->open(explorationCsvPath_.c_str(), std::ofstream::app);
}

void EvaCamExplorer::RunPrimaryExploration() {
    const long long total = (long long)numRowMatValues_.size()
        * (long long)numColumnMatValues_.size()
        * (long long)numRowSubarrayValues_.size();
    std::atomic<long long> loopsComplete = 0;

    if (fixedOuterGeometry_) {
        config_->logger.Verbose() << "Fixed bank/mat/subarray sizes detected; bypassing outer geometry loops.";
        EvaluateGeometry(config_->minNumRowMat, config_->minNumColumnMat, config_->minNumRowSubarray,
                bestResults_, numSolution_, explorationCsv_.get(), localWire_, globalWire_);
        return;
    }

#pragma omp parallel
    {
        auto threadLocalWire = config_->CreateDefaultLocalWire();
        auto threadGlobalWire = config_->CreateDefaultGlobalWire();
        auto threadBestResults = CreateBestResultsBuffer();
        long long threadNumSolutions = 0;
        std::ostringstream threadCsv;

#pragma omp for collapse(3)
        for (int i = 0; i < (int)numRowMatValues_.size(); i++) {
            for (int j = 0; j < (int)numColumnMatValues_.size(); j++) {
                for (int k = 0; k < (int)numRowSubarrayValues_.size(); k++) {
                    loopsComplete += 1;
                    const long long percentage = loopsComplete * 100 / total;
                    if (total > 1) {
#pragma omp critical
                        std::cout << "\rProgress: " << percentage << "%" << std::flush;
                    }
                    EvaluateGeometry(numRowMatValues_[i], numColumnMatValues_[j], numRowSubarrayValues_[k],
                            threadBestResults, threadNumSolutions, &threadCsv, threadLocalWire, threadGlobalWire);
                }
            }
        }

        if (threadNumSolutions > 0) {
            std::lock_guard<std::mutex> solutionsLock(numSolutionsMutex_);
            numSolution_ += threadNumSolutions;
        }

        {
            std::lock_guard<std::mutex> resultsLock(bestResultsMutex_);
            MergeBestResults(threadBestResults);
        }

        if (!threadCsv.str().empty() && explorationCsv_) {
            std::lock_guard<std::mutex> csvLock(explorationCsvMutex_);
            *explorationCsv_ << threadCsv.str();
        }
    }
}

void EvaCamExplorer::EvaluateGeometry(int numRowMat, int numColumnMat, int numRowSubarray,
        std::vector<std::shared_ptr<CAM_Result>> &bestResults,
        long long &numSolutions,
        std::ostream *csvStream,
        const std::shared_ptr<Wire> &localWire,
        const std::shared_ptr<Wire> &globalWire) {
    const auto numActiveMatPerRowValues = config_->NumActiveMatPerRowValues(numColumnMat);
    const auto numActiveMatPerColumnValues = config_->NumActiveMatPerColumnValues(numRowMat);
    const auto numColumnSubarrayValues = config_->NumColumnSubarrayValues();
    const auto muxSenseAmpValues = config_->MuxSenseAmpValues();
    const auto muxOutputLev1Values = config_->MuxOutputLev1Values();
    const auto muxOutputLev2Values = config_->MuxOutputLev2Values();
    const auto numRowPerSetValues = config_->NumRowPerSetValues();
    const auto areaOptimizationLevels = config_->AreaOptimizationLevels();
    const auto rowDriverOptimizationLevels = config_->RowDriverOptimizationLevels();
    const auto priorityOptimizationLevels = config_->PriorityOptimizationLevels();
    const auto bitSerialWidthValues = config_->BitSerialWidthValues();

    for (int numActiveMatPerRow : numActiveMatPerRowValues)
        for (int numActiveMatPerColumn : numActiveMatPerColumnValues)
            for (int numColumnSubarray : numColumnSubarrayValues)
                for (int numActiveSubarrayPerRow : config_->NumActiveSubarrayPerRowValues(numColumnSubarray))
                    for (int numActiveSubarrayPerColumn : config_->NumActiveSubarrayPerColumnValues(numRowSubarray))
                        for (int muxSenseAmp : muxSenseAmpValues)
                            for (int muxOutputLev1 : muxOutputLev1Values)
                                for (int muxOutputLev2 : muxOutputLev2Values)
                                    for (int numRowPerSet : numRowPerSetValues)
                                        for (int areaOptimizationLevel : areaOptimizationLevels)
                                            for (int rowDriverOptLevel : rowDriverOptimizationLevels)
                                                for (int priorityOptLevel : priorityOptimizationLevels)
                                                    for (int bitSerialWidth : bitSerialWidthValues) {
                                                        if (!config_->IsValidPartitioning(blockSizeBits_,
                                                                    numActiveMatPerRow,
                                                                    numActiveMatPerColumn,
                                                                    numActiveSubarrayPerRow,
                                                                    numActiveSubarrayPerColumn)) {
                                                            continue;
                                                        }

                                                        auto iterCamOpt = std::make_shared<CAM_Opt>();
                                                        iterCamOpt->RowDriver = rowDriverOptLevel;
                                                        iterCamOpt->Proirity = priorityOptLevel;
                                                        iterCamOpt->BitSerialWidth = bitSerialWidth;

                                                        const auto dataBank = BuildBank(numRowMat, numColumnMat, numRowSubarray,
                                                                numColumnSubarray, numActiveMatPerRow,
                                                                numActiveMatPerColumn, numActiveSubarrayPerRow,
                                                                numActiveSubarrayPerColumn, muxSenseAmp, muxOutputLev1,
                                                                muxOutputLev2, numRowPerSet,
                                                                (BufferDesignTarget)areaOptimizationLevel,
                                                                localWire, globalWire, iterCamOpt);

                                                        if (!IsValidCandidate(dataBank)) {
                                                            continue;
                                                        }

                                                        ValidateCapacityOrThrow(dataBank);
                                                        numSolutions++;

                                                        const auto tempResult = MakeResult(dataBank, localWire, globalWire, false);
                                                        UpdateBestResults(bestResults, tempResult);
                                                        if (csvStream) {
                                                            MaybeWriteExplorationCsv(tempResult, *csvStream);
                                                        }
                                                    }
}

void EvaCamExplorer::RefineWires() {
    if (numSolution_ <= 0) {
        return;
    }

    std::cout << std::endl;
    std::cout << "*** There are " << numSolution_ << " Solutions. ***" << std::endl;

    RefineLocalWires();
    RefineGlobalWires();
}

void EvaCamExplorer::RefineLocalWires() {
    for (int localWireType = config_->minLocalWireType;
            localWireType <= config_->maxLocalWireType;
            localWireType++) {
        for (int localWireRepeaterType = config_->minLocalWireRepeaterType;
                localWireRepeaterType <= config_->maxLocalWireRepeaterType;
                localWireRepeaterType++) {
            for (int isLocalWireLowSwing = config_->minIsLocalWireLowSwing;
                    isLocalWireLowSwing <= config_->maxIsLocalWireLowSwing;
                    isLocalWireLowSwing++) {
                if ((WireRepeaterType)localWireRepeaterType != repeated_none
                        && (bool)isLocalWireLowSwing != false) {
                    continue;
                }

                localWire_->Initialize(config_->processNode,
                        (WireType)localWireType,
                        (WireRepeaterType)localWireRepeaterType,
                        config_->temperature,
                        (bool)isLocalWireLowSwing,
                        config_);

                std::shared_ptr<Result> tempResult;
                for (int i = 0; i < (int)full_exploration; i++) {
                    if (!bestResults_[i]->bank->initialized) {
                        continue;
                    }

                    globalWire_->Initialize(config_->processNode,
                            bestResults_[i]->globalWire->wireType,
                            bestResults_[i]->globalWire->wireRepeaterType,
                            config_->temperature,
                            bestResults_[i]->globalWire->isLowSwing,
                            config_);
                    tempResult = ReevaluateBestResultWithWires(i, localWire_, globalWire_);
                }

                MaybeWriteExplorationCsv(tempResult);
            }
        }
    }
}

void EvaCamExplorer::RefineGlobalWires() {
    for (int globalWireType = config_->minGlobalWireType;
            globalWireType <= config_->maxGlobalWireType;
            globalWireType++) {
        for (int globalWireRepeaterType = config_->minGlobalWireRepeaterType;
                globalWireRepeaterType <= config_->maxGlobalWireRepeaterType;
                globalWireRepeaterType++) {
            for (int isGlobalWireLowSwing = config_->minIsGlobalWireLowSwing;
                    isGlobalWireLowSwing <= config_->maxIsGlobalWireLowSwing;
                    isGlobalWireLowSwing++) {
                if ((WireRepeaterType)globalWireRepeaterType != repeated_none
                        && (bool)isGlobalWireLowSwing != false) {
                    continue;
                }

                globalWire_->Initialize(config_->processNode,
                        (WireType)globalWireType,
                        (WireRepeaterType)globalWireRepeaterType,
                        config_->temperature,
                        (bool)isGlobalWireLowSwing,
                        config_);

                std::shared_ptr<Result> tempResult;
                for (int i = 0; i < (int)full_exploration; i++) {
                    if (!bestResults_[i]->bank->initialized) {
                        continue;
                    }

                    localWire_->Initialize(config_->processNode,
                            bestResults_[i]->localWire->wireType,
                            bestResults_[i]->localWire->wireRepeaterType,
                            config_->temperature,
                            bestResults_[i]->localWire->isLowSwing,
                            config_);
                    tempResult = ReevaluateBestResultWithWires(i, localWire_, globalWire_);
                }

                MaybeWriteExplorationCsv(tempResult);
            }
        }
    }
}

std::shared_ptr<Result> EvaCamExplorer::ReevaluateBestResultWithWires(int optimizationIndex,
        const std::shared_ptr<Wire> &localWire,
        const std::shared_ptr<Wire> &globalWire) {
    camOpt_->BitSerialWidth = bestResults_[optimizationIndex]->bank->numBitSerial;
    camOpt_->Proirity = bestResults_[optimizationIndex]->bank->mat->subarray->PriorityOptLevel;
    camOpt_->RowDriver = bestResults_[optimizationIndex]->bank->mat->subarray->DriverOptLevel;

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
            bestResults_[optimizationIndex]->bank->numRowPerSet,
            bestResults_[optimizationIndex]->bank->areaOptimizationLevel,
            localWire,
            globalWire,
            camOpt_);

    bestResults_[optimizationIndex]->bank = trialBank;
    const auto tempResult = MakeResult(trialBank, localWire, globalWire, false);
    bestResults_[optimizationIndex]->compareAndUpdate(tempResult);
    return tempResult;
}

void EvaCamExplorer::BuildPruningResults() {
    if (!config_->IsPruningEnabledForExploration()) {
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
                *(pruningResults[i][j][k]->localWire) = *(bestResults_[i]->localWire);
                *(pruningResults[i][j][k]->globalWire) = *(bestResults_[i]->globalWire);
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
    if (config_->optimizationTarget == full_exploration || !config_->isConstraintApplied) {
        return;
    }

    auto bestResults = AsResults(bestResults_);
    constraintLimits_ = config_->BuildResultLimits(bestResults);
    hasConstraintLimits_ = true;
    config_->ApplyResultLimits(constraintLimits_, bestResults);

    numSolution_ = 0;
    localWire_ = config_->CreateDefaultLocalWire();
    globalWire_ = config_->CreateDefaultGlobalWire();

    if (fixedOuterGeometry_) {
        config_->logger.Verbose() << "Fixed bank/mat/subarray sizes detected; bypassing constrained outer geometry loops.";
        EvaluateConstrainedGeometry(config_->minNumRowMat, config_->minNumColumnMat, config_->minNumRowSubarray);
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
    const auto numActiveMatPerRowValues = config_->NumActiveMatPerRowValues(numColumnMat);
    const auto numActiveMatPerColumnValues = config_->NumActiveMatPerColumnValues(numRowMat);
    const auto numColumnSubarrayValues = config_->NumColumnSubarrayValues();
    const auto muxSenseAmpValues = config_->MuxSenseAmpValues();
    const auto muxOutputLev1Values = config_->MuxOutputLev1Values();
    const auto muxOutputLev2Values = config_->MuxOutputLev2Values();
    const auto numRowPerSetValues = config_->NumRowPerSetValues();
    const auto areaOptimizationLevels = config_->AreaOptimizationLevels();

    for (int numActiveMatPerRow : numActiveMatPerRowValues)
        for (int numActiveMatPerColumn : numActiveMatPerColumnValues)
            for (int numColumnSubarray : numColumnSubarrayValues)
                for (int numActiveSubarrayPerRow : config_->NumActiveSubarrayPerRowValues(numColumnSubarray))
                    for (int numActiveSubarrayPerColumn : config_->NumActiveSubarrayPerColumnValues(numRowSubarray))
                        for (int muxSenseAmp : muxSenseAmpValues)
                            for (int muxOutputLev1 : muxOutputLev1Values)
                                for (int muxOutputLev2 : muxOutputLev2Values)
                                    for (int numRowPerSet : numRowPerSetValues)
                                        for (int areaOptimizationLevel : areaOptimizationLevels) {
                                            if (!config_->IsValidPartitioning(blockSizeBits_,
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
                                                    muxOutputLev2, numRowPerSet,
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
                                                const auto tempResult = MakeResult(dataBank, localWire_, globalWire_, true);
                                                UpdateBestResults(tempResult);
                                            }
                                        }
}

std::shared_ptr<Bank> EvaCamExplorer::BuildBank(int numRowMat, int numColumnMat, int numRowSubarray,
        int numColumnSubarray, int numActiveMatPerRow, int numActiveMatPerColumn,
        int numActiveSubarrayPerRow, int numActiveSubarrayPerColumn, int muxSenseAmp,
        int muxOutputLev1, int muxOutputLev2, int numRowPerSet,
        BufferDesignTarget areaOptimizationLevel, const std::shared_ptr<Wire> &localWire,
        const std::shared_ptr<Wire> &globalWire, const std::shared_ptr<CAM_Opt> &camOpt) const {
    const auto bank = config_->CreateBank();
    config_->InitializeBank(bank, numRowMat, numColumnMat, capacityBits_, blockSizeBits_,
            associativity_, numRowPerSet, numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
            muxOutputLev1, muxOutputLev2, numRowSubarray, numColumnSubarray,
            numActiveSubarrayPerRow, numActiveSubarrayPerColumn, areaOptimizationLevel,
            mem_data, localWire, globalWire, camOpt);
    return bank;
}

std::shared_ptr<Result> EvaCamExplorer::MakeResult(const std::shared_ptr<Bank> &bank,
        const std::shared_ptr<Wire> &localWire,
        const std::shared_ptr<Wire> &globalWire,
        bool cloneBank) const {
    auto result = std::make_shared<Result>();
    result->Initialize(config_);
    result->bank = cloneBank ? bank->clone_bank() : bank;
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
    throw std::runtime_error("ERROR: DATA capacity violation. Shouldn't happen");
}

void EvaCamExplorer::UpdateBestResults(const std::shared_ptr<Result> &result) {
    UpdateBestResults(bestResults_, result);
}

void EvaCamExplorer::UpdateBestResults(std::vector<std::shared_ptr<CAM_Result>> &bestResults,
        const std::shared_ptr<Result> &result) const {
    for (int i = 0; i < (int)full_exploration; i++) {
        bestResults[i]->compareAndUpdate(result);
    }
}

void EvaCamExplorer::MergeBestResults(const std::vector<std::shared_ptr<CAM_Result>> &bestResults) {
    for (int i = 0; i < (int)full_exploration; i++) {
        bestResults_[i]->compareAndUpdate(bestResults[i]);
    }
}

void EvaCamExplorer::MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result) {
    if (!explorationCsv_) {
        return;
    }

    MaybeWriteExplorationCsv(result, *explorationCsv_);
}

void EvaCamExplorer::MaybeWriteExplorationCsv(const std::shared_ptr<Result> &result, std::ostream &stream) const {
    if (!result || !config_->ShouldWriteExplorationCsv()) {
        return;
    }

    result->printToCsvFile(stream);
    stream << std::endl;
}
