#include "PredecodeBlock.h"
#include "formula.h"
void PredecodeBlock::Initialize(int _numAddressBit, double _capLoad, double _resLoad,
        std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[Predecoder Block] Warning: Already initialized!";

    config = _config;
    numAddressBit =_numAddressBit;
    if (numAddressBit > 27 ) {
        throw std::runtime_error("[Predecoder Block] Error: invalid number of address bits.");
    } else if (numAddressBit == 0) {
        height = width = area = 0;
        readLatency = writeLatency = 0;
        readDynamicEnergy = writeDynamicEnergy = 0;
        leakage = 0;
        initialized = true;
    } else {
        capLoad = _capLoad;
        resLoad = _resLoad;

        numDecoder12 = numDecoder24 = numDecoder38 = 0;
        numOutputAddressBit = 1 << numAddressBit;
        if (numAddressBit == 1) {
            numDecoder12 = 1;
        } else {
            int numAddressMod3 = numAddressBit % 3;
            if (numAddressMod3 == 2) {
                numDecoder24 = 1;
            } else if (numAddressMod3 == 1) {
                numDecoder24 = 2;
            }
            numDecoder38 = (numAddressBit - 2 * numDecoder24) / 3;
        }
        int numBasicDecoder = numDecoder12 + numDecoder24 + numDecoder38;
        if (numBasicDecoder <= 1){
            rowDecoderStage1A = NULL;
            rowDecoderStage1B = NULL;
            rowDecoderStage1C = NULL;
            rowDecoderStage2 = NULL;
        } else if (numBasicDecoder <= 3) {
            numNandInputStage1A = numBasicDecoder;
            rowDecoderStage2 = NULL;
            rowDecoderStage1B = NULL;
            rowDecoderStage1C = NULL;
            rowDecoderStage1A = std::make_unique<RowDecoder>();
            rowDecoderStage1A->Initialize(numOutputAddressBit, capLoad, resLoad, numNandInputStage1A == 3, latency_first, 0, config);
            //rowDecoderStage1A->CalculateRC();
        } else {
            rowDecoderStage2 = std::make_unique<RowDecoder>();
            double capLoadStage1A, capLoadStage1B, capLoadStage1C;
            if (numBasicDecoder <= 6) {
                rowDecoderStage2->Initialize(numOutputAddressBit, capLoad, resLoad, false, latency_first, 0, config);
                //rowDecoderStage2->CalculateRC();
                numNandInputStage1B = numBasicDecoder / 2;
                numNandInputStage1A = numBasicDecoder - numNandInputStage1B;
                numAddressBitStage1A = numAddressBitStage1B = 1;
                int i = 3 * numNandInputStage1A - numDecoder24;
                numAddressBitStage1A <<= i;
                numAddressBitStage1B <<= 3 * numNandInputStage1B;
                capLoadStage1A = numAddressBitStage1B * rowDecoderStage2->capNandInput;
                capLoadStage1B = numAddressBitStage1A * rowDecoderStage2->capNandInput;
                rowDecoderStage1C = NULL;
                rowDecoderStage1A = std::make_unique<RowDecoder>();
                rowDecoderStage1A->Initialize(numAddressBitStage1A, capLoadStage1A, 0 /* TODO */, numNandInputStage1A == 3, latency_first, 0, config);
                //rowDecoderStage1A->CalculateRC();
                rowDecoderStage1B = std::make_unique<RowDecoder>();
                rowDecoderStage1B->Initialize(numAddressBitStage1B, capLoadStage1B, 0 /* TODO */, numNandInputStage1B == 3, latency_first, 0, config);
                //rowDecoderStage1B->CalculateRC();
            } else if (numBasicDecoder <= 9){
                rowDecoderStage2->Initialize(numOutputAddressBit, capLoad, resLoad, true, latency_first, 0, config);
                //rowDecoderStage2->CalculateRC();
                if (numBasicDecoder == 7) {
                    numNandInputStage1A = 3;
                    numNandInputStage1B = 2;
                    numNandInputStage1C = 2;
                    numAddressBitStage1B = 64;
                    numAddressBitStage1C = 64;
                } else if (numBasicDecoder == 8) {
                    numNandInputStage1A = 3;
                    numNandInputStage1B = 3;
                    numNandInputStage1C = 2;
                    numAddressBitStage1B = 512;
                    numAddressBitStage1C = 64;
                } else {
                    numNandInputStage1A = 3;
                    numNandInputStage1B = 3;
                    numNandInputStage1C = 3;
                    numAddressBitStage1B = 512;
                    numAddressBitStage1C = 512;
                }
                int i = 3 * numNandInputStage1A - numDecoder24;
                numAddressBitStage1A <<= i;
                capLoadStage1A = numAddressBitStage1B * numAddressBitStage1C * rowDecoderStage2->capNandInput;
                capLoadStage1B = numAddressBitStage1A * numAddressBitStage1C * rowDecoderStage2->capNandInput;
                capLoadStage1C = numAddressBitStage1A * numAddressBitStage1B * rowDecoderStage2->capNandInput;
                rowDecoderStage1A = std::make_unique<RowDecoder>();
                rowDecoderStage1A->Initialize(numAddressBitStage1A, capLoadStage1A, 0 /* TODO */, numNandInputStage1A == 3, latency_first, 0, config);
                //rowDecoderStage1A->CalculateRC();
                rowDecoderStage1B = std::make_unique<RowDecoder>();
                rowDecoderStage1B->Initialize(numAddressBitStage1B, capLoadStage1B, 0 /* TODO */, numNandInputStage1B == 3, latency_first, 0, config);
                //rowDecoderStage1B->CalculateRC();
                rowDecoderStage1C = std::make_unique<RowDecoder>();
                rowDecoderStage1C->Initialize(numAddressBitStage1C, capLoadStage1C, 0 /* TODO */, numNandInputStage1C == 3, latency_first, 0, config);
                //rowDecoderStage1C->CalculateRC();
            }
        }
        if (rowDecoderStage1C != NULL) {
            if (numNandInputStage1C == 2){
                capLoadBasicDecoderC = 8 * rowDecoderStage1C->capNandInput;
            } else {
                capLoadBasicDecoderC = 64 * rowDecoderStage1C->capNandInput;
            }
            basicDecoderC = std::make_unique<BasicDecoder>();
            basicDecoderC->Initialize(3, capLoadBasicDecoderC, 0, config /* TODO */);
        } else {
            basicDecoderC = NULL;
        }
        if (rowDecoderStage1B != NULL) {
            if (numNandInputStage1B == 2){
                capLoadBasicDecoderB = 8 * rowDecoderStage1B->capNandInput;
            } else {
                capLoadBasicDecoderB = 64 * rowDecoderStage1B->capNandInput;
            }
            basicDecoderB = std::make_unique<BasicDecoder>();
            basicDecoderB->Initialize(3, capLoadBasicDecoderB, 0, config /* TODO */);
        } else {
            basicDecoderB = NULL;
        }
        if (rowDecoderStage1A != NULL) {
            int numCapNandA1, numCapNandA2;
            if (numDecoder24 == 0) {
                numBasicDecoderA1 = numNandInputStage1A;
                numBasicDecoderA2 = 0;
                numCapNandA1 = 1 << ( 3* (numNandInputStage1A - 1));
                capLoadBasicDecoderA1 = numCapNandA1 * rowDecoderStage1A->capNandInput;
                basicDecoderA1 = std::make_unique<BasicDecoder>();
                basicDecoderA1->Initialize(3, capLoadBasicDecoderA1, 0, config /* TODO */);
                basicDecoderA2 = NULL;
            } else if (numDecoder24 == 1) {
                numBasicDecoderA1 = 1;
                numBasicDecoderA2 = numNandInputStage1A - numBasicDecoderA1;
                numCapNandA1 = 1 << (3 * numBasicDecoderA2);
                numCapNandA2 = 1 << (2 + 3 * (numBasicDecoderA2 - 1));
                capLoadBasicDecoderA1 = numCapNandA1 * rowDecoderStage1A->capNandInput;
                capLoadBasicDecoderA2 = numCapNandA2 * rowDecoderStage1A->capNandInput;
                basicDecoderA1 = std::make_unique<BasicDecoder>();
                basicDecoderA1->Initialize(2, capLoadBasicDecoderA1, 0, config /* TODO */);
                basicDecoderA2 = std::make_unique<BasicDecoder>();
                basicDecoderA2->Initialize(3, capLoadBasicDecoderA2, 0, config /* TODO */);
            } else if (numDecoder24 == 2) {
                if (numNandInputStage1A == 2) {
                    numBasicDecoderA1 = 2;
                    numBasicDecoderA2 = 0;
                    basicDecoderA1 = std::make_unique<BasicDecoder>();
                    basicDecoderA1->Initialize(2, 4 * rowDecoderStage1A->capNandInput, 0, config /* TODO */);
                    basicDecoderA2 = NULL;
                } else {
                    numBasicDecoderA1 = 2;
                    numBasicDecoderA2 = 1;
                    basicDecoderA1 = std::make_unique<BasicDecoder>();
                    basicDecoderA1->Initialize(2, 32 * rowDecoderStage1A->capNandInput, 0, config /* TODO */);
                    basicDecoderA2 = std::make_unique<BasicDecoder>();
                    basicDecoderA2->Initialize(3, 16 * rowDecoderStage1A->capNandInput, 0, config /* TODO */);
                }
            }
        } else {
            numBasicDecoderA1 = 1;
            numBasicDecoderA2 = 0;
            basicDecoderA1 = std::make_unique<BasicDecoder>();
            basicDecoderA2 = NULL;
            if (numDecoder12 == 1) {
                basicDecoderA1->Initialize(1, capLoad, resLoad, config);
            } else if (numDecoder24 == 1) {
                basicDecoderA1->Initialize(2, capLoad, resLoad, config);
            } else if (numDecoder38 == 1)
                basicDecoderA1->Initialize(3, capLoad, resLoad, config);
        }
    }
    initialized = true;
    //CalculateArea();
    //CalculateRC();
    //CalculatePower();
}

void PredecodeBlock::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[Predecoder Block]");
    } else if (numAddressBit == 0) {
        height = width = area = 0;
    } else {
        double hTemp,wTemp;
        hTemp = wTemp = 0;
        if (basicDecoderA1 != NULL) {
            //basicDecoderA1->CalculateArea();
            wTemp = std::max(wTemp, basicDecoderA1->width);
            hTemp += numBasicDecoderA1 * basicDecoderA1->height;
            if (basicDecoderA2 != NULL) {
                //basicDecoderA2->CalculateArea();
                wTemp = std::max(wTemp, basicDecoderA2->width);
                hTemp += numBasicDecoderA2 * basicDecoderA2->height;
            }
            if (basicDecoderB != NULL) {
                //basicDecoderB->CalculateArea();
                wTemp = std::max(wTemp, basicDecoderB->width);
                hTemp += numNandInputStage1B * basicDecoderB->height;
                if (basicDecoderC != NULL) {
                    //basicDecoderC->CalculateArea();
                    wTemp = std::max(wTemp, basicDecoderC->width);
                    hTemp += numNandInputStage1C * basicDecoderC->height;
                }
            }
        }
        width = wTemp;
        height = hTemp;
        hTemp = wTemp = 0;
        if (rowDecoderStage1A != NULL) {
            //rowDecoderStage1A->CalculateArea();
            wTemp = std::max(wTemp, rowDecoderStage1A->width);
            hTemp += rowDecoderStage1A->height;
            if (rowDecoderStage1B != NULL) {
                //rowDecoderStage1B->CalculateArea();
                wTemp = std::max(wTemp, rowDecoderStage1B->width);
                hTemp += rowDecoderStage1B->height;
                if (rowDecoderStage1C != NULL) {
                    //rowDecoderStage1C->CalculateArea();
                    wTemp = std::max(wTemp, rowDecoderStage1C->width);
                    hTemp += rowDecoderStage1C->height;
                }
            }
            if (rowDecoderStage2 != NULL) {
                //rowDecoderStage2->CalculateArea();
                wTemp += rowDecoderStage2->width;
                hTemp = std::max(hTemp, rowDecoderStage2->height);
            }
        }
        width += wTemp;
        height = std::max(height, hTemp);
        area = width * height;
    }
}

void PredecodeBlock::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[Predecoder Block]");
    } else if (numAddressBit > 0) {
        if (basicDecoderA1 != NULL) {
            //basicDecoderA1->CalculateRC();
            if (basicDecoderA2 != NULL) {
                //basicDecoderA2->CalculateRC();
            }
            if (basicDecoderB != NULL) {
                //basicDecoderB->CalculateRC();
                if (basicDecoderC != NULL) {
                    //basicDecoderC->CalculateRC();
                }
            }
        }
    }
}

void PredecodeBlock::CalculateLatency(double _rampInput) {
    if (!initialized) {
        ThrowInitializationError("[Predecoder Block]");
    } else if (numAddressBit == 0) {
        readLatency = writeLatency = 0;
        rampOutput = _rampInput;
    } else {
        rampInput = _rampInput;
        double delayA1, delayA2, delayB, delayC;
        double maxRampOutput = 0;
        delayA1 = delayA2 = delayB = delayC = 0;
        rampOutput = 0;
        readLatency = writeLatency = 0;
        if (basicDecoderA1 != NULL) {
            basicDecoderA1->CalculateLatency(rampInput);
            delayA1 += basicDecoderA1->readLatency;
            maxRampOutput = basicDecoderA1->rampOutput;
            if (rowDecoderStage1A != NULL) {
                rowDecoderStage1A->CalculateLatency(basicDecoderA1->rampOutput);
                delayA1 += rowDecoderStage1A->readLatency;
                maxRampOutput = rowDecoderStage1A->rampOutput;
                if (rowDecoderStage2 != NULL) {
                    rowDecoderStage2->CalculateLatency(rowDecoderStage1A->rampOutput);
                    delayA1 += rowDecoderStage2->readLatency;
                    maxRampOutput = rowDecoderStage2->rampOutput;
                }
            }
        }
        rampOutput = std::max(rampOutput, maxRampOutput);
        readLatency = std::max(readLatency, delayA1);
        maxRampOutput = 0;
        if (basicDecoderA2 != NULL) {
            basicDecoderA2->CalculateLatency(rampInput);
            delayA2 += basicDecoderA2->readLatency;
            rowDecoderStage1A->CalculateLatency(basicDecoderA2->rampOutput);
            delayA2 += rowDecoderStage1A->readLatency;
            maxRampOutput = rowDecoderStage1A->rampOutput;
            if (rowDecoderStage2 != NULL) {
                rowDecoderStage2->CalculateLatency(rowDecoderStage1A->rampOutput);
                delayA2 += rowDecoderStage2->readLatency;
                maxRampOutput = rowDecoderStage2->rampOutput;
            }
        }
        rampOutput = std::max(rampOutput, maxRampOutput);
        readLatency = std::max(readLatency, delayA2);
        maxRampOutput = 0;
        if (basicDecoderB !=NULL) {
            basicDecoderB->CalculateLatency(rampInput);
            delayB += basicDecoderB->readLatency;
            rowDecoderStage1B->CalculateLatency(basicDecoderB->rampOutput);
            delayB += rowDecoderStage1B->readLatency;
            rowDecoderStage2->CalculateLatency(rowDecoderStage1B->rampOutput);
            delayB += rowDecoderStage2->readLatency;
            maxRampOutput = rowDecoderStage2->rampOutput;
        }
        rampOutput = std::max(rampOutput, maxRampOutput);
        readLatency = std::max(readLatency, delayB);
        maxRampOutput = 0;
        if (basicDecoderC !=NULL) {
            basicDecoderC->CalculateLatency(rampInput);
            delayC += basicDecoderC->readLatency;
            rowDecoderStage1C->CalculateLatency(basicDecoderC->rampOutput);
            delayC += rowDecoderStage1C->readLatency;
            rowDecoderStage2->CalculateLatency(rowDecoderStage1C->rampOutput);
            delayC += rowDecoderStage2->readLatency;
            maxRampOutput = rowDecoderStage2->rampOutput;
        }
        rampOutput = std::max(rampOutput, maxRampOutput);
        readLatency = std::max(readLatency, delayC);
        writeLatency = readLatency;
    }
}


void PredecodeBlock::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[Predecoder Block]");
    } else if (numAddressBit == 0) {
        leakage = readDynamicEnergy = writeDynamicEnergy = 0;
    } else {
        leakage = readDynamicEnergy = 0;
        if (basicDecoderA1 != NULL) {
            basicDecoderA1->CalculatePower();
            leakage += basicDecoderA1->leakage;
            readDynamicEnergy += basicDecoderA1->readDynamicEnergy;
            if (basicDecoderA2 != NULL) {
                basicDecoderA2->CalculatePower();
                leakage += basicDecoderA2->leakage;
                readDynamicEnergy += basicDecoderA2->readDynamicEnergy;
            }
            if (basicDecoderB != NULL) {
                basicDecoderB->CalculatePower();
                leakage += basicDecoderB->leakage;
                readDynamicEnergy += basicDecoderB->readDynamicEnergy;
                if (basicDecoderC != NULL) {
                    basicDecoderC->CalculatePower();
                    leakage += basicDecoderC->leakage;
                    readDynamicEnergy += basicDecoderC->readDynamicEnergy;
                }
            }
        }
        if (rowDecoderStage1A != NULL) {
            rowDecoderStage1A->CalculatePower();
            leakage += rowDecoderStage1A->leakage;
            readDynamicEnergy += rowDecoderStage1A->readDynamicEnergy;
            if (rowDecoderStage1B != NULL) {
                rowDecoderStage1B->CalculatePower();
                leakage += rowDecoderStage1B->leakage;
                readDynamicEnergy += rowDecoderStage1B->readDynamicEnergy;
                if (rowDecoderStage1C != NULL) {
                    rowDecoderStage1C->CalculatePower();
                    leakage += rowDecoderStage1C->leakage;
                    readDynamicEnergy += rowDecoderStage1C->readDynamicEnergy;
                }
            }
            if (rowDecoderStage2 != NULL) {
                rowDecoderStage2->CalculatePower();
                leakage += rowDecoderStage2->leakage;
                readDynamicEnergy += rowDecoderStage2->readDynamicEnergy;
            }
        }
        writeDynamicEnergy = readDynamicEnergy;
    }
}

void PredecodeBlock::PrintProperty() {
    std::cout << "Predecoding Block Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
