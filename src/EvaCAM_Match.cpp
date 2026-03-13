#include "EvaCAM_Match.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "Bank.h"
#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "CAM_SubArray.h"
#include "EvaCamConfig.h"
#include "Wire.h"

class EvaCAM_Match::Impl {
    public:
        explicit Impl(const std::string &configPath);

        bool match(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult evaluate(const std::vector<int> &stored, const std::vector<int> &query) const;
        size_t word_width() const;

    private:
        void InitializeConfiguredBank();
        int SelectConfiguredValue(int minValue, int maxValue) const;
        void ValidateBinaryVector(const std::vector<int> &value, const char *name) const;
        std::shared_ptr<Wire> CreateLocalWire() const;
        std::shared_ptr<Wire> CreateGlobalWire() const;

        std::shared_ptr<EvaCamConfig> config;
        std::shared_ptr<Bank> bank;
        std::shared_ptr<Wire> localWire;
        std::shared_ptr<Wire> globalWire;
        std::shared_ptr<CAM_Opt> camOpt;
};

EvaCAM_Match::Impl::Impl(const std::string &configPath) {
    config = std::make_shared<EvaCamConfig>();
    config->SetDeepExploration(false);
    config->ReadConfigFromFile(configPath);
    config->ValidateSupportedConfiguration();

    InitializeConfiguredBank();
}

bool EvaCAM_Match::Impl::match(const std::vector<int> &stored, const std::vector<int> &query) const {
    return evaluate(stored, query).hit;
}

EvaCAMMatchResult EvaCAM_Match::Impl::evaluate(const std::vector<int> &stored, const std::vector<int> &query) const {
    if (!bank || !bank->mat || !bank->mat->subarray) {
        throw std::runtime_error("[EvaCAM_Match] Error: matcher is not initialized.");
    }

    ValidateBinaryVector(stored, "stored");
    ValidateBinaryVector(query, "query");

    return bank->mat->subarray->EvaluateBinaryMatch(stored, query);
}

size_t EvaCAM_Match::Impl::word_width() const {
    return static_cast<size_t>(config->wordWidth);
}

void EvaCAM_Match::Impl::InitializeConfiguredBank() {
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

    if (config->routingMode == h_tree) {
        bank = std::make_shared<BankWithHtree>();
    } else {
        bank = std::make_shared<BankWithoutHtree>();
    }

    bank->Initialize(numRowMat, numColumnMat, capacity, blockSize, config->associativity,
            numRowPerSet, numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
            config->internalSensing, muxOutputLev1, muxOutputLev2, numRowSubarray,
            numColumnSubarray, numActiveSubarrayPerRow, numActiveSubarrayPerColumn,
            static_cast<BufferDesignTarget>(areaOptimizationLevel), mem_data,
            config->cell->camType, config->searchFunction, config,
            localWire, globalWire, camOpt);

    if (bank->invalid) {
        throw std::runtime_error("[EvaCAM_Match] Error: configured bank is invalid for matching.");
    }
}

int EvaCAM_Match::Impl::SelectConfiguredValue(int minValue, int maxValue) const {
    (void)maxValue;
    return minValue;
}

void EvaCAM_Match::Impl::ValidateBinaryVector(const std::vector<int> &value, const char *name) const {
    if (value.size() != word_width()) {
        throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                + " vector length does not match configured word width.");
    }

    for (int bit : value) {
        if (bit != 0 && bit != 1) {
            throw std::invalid_argument(std::string("[EvaCAM_Match] Error: ") + name
                    + " vector must contain only binary values.");
        }
    }
}

std::shared_ptr<Wire> EvaCAM_Match::Impl::CreateLocalWire() const {
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

std::shared_ptr<Wire> EvaCAM_Match::Impl::CreateGlobalWire() const {
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

EvaCAM_Match::EvaCAM_Match(const std::string &configPath)
    : impl_(std::make_unique<Impl>(configPath)) {}

EvaCAM_Match::~EvaCAM_Match() = default;

EvaCAM_Match::EvaCAM_Match(EvaCAM_Match&&) noexcept = default;

EvaCAM_Match& EvaCAM_Match::operator=(EvaCAM_Match&&) noexcept = default;

bool EvaCAM_Match::match(const std::vector<int> &stored, const std::vector<int> &query) const {
    return impl_->match(stored, query);
}

EvaCAMMatchResult EvaCAM_Match::evaluate(const std::vector<int> &stored, const std::vector<int> &query) const {
    return impl_->evaluate(stored, query);
}

size_t EvaCAM_Match::word_width() const {
    return impl_->word_width();
}
