#include "../include/Mat.h"
#include "../include/formula.h"
#include "../include/macros.h"
void Mat::Initialize(int _numRowSubarray, int _numColumnSubarray, int _numAddressBit, long _numDataBit,
        int _numWay, int _numRowPerSet, bool _split, int _numActiveSubarrayPerRow, 
        int _numActiveSubarrayPerColumn, int _muxSenseAmp, bool _internalSenseAmp, 
        int _muxOutputLev1, int _muxOutputLev2, BufferDesignTarget _areaOptimizationLevel, 
        MemoryType _memoryType, CAMType _camType, SearchFunction _searchFunction, 
        std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire, 
        std::shared_ptr<CAM_Opt> _CAM_opt) {
    if (initialized)
        config->logger.Verbose() << "[Mat] Warning: Already initialized!";

    numRowSubarray = _numRowSubarray;
    numColumnSubarray = _numColumnSubarray;
    numAddressBit = _numAddressBit;
    numDataBit = _numDataBit;
    numWay = _numWay;
    numRowPerSet = _numRowPerSet;
    split = _split;
    internalSenseAmp = _internalSenseAmp;
    areaOptimizationLevel = _areaOptimizationLevel;
    memoryType =_memoryType;
    camType = _camType;
    searchFunction = _searchFunction;
    config = _config;
    localWire = _localWire;
    CAM_opt = _CAM_opt;

    if (_numActiveSubarrayPerRow > numColumnSubarray) {
        config->logger.Verbose() << "[Mat] Warning: The number of active subarray per row is larger than the number of subarray per row!";
        std::cout << _numActiveSubarrayPerRow << " > " << numColumnSubarray << std::endl;
        numActiveSubarrayPerRow = numColumnSubarray;
    } else {
        numActiveSubarrayPerRow = _numActiveSubarrayPerRow;
    }
    if (_numActiveSubarrayPerColumn > numRowSubarray) {
        config->logger.Verbose() << "[Mat] Warning: The number of active subarray per column is larger than the number of subarray per column!";
        std::cout << _numActiveSubarrayPerColumn << " > " << numRowSubarray << std::endl;
        numActiveSubarrayPerColumn = numRowSubarray;
    } else {
        numActiveSubarrayPerColumn = _numActiveSubarrayPerColumn;
    }
    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;

    // modified for Eva-CAM
    long long numRow = 0;		/* Number of rows in a subarray */
    long long numColumn = 0;	/* Number of columns in a subarray */

    /* The number of address bits that are used to power gate inactive subarrays */
    int numAddressForGating = (int)(log2(numRowSubarray * numColumnSubarray / numActiveSubarrayPerColumn / numActiveSubarrayPerRow)+0.1);
    _numAddressBit -= numAddressForGating;	/* Only use the effective address bits in the following calculation */
    if (_numAddressBit <= 0) {
        /* too aggressive partitioning */
        invalid = true;
        config->logger.Verbose() << "[Mat]: number of address bit error.";
        initialized = true;
        return;
    }

    /* Determine the number of rows in a subarray */
    // modified for Eva-CAM
    numRow = 1 << _numAddressBit;
    if (numRow < 16) {
        invalid = true;
        config->logger.Verbose() << "[Mat]: Word width is impractically small.";
        initialized = true;
        return;
    } else if (numRow > 512) {
        invalid = true;
        config->logger.Verbose() << "[Mat]: Word width is impractically large.";
        initialized = true;
        return;
    }
    // TODO
    //numRow /= (muxSenseAmp);	/* Distribute to column decoding */

    // modified for Eva-CAM
    numColumn = (long long)numDataBit / (numActiveSubarrayPerRow * numActiveSubarrayPerColumn);	/* Adjust the number of columns depending on the access types */



    // if (config->designTarget == CAM_chip) {
    // 	numColumn *= muxSenseAmp;
    // } else {
    // 	numColumn *= muxSenseAmp * muxOutputLev1 * muxOutputLev2;
    // }

    if (numColumn < 8) {
        invalid = true;
        config->logger.Verbose() << "[Mat]: Column width is too small.";
        initialized = true;
        return;
    } else if (numColumn > 512) {
        invalid = true;
        config->logger.Verbose() << "[Mat]: Column width is too big.";
        initialized = true;
        return;
    }

    subarray = std::make_shared<CAM_SubArray>();
    subarray->Initialize(
            numRow, 
            numColumn, 
            numRowPerSet > 1, 
            true /* TODO: need to correct */,		
            muxSenseAmp, 
            internalSenseAmp, 
            muxOutputLev1, 
            muxOutputLev2,			
            areaOptimizationLevel, 
            (BufferDesignTarget)CAM_opt->RowDriver,		
            config->withInputEnc, 
            config->typeInputEnc,  
            config->customInputEnc,
            config->typeSenseAmp,  
            config->customSenseAmp, 
            config->withWriteDriver,
            config->withOutputAcc, 
            config->withPriorityEnc,  
            (BufferDesignTarget)CAM_opt->Proirity,
            config->withInputBuffer, 
            config->withOutputBuffer, 
            config->cell->camType, 
            config->searchFunction, 
            config->cell->withVariation, 
            config,
            localWire,
            CAM_opt);

    if (subarray->invalid) {
        invalid = true;
        config->logger.Verbose() << "[Mat]: Subarray is invalid.";
        initialized = true;
        return;
    }
    //subarray->CalculateArea();	/* the area needs to be calculated during the initialization because the size dimension needs to be called by others */

    int numAddressRowPredecoderBlock1 = _numAddressBit - (int)(log2(muxSenseAmp * muxOutputLev1 * muxOutputLev2)+0.1);	/* The address bit on row decodeing */

    if( config->designTarget == CAM_chip) {
        numAddressRowPredecoderBlock1 = _numAddressBit - (int)(log2(muxSenseAmp)+0.1);	/* The address bit on row decodeing */
    }


    if (numAddressRowPredecoderBlock1 < 0) {
        invalid = true;
        config->logger.Verbose() << "numAddressRowPrecoderBlock1 < 0";
        initialized = true;
        return;
    }
    int numAddressRowPredecoderBlock2 = 0;
    if (numAddressRowPredecoderBlock1 > 3) {	/* Block 2 is needed */
        numAddressRowPredecoderBlock2 = numAddressRowPredecoderBlock1 / 2;
        numAddressRowPredecoderBlock1 = numAddressRowPredecoderBlock1 - numAddressRowPredecoderBlock2;
    }
    double capLoadRowPredecoder = subarray->height * localWire->capWirePerUnit * numRowSubarray / 2
        + subarray->width * localWire->capWirePerUnit * numColumnSubarray / 2;	/* Assume the predecoder is at the center */
    rowPredecoderBlock1 = std::make_shared<PredecodeBlock>();
    rowPredecoderBlock2 = std::make_shared<PredecodeBlock>();
    rowPredecoderBlock1->Initialize(numAddressRowPredecoderBlock1, capLoadRowPredecoder, 0 /* TODO */, config);
    rowPredecoderBlock2->Initialize(numAddressRowPredecoderBlock2, capLoadRowPredecoder, 0 /* TODO */, config);

    double capLoadMuxPredecoder = MAX(0, subarray->height * localWire->capWirePerUnit * (numRowSubarray - 2) / 2)
        + MAX(0, subarray->width * localWire->capWirePerUnit * (numColumnSubarray - 2) / 2);
    int numAddressBitlineMuxPredecoderBlock1 = (int)(log2(muxSenseAmp) + 0.1);
    int numAddressBitlineMuxPredecoderBlock2 = 0;
    if (numAddressBitlineMuxPredecoderBlock1 > 3) {		/* Block 2 is needed */
        numAddressBitlineMuxPredecoderBlock2 = numAddressBitlineMuxPredecoderBlock1 / 2;
        numAddressBitlineMuxPredecoderBlock1 = numAddressBitlineMuxPredecoderBlock1 - numAddressBitlineMuxPredecoderBlock2;
    }
    bitlineMuxPredecoderBlock1 = std::make_shared<PredecodeBlock>();
    bitlineMuxPredecoderBlock2 = std::make_shared<PredecodeBlock>();
    bitlineMuxPredecoderBlock1->Initialize(numAddressBitlineMuxPredecoderBlock1, capLoadMuxPredecoder, 0 /* TODO */, config);
    bitlineMuxPredecoderBlock2->Initialize(numAddressBitlineMuxPredecoderBlock2, capLoadMuxPredecoder, 0 /* TODO */, config);

    int numAddressSenseAmpMuxLev1PredecoderBlock1 = (int)(log2(muxOutputLev1) + 0.1);
    int numAddressSenseAmpMuxLev1PredecoderBlock2 = 0;
    if (numAddressSenseAmpMuxLev1PredecoderBlock1 > 3) { /* Block 2 is needed */
        numAddressSenseAmpMuxLev1PredecoderBlock2 = numAddressSenseAmpMuxLev1PredecoderBlock1 / 2;
        numAddressSenseAmpMuxLev1PredecoderBlock1 = numAddressSenseAmpMuxLev1PredecoderBlock1 - numAddressSenseAmpMuxLev1PredecoderBlock2;
    }
    senseAmpMuxLev1PredecoderBlock1 = std::make_shared<PredecodeBlock>();
    senseAmpMuxLev1PredecoderBlock2 = std::make_shared<PredecodeBlock>();
    senseAmpMuxLev1PredecoderBlock1->Initialize(numAddressSenseAmpMuxLev1PredecoderBlock1, capLoadMuxPredecoder, 0 /* TODO */, config);
    senseAmpMuxLev1PredecoderBlock2->Initialize(numAddressSenseAmpMuxLev1PredecoderBlock2, capLoadMuxPredecoder, 0 /* TODO */, config);

    int numAddressSenseAmpMuxLev2PredecoderBlock1 = (int)(log2(muxOutputLev2) + 0.1);
    int numAddressSenseAmpMuxLev2PredecoderBlock2 = 0;
    if (numAddressSenseAmpMuxLev2PredecoderBlock1 > 3) { /* Block 2 is needed */
        numAddressSenseAmpMuxLev2PredecoderBlock2 = numAddressSenseAmpMuxLev2PredecoderBlock1 / 2;
        numAddressSenseAmpMuxLev2PredecoderBlock1 = numAddressSenseAmpMuxLev2PredecoderBlock1 - numAddressSenseAmpMuxLev2PredecoderBlock2;
    }
    senseAmpMuxLev2PredecoderBlock1 = std::make_shared<PredecodeBlock>();
    senseAmpMuxLev2PredecoderBlock2 = std::make_shared<PredecodeBlock>();
    senseAmpMuxLev2PredecoderBlock1->Initialize(numAddressSenseAmpMuxLev2PredecoderBlock1, capLoadMuxPredecoder, 0 /* TODO */, config);
    senseAmpMuxLev2PredecoderBlock2->Initialize(numAddressSenseAmpMuxLev2PredecoderBlock2, capLoadMuxPredecoder, 0 /* TODO */, config);

    initialized = true;
    CalculateRC();
    CalculateArea();
    CalculatePower();
}

void Mat::CalculateArea() {
    if (!initialized) {
        throw std::runtime_error("[Mat] Error: Require initialization first!");
    } else if (invalid) {
        height = width = area = 1e41;
    } else {
        /* subarray CalculateArea() is already called during the initialization */
        rowPredecoderBlock1->CalculateArea();
        rowPredecoderBlock2->CalculateArea();
        bitlineMuxPredecoderBlock1->CalculateArea();
        bitlineMuxPredecoderBlock2->CalculateArea();
        senseAmpMuxLev1PredecoderBlock1->CalculateArea();
        senseAmpMuxLev1PredecoderBlock2->CalculateArea();
        senseAmpMuxLev2PredecoderBlock1->CalculateArea();
        senseAmpMuxLev2PredecoderBlock2->CalculateArea();

        areaAllPredecoderBlocks = rowPredecoderBlock1->area + rowPredecoderBlock2->area
            + bitlineMuxPredecoderBlock1->area + bitlineMuxPredecoderBlock2->area
            + senseAmpMuxLev1PredecoderBlock1->area + senseAmpMuxLev1PredecoderBlock2->area
            + senseAmpMuxLev2PredecoderBlock1->area + senseAmpMuxLev2PredecoderBlock2->area;
        width = subarray->width * numColumnSubarray;
        height = subarray->height * numRowSubarray;

        area = height * width + areaAllPredecoderBlocks;
        /* Add the predecoders' area */
        if (width > height){
            width += sqrt(areaAllPredecoderBlocks); // we don't want to have too much white space here.
            height = area / width;

        } else {
            height += sqrt(areaAllPredecoderBlocks);
            width = area / height;
        }
        //TODO confusing expression
        // area = subarray->area + areaAllPredecoderBlocks;
        // height = subarray->area / width;
        // area = width * height + areaAllPredecoderBlocks;
        // area = height * width;
    }
}

void Mat::CalculateRC() {
    if (!initialized) {
        throw std::runtime_error("[Mat] Error: Require initialization first!");
    } else if (!invalid){
        /* subarray does not have CalculateRC() function, since it is integrated as a part of initialization */
        rowPredecoderBlock1->CalculateRC();
        rowPredecoderBlock2->CalculateRC();
        bitlineMuxPredecoderBlock1->CalculateRC();
        bitlineMuxPredecoderBlock2->CalculateRC();
        senseAmpMuxLev1PredecoderBlock1->CalculateRC();
        senseAmpMuxLev1PredecoderBlock2->CalculateRC();
        senseAmpMuxLev2PredecoderBlock1->CalculateRC();
        senseAmpMuxLev2PredecoderBlock2->CalculateRC();
    }
}

void Mat::CalculateLatency(double _rampInput) {
    if (!initialized) {
        throw std::runtime_error("[Mat] Error: Require initialization first!");
    } else if (invalid) {
        readLatency = writeLatency = 1e41;
    } else {
        /* Calculate the predecoder blocks latency */
        rowPredecoderBlock1->CalculateLatency(_rampInput);
        rowPredecoderBlock2->CalculateLatency(_rampInput);
        bitlineMuxPredecoderBlock1->CalculateLatency(_rampInput);
        bitlineMuxPredecoderBlock2->CalculateLatency(_rampInput);
        senseAmpMuxLev1PredecoderBlock1->CalculateLatency(_rampInput);
        senseAmpMuxLev1PredecoderBlock2->CalculateLatency(_rampInput);
        senseAmpMuxLev2PredecoderBlock1->CalculateLatency(_rampInput);
        senseAmpMuxLev2PredecoderBlock2->CalculateLatency(_rampInput);

        double rowPredecoderLatency = MAX(rowPredecoderBlock1->readLatency, rowPredecoderBlock2->readLatency);
        double bitlineMuxPredecoderLatency = MAX(bitlineMuxPredecoderBlock1->readLatency,
                bitlineMuxPredecoderBlock2->readLatency);
        double senseAmpMuxLev1PredecoderLatency = MAX(senseAmpMuxLev1PredecoderBlock1->readLatency,
                senseAmpMuxLev1PredecoderBlock2->readLatency);
        double senseAmpMuxLev2PredecoderLatency = MAX(senseAmpMuxLev2PredecoderBlock1->readLatency,
                senseAmpMuxLev2PredecoderBlock2->readLatency);
        predecoderLatency = MAX(MAX(rowPredecoderLatency, bitlineMuxPredecoderLatency),
                MAX(senseAmpMuxLev1PredecoderLatency, senseAmpMuxLev2PredecoderLatency));

        /* Caluclate subarray latency */
        subarray->CalculateLatency(MIN(rowPredecoderBlock1->rampOutput, rowPredecoderBlock2->rampOutput));
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
        readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
    } else {
        rowPredecoderBlock1->CalculatePower();
        rowPredecoderBlock2->CalculatePower();
        bitlineMuxPredecoderBlock1->CalculatePower();
        bitlineMuxPredecoderBlock2->CalculatePower();
        senseAmpMuxLev1PredecoderBlock1->CalculatePower();
        senseAmpMuxLev1PredecoderBlock2->CalculatePower();
        senseAmpMuxLev2PredecoderBlock1->CalculatePower();
        senseAmpMuxLev2PredecoderBlock2->CalculatePower();
        subarray->CalculatePower();

        readDynamicEnergy = rowPredecoderBlock1->readDynamicEnergy 
            + rowPredecoderBlock2->readDynamicEnergy
            + bitlineMuxPredecoderBlock1->readDynamicEnergy 
            + bitlineMuxPredecoderBlock2->readDynamicEnergy
            + senseAmpMuxLev1PredecoderBlock1->readDynamicEnergy 
            + senseAmpMuxLev1PredecoderBlock2->readDynamicEnergy
            + senseAmpMuxLev2PredecoderBlock1->readDynamicEnergy 
            + senseAmpMuxLev2PredecoderBlock2->readDynamicEnergy;

        writeDynamicEnergy = rowPredecoderBlock1->writeDynamicEnergy 
            + rowPredecoderBlock2->writeDynamicEnergy
            + bitlineMuxPredecoderBlock1->writeDynamicEnergy 
            + bitlineMuxPredecoderBlock2->writeDynamicEnergy
            + senseAmpMuxLev1PredecoderBlock1->writeDynamicEnergy 
            + senseAmpMuxLev1PredecoderBlock2->writeDynamicEnergy
            + senseAmpMuxLev2PredecoderBlock1->writeDynamicEnergy 
            + senseAmpMuxLev2PredecoderBlock2->writeDynamicEnergy;

        leakage = rowPredecoderBlock1->leakage 
            + rowPredecoderBlock2->leakage
            + bitlineMuxPredecoderBlock1->leakage 
            + bitlineMuxPredecoderBlock2->leakage
            + senseAmpMuxLev1PredecoderBlock1->leakage 
            + senseAmpMuxLev1PredecoderBlock2->leakage
            + senseAmpMuxLev2PredecoderBlock1->leakage 
            + senseAmpMuxLev2PredecoderBlock2->leakage;

        //std::cout << "[MAT] subarray->readyDyanmicEnergy: " << subarray->readDynamicEnergy << std::endl;

        readDynamicEnergy += subarray->readDynamicEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        //std::cout << "[Mat] readDynamicEnergy " << readDynamicEnergy << std::endl;

        /* energy consumption on cells */
        cellReadEnergy = subarray->cellReadEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        cellSetEnergy = subarray->cellSetEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        cellResetEnergy = subarray->cellResetEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        /* for RESET and SET only */
        resetDynamicEnergy = writeDynamicEnergy 
            + subarray->resetDynamicEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;

        setDynamicEnergy = writeDynamicEnergy 
            + subarray->setDynamicEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;

        /* total write energy */
        writeDynamicEnergy += subarray->writeDynamicEnergy * numActiveSubarrayPerRow * numActiveSubarrayPerColumn;
        leakage += subarray->leakage * numRowSubarray * numColumnSubarray;
    }
}

void Mat::PrintProperty() {
    std::cout << "Mat Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}


Mat & Mat::operator=(const Mat &rhs) {
    height = rhs.height;
    width = rhs.width;
    area = rhs.area;
    readLatency = rhs.readLatency;
    writeLatency = rhs.writeLatency;
    readDynamicEnergy = rhs.readDynamicEnergy;
    writeDynamicEnergy = rhs.writeDynamicEnergy;
    resetLatency = rhs.resetLatency;
    setLatency = rhs.setLatency;
    resetDynamicEnergy = rhs.resetDynamicEnergy;
    setDynamicEnergy = rhs.setDynamicEnergy;
    cellReadEnergy = rhs.cellReadEnergy;
    cellSetEnergy = rhs.cellSetEnergy;
    cellResetEnergy = rhs.cellResetEnergy;
    leakage = rhs.leakage;
    initialized = rhs.initialized;
    invalid = rhs.invalid;
    numRowSubarray = rhs.numRowSubarray;
    numColumnSubarray = rhs.numColumnSubarray;
    numAddressBit = rhs.numAddressBit;
    numDataBit = rhs.numDataBit;
    numWay = rhs.numWay;
    numRowPerSet = rhs.numRowPerSet;
    split = rhs.split;
    internalSenseAmp = rhs.internalSenseAmp;
    numActiveSubarrayPerRow = rhs.numActiveSubarrayPerRow;
    numActiveSubarrayPerColumn = rhs.numActiveSubarrayPerColumn;
    muxSenseAmp = rhs.muxSenseAmp;
    muxOutputLev1 = rhs.muxOutputLev1;
    muxOutputLev2 = rhs.muxOutputLev2;
    areaOptimizationLevel = rhs.areaOptimizationLevel;
    memoryType = rhs.memoryType;
    predecoderLatency = rhs.predecoderLatency;

    subarray = rhs.subarray;
    rowPredecoderBlock1 = rhs.rowPredecoderBlock1;
    rowPredecoderBlock2 = rhs.rowPredecoderBlock2;
    bitlineMuxPredecoderBlock1 = rhs.bitlineMuxPredecoderBlock1;
    bitlineMuxPredecoderBlock2 = rhs.bitlineMuxPredecoderBlock2;
    senseAmpMuxLev1PredecoderBlock1 = rhs.senseAmpMuxLev1PredecoderBlock1;
    senseAmpMuxLev1PredecoderBlock2 = rhs.senseAmpMuxLev1PredecoderBlock2;
    senseAmpMuxLev2PredecoderBlock1 = rhs.senseAmpMuxLev2PredecoderBlock1;
    senseAmpMuxLev2PredecoderBlock2 = rhs.senseAmpMuxLev2PredecoderBlock2;

    return *this;
}
