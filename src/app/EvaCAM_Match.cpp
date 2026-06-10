#include "EvaCAM_Match.h"

#include <stdexcept>

#include "Bank.h"
#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "CAM_SubArray.h"
#include "EvaCamConfig.h"
#include "Wire.h"
#include "config/EvaCamConfigValidator.h"

EvaCAM_Match::EvaCAM_Match(const std::string &configPath) {
    config = std::make_shared<EvaCamConfig>();
    config->SetDeepExploration(false);
    config->ReadConfigFromFile(configPath);
    EvaCamConfigValidator::Validate(*config);

    InitializeConfiguredBank();
}

EvaCAM_Match::~EvaCAM_Match() = default;

EvaCAM_Match::EvaCAM_Match(EvaCAM_Match&&) noexcept = default;

EvaCAM_Match& EvaCAM_Match::operator=(EvaCAM_Match&&) noexcept = default;

bool EvaCAM_Match::match(const std::vector<int> &stored, const std::vector<int> &query) const {
    return evaluate_vector(stored, query).hit;
}

EvaCAMMatchResult EvaCAM_Match::evaluate_vector(const std::vector<int> &stored, const std::vector<int> &query) const {
    EnsureInitialized();

    switch (config->input.searchFunction) {
        case EX:
            return EvaluateExactVector(stored, query);
        case BE:
            return EvaluateBestVector(stored, query);
        case TH:
            return EvaluateThresholdVector(stored, query);
    }

    throw std::runtime_error("[EvaCAM_Match] Error: unsupported search function.");
}

EvaCAMMatchResult EvaCAM_Match::evaluate_mismatches(int mismatches) const {
    EnsureInitialized();
    ValidateMismatchCount(mismatches);
    return LookupMismatchResult(mismatches);
}

EvaCAMMatchResult EvaCAM_Match::evaluate_vector(
        const std::vector<std::pair<double, double>> &stored,
        const std::vector<double> &query) const {
    EnsureInitialized();

    if (config->technology.cell->camType != ACAM) {
        throw std::invalid_argument("[EvaCAM_Match] Error: range/value vector input is only valid for ACAM.");
    }

    switch (config->input.searchFunction) {
        case EX:
            return EvaluateExactAcamVector(stored, query);
        case BE:
            return EvaluateBestAcamVector(stored, query);
        case TH:
            return EvaluateThresholdAcamVector(stored, query);
    }

    throw std::runtime_error("[EvaCAM_Match] Error: unsupported search function.");
}

std::vector<EvaCAMMatchResult> EvaCAM_Match::evaluate_array(const std::vector<int> &mismatchCounts) const {
    EnsureInitialized();

    std::vector<EvaCAMMatchResult> results;
    results.reserve(mismatchCounts.size());
    for (int mismatches : mismatchCounts) {
        results.push_back(evaluate_mismatches(mismatches));
    }
    return results;
}

std::vector<EvaCAMMatchResult> EvaCAM_Match::evaluate_array(
        const std::vector<std::vector<int>> &storedRows,
        const std::vector<int> &query) const {
    EnsureInitialized();

    switch (config->technology.cell->camType) {
        case TCAM:
        case MCAM:
            break;
        case ACAM:
            throw std::invalid_argument("[EvaCAM_Match] Error: ACAM requires range/value array input.");
    }

    std::vector<EvaCAMMatchResult> results;
    results.reserve(storedRows.size());
    for (const auto &stored : storedRows) {
        results.push_back(evaluate_vector(stored, query));
    }
    return results;
}

std::vector<EvaCAMMatchResult> EvaCAM_Match::evaluate_array(
        const std::vector<std::vector<std::pair<double, double>>> &storedRows,
        const std::vector<double> &query) const {
    EnsureInitialized();

    if (config->technology.cell->camType != ACAM) {
        throw std::invalid_argument("[EvaCAM_Match] Error: range/value array input is only valid for ACAM.");
    }

    std::vector<EvaCAMMatchResult> results;
    results.reserve(storedRows.size());
    for (const auto &stored : storedRows) {
        results.push_back(evaluate_vector(stored, query));
    }
    return results;
}

size_t EvaCAM_Match::word_width() const {
    return static_cast<size_t>(config->input.wordWidth);
}

void EvaCAM_Match::InitializeConfiguredBank() {
    long long capacity = config->input.capacity * 8;
    long blockSize = config->input.wordWidth;

    const auto &resolved = config->resolvedExploration;
    int numRowMat = resolved.geometry.numRowMatValues.front();
    int numColumnMat = resolved.geometry.numColumnMatValues.front();
    int numActiveMatPerRow = config->exploration.ActiveMatPerRowValues(numColumnMat).front();
    int numActiveMatPerColumn = config->exploration.ActiveMatPerColumnValues(numRowMat).front();
    int numRowSubarray = resolved.geometry.numRowSubarrayValues.front();
    int numColumnSubarray = resolved.geometry.numColumnSubarrayValues.front();
    int numActiveSubarrayPerRow = config->exploration.ActiveSubarrayPerRowValues(numColumnSubarray).front();
    int numActiveSubarrayPerColumn = config->exploration.ActiveSubarrayPerColumnValues(numRowSubarray).front();
    int muxSenseAmp = resolved.geometry.muxSenseAmpValues.front();
    int muxOutputLev1 = resolved.geometry.muxOutputLev1Values.front();
    int muxOutputLev2 = resolved.geometry.muxOutputLev2Values.front();
    int numRowPerSet = resolved.geometry.numRowPerSetValues.front();
    int areaOptimizationLevel = resolved.cam.areaOptimizationLevelValues.front();
    int rowDriverOpt = resolved.cam.rowDriverOptLevelValues.front();
    int priorityOpt = resolved.cam.priorityOptLevelValues.front();
    int bitSerialWidth = resolved.cam.bitSerialWidthValues.front();

    localWire = CreateLocalWire();
    globalWire = CreateGlobalWire();

    camOpt.RowDriver = rowDriverOpt;
    camOpt.Proirity = priorityOpt;
    camOpt.BitSerialWidth = bitSerialWidth;

    if (config->input.routingMode == h_tree) {
        bank = std::make_shared<BankWithHtree>();
    } else {
        bank = std::make_shared<BankWithoutHtree>();
    }

    bank->Initialize(numRowMat, numColumnMat, capacity, blockSize, config->input.associativity,
            numRowPerSet, numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
            config->input.internalSensing, muxOutputLev1, muxOutputLev2, numRowSubarray,
            numColumnSubarray, numActiveSubarrayPerRow, numActiveSubarrayPerColumn,
            static_cast<BufferDesignTarget>(areaOptimizationLevel), mem_data,
            config->technology.cell->camType, config->input.searchFunction, config,
            localWire, globalWire, camOpt);

    if (bank->invalid) {
        throw std::runtime_error("[EvaCAM_Match] Error: configured bank is invalid for matching.");
    }

    BuildMismatchLut();
}

void EvaCAM_Match::BuildMismatchLut() {
    if (!bank || !bank->mat || !bank->mat->subarray) {
        throw std::runtime_error("[EvaCAM_Match] Error: matcher is not initialized.");
    }

    mismatchResults.clear();
    if (config->technology.cell->camType != TCAM || config->input.searchFunction != EX) {
        return;
    }

    mismatchResults.reserve(word_width() + 1);
    for (size_t mismatches = 0; mismatches <= word_width(); mismatches++) {
        mismatchResults.push_back(bank->mat->subarray->EvaluateBinaryMatchByMismatches(
                static_cast<int>(mismatches)));
    }
}

void EvaCAM_Match::EnsureInitialized() const {
    if (!bank || !bank->mat || !bank->mat->subarray) {
        throw std::runtime_error("[EvaCAM_Match] Error: matcher is not initialized.");
    }
}

EvaCAMMatchResult EvaCAM_Match::EvaluateExactVector(
        const std::vector<int> &stored,
        const std::vector<int> &query) const {
    switch (config->technology.cell->camType) {
        case TCAM:
            return EvaluateExactTcamVector(stored, query);
        case MCAM:
            ValidateVectorLength(stored.size(), "stored");
            ValidateVectorLength(query.size(), "query");
            throw std::runtime_error("[EvaCAM_Match] Error: exact MCAM vector evaluation is not implemented.");
        case ACAM:
            throw std::invalid_argument("[EvaCAM_Match] Error: ACAM requires range/value vector input.");
    }

    throw std::runtime_error("[EvaCAM_Match] Error: unsupported CAM type.");
}

EvaCAMMatchResult EvaCAM_Match::EvaluateBestVector(
        const std::vector<int> &stored,
        const std::vector<int> &query) const {
    switch (config->technology.cell->camType) {
        case TCAM:
            ValidateTcamStoredVector(stored, "stored");
            ValidateBinaryVector(query, "query");
            throw std::runtime_error("[EvaCAM_Match] Error: best TCAM vector evaluation is not implemented.");
        case MCAM:
            ValidateVectorLength(stored.size(), "stored");
            ValidateVectorLength(query.size(), "query");
            throw std::runtime_error("[EvaCAM_Match] Error: best MCAM vector evaluation is not implemented.");
        case ACAM:
            throw std::invalid_argument("[EvaCAM_Match] Error: ACAM requires range/value vector input.");
    }

    throw std::runtime_error("[EvaCAM_Match] Error: unsupported CAM type.");
}

EvaCAMMatchResult EvaCAM_Match::EvaluateThresholdVector(
        const std::vector<int> &stored,
        const std::vector<int> &query) const {
    switch (config->technology.cell->camType) {
        case TCAM:
            ValidateTcamStoredVector(stored, "stored");
            ValidateBinaryVector(query, "query");
            throw std::runtime_error("[EvaCAM_Match] Error: threshold TCAM vector evaluation is not implemented.");
        case MCAM:
            ValidateVectorLength(stored.size(), "stored");
            ValidateVectorLength(query.size(), "query");
            throw std::runtime_error("[EvaCAM_Match] Error: threshold MCAM vector evaluation is not implemented.");
        case ACAM:
            throw std::invalid_argument("[EvaCAM_Match] Error: ACAM requires range/value vector input.");
    }

    throw std::runtime_error("[EvaCAM_Match] Error: unsupported CAM type.");
}

EvaCAMMatchResult EvaCAM_Match::EvaluateExactTcamVector(
        const std::vector<int> &stored,
        const std::vector<int> &query) const {
    ValidateTcamStoredVector(stored, "stored");
    ValidateBinaryVector(query, "query");

    return evaluate_mismatches(CountTcamMismatches(stored, query));
}

EvaCAMMatchResult EvaCAM_Match::EvaluateExactAcamVector(
        const std::vector<std::pair<double, double>> &stored,
        const std::vector<double> &query) const {
    ValidateAcamRangeVector(stored, "stored");
    ValidateAnalogVector(query, "query");
    throw std::runtime_error("[EvaCAM_Match] Error: exact ACAM vector evaluation is not implemented.");
}

EvaCAMMatchResult EvaCAM_Match::EvaluateBestAcamVector(
        const std::vector<std::pair<double, double>> &stored,
        const std::vector<double> &query) const {
    ValidateAcamRangeVector(stored, "stored");
    ValidateAnalogVector(query, "query");
    throw std::runtime_error("[EvaCAM_Match] Error: best ACAM vector evaluation is not implemented.");
}

EvaCAMMatchResult EvaCAM_Match::EvaluateThresholdAcamVector(
        const std::vector<std::pair<double, double>> &stored,
        const std::vector<double> &query) const {
    ValidateAcamRangeVector(stored, "stored");
    ValidateAnalogVector(query, "query");
    throw std::runtime_error("[EvaCAM_Match] Error: threshold ACAM vector evaluation is not implemented.");
}

EvaCAMMatchResult EvaCAM_Match::LookupMismatchResult(int mismatches) const {
    if (mismatches < 0 || static_cast<size_t>(mismatches) >= mismatchResults.size()) {
        throw std::runtime_error("[EvaCAM_Match] Error: mismatch lookup table is not initialized.");
    }
    return mismatchResults[static_cast<size_t>(mismatches)];
}

int EvaCAM_Match::CountTcamMismatches(const std::vector<int> &stored, const std::vector<int> &query) const {
    int mismatches = 0;
    for (size_t i = 0; i < stored.size(); i++) {
        if (stored[i] == -1) {
            continue;
        }
        if (stored[i] != query[i]) {
            mismatches++;
        }
    }
    return mismatches;
}

void EvaCAM_Match::ValidateMismatchCount(int mismatches) const {
    if (config->technology.cell->camType != TCAM) {
        throw std::invalid_argument("[EvaCAM_Match] Error: mismatch-count evaluation is only valid for TCAM.");
    }
    if (config->input.searchFunction != EX) {
        throw std::runtime_error("[EvaCAM_Match] Error: mismatch-count evaluation currently supports exact search only.");
    }
    if (mismatches < 0 || static_cast<size_t>(mismatches) > word_width()) {
        throw std::invalid_argument("[EvaCAM_Match] Error: mismatch count is out of range.");
    }
}

void EvaCAM_Match::ValidateBinaryVector(const std::vector<int> &value, const char *name) const {
    ValidateVectorLength(value.size(), name);

    for (int bit : value) {
        if (bit != 0 && bit != 1) {
            throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                    + " vector must contain only binary values.");
        }
    }
}

void EvaCAM_Match::ValidateVectorLength(size_t size, const char *name) const {
    if (size != word_width()) {
        throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                + " vector length does not match configured word width.");
    }
}

void EvaCAM_Match::ValidateTcamStoredVector(const std::vector<int> &value, const char *name) const {
    ValidateVectorLength(value.size(), name);

    for (int bit : value) {
        if (bit != -1 && bit != 0 && bit != 1) {
            throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                    + " vector must contain only 0, 1, or -1 wildcard values.");
        }
    }
}

void EvaCAM_Match::ValidateAnalogVector(const std::vector<double> &value, const char *name) const {
    ValidateVectorLength(value.size(), name);
}

void EvaCAM_Match::ValidateAcamRangeVector(
        const std::vector<std::pair<double, double>> &value,
        const char *name) const {
    ValidateVectorLength(value.size(), name);

    for (const auto &range : value) {
        if (range.first > range.second) {
            throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                    + " ranges must have lower bound less than or equal to upper bound.");
        }
    }
}

Wire EvaCAM_Match::CreateLocalWire() const {
    Wire wire;
    const auto &resolved = config->resolvedExploration;
    WireType wireType = static_cast<WireType>(resolved.wires.localWireTypeValues.front());
    WireRepeaterType repeaterType = static_cast<WireRepeaterType>(
            resolved.wires.localWireRepeaterTypeValues.front());
    bool isLowSwing = static_cast<bool>(resolved.wires.isLocalWireLowSwingValues.front());

    wire.Initialize(config->input.processNode, wireType, repeaterType,
            config->input.temperature, isLowSwing, config);
    return wire;
}

Wire EvaCAM_Match::CreateGlobalWire() const {
    Wire wire;
    const auto &resolved = config->resolvedExploration;
    WireType wireType = static_cast<WireType>(resolved.wires.globalWireTypeValues.front());
    WireRepeaterType repeaterType = static_cast<WireRepeaterType>(
            resolved.wires.globalWireRepeaterTypeValues.front());
    bool isLowSwing = static_cast<bool>(resolved.wires.isGlobalWireLowSwingValues.front());

    wire.Initialize(config->input.processNode, wireType, repeaterType,
            config->input.temperature, isLowSwing, config);
    return wire;
}
