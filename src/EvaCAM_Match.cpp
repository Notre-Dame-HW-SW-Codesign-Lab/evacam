#include "../include/EvaCAM_Match.h"

#include "../include/BankWithHtree.h"
#include "../include/BankWithoutHtree.h"

EvaCAM_Match::EvaCAM_Match(const std::string &configPath) {
    config = std::make_shared<EvaCamConfig>();
    config->SetDeepExploration(false);
    config->ReadConfigFromFile(configPath);
    config->ValidateSupportedConfiguration();

    InitializeConfiguredBank();
}

bool EvaCAM_Match::match(const std::vector<int> &stored, const std::vector<int> &query) const {
    return evaluate(stored, query).hit;
}

EvaCAMMatchResult EvaCAM_Match::evaluate(const std::vector<int> &stored, const std::vector<int> &query) const {
    if (!bank || !bank->mat || !bank->mat->subarray)
        throw std::runtime_error("[EvaCAM_Match] Error: matcher is not initialized.");

    ValidateBinaryVector(stored, "stored");
    ValidateBinaryVector(query, "query");

    return bank->mat->subarray->EvaluateBinaryMatch(stored, query);
}

size_t EvaCAM_Match::word_width() const {
    return static_cast<size_t>(config->wordWidth);
}

void EvaCAM_Match::InitializeConfiguredBank() {
    long long capacity = config->capacity * 8;
    long blockSize = config->wordWidth;

    int numRowMat = SelectConfiguredValue(config->minNumRowMat, config->maxNumRowMat);
    int numColumnMat = SelectConfiguredValue(config->minNumColumnMat, config->maxNumColumnMat);
    int numActiveMatPerRow = SelectConfiguredValue(config->minNumActiveMatPerRow, config->maxNumActiveMatPerRow);
    int numActiveMatPerColumn = SelectConfiguredValue(config->minNumActiveMatPerColumn, config->maxNumActiveMatPerColumn);
    int numRowSubarray = SelectConfiguredValue(config->minNumRowSubarray, config->maxNumRowSubarray);
    int numColumnSubarray = SelectConfiguredValue(config->minNumColumnSubarray, config->maxNumColumnSubarray);
    int numActiveSubarrayPerRow = SelectConfiguredValue(config->minNumActiveSubarrayPerRow, config->maxNumActiveSubarrayPerRow);
    int numActiveSubarrayPerColumn = SelectConfiguredValue(config->minNumActiveSubarrayPerColumn, config->maxNumActiveSubarrayPerColumn);
    int muxSenseAmp = SelectConfiguredValue(config->minMuxSenseAmp, config->maxMuxSenseAmp);
    int muxOutputLev1 = SelectConfiguredValue(config->minMuxOutputLev1, config->maxMuxOutputLev1);
    int muxOutputLev2 = SelectConfiguredValue(config->minMuxOutputLev2, config->maxMuxOutputLev2);
    int numRowPerSet = SelectConfiguredValue(config->minNumRowPerSet, config->maxNumRowPerSet);
    int areaOptimizationLevel = SelectConfiguredValue(config->minAreaOptimizationLevel, config->maxAreaOptimizationLevel);
    int rowDriverOpt = SelectConfiguredValue(config->minRowDriverOptLevel, config->maxRowDriverOptLevel);
    int priorityOpt = SelectConfiguredValue(config->minPriorityOptLevel, config->maxPriorityOptLevel);
    int bitSerialWidth = SelectConfiguredValue(config->minBitSerialWidth, config->maxBitSerialWidth);

    localWire = CreateLocalWire();
    globalWire = CreateGlobalWire();

    camOpt = std::make_shared<CAM_Opt>();
    camOpt->RowDriver = rowDriverOpt;
    camOpt->Proirity = priorityOpt;
    camOpt->BitSerialWidth = bitSerialWidth;

    if (config->routingMode == h_tree)
        bank = std::make_shared<BankWithHtree>();
    else
        bank = std::make_shared<BankWithoutHtree>();

    bank->Initialize(numRowMat, numColumnMat, capacity, blockSize, config->associativity,
            numRowPerSet, numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
            config->internalSensing, muxOutputLev1, muxOutputLev2, numRowSubarray,
            numColumnSubarray, numActiveSubarrayPerRow, numActiveSubarrayPerColumn,
            static_cast<BufferDesignTarget>(areaOptimizationLevel), mem_data,
            config->cell->camType, config->searchFunction, config,
            localWire, globalWire, camOpt);

    if (bank->invalid)
        throw std::runtime_error("[EvaCAM_Match] Error: configured bank is invalid for matching.");
}

int EvaCAM_Match::SelectConfiguredValue(int minValue, int maxValue) const {
    (void)maxValue;
    return minValue;
}

void EvaCAM_Match::ValidateBinaryVector(const std::vector<int> &value, const char *name) const {
    if (value.size() != word_width())
        throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                + " vector length does not match configured word width.");

    for (int bit : value) {
        if (bit != 0 && bit != 1)
            throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                    + " vector must contain only binary values.");
    }
}

std::shared_ptr<Wire> EvaCAM_Match::CreateLocalWire() const {
    auto wire = std::make_shared<Wire>();
    WireType wireType = static_cast<WireType>(SelectConfiguredValue(
                config->minLocalWireType, config->maxLocalWireType));
    WireRepeaterType repeaterType = static_cast<WireRepeaterType>(SelectConfiguredValue(
                config->minLocalWireRepeaterType, config->maxLocalWireRepeaterType));
    bool isLowSwing = static_cast<bool>(SelectConfiguredValue(
                config->minIsLocalWireLowSwing, config->maxIsLocalWireLowSwing));

    wire->Initialize(config->processNode, wireType, repeaterType,
            config->temperature, isLowSwing, config);
    return wire;
}

std::shared_ptr<Wire> EvaCAM_Match::CreateGlobalWire() const {
    auto wire = std::make_shared<Wire>();
    WireType wireType = static_cast<WireType>(SelectConfiguredValue(
                config->minGlobalWireType, config->maxGlobalWireType));
    WireRepeaterType repeaterType = static_cast<WireRepeaterType>(SelectConfiguredValue(
                config->minGlobalWireRepeaterType, config->maxGlobalWireRepeaterType));
    bool isLowSwing = static_cast<bool>(SelectConfiguredValue(
                config->minIsGlobalWireLowSwing, config->maxIsGlobalWireLowSwing));

    wire->Initialize(config->processNode, wireType, repeaterType,
            config->temperature, isLowSwing, config);
    return wire;
}
