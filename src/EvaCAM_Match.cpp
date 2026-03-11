#include "../include/EvaCAM_Match.h"

#include "../include/BankWithHtree.h"
#include "../include/BankWithoutHtree.h"

#include <stdexcept>

EvaCAM_Match::EvaCAM_Match(const std::string &configPath) {
	inputParameter = std::make_shared<InputParameter>();
	inputParameter->RestoreSearchSize();
	inputParameter->ReadInputParameterFromFile(configPath);
	inputParameter->ApplyConstraint();

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
	return static_cast<size_t>(inputParameter->wordWidth);
}

void EvaCAM_Match::InitializeConfiguredBank() {
	long long capacity = inputParameter->capacity * 8;
	long blockSize = inputParameter->wordWidth;

	int numRowMat = SelectConfiguredValue(inputParameter->minNumRowMat, inputParameter->maxNumRowMat);
	int numColumnMat = SelectConfiguredValue(inputParameter->minNumColumnMat, inputParameter->maxNumColumnMat);
	int numActiveMatPerRow = SelectConfiguredValue(inputParameter->minNumActiveMatPerRow, inputParameter->maxNumActiveMatPerRow);
	int numActiveMatPerColumn = SelectConfiguredValue(inputParameter->minNumActiveMatPerColumn, inputParameter->maxNumActiveMatPerColumn);
	int numRowSubarray = SelectConfiguredValue(inputParameter->minNumRowSubarray, inputParameter->maxNumRowSubarray);
	int numColumnSubarray = SelectConfiguredValue(inputParameter->minNumColumnSubarray, inputParameter->maxNumColumnSubarray);
	int numActiveSubarrayPerRow = SelectConfiguredValue(inputParameter->minNumActiveSubarrayPerRow, inputParameter->maxNumActiveSubarrayPerRow);
	int numActiveSubarrayPerColumn = SelectConfiguredValue(inputParameter->minNumActiveSubarrayPerColumn, inputParameter->maxNumActiveSubarrayPerColumn);
	int muxSenseAmp = SelectConfiguredValue(inputParameter->minMuxSenseAmp, inputParameter->maxMuxSenseAmp);
	int muxOutputLev1 = SelectConfiguredValue(inputParameter->minMuxOutputLev1, inputParameter->maxMuxOutputLev1);
	int muxOutputLev2 = SelectConfiguredValue(inputParameter->minMuxOutputLev2, inputParameter->maxMuxOutputLev2);
	int numRowPerSet = SelectConfiguredValue(inputParameter->minNumRowPerSet, inputParameter->maxNumRowPerSet);
	int areaOptimizationLevel = SelectConfiguredValue(inputParameter->minAreaOptimizationLevel, inputParameter->maxAreaOptimizationLevel);
	int rowDriverOpt = SelectConfiguredValue(inputParameter->minRowDriverOptLevel, inputParameter->maxRowDriverOptLevel);
	int priorityOpt = SelectConfiguredValue(inputParameter->minPriorityOptLevel, inputParameter->maxPriorityOptLevel);
	int bitSerialWidth = SelectConfiguredValue(inputParameter->minBitSerialWidth, inputParameter->maxBitSerialWidth);

	localWire = CreateLocalWire();
	globalWire = CreateGlobalWire();

	camOpt = std::make_shared<CAM_Opt>();
	camOpt->RowDriver = rowDriverOpt;
	camOpt->Proirity = priorityOpt;
	camOpt->BitSerialWidth = bitSerialWidth;

	if (inputParameter->routingMode == h_tree)
		bank = std::make_shared<BankWithHtree>();
	else
		bank = std::make_shared<BankWithoutHtree>();

	bank->Initialize(numRowMat, numColumnMat, capacity, blockSize, inputParameter->associativity,
			numRowPerSet, numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
			inputParameter->internalSensing, muxOutputLev1, muxOutputLev2, numRowSubarray,
			numColumnSubarray, numActiveSubarrayPerRow, numActiveSubarrayPerColumn,
			static_cast<BufferDesignTarget>(areaOptimizationLevel), mem_data,
			inputParameter->cell->camType, inputParameter->searchFunction, inputParameter,
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
			inputParameter->minLocalWireType, inputParameter->maxLocalWireType));
	WireRepeaterType repeaterType = static_cast<WireRepeaterType>(SelectConfiguredValue(
			inputParameter->minLocalWireRepeaterType, inputParameter->maxLocalWireRepeaterType));
	bool isLowSwing = static_cast<bool>(SelectConfiguredValue(
			inputParameter->minIsLocalWireLowSwing, inputParameter->maxIsLocalWireLowSwing));

	wire->Initialize(inputParameter->processNode, wireType, repeaterType,
			inputParameter->temperature, isLowSwing, inputParameter);
	return wire;
}

std::shared_ptr<Wire> EvaCAM_Match::CreateGlobalWire() const {
	auto wire = std::make_shared<Wire>();
	WireType wireType = static_cast<WireType>(SelectConfiguredValue(
			inputParameter->minGlobalWireType, inputParameter->maxGlobalWireType));
	WireRepeaterType repeaterType = static_cast<WireRepeaterType>(SelectConfiguredValue(
			inputParameter->minGlobalWireRepeaterType, inputParameter->maxGlobalWireRepeaterType));
	bool isLowSwing = static_cast<bool>(SelectConfiguredValue(
			inputParameter->minIsGlobalWireLowSwing, inputParameter->maxIsGlobalWireLowSwing));

	wire->Initialize(inputParameter->processNode, wireType, repeaterType,
			inputParameter->temperature, isLowSwing, inputParameter);
	return wire;
}
