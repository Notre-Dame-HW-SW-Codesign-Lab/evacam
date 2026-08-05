#include "BankWithoutHtree.h"
#include "formula.h"

namespace {

constexpr double kInvalidResult = 1e41;

int ActiveMatCount(const BankWithoutHtree &bank) {
    return bank.numActiveMatPerRow * bank.numActiveMatPerColumn;
}

}

void BankWithoutHtree::Initialize(int _numRowMat, int _numColumnMat, long long _capacity,
        long _blockSize, int _numActiveMatPerRow,
        int _numActiveMatPerColumn, int _muxSenseAmp, bool _internalSenseAmp, int _muxOutputLev1, 
        int _muxOutputLev2, int _numRowSubarray, int _numColumnSubarray,
        int _numActiveSubarrayPerRow, int _numActiveSubarrayPerColumn,
        BufferDesignTarget _areaOptimizationLevel, CAMType _camType,
        SearchFunction _searchFunction, std::shared_ptr<EvaCamConfig> _config,
        const Wire &_localWire, const Wire &_globalWire,
        const CAM_Opt &_CAM_opt) {
    localWire = _localWire;
    globalWire = _globalWire;
    config = _config;

    if (initialized) {
        /* Reset the class for re-initialization */
        initialized = false;
        invalid = false;
    }

    if (!_internalSenseAmp) {
        invalid = true;
        initialized = true;
        config->logger.Verbose()
            << "[BankWithoutHtree] CAM routing does not support bank-level external sensing.";
        return;
    }

    numRowMat = _numRowMat;
    numColumnMat = _numColumnMat;
    capacity = _capacity;
    blockSize = _blockSize;
    internalSenseAmp = _internalSenseAmp;
    areaOptimizationLevel = _areaOptimizationLevel;
    camType = _camType;
    searchFunction = _searchFunction;
    CAM_opt = _CAM_opt;

    /* Calculate the physical signals that are required in routing */
    numAddressBit = (int)(log2((double)capacity / blockSize) + 0.1);
    /* use double during the calculation to avoid overflow */

    if (_numActiveMatPerRow > numColumnMat) {
        config->logger.Log() << "[Bank] Warning: The number of active subarray per row is larger than the number of subarray per row!";
        config->logger.Log() << _numActiveMatPerRow << " > " << numColumnMat;
        numActiveMatPerRow = numColumnMat;
    } else {
        numActiveMatPerRow = _numActiveMatPerRow;
    }
    if (_numActiveMatPerColumn > numRowMat) {
        config->logger.Log() << "[Bank] Warning: The number of active subarray per column is larger than the number of subarray per column!";
        config->logger.Log() << _numActiveMatPerColumn << " > " << numRowMat;
        numActiveMatPerColumn = numRowMat;
    } else {
        numActiveMatPerColumn = _numActiveMatPerColumn;
    }
    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;

    numRowSubarray = _numRowSubarray;
    numColumnSubarray = _numColumnSubarray;
    if (_numActiveSubarrayPerRow > numColumnSubarray) {
        config->logger.Log() << "[Bank] Warning: The number of active subarray per row is larger than the number of subarray per row!";
        config->logger.Log() << _numActiveSubarrayPerRow << " > " << numColumnSubarray;
        numActiveSubarrayPerRow = numColumnSubarray;
    } else {
        numActiveSubarrayPerRow = _numActiveSubarrayPerRow;
    }
    if (_numActiveSubarrayPerColumn > numRowSubarray) {
        config->logger.Log() << "[Bank] Warning: The number of active subarray per column is larger than the number of subarray per column!";
        config->logger.Log() << _numActiveSubarrayPerColumn << " > " << numRowSubarray;
        numActiveSubarrayPerColumn = numRowSubarray;
    } else {
        numActiveSubarrayPerColumn = _numActiveSubarrayPerColumn;
    }

    /* The number of address bits that are used to power gate inactive mats */
    int numAddressForGating = (int)(log2(numRowMat * numColumnMat / numActiveMatPerColumn / numActiveMatPerRow)+0.1);
    numAddressBitRouteToMat = numAddressBit - numAddressForGating;	/* Only use the effective address bits in the following calculation */
    const int activeMats = numActiveMatPerColumn * numActiveMatPerRow;
    if (numAddressBitRouteToMat <= 0 || activeMats <= 0 || blockSize % activeMats != 0) {
        invalid = true;
        initialized = true;
        config->logger.Verbose()
            << "[BankWithoutHtree] Invalid address/data partition across active mats.";
        return;
    }
    numDataBitRouteToMat = blockSize / activeMats;

    mat = std::make_unique<Mat>();

    mat->Initialize(numRowSubarray, numColumnSubarray, numAddressBitRouteToMat, numDataBitRouteToMat,
            false, numActiveSubarrayPerRow, numActiveSubarrayPerColumn,
            muxSenseAmp, internalSenseAmp, muxOutputLev1, muxOutputLev2, areaOptimizationLevel, 
            camType, searchFunction, config, localWire, CAM_opt);
    /* Check if mat is under a legal configuration */
    if (mat->invalid) {
        invalid = true;
        initialized = true;
        return;
    }

    /* Reset the mux values for correct printing */
    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;

    initialized = true;
    CalculateArea();
    if (!invalid) {
        CalculateRC();
        CalculateLatencyAndPower();
    }
}

void BankWithoutHtree::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[BankWithoutHtree]");
    } else if (invalid) {
        height = width = area = 1e41;
    } else {
        height = mat->height * numRowMat;
        width = mat->width * numColumnMat;

        int numWireSharingWidth;
        double effectivePitch;
        if (globalWire.wireRepeaterType == repeated_none) {
            numWireSharingWidth = 1;
            effectivePitch = 0;		/* assume that the wire is built on another metal layer, there does not cause silicon area */
            //effectivePitch = globalWire.wirePitch;
        } else {
            numWireSharingWidth = (int)floor(globalWire.repeaterSpacing / globalWire.repeaterHeight);
            effectivePitch = globalWire.repeatedWirePitch;
        }

        const int routedBits = numAddressBitRouteToMat + numDataBitRouteToMat;
        width += ceil((double)numRowMat * numColumnMat * routedBits
                / numWireSharingWidth) * effectivePitch;

        /* Determine if the aspect ratio meets the constraint */
        if (height / width > CONSTRAINT_ASPECT_RATIO_BANK
                || width / height > CONSTRAINT_ASPECT_RATIO_BANK) {
            invalid = true;
            height = width = area = 1e41;
            return;
        }

        area = height * width;
    }

}

void BankWithoutHtree::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[BankWithoutHtree]");
    } else if (!invalid) {
        mat->CalculateRC();
    }
}

void BankWithoutHtree::CalculateLatencyAndPower() {
    if (!initialized) {
        ThrowInitializationError("[BankWithoutHtree]");
    } else if (invalid) {
        readLatency = writeLatency = searchLatency = kInvalidResult;
        resetLatency = setLatency = kInvalidResult;
        readDynamicEnergy = writeDynamicEnergy = searchDynamicEnergy = kInvalidResult;
        resetDynamicEnergy = setDynamicEnergy = kInvalidResult;
        leakage = kInvalidResult;
        return;
    }

    mat->CalculateLatency(kInvalidResult);
    mat->CalculatePower();

    const int activeMats = ActiveMatCount(*this);
    const int routedBits = numAddressBitRouteToMat + numDataBitRouteToMat;

    readLatency = mat->readLatency;
    writeLatency = mat->writeLatency;
    resetLatency = mat->resetLatency;
    setLatency = mat->setLatency;
    readDynamicEnergy = mat->readDynamicEnergy * activeMats;
    writeDynamicEnergy = mat->writeDynamicEnergy * activeMats;
    resetDynamicEnergy = mat->resetDynamicEnergy * activeMats;
    setDynamicEnergy = mat->setDynamicEnergy * activeMats;
    leakage = mat->leakage * numRowMat * numColumnMat;
    cellReadEnergy = mat->cellReadEnergy * activeMats;
    cellSetEnergy = mat->cellSetEnergy * activeMats;
    cellResetEnergy = mat->cellResetEnergy * activeMats;

    if (config->peripherals.noPrechargeInc) {
        searchLatency = mat->subarray->matchlineDelay
            + mat->subarray->ColMux[mat->subarray->indexMatchline]->readLatency
            + mat->subarray->senseAmpLatency + mat->subarray->outputAcc->readLatency;
    } else {
        searchLatency = mat->subarray->searchLatency * mat->muxSenseAmp
            - mat->subarray->inputBuf->readLatency * (mat->muxSenseAmp - 1);
        if (config->peripherals.withOutputAcc) {
            searchLatency *= config->input.wordWidth / CAM_opt.BitSerialWidth;
        }
    }

    double localSearchEnergy = mat->subarray->searchDynamicEnergy * mat->muxSenseAmp
        - (mat->subarray->inputBuf->readDynamicEnergy
                + mat->subarray->inputEnc->readDynamicEnergy)
        * (mat->muxSenseAmp - 1);
    if (config->peripherals.withOutputAcc) {
        localSearchEnergy *= config->input.wordWidth / CAM_opt.BitSerialWidth;
    }
    searchDynamicEnergy = localSearchEnergy
        * numRowMat * numColumnMat * numRowSubarray * numColumnSubarray;
    numBitSerial = CAM_opt.BitSerialWidth;

    double lengthWire = mat->height * (numRowMat + 1);
    for (int i = 0; i < numRowMat; i++) {
        lengthWire -= mat->height;
        double latency = 0;
        double energy = 0;
        double leakageWire = 0;
        globalWire.CalculateLatencyAndPower(lengthWire, &latency, &energy, &leakageWire);

        if (i == 0) {
            readLatency += latency * 2;
            writeLatency += latency;
            resetLatency += latency;
            setLatency += latency;
            if (!config->peripherals.noPrechargeInc) {
                searchLatency += latency * 2;
            }
        }
        if (i < numActiveMatPerColumn) {
            const double activeRouteEnergy = energy * routedBits * numActiveMatPerRow;
            readDynamicEnergy += activeRouteEnergy;
            writeDynamicEnergy += activeRouteEnergy;
            resetDynamicEnergy += activeRouteEnergy;
            setDynamicEnergy += activeRouteEnergy;
        }
        searchDynamicEnergy += energy * routedBits * numColumnMat;
        leakage += leakageWire * routedBits * numColumnMat;
    }
}
