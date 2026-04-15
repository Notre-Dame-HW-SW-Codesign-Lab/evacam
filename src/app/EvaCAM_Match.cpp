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
#include "config/EvaCamConfigValidator.h"

class EvaCAM_Match::Impl {
    public:
        explicit Impl(const std::string &configPath);

        bool match(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult evaluate(const std::vector<int> &stored, const std::vector<int> &query) const;
        size_t word_width() const;

    private:
        void InitializeConfiguredBank();
        void ValidateBinaryVector(const std::vector<int> &value, const char *name) const;
        std::shared_ptr<Wire> CreateLocalWire() const;
        std::shared_ptr<Wire> CreateGlobalWire() const;

        std::shared_ptr<EvaCamConfig> config;
        std::shared_ptr<Bank> bank;
        std::shared_ptr<Wire> localWire;
        std::shared_ptr<Wire> globalWire;
        CAM_Opt camOpt{};
};

EvaCAM_Match::Impl::Impl(const std::string &configPath) {
    config = std::make_shared<EvaCamConfig>();
    config->SetDeepExploration(false);
    config->ReadConfigFromFile(configPath);
    EvaCamConfigValidator::Validate(*config);

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
    return static_cast<size_t>(config->input.wordWidth);
}

void EvaCAM_Match::Impl::InitializeConfiguredBank() {
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
    const auto &resolved = config->resolvedExploration;
    WireType wireType = static_cast<WireType>(resolved.wires.localWireTypeValues.front());
    WireRepeaterType repeaterType = static_cast<WireRepeaterType>(
            resolved.wires.localWireRepeaterTypeValues.front());
    bool isLowSwing = static_cast<bool>(resolved.wires.isLocalWireLowSwingValues.front());

    wire->Initialize(config->input.processNode, wireType, repeaterType,
            config->input.temperature, isLowSwing, config);
    return wire;
}

std::shared_ptr<Wire> EvaCAM_Match::Impl::CreateGlobalWire() const {
    auto wire = std::make_shared<Wire>();
    const auto &resolved = config->resolvedExploration;
    WireType wireType = static_cast<WireType>(resolved.wires.globalWireTypeValues.front());
    WireRepeaterType repeaterType = static_cast<WireRepeaterType>(
            resolved.wires.globalWireRepeaterTypeValues.front());
    bool isLowSwing = static_cast<bool>(resolved.wires.isGlobalWireLowSwingValues.front());

    wire->Initialize(config->input.processNode, wireType, repeaterType,
            config->input.temperature, isLowSwing, config);
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
