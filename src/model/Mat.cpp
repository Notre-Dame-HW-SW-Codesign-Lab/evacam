#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "Mat.h"
#include "formula.h"

namespace {
constexpr double kInvalidResult = 1e41;
constexpr int kMaxAddressBitsPerPredecoderBlock = 3;

int Log2Rounded(int value) {
    return static_cast<int>(std::log2(value) + 0.1);
}

void MarkInvalid(Mat &mat, const char *message) {
    mat.invalid = true;
    mat.config->logger.Verbose() << message;
    mat.initialized = true;
}

void SplitPredecoderAddressBits(int &block1AddressBits, int &block2AddressBits) {
    block2AddressBits = 0;
    if (block1AddressBits > kMaxAddressBitsPerPredecoderBlock) {
        block2AddressBits = block1AddressBits / 2;
        block1AddressBits -= block2AddressBits;
    }
}

void InitializePredecoderPair(std::unique_ptr<PredecodeBlock> &block1,
        std::unique_ptr<PredecodeBlock> &block2, int addressBits, double capLoad,
        const std::shared_ptr<EvaCamConfig> &config) {
    int block1AddressBits = addressBits;
    int block2AddressBits = 0;
    SplitPredecoderAddressBits(block1AddressBits, block2AddressBits);

    block1 = std::make_unique<PredecodeBlock>();
    block2 = std::make_unique<PredecodeBlock>();
    block1->Initialize(block1AddressBits, capLoad, 0 /* TODO */, config);
    block2->Initialize(block2AddressBits, capLoad, 0 /* TODO */, config);
}

template <typename Operation>
void ForEachPredecoderBlock(Mat &mat, Operation operation) {
    operation(*mat.rowPredecoderBlock1);
    operation(*mat.rowPredecoderBlock2);
    operation(*mat.bitlineMuxPredecoderBlock1);
    operation(*mat.bitlineMuxPredecoderBlock2);
    operation(*mat.senseAmpMuxLev1PredecoderBlock1);
    operation(*mat.senseAmpMuxLev1PredecoderBlock2);
    operation(*mat.senseAmpMuxLev2PredecoderBlock1);
    operation(*mat.senseAmpMuxLev2PredecoderBlock2);
}

double SumPredecoderMetric(const Mat &mat, double FunctionUnit::*metric) {
    return mat.rowPredecoderBlock1.get()->*metric
        + mat.rowPredecoderBlock2.get()->*metric
        + mat.bitlineMuxPredecoderBlock1.get()->*metric
        + mat.bitlineMuxPredecoderBlock2.get()->*metric
        + mat.senseAmpMuxLev1PredecoderBlock1.get()->*metric
        + mat.senseAmpMuxLev1PredecoderBlock2.get()->*metric
        + mat.senseAmpMuxLev2PredecoderBlock1.get()->*metric
        + mat.senseAmpMuxLev2PredecoderBlock2.get()->*metric;
}

double MaxPairReadLatency(const std::unique_ptr<PredecodeBlock> &block1,
        const std::unique_ptr<PredecodeBlock> &block2) {
    return std::max(block1->readLatency, block2->readLatency);
}
} // namespace

void Mat::Initialize(int _numRowSubarray, int _numColumnSubarray, int _numAddressBit,
        long _numDataBit, bool _split, int _numActiveSubarrayPerRow,
        int _numActiveSubarrayPerColumn, int _muxSenseAmp, bool _internalSenseAmp, 
        int _muxOutputLev1, int _muxOutputLev2, BufferDesignTarget _areaOptimizationLevel, 
        CAMType _camType, SearchFunction _searchFunction,
        std::shared_ptr<EvaCamConfig> _config, const Wire &_localWire,
        const CAM_Opt &_CAM_opt) {
    config = _config;

    if (initialized)
        config->logger.Verbose() << "[Mat] Warning: Already initialized!";

    numRowSubarray = _numRowSubarray;
    numColumnSubarray = _numColumnSubarray;
    numAddressBit = _numAddressBit;
    numDataBit = _numDataBit;
    split = _split;
    internalSenseAmp = _internalSenseAmp;
    areaOptimizationLevel = _areaOptimizationLevel;
    camType = _camType;
    searchFunction = _searchFunction;
    localWire = _localWire;
    CAM_opt = _CAM_opt;

    if (_numActiveSubarrayPerRow > numColumnSubarray) {
        config->logger.Log()
            << "[Mat] Warning: Active subarrays per row exceeds subarrays per row!";
        config->logger.Log() << _numActiveSubarrayPerRow << " > " << numColumnSubarray;
        numActiveSubarrayPerRow = numColumnSubarray;
    } else {
        numActiveSubarrayPerRow = _numActiveSubarrayPerRow;
    }
    if (_numActiveSubarrayPerColumn > numRowSubarray) {
        config->logger.Log()
            << "[Mat] Warning: Active subarrays per column exceeds subarrays per column!";
        config->logger.Log() << _numActiveSubarrayPerColumn << " > " << numRowSubarray;
        numActiveSubarrayPerColumn = numRowSubarray;
    } else {
        numActiveSubarrayPerColumn = _numActiveSubarrayPerColumn;
    }
    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;

    // modified for EvaCAM
    long long numRow = 0;		/* Number of rows in a subarray */
    long long numColumn = 0;	/* Number of columns in a subarray */

    /* The number of address bits that are used to power gate inactive subarrays */
    int numAddressForGating = Log2Rounded(numRowSubarray * numColumnSubarray
            / numActiveSubarrayPerColumn / numActiveSubarrayPerRow);
    /* Only use the effective address bits in the following calculation */
    _numAddressBit -= numAddressForGating;
    if (_numAddressBit <= 0) {
        /* too aggressive partitioning */
        MarkInvalid(*this, "[Mat]: number of address bit error.");
        return;
    }

    if (config->runtimeSizing.hasFixedSubarrayDimensions) {
        numRow = config->runtimeSizing.fixedSubarrayRows;
        numColumn = config->runtimeSizing.fixedSubarrayColumns;
    } else {
        /* Determine the number of rows in a subarray */
        // modified for EvaCAM
        numRow = 1 << _numAddressBit;
    }
    if (numRow < 16) {
        MarkInvalid(*this, "[Mat]: Word width is impractically small.");
        return;
    } else if (numRow > 512) {
        MarkInvalid(*this, "[Mat]: Word width is impractically large.");
        return;
    }
    // TODO
    //numRow /= (muxSenseAmp);	/* Distribute to column decoding */

    if (!config->runtimeSizing.hasFixedSubarrayDimensions) {
        // modified for EvaCAM
        numColumn = (long long)numDataBit
            / (numActiveSubarrayPerRow * numActiveSubarrayPerColumn);
    }
    if (numColumn < 8) {
        MarkInvalid(*this, "[Mat]: Column width is too small.");
        return;
    } else if (numColumn > 512) {
        MarkInvalid(*this, "[Mat]: Column width is too big.");
        return;
    }

    subarray = std::make_unique<CAM_SubArray>();
    subarray->Initialize(
            numRow, 
            numColumn, 
            split,
            muxSenseAmp, 
            internalSenseAmp, 
            muxOutputLev1, 
            muxOutputLev2,
            areaOptimizationLevel, 
            (BufferDesignTarget)CAM_opt.RowDriver,
            config->peripherals.withInputEnc, 
            config->peripherals.typeInputEnc,  
            config->peripherals.customInputEnc,
            config->peripherals.typeSenseAmp,  
            config->peripherals.customSenseAmp, 
            config->peripherals.withWriteDriver,
            config->peripherals.withOutputAcc, 
            config->peripherals.withPriorityEnc,  
            (BufferDesignTarget)CAM_opt.Proirity,
            config->peripherals.withInputBuffer, 
            config->peripherals.withOutputBuffer, 
            config->technology.cell->camType, 
            config->input.searchFunction, 
            config->technology.cell->withVariation, 
            config,
            localWire,
            CAM_opt);

    if (subarray->invalid) {
        MarkInvalid(*this, "[Mat]: Subarray is invalid.");
        return;
    }
    /* Subarray dimensions are used by the predecoder setup below. */

    int numAddressRowPredecoder = _numAddressBit - Log2Rounded(muxSenseAmp);


    if (numAddressRowPredecoder < 0) {
        MarkInvalid(*this, "numAddressRowPredecoder < 0");
        return;
    }
    double capLoadRowPredecoder = subarray->height * localWire.capWirePerUnit * numRowSubarray / 2
        + subarray->width * localWire.capWirePerUnit * numColumnSubarray / 2;
    InitializePredecoderPair(rowPredecoderBlock1, rowPredecoderBlock2,
            numAddressRowPredecoder, capLoadRowPredecoder, config);

    double capLoadMuxPredecoder = std::max(0.0,
            subarray->height * localWire.capWirePerUnit * (numRowSubarray - 2) / 2)
        + std::max(0.0, subarray->width * localWire.capWirePerUnit * (numColumnSubarray - 2) / 2);
    InitializePredecoderPair(bitlineMuxPredecoderBlock1, bitlineMuxPredecoderBlock2,
            Log2Rounded(muxSenseAmp), capLoadMuxPredecoder, config);
    InitializePredecoderPair(senseAmpMuxLev1PredecoderBlock1, senseAmpMuxLev1PredecoderBlock2,
            Log2Rounded(muxOutputLev1), capLoadMuxPredecoder, config);
    InitializePredecoderPair(senseAmpMuxLev2PredecoderBlock1, senseAmpMuxLev2PredecoderBlock2,
            Log2Rounded(muxOutputLev2), capLoadMuxPredecoder, config);

    initialized = true;
    CalculateArea();
}

void Mat::CalculateArea() {
    if (!initialized) {
        throw std::runtime_error("[Mat] Error: Require initialization first!");
    } else if (invalid) {
        height = width = area = kInvalidResult;
    } else {
        /* subarray CalculateArea() is already called during the initialization */
        ForEachPredecoderBlock(*this, [](PredecodeBlock &block) { block.CalculateArea(); });

        areaAllPredecoderBlocks = SumPredecoderMetric(*this, &FunctionUnit::area);
        width = subarray->width * numColumnSubarray;
        height = subarray->height * numRowSubarray;

        area = height * width + areaAllPredecoderBlocks;
        /* Add the predecoders' area */
        if (width > height){
            width += sqrt(areaAllPredecoderBlocks);
            height = area / width;

        } else {
            height += sqrt(areaAllPredecoderBlocks);
            width = area / height;
        }
    }
}

void Mat::CalculateRC() {
    if (!initialized) {
        throw std::runtime_error("[Mat] Error: Require initialization first!");
    } else if (!invalid) {
        /* Subarray RC is integrated into initialization. */
        ForEachPredecoderBlock(*this, [](PredecodeBlock &block) { block.CalculateRC(); });
    }
}

void Mat::CalculateLatency(double _rampInput) {
    if (!initialized) {
        throw std::runtime_error("[Mat] Error: Require initialization first!");
    } else if (invalid) {
        readLatency = writeLatency = kInvalidResult;
    } else {
        /* Calculate the predecoder blocks latency */
        ForEachPredecoderBlock(*this,
                [_rampInput](PredecodeBlock &block) { block.CalculateLatency(_rampInput); });

        double rowPredecoderLatency = MaxPairReadLatency(rowPredecoderBlock1, rowPredecoderBlock2);
        double bitlineMuxPredecoderLatency = MaxPairReadLatency(bitlineMuxPredecoderBlock1,
                bitlineMuxPredecoderBlock2);
        double senseAmpMuxLev1PredecoderLatency = MaxPairReadLatency(
                senseAmpMuxLev1PredecoderBlock1, senseAmpMuxLev1PredecoderBlock2);
        double senseAmpMuxLev2PredecoderLatency = MaxPairReadLatency(
                senseAmpMuxLev2PredecoderBlock1, senseAmpMuxLev2PredecoderBlock2);
        predecoderLatency = std::max(std::max(rowPredecoderLatency, bitlineMuxPredecoderLatency),
                std::max(senseAmpMuxLev1PredecoderLatency, senseAmpMuxLev2PredecoderLatency));

        /* Calculate subarray latency */
        subarray->CalculateLatency(std::min(rowPredecoderBlock1->rampOutput,
                rowPredecoderBlock2->rampOutput));
        /* Add them together */
        readLatency = predecoderLatency + subarray->readLatency;
        writeLatency = predecoderLatency + subarray->writeLatency;
        /* for RESET and SET only */
        resetLatency = predecoderLatency + subarray->resetLatency;
        setLatency = predecoderLatency + subarray->setLatency;
    }

}

void Mat::CalculatePower() {
    if (!initialized) {
        throw std::runtime_error("[Mat] Error: Require initialization first!");
    } else if (invalid) {
        readDynamicEnergy = writeDynamicEnergy = leakage = kInvalidResult;
    } else {
        ForEachPredecoderBlock(*this, [](PredecodeBlock &block) { block.CalculatePower(); });
        subarray->CalculatePower();

        readDynamicEnergy = SumPredecoderMetric(*this, &FunctionUnit::readDynamicEnergy);
        writeDynamicEnergy = SumPredecoderMetric(*this, &FunctionUnit::writeDynamicEnergy);
        leakage = SumPredecoderMetric(*this, &FunctionUnit::leakage);

        readDynamicEnergy += subarray->readDynamicEnergy
            * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        //std::cout << "[Mat] readDynamicEnergy " << readDynamicEnergy << std::endl;

        /* energy consumption on cells */
        cellReadEnergy = subarray->cellReadEnergy
            * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        cellSetEnergy = subarray->cellSetEnergy
            * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        cellResetEnergy = subarray->cellResetEnergy
            * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        /* for RESET and SET only */
        resetDynamicEnergy = writeDynamicEnergy 
            + subarray->resetDynamicEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;

        setDynamicEnergy = writeDynamicEnergy 
            + subarray->setDynamicEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;

        /* total write energy */
        writeDynamicEnergy += subarray->writeDynamicEnergy
            * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        leakage += subarray->leakage * numRowSubarray * numColumnSubarray;
    }
}

void Mat::PrintProperty() {
    std::cout << "Mat Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
