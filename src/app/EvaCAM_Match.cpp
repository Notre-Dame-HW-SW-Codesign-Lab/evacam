#include "EvaCAM_Match.h"

#include <algorithm>
#include <mutex>
#include <stdexcept>

#include "Bank.h"
#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "CAM_SubArray.h"
#include "EvaCamConfig.h"
#include "Wire.h"
#include "config/EvaCamConfigValidator.h"
#include "input/YamlNodeHelpers.h"

EvaCAM_Match::EvaCAM_Match(const std::string &configPath) {
    config = std::make_shared<EvaCamConfig>();
    config->SetDeepExploration(false);
    {
        std::lock_guard<std::recursive_mutex> parserLock(
                YamlHelpers::ParserMutex());
        config->ReadConfigFromFile(configPath);
    }
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

EvaCAMMatchResult EvaCAM_Match::evaluate_threshold(
        const std::vector<int> &stored,
        const std::vector<int> &query,
        int maxMismatches) const {
    EnsureInitialized();
    ValidateTcamStoredVector(stored, "stored");
    ValidateBinaryVector(query, "query");

    return evaluate_threshold(CountTcamMismatches(stored, query), maxMismatches);
}

EvaCAMMatchResult EvaCAM_Match::evaluate_threshold(int mismatches, int maxMismatches) const {
    EnsureInitialized();
    ValidateTcamMismatchCount(mismatches);
    ValidateMaxMismatches(maxMismatches);
    ValidateThresholdSenseMargin(maxMismatches);

    EvaCAMMatchResult result = LookupMismatchResult(mismatches);
    result.hit = (mismatches <= maxMismatches);
    return result;
}

EvaCAMMatchResult EvaCAM_Match::evaluate_vector(
        const std::vector<std::pair<double, double>> &,
        const std::vector<double> &) const {
    EnsureInitialized();

    if (config->technology.cell->camType != ACAM) {
        throw std::invalid_argument("[EvaCAM_Match] Error: range/value vector input is only valid for ACAM.");
    }

    throw std::runtime_error("[EvaCAM_Match] Error: ACAM vector evaluation is not implemented.");
}

std::vector<EvaCAMMatchResult> EvaCAM_Match::evaluate_array(const std::vector<int> &mismatchCounts) const {
    EnsureInitialized();

    if (config->technology.cell->camType != TCAM) {
        ValidateTcamMismatchCounts(mismatchCounts);
    }

    switch (config->input.searchFunction) {
        case EX:
            break;
        case BE:
            return EvaluateBestTcamArray(mismatchCounts);
        case TH:
            throw std::runtime_error("[EvaCAM_Match] Error: threshold TCAM array evaluation requires evaluate_threshold(..., maxMismatches).");
    }

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

    if (config->technology.cell->camType == TCAM && config->input.searchFunction == BE) {
        ValidateBinaryVector(query, "query");
        std::vector<int> mismatchCounts;
        mismatchCounts.reserve(storedRows.size());
        for (const auto &stored : storedRows) {
            ValidateTcamStoredVector(stored, "stored");
            mismatchCounts.push_back(CountTcamMismatches(stored, query));
        }
        return EvaluateBestTcamArray(mismatchCounts);
    }

    if (config->technology.cell->camType == TCAM && config->input.searchFunction == TH) {
        throw std::runtime_error("[EvaCAM_Match] Error: threshold TCAM array evaluation requires evaluate_threshold(..., maxMismatches).");
    }

    std::vector<EvaCAMMatchResult> results;
    results.reserve(storedRows.size());
    for (const auto &stored : storedRows) {
        results.push_back(evaluate_vector(stored, query));
    }
    return results;
}

std::vector<EvaCAMMatchResult> EvaCAM_Match::evaluate_array(
        const std::vector<std::vector<std::pair<double, double>>> &,
        const std::vector<double> &) const {
    EnsureInitialized();

    if (config->technology.cell->camType != ACAM) {
        throw std::invalid_argument("[EvaCAM_Match] Error: range/value array input is only valid for ACAM.");
    }

    throw std::runtime_error("[EvaCAM_Match] Error: ACAM array evaluation is not implemented.");
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

    bank->Initialize(numRowMat, numColumnMat, capacity, blockSize,
            numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
            config->input.internalSensing, muxOutputLev1, muxOutputLev2, numRowSubarray,
            numColumnSubarray, numActiveSubarrayPerRow, numActiveSubarrayPerColumn,
            static_cast<BufferDesignTarget>(areaOptimizationLevel),
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
    if (config->technology.cell->camType != TCAM) {
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
            ValidateMcamVector(stored, "stored");
            ValidateMcamVector(query, "query");
            return bank->mat->subarray->EvaluateMcamExactMatch(stored, query);
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
            throw std::runtime_error("[EvaCAM_Match] Error: best TCAM vector evaluation requires evaluate_array(rows, query).");
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
            throw std::runtime_error("[EvaCAM_Match] Error: threshold TCAM vector evaluation requires evaluate_threshold(..., maxMismatches).");
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

std::vector<EvaCAMMatchResult> EvaCAM_Match::EvaluateBestTcamArray(
        const std::vector<int> &mismatchCounts) const {
    ValidateTcamMismatchCounts(mismatchCounts);

    const int bestMismatches = *std::min_element(mismatchCounts.begin(), mismatchCounts.end());
    ValidateBestMatchSenseMargin(mismatchCounts, bestMismatches);

    std::vector<EvaCAMMatchResult> results;
    results.reserve(mismatchCounts.size());
    for (int mismatches : mismatchCounts) {
        EvaCAMMatchResult result = LookupMismatchResult(mismatches);
        result.hit = (mismatches == bestMismatches);
        results.push_back(result);
    }
    return results;
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

int EvaCAM_Match::MaxDetectableMismatches() const {
    int maxDetectable = 0;
    const double requiredSenseMargin = bank->mat->subarray->senseVoltage;
    for (size_t mismatches = 1; mismatches <= word_width(); mismatches++) {
        if (LookupMismatchResult(static_cast<int>(mismatches)).senseMargin < requiredSenseMargin) {
            break;
        }
        maxDetectable = static_cast<int>(mismatches);
    }
    return maxDetectable;
}

void EvaCAM_Match::ValidateTcamMismatchCounts(const std::vector<int> &mismatchCounts) const {
    if (mismatchCounts.empty()) {
        throw std::invalid_argument("[EvaCAM_Match] Error: mismatch-count array must not be empty.");
    }
    for (int mismatches : mismatchCounts) {
        ValidateTcamMismatchCount(mismatches);
    }
}

void EvaCAM_Match::ValidateBestMatchSenseMargin(
        const std::vector<int> &mismatchCounts,
        int bestMismatches) const {
    if (bestMismatches > MaxDetectableMismatches()) {
        throw std::runtime_error("[EvaCAM_Match] Error: best match exceeds sense-margin capability.");
    }

    const bool allBest = std::all_of(mismatchCounts.begin(), mismatchCounts.end(),
            [bestMismatches](int mismatches) {
                return mismatches == bestMismatches;
            });
    if (allBest || static_cast<size_t>(bestMismatches) == word_width()) {
        return;
    }

    const EvaCAMMatchResult boundary = LookupMismatchResult(bestMismatches + 1);
    const double requiredSenseMargin = bank->mat->subarray->senseVoltage;
    if (boundary.senseMargin < requiredSenseMargin) {
        throw std::runtime_error("[EvaCAM_Match] Error: best-match boundary exceeds sense-margin capability.");
    }
}

void EvaCAM_Match::ValidateTcamMismatchCount(int mismatches) const {
    if (config->technology.cell->camType != TCAM) {
        throw std::invalid_argument("[EvaCAM_Match] Error: mismatch-count evaluation is only valid for TCAM.");
    }
    if (mismatches < 0 || static_cast<size_t>(mismatches) > word_width()) {
        throw std::invalid_argument("[EvaCAM_Match] Error: mismatch count is out of range.");
    }
}

void EvaCAM_Match::ValidateMismatchCount(int mismatches) const {
    ValidateTcamMismatchCount(mismatches);
    if (config->input.searchFunction != EX) {
        throw std::runtime_error("[EvaCAM_Match] Error: mismatch-count evaluation currently supports exact search only.");
    }
}

void EvaCAM_Match::ValidateMaxMismatches(int maxMismatches) const {
    if (maxMismatches < 0 || static_cast<size_t>(maxMismatches) > word_width()) {
        throw std::invalid_argument("[EvaCAM_Match] Error: maxMismatches is out of range.");
    }
}

void EvaCAM_Match::ValidateThresholdSenseMargin(int maxMismatches) const {
    if (static_cast<size_t>(maxMismatches) == word_width()) {
        return;
    }

    const EvaCAMMatchResult boundary = LookupMismatchResult(maxMismatches + 1);
    const double requiredSenseMargin = bank->mat->subarray->senseVoltage;
    if (boundary.senseMargin < requiredSenseMargin) {
        throw std::runtime_error("[EvaCAM_Match] Error: maxMismatches exceeds sense-margin capability.");
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

void EvaCAM_Match::ValidateMcamVector(
        const std::vector<int> &value,
        const char *name) const {
    ValidateVectorLength(value.size(), name);
    const int numStates = config->technology.cell->numResistanceState;
    for (int symbol : value) {
        if (symbol < 0 || symbol >= numStates) {
            throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                    + " MCAM vector values must be between 0 and "
                    + std::to_string(numStates - 1) + ".");
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
