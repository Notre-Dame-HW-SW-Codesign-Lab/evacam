#include "BankWithoutHtree.h"
#include "formula.h"

void BankWithoutHtree::Initialize(int _numRowMat, int _numColumnMat, long long _capacity,
        long _blockSize, int _associativity, int _numRowPerSet, int _numActiveMatPerRow,
        int _numActiveMatPerColumn, int _muxSenseAmp, bool _internalSenseAmp, int _muxOutputLev1, 
        int _muxOutputLev2, int _numRowSubarray, int _numColumnSubarray,
        int _numActiveSubarrayPerRow, int _numActiveSubarrayPerColumn,
        BufferDesignTarget _areaOptimizationLevel, MemoryType _memoryType, CAMType _camType, 
        SearchFunction _searchFunction, std::shared_ptr<EvaCamConfig> _config,
        std::shared_ptr<Wire> _localWire, std::shared_ptr<Wire> _globalWire, 
        const CAM_Opt &_CAM_opt) {
    (void)_searchFunction;

    if (!_localWire || !_globalWire)
        throw std::runtime_error("[BankWithoutHtree] Error: wires not delcared.");

    localWire = _localWire;
    globalWire = _globalWire;
    config = _config;

    if (initialized) {
        /* Reset the class for re-initialization */
        initialized = false;
        invalid = false;
    }

    if (!_internalSenseAmp) {
        if (config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
            invalid = true;
            config->logger.Verbose() << "[BankWithoutHtree] DRAM does not support external sense amplification.";
            return;
        } else if (globalWire->wireRepeaterType != repeated_none) {
            invalid = true;
            initialized = true;
            return;
        }
    }

    numRowMat = _numRowMat;
    numColumnMat = _numColumnMat;
    capacity = _capacity;
    blockSize = _blockSize;
    associativity = _associativity;
    numRowPerSet = _numRowPerSet;
    internalSenseAmp = _internalSenseAmp;
    areaOptimizationLevel = _areaOptimizationLevel;
    memoryType = _memoryType;
    numWay = 1;	/* default value for non-cache design */

    camType = _camType;
    CAM_opt = _CAM_opt;

    /* Calculate the physical signals that are required in routing */
    numAddressBit = (int)(log2((double)capacity / blockSize / associativity) + 0.1);
    /* use double during the calculation to avoid overflow */

    globalBitlineMux = std::make_shared<Mux>();
    globalSenseAmp = std::make_shared<SenseAmp>();
    globalComparator = std::make_shared<Comparator>();


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
    numDataBitRouteToMat = blockSize;


    if (memoryType == mem_data) { /* Data array */
        numDataBitRouteToMat = blockSize / numActiveMatPerColumn / numActiveMatPerRow;
        if (numRowPerSet > associativity) {
            /* There is no enough ways to distribute into multiple rows */
            invalid = true;
            initialized = true;
            return;
        }
        numWay = associativity;
        int numWayPerRow = numWay / numRowPerSet;	/* At least 1, otherwise it is invalid, and returned already */
        if (numWayPerRow > 1) {		/* multiple ways per row, needs extra mux level */
            /* Do mux level recalculation to contain the multiple ways */
            if (config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
                /* for DRAM, mux before sense amp has to be 1, only mux output1 and mux output2 can be used */
                int numWayPerRowInLog = (int)(log2((double)numWayPerRow) + 0.1);
                int extraMuxOutputLev2 = (int)pow(2, numWayPerRowInLog / 2);
                int extraMuxOutputLev1 = numWayPerRow / extraMuxOutputLev2;
                muxOutputLev1 *= extraMuxOutputLev1;
                muxOutputLev2 *= extraMuxOutputLev2;
            } else {
                /* for non-DRAM, all mux levels can be used */
                int numWayPerRowInLog = (int)(log2((double)numWayPerRow) + 0.1);
                int extraMuxOutputLev2 = (int)pow(2, numWayPerRowInLog / 3);
                int extraMuxOutputLev1 = extraMuxOutputLev2;
                int extraMuxSenseAmp = numWayPerRow / extraMuxOutputLev1 / extraMuxOutputLev2;
                muxSenseAmp *= extraMuxSenseAmp;
                muxOutputLev1 *= extraMuxOutputLev1;
                muxOutputLev2 *= extraMuxOutputLev2;
            }
        }
    } else { /* CAM */
        numDataBitRouteToMat = blockSize;
        numWay = 1;
    }

    mat = std::make_shared<Mat>();

    mat->Initialize(numRowSubarray, numColumnSubarray, numAddressBitRouteToMat, numDataBitRouteToMat,
            numWay, numRowPerSet, false, numActiveSubarrayPerRow, numActiveSubarrayPerColumn,
            muxSenseAmp, internalSenseAmp, muxOutputLev1, muxOutputLev2, areaOptimizationLevel, 
            memoryType, camType, searchFunction, config, localWire, CAM_opt);
    /* Check if mat is under a legal configuration */
    if (mat->invalid) {
        invalid = true;
        initialized = true;
        return;
    }

    mat->CalculateArea();

    if (!internalSenseAmp) {
        bool voltageSense = true;
        double senseVoltage;
        senseVoltage = config->technology.cell->minSenseVoltage;
        if (config->technology.cell->memCellType == SRAM) {
            /* SRAM, DRAM, and eDRAM all use voltage sensing */
            voltageSense = true;
        } else if (config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == memristor || config->technology.cell->memCellType == FBRAM || config->technology.cell->memCellType == FEFETRAM) {
            voltageSense = config->technology.cell->readMode;
        } else {/* NAND flash */
            // TODO
        }

        int numSenseAmp;
        if (memoryType == mem_data)
            numSenseAmp = blockSize;
        else
            numSenseAmp = blockSize * associativity;

        globalSenseAmp->Initialize(numSenseAmp, !voltageSense, senseVoltage, mat->width * numColumnMat / numSenseAmp, config);
        if (globalSenseAmp->invalid) {
            invalid = true;
            initialized = true;
            return;
        }
        globalSenseAmp->CalculateRC();
        globalBitlineMux->Initialize(numRowMat * numColumnMat / numActiveMatPerColumn / numActiveMatPerRow, numSenseAmp, globalSenseAmp->capLoad, globalSenseAmp->capLoad, 0, config);
        globalBitlineMux->CalculateRC();
    }

    /* Reset the mux values for correct printing */
    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;

    initialized = true;
    CalculateArea();
    CalculateRC();
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
        if (globalWire->wireRepeaterType == repeated_none) {
            numWireSharingWidth = 1;
            effectivePitch = 0;		/* assume that the wire is built on another metal layer, there does not cause silicon area */
            //effectivePitch = globalWire->wirePitch;
        } else {
            numWireSharingWidth = (int)floor(globalWire->repeaterSpacing / globalWire->repeaterHeight);
            effectivePitch = globalWire->repeatedWirePitch;
        }

        width += ceil((double)numRowMat * numColumnMat * numAddressBitRouteToMat / numWireSharingWidth) * effectivePitch;

        if (!internalSenseAmp) {
            globalSenseAmp->CalculateArea();
            height += globalSenseAmp->height;
            globalBitlineMux->CalculateArea();
            height += globalBitlineMux->height;
        }

        /* Determine if the aspect ratio meets the constraint */
        if (memoryType == mem_data)
            if (height / width > CONSTRAINT_ASPECT_RATIO_BANK || width / height > CONSTRAINT_ASPECT_RATIO_BANK) {
                /* illegal */
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
        if (!internalSenseAmp) {
            globalBitlineMux->CalculateRC();
            globalSenseAmp->CalculateRC();
        }
    }
}

void BankWithoutHtree::CalculateLatencyAndPower() {
    if (!initialized) {
        ThrowInitializationError("[BankWithoutHtree]");
    } else if (invalid) {
        readLatency = writeLatency = 1e41;
        readDynamicEnergy = writeDynamicEnergy = 1e41;
        leakage = 1e41;
    } else {
        double latency = 0;
        double energy = 0;
        double leakageWire = 0;

        mat->CalculateLatency(1e41 /* means Inf */);
        mat->CalculatePower();
        readLatency = resetLatency = setLatency = writeLatency = 0;
        readDynamicEnergy = writeDynamicEnergy = resetDynamicEnergy = setDynamicEnergy = 0;
        leakage = 0;

        double lengthWire;
        lengthWire = mat->height * (numRowMat + 1);
        for (int i = 0; i < numRowMat; i++) {
            lengthWire -= mat->height;
            if (internalSenseAmp) {
                double numBitRouteToMat = 0;
                globalWire->CalculateLatencyAndPower(lengthWire, &latency, &energy, &leakageWire);
                if (i == 0){
                    readLatency += latency;
                    writeLatency += latency;
                }
                if (i < numActiveMatPerColumn) {
                    numBitRouteToMat = numAddressBitRouteToMat + numDataBitRouteToMat;
                    readDynamicEnergy += energy * numBitRouteToMat * numActiveMatPerRow;
                    writeDynamicEnergy += energy * numBitRouteToMat * numActiveMatPerRow;
                }
                leakage += leakageWire * numBitRouteToMat * numColumnMat;
            } else {
                double resLocalBitline, capLocalBitline, resBitlineMux, capBitlineMux;
                capBitlineMux = globalBitlineMux->capNMOSPassTransistor;
                resBitlineMux = globalBitlineMux->resNMOSPassTransistor;

                resLocalBitline = mat->subarray->resBitline + 3 * resBitlineMux;
                capLocalBitline = mat->subarray->capBitline + 6 * capBitlineMux;
                double resGlobalBitline, capGlobalBitline;
                resGlobalBitline = lengthWire * globalWire->resWirePerUnit;
                capGlobalBitline = lengthWire * globalWire->capWirePerUnit;
                double capGlobalBitlineMux;
                capGlobalBitlineMux = globalBitlineMux->capForPreviousDelayCalculation;
                if (config->technology.cell->memCellType == SRAM) {
                    double vpre = config->technology.cell->readVoltage;	/* This value should be equal to resetVoltage and setVoltage for SRAM */
                    if (i == 0) {
                        latency = resLocalBitline * capGlobalBitline / 2 +
                            (resLocalBitline + resGlobalBitline) * (capGlobalBitline / 2 + capGlobalBitlineMux);
                        latency *= log(vpre / (vpre - globalSenseAmp->senseVoltage));
                        latency += resLocalBitline * capGlobalBitline / 2;
                        globalBitlineMux->CalculateLatency(1e20);
                        latency += globalBitlineMux->readLatency;
                        globalSenseAmp->CalculateLatency(1e20);
                        writeLatency += latency;
                        latency += globalSenseAmp->readLatency;
                        readLatency += latency;
                    }
                    if (i <  numActiveMatPerColumn) {
                        energy = capGlobalBitline * config->technology.tech->vdd() * config->technology.tech->vdd() * numAddressBitRouteToMat;
                        readDynamicEnergy += energy;
                        writeDynamicEnergy += energy;
                        readDynamicEnergy += capGlobalBitline * vpre * vpre * numWay;
                        writeDynamicEnergy += capGlobalBitline * vpre * vpre * numDataBitRouteToMat;
                    }
                    // TODO: cap calculation needs further consideration
                } else if (config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == memristor || config->technology.cell->memCellType == FBRAM || config->technology.cell->memCellType == FEFETRAM) {
                    double vWrite = MAX(fabs(config->technology.cell->resetVoltage), fabs(config->technology.cell->setVoltage));
                    double tau, latencyOff, latencyOn;
                    double vPre = mat->subarray->voltagePrecharge;
                    double vOn = mat->subarray->voltageMemCellOn;
                    double vOff = mat->subarray->voltageMemCellOff;
                    if (i == 0) {
                        tau = resBitlineMux * capGlobalBitline / 2 + (resBitlineMux + resGlobalBitline)
                            * (capGlobalBitline + capLocalBitline) / 2 + (resBitlineMux + resGlobalBitline
                                    + resLocalBitline) * capLocalBitline / 2;
                        writeLatency += 0.63 * tau;
                        if (config->technology.cell->readMode == false) {	/* current-sensing */
                            /* Use ICCAD 2009 model */
                            resLocalBitline += mat->subarray->resMemCellOff;
                            tau = resGlobalBitline * capGlobalBitline / 2 *
                                (resLocalBitline + resGlobalBitline / 3) / (resLocalBitline + resGlobalBitline);
                            readLatency += 0.63 * tau;
                        } else {						/* voltage-sensing */
                            if (config->technology.cell->readVoltage == 0) {  /* Current-in voltage sensing */
                                resLocalBitline += mat->subarray->resMemCellOn;
                                tau = resLocalBitline * capGlobalBitline + (resLocalBitline + resGlobalBitline) * capGlobalBitline / 2;
                                latencyOn = tau * log((vPre - vOn)/(vPre - vOn - globalSenseAmp->senseVoltage));
                                resLocalBitline += config->technology.cell->resistanceOff - config->technology.cell->resistanceOn;
                                tau = resLocalBitline * capGlobalBitline + (resLocalBitline + resGlobalBitline) * capGlobalBitline / 2;
                                latencyOff = tau * log((vOff - vPre)/(vOff - vPre - globalSenseAmp->senseVoltage));
                            } else {   /*Voltage-in voltage sensing */
                                resLocalBitline += mat->subarray->resEquivalentOn;
                                tau = resLocalBitline * capGlobalBitline + (resLocalBitline + resGlobalBitline) * capGlobalBitline / 2;
                                latencyOn = tau * log((vPre - vOn)/(vPre - vOn - globalSenseAmp->senseVoltage));
                                resLocalBitline += mat->subarray->resEquivalentOff - mat->subarray->resEquivalentOn;
                                tau = resLocalBitline * capGlobalBitline + (resLocalBitline + resGlobalBitline) * capGlobalBitline / 2;
                                latencyOff = tau * log((vOff - vPre)/(vOff - vPre - globalSenseAmp->senseVoltage));
                            }
                            readLatency -= mat->subarray->bitlineDelay;
                            if ((latencyOn + mat->subarray->bitlineDelayOn) > (latencyOff + mat->subarray->bitlineDelayOff))
                                readLatency += latencyOn + mat->subarray->bitlineDelayOn;
                            else
                                readLatency += latencyOff + mat->subarray->bitlineDelayOff;
                        }
                    }
                    if (i <  numActiveMatPerColumn) {
                        energy = capGlobalBitline * config->technology.tech->vdd() * config->technology.tech->vdd() * numAddressBitRouteToMat;
                        readDynamicEnergy += energy;
                        writeDynamicEnergy += energy;
                        writeDynamicEnergy += capGlobalBitline * vWrite * vWrite * numDataBitRouteToMat;
                        if (config->technology.cell->readMode) { /*Voltage-in voltage sensing */
                            readDynamicEnergy += capGlobalBitline * (vPre * vPre - vOn * vOn )* numDataBitRouteToMat;
                        }
                    }
                }

            }
        }
        if (!internalSenseAmp) {
            globalBitlineMux->CalculateLatency(1e40);
            globalSenseAmp->CalculateLatency(1e40);
            readLatency += globalBitlineMux->readLatency + globalSenseAmp->readLatency;
            writeLatency += globalBitlineMux->writeLatency + globalSenseAmp->writeLatency;
            globalBitlineMux->CalculatePower();
            globalSenseAmp->CalculatePower();
            readDynamicEnergy += (globalBitlineMux->readDynamicEnergy + globalSenseAmp->readDynamicEnergy) * numActiveMatPerRow;
            writeDynamicEnergy += (globalBitlineMux->writeDynamicEnergy + globalSenseAmp->writeDynamicEnergy) * numActiveMatPerRow;
            leakage += (globalBitlineMux->leakage + globalSenseAmp->leakage) * numColumnMat;
        }
    }

    readLatency += mat->readLatency;
    resetLatency = writeLatency + mat->resetLatency;
    setLatency = writeLatency + mat->setLatency;
    writeLatency += mat->writeLatency;
    readDynamicEnergy += mat->readDynamicEnergy * numActiveMatPerRow * numActiveMatPerColumn;
    cellReadEnergy = mat->cellReadEnergy * numActiveMatPerRow * numActiveMatPerColumn;
    cellSetEnergy = mat->cellSetEnergy * numActiveMatPerRow * numActiveMatPerColumn;
    cellResetEnergy = mat->cellResetEnergy * numActiveMatPerRow * numActiveMatPerColumn;
    resetDynamicEnergy = writeDynamicEnergy + mat->resetDynamicEnergy * numActiveMatPerRow * numActiveMatPerColumn;
    setDynamicEnergy = writeDynamicEnergy + mat->setDynamicEnergy * numActiveMatPerRow * numActiveMatPerColumn;
    writeDynamicEnergy += mat->writeDynamicEnergy * numActiveMatPerRow * numActiveMatPerColumn;
    leakage += mat->leakage * numRowMat * numColumnMat;
}
