// Fully legacy file, no current instantiation of this object

#include "SubArray.h"
#include "formula.h"
void SubArray::Initialize(long long _numRow, long long _numColumn, bool _multipleRowPerSet, bool _split,
        int _muxSenseAmp, bool _internalSenseAmp, int _muxOutputLev1, int _muxOutputLev2,
        BufferDesignTarget _areaOptimizationLevel, std::shared_ptr<EvaCamConfig> _config,
        const Wire &_localWire) {
    if (initialized)
        _config->logger.Verbose() << "[Subarray] Warning: Already initialized!";

    numRow = _numRow;
    numColumn = _numColumn;
    multipleRowPerSet = _multipleRowPerSet;
    split = _split;
    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;
    internalSenseAmp = _internalSenseAmp;
    areaOptimizationLevel = _areaOptimizationLevel;
    config = _config;
    localWire = _localWire;

    double maxWordlineCurrent = 0;
    double maxBitlineCurrent = 0;

    rowDecoder = std::make_unique<RowDecoder>();
    bitlineMuxDecoder = std::make_unique<RowDecoder>();
    bitlineMux = std::make_unique<Mux>();
    senseAmpMuxLev1Decoder = std::make_unique<RowDecoder>();
    senseAmpMuxLev1 = std::make_unique<Mux>();
    senseAmpMuxLev2Decoder = std::make_unique<RowDecoder>();
    senseAmpMuxLev2 = std::make_unique<Mux>();
    precharger = std::make_unique<Precharger>();
    senseAmp = std::make_unique<SenseAmp>();

    if (config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
        if (muxSenseAmp > 1) {
            /* DRAM does not allow muxed bitline because of its destructive readout */
            invalid = true;
            initialized = true;
            return;
        }
    }
    if (config->technology.cell->memCellType == SLCNAND) {
        if (numRow < config->input.flashBlockSize / config->input.pageSize) {
            /* SLC NAND does not have enough rows to hold the page count */
            invalid = true;
            initialized = true;
            return;
        }
        if (internalSenseAmp && muxSenseAmp < 2) {
            /* There is no way to put the sense amp */
            invalid = true;
            initialized = true;
            return;
        }
    }

    if (config->technology.cell->memCellType == memristor || config->technology.cell->memCellType == FBRAM) {
        if (internalSenseAmp && muxSenseAmp < 2) {
            /* There is no way to put the sense amp */
            invalid = true;
            std::cout << "RRAM/FBRAM: There is no way to put the sense amp" << std::endl;
            initialized = true;
            return;
        }
    }

    if (config->technology.cell->memCellType == FBRAM) {
        if (config->technology.cell->resistanceOff / config->technology.cell->resistanceOn < numRow / BITLINE_LEAKAGE_TOLERANCE) {
            /* bitline too long */
            invalid = true;
            std::cout << "bitline too long" << std::endl;
            initialized = true;
            return;
        }
        maxBitlineCurrent = MAX(config->technology.cell->resetCurrent, config->technology.cell->setCurrent) + config->technology.cell->leakageCurrentAccessDevice * (numRow - 1);
    }

    if (config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == memristor) {
        if (config->technology.cell->accessType == CMOS_access){
            if (config->technology.tech->currentOnNmos()[config->input.temperature - 300]
                    / config->technology.tech->currentOffNmos()[config->input.temperature - 300] < numRow / BITLINE_LEAKAGE_TOLERANCE) {
                /* bitline too long */
                invalid = true;
                std::cout << "bitline too long" << std::endl;
                initialized = true;
                return;
            }
            maxBitlineCurrent = MAX(config->technology.cell->resetCurrent, config->technology.cell->setCurrent) + config->technology.cell->leakageCurrentAccessDevice * (numRow - 1);
        } else { //non-CMOS access
            /* Write half select problem limit the array size */
            double resetCurrent;
            if (config->technology.cell->resetCurrent == 0) {
                resetCurrent = (fabs (config->technology.cell->resetVoltage) - config->technology.cell->voltageDropAccessDevice) / config->technology.cell->resistanceOnAtResetVoltage;
            } else
                resetCurrent = config->technology.cell->resetCurrent;
            int numSelectedColumnPerRow = numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
            if (config->technology.cell->accessType == none_access) {
                maxWordlineCurrent = resetCurrent * numSelectedColumnPerRow + resetCurrent * config->technology.cell->resistanceOnAtResetVoltage
                    / 2 / config->technology.cell->resistanceOnAtHalfResetVoltage * (numColumn - numSelectedColumnPerRow);
            } else { //diode or BJT
                maxWordlineCurrent = resetCurrent * numSelectedColumnPerRow + config->technology.cell->leakageCurrentAccessDevice
                    * (numColumn - numSelectedColumnPerRow);
            }
            double minWordlineDriverWidth = maxWordlineCurrent / config->technology.tech->currentOnNmos()[config->input.temperature - 300];
            if (minWordlineDriverWidth > config->input.maxNmosSize * config->technology.tech->featureSize()) {
                invalid = true;
                std::cout << "too large minWordlineDriverWidth" << std::endl;
                return;
            }
            if (config->technology.cell->accessType == none_access) {
                maxBitlineCurrent = resetCurrent + resetCurrent * config->technology.cell->resistanceOnAtResetVoltage / 2
                    / config->technology.cell->resistanceOnAtHalfResetVoltage * (numRow - 1);
            } else { //diode or BJT
                maxBitlineCurrent = resetCurrent + config->technology.cell->leakageCurrentAccessDevice * (numRow - 1);
            }
        }
    }

    double minBitlineMuxWidth = maxBitlineCurrent / config->technology.tech->currentOnNmos()[config->input.temperature - 300];
    minBitlineMuxWidth = MAX(MIN_NMOS_SIZE * config->technology.tech->featureSize(), minBitlineMuxWidth);
    if (minBitlineMuxWidth > config->input.maxNmosSize * config->technology.tech->featureSize()) {
        invalid = true;
        config->logger.Verbose() << "[Subarray] minBitlineMuxWidth exceeds max supported NMOS size.";
        return;
    }

    if (internalSenseAmp) {
        if (config->technology.cell->memCellType == SRAM || config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
            /* SRAM, DRAM, and eDRAM all use voltage sensing */
            voltageSense = true;
        } else if (config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == memristor || config->technology.cell->memCellType == FBRAM) {
            voltageSense = config->technology.cell->readMode;
        } else {/* NAND flash */
            voltageSense = true;
        }
    } else if (config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
        invalid = true;
        config->logger.Verbose() << "[Subarray] DRAM does not support external sense amplifiers.";
        initialized = true;
        return;
    }

    if (config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
        senseVoltage = config->technology.tech->vdd() / 2 * config->technology.cell->capDRAMCell / (config->technology.cell->capDRAMCell + capBitline);
        if (senseVoltage < config->technology.cell->minSenseVoltage) {		/* Bitline is too long */
            invalid = true;
            config->logger.Verbose() << "[Subarray] Bitline is too long.";
            initialized = true;
            return;
        }
    } else if (config->technology.cell->memCellType == SLCNAND){
        /* suppose the reference voltage is 0.5Vdd, the initial bitline voltage is 0.6Vdd
         * if the bitline drops to 0.4Vdd, the senseamp can tell which mem_data is stored */
        senseVoltage = MAX(config->technology.cell->minSenseVoltage, 0.2 * config->technology.tech->vdd());
    } else {
        /* TODO: different memory config->technology might have different values here */
        senseVoltage = config->technology.cell->minSenseVoltage;
    }

    /* Derived parameters */
    numSenseAmp = numColumn / muxSenseAmp;
    lenWordline = (double)numColumn * config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize();
    lenBitline = (double)numRow * config->technology.cell->heightInFeatureSize * config->technology.tech->featureSize();
    /* Add stitching overhead if necessary */
    if (config->technology.cell->stitching) {
        lenWordline += ((numColumn - 1) / config->technology.cell->stitching + 1) * STITCHING_OVERHEAD * config->technology.tech->featureSize();
    }
    /* Add select transistors into the length calculation */
    if (config->technology.cell->memCellType == SLCNAND) {
        int pageCount = config->input.flashBlockSize / config->input.pageSize;
        /* Two select transistor including contacts have total length of 5F */
        lenBitline += (numRow / pageCount) * 5 * config->technology.tech->featureSize();
    }
    /* Calculate wire resistance/capacitance */
    capWordline = lenWordline * localWire.capWirePerUnit;
    resWordline = lenWordline * localWire.resWirePerUnit;
    capBitline = lenBitline * localWire.capWirePerUnit;
    resBitline = lenBitline * localWire.resWirePerUnit;

    /* Caclulate the load resistance and capacitance for Mux Decoders */
    double capMuxLoad, resMuxLoad;
    resMuxLoad = resWordline;
    capMuxLoad = CalculateGateCap(minBitlineMuxWidth, *config->technology.tech) * numColumn;
    capMuxLoad += capWordline;

    /* Add transistor resistance/capacitance */
    if (config->technology.cell->memCellType == SRAM) {
        /* SRAM has two access transistors */
        resCellAccess = CalculateOnResistance(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), NMOS, config->input.temperature, *config->technology.tech);
        capCellAccess = CalculateDrainCap(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), NMOS, config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize(), *config->technology.tech);
        capWordline += 2 * CalculateGateCap(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), *config->technology.tech) * numColumn;
        capBitline  += capCellAccess * numRow / 2;	/* Due to shared contact */
        voltagePrecharge = config->technology.tech->vdd() / 2;	/* SRAM read voltage is always half of vdd */
    } else if (config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
        /* DRAM and eDRAM only has one access transistors */
        resCellAccess = CalculateOnResistance(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), NMOS, config->input.temperature, *config->technology.tech);
        capCellAccess = CalculateDrainCap(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), NMOS, config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize(), *config->technology.tech);
        capWordline += CalculateGateCap(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), *config->technology.tech) * numColumn;
        capBitline  += capCellAccess * numRow / 2;	/* Due to shared contact */
        voltagePrecharge = config->technology.tech->vdd() / 2;	/* DRAM read voltage is always half of vdd */
    } else if (config->technology.cell->memCellType == FBRAM) { /* Floating Body RAM */
        resCellAccess = 0;
        capCellAccess = CalculateFBRAMDrainCap(config->technology.cell->widthSOIDevice * config->technology.tech->featureSize(), *config->technology.tech);
        capWordline += CalculateFBRAMGateCap(config->technology.cell->widthSOIDevice * config->technology.tech->featureSize(), config->technology.cell->gateOxThicknessFactor, *config->technology.tech) * numColumn;
        capBitline  += capCellAccess * numRow / 2;	/* Due to shared contact */
        resMemCellOff = config->technology.cell->resistanceOff;
        resMemCellOn = config->technology.cell->resistanceOn;
        if (config->technology.cell->readMode) {						/* voltage-sensing */
            if (config->technology.cell->readVoltage == 0) {  /* Current-in voltage sensing */
                voltageMemCellOff = config->technology.cell->readCurrent * resMemCellOff;
                voltageMemCellOn = config->technology.cell->readCurrent * resMemCellOn;
                voltagePrecharge = (voltageMemCellOff + voltageMemCellOn) / 2;
                voltagePrecharge = MIN(config->technology.tech->vdd(), voltagePrecharge);  /* TODO: we can have charge bump to increase SA working point */
                if ((voltagePrecharge - voltageMemCellOn) <= senseVoltage) {
                    invalid = true;
                    config->logger.Verbose() << "[Subarray] Read current does not allow a reasonable precharge voltage.";
                    return;
                }
            } else {   /*Voltage-divider sensing */
                resInSerialForSenseAmp = sqrt(resMemCellOn * resMemCellOff);
                resEquivalentOn = resMemCellOn * resInSerialForSenseAmp / (resMemCellOn + resInSerialForSenseAmp);
                resEquivalentOff = resMemCellOff * resInSerialForSenseAmp / (resMemCellOff + resInSerialForSenseAmp);
                voltageMemCellOff = config->technology.cell->readVoltage * resMemCellOff / (resMemCellOff + resInSerialForSenseAmp);
                voltageMemCellOn = config->technology.cell->readVoltage * resMemCellOn / (resMemCellOn + resInSerialForSenseAmp);
                voltagePrecharge = (voltageMemCellOff + voltageMemCellOn) / 2;
                voltagePrecharge = MIN(config->technology.tech->vdd(), voltagePrecharge);  /* TODO: we can have charge bump to increase SA working point */
                if ((voltagePrecharge - voltageMemCellOn) <= senseVoltage) {
                    invalid = true;
                    config->logger.Verbose() << "[Subarray] Read voltage does not allow a reasonable precharge voltage.";
                    return;
                }
            }
        }
    } else if (config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == memristor) {
        /* MRAM, PCRAM, and memristor have three types of access devices: CMOS, BJT, and diode */
        if (config->technology.cell->accessType == CMOS_access) {
            resCellAccess = CalculateOnResistance(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), NMOS, config->input.temperature, *config->technology.tech);
            capCellAccess = CalculateDrainCap(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), NMOS, config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize(), *config->technology.tech);
            capWordline += CalculateGateCap(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), *config->technology.tech) * numColumn;
            capBitline  += capCellAccess * numRow / 2;	/* Due to shared contact */
        } else if (config->technology.cell->accessType == BJT_access) {
            invalid = true;
            config->logger.Verbose() << "[Subarray] BJT access is not implemented for MRAM, PCRAM, or memristor cells.";
            return;
            /*	} else if (config->technology.cell->accessType == diode_access){
                if (config->technology.cell->readVoltage == 0) {
                resCellAccess = config->technology.cell->voltageDropAccessDevice / config->technology.cell->readCurrent;
                } else {
                if (config->technology.cell->readMode == false) {
                resCellAccess = config->technology.cell->voltageDropAccessDevice / (config->technology.cell->readVoltage
                - config->technology.cell->voltageDropAccessDevice) * config->technology.cell->resistanceOn;
                } else {
                throw std::runtime_error("[Subarray] Error: diode access does not support voltage-input voltage sensing.");
                }
                }
                capCellAccess = MAX(config->technology.cell->capacitanceOn, config->technology.cell->capacitanceOff);
                capWordline += MAX(config->technology.cell->capacitanceOff, config->technology.cell->capacitanceOn) * numColumn;
                capBitline += MAX(config->technology.cell->capacitanceOff, config->technology.cell->capacitanceOn) * numRow;      */
    } else { // none_access || diode_access
             // resCellAccess = 0;
             // capCellAccess = MAX(config->technology.cell->capacitanceOn, config->technology.cell->capacitanceOff);
             // capWordline += MAX(config->technology.cell->capacitanceOff, config->technology.cell->capacitanceOn) * numColumn;  //TODO: choose the right capacitance
             // capBitline += MAX(config->technology.cell->capacitanceOff, config->technology.cell->capacitanceOn) * numRow;      //TODO: choose the right capacitance
    }
    resMemCellOff = resCellAccess + config->technology.cell->resistanceOff;
    resMemCellOn = resCellAccess + config->technology.cell->resistanceOn;
    if (config->technology.cell->readMode) {						/* voltage-sensing */
        if (config->technology.cell->readVoltage == 0) {  /* Current-in voltage sensing */
            voltageMemCellOff = config->technology.cell->readCurrent * resMemCellOff;
            voltageMemCellOn = config->technology.cell->readCurrent * resMemCellOn;
            voltagePrecharge = (voltageMemCellOff + voltageMemCellOn) / 2;
            voltagePrecharge = MIN(config->technology.tech->vdd(), voltagePrecharge);  /* TODO: we can have charge bump to increase SA working point */
            if ((voltagePrecharge - voltageMemCellOn) <= senseVoltage) {
                invalid = true;
                config->logger.Verbose() << "[Subarray] Read current does not allow a reasonable precharge voltage.";
                return;
            }
        } else {   /*Voltage-in voltage sensing */
            resInSerialForSenseAmp = sqrt(resMemCellOn * resMemCellOff);
            resEquivalentOn = resMemCellOn * resInSerialForSenseAmp / (resMemCellOn + resInSerialForSenseAmp);
            resEquivalentOff = resMemCellOff * resInSerialForSenseAmp / (resMemCellOff + resInSerialForSenseAmp);
            voltageMemCellOff = config->technology.cell->readVoltage * resMemCellOff / (resMemCellOff + resInSerialForSenseAmp);
            voltageMemCellOn = config->technology.cell->readVoltage * resMemCellOn / (resMemCellOn + resInSerialForSenseAmp);
            voltagePrecharge = (voltageMemCellOff + voltageMemCellOn) / 2;
            voltagePrecharge = MIN(config->technology.tech->vdd(), voltagePrecharge);  /* TODO: we can have charge bump to increase SA working point */
            if ((voltagePrecharge - voltageMemCellOn) <= senseVoltage) {
                invalid = true;
                config->logger.Verbose() << "[Subarray] Read voltage does not allow a reasonable precharge voltage.";
                return;
            }
        }
    }
    } else if (config->technology.cell->memCellType == SLCNAND) {
        /* Calculate the NAND flash string length, which is the page count per block plus 2 (two select transistors) */
        int pageCount = config->input.flashBlockSize / config->input.pageSize;
        int stringLength = pageCount + 2;
        resCellAccess = CalculateOnResistance(config->technology.tech->featureSize(), NMOS, config->input.temperature, *config->technology.tech) * stringLength;
        capCellAccess = CalculateDrainCap(config->technology.tech->featureSize(), NMOS, config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize(), *config->technology.tech);
        /* The capacitance of each cell at the gate terminal is the series of C_control_gate | C_floating_gate */
        capWordline += CalculateGateCap(config->technology.tech->featureSize(), *config->technology.tech) * numColumn * config->technology.cell->gateCouplingRatio / (config->technology.cell->gateCouplingRatio + 1);
        capBitline  += capCellAccess * (numRow / pageCount) / 2;	/* 2 is due to shared contact and the effective row count is numRow/pageCount */
        voltagePrecharge = config->technology.tech->vdd() * 0.6;	/* SLC NAND flash bitline precharge voltage is assumed to 0.6Vdd */
    } else {	/* MLC NAND flash */
        // TODO
    }

    /* Initialize sub-component */

    precharger->Initialize(config->technology.tech->vdd(), numColumn, capBitline, resBitline, config, localWire);
    //precharger->CalculateRC();

    rowDecoder->Initialize(numRow, capWordline, resWordline, multipleRowPerSet, areaOptimizationLevel, maxWordlineCurrent, config);
    if (rowDecoder->invalid) {
        invalid = true;
        config->logger.Verbose() << "[Subarray] Row decoder initialization failed.";
        return;
    }
    //rowDecoder->CalculateRC();

    if (!invalid) {
        bitlineMuxDecoder->Initialize(muxSenseAmp, capMuxLoad, resMuxLoad /* TODO: need to fix */, false, areaOptimizationLevel, 0, config);
        if (bitlineMuxDecoder->invalid){
            std::cout << "bitlineMuxDecoder invalid" << std::endl;
            invalid = true;
        }else{
            //bitlineMuxDecoder->CalculateRC();
        }
    }

    if (!invalid) {
        senseAmpMuxLev1Decoder->Initialize(muxOutputLev1, capMuxLoad, resMuxLoad /* TODO: need to fix */, false, areaOptimizationLevel, 0, config);
        if (senseAmpMuxLev1Decoder->invalid){
            std::cout << "senseAmpMuxLev1Decoder invalid" << std::endl;
            invalid = true;
        }else{
            //senseAmpMuxLev1Decoder->CalculateRC();
        }
    }

    if (!invalid) {
        senseAmpMuxLev2Decoder->Initialize(muxOutputLev2, capMuxLoad, resMuxLoad /* TODO: need to fix */, false, areaOptimizationLevel, 0, config);
        if (senseAmpMuxLev2Decoder->invalid){
            std::cout << "senseAmpMuxLev2Decoder invalid" << std::endl;
            invalid = true;
        }else{
            //senseAmpMuxLev2Decoder->CalculateRC();
        }

    }

    senseAmpMuxLev2->Initialize(muxOutputLev2, numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2, 0, 0 /* TODO: need to fix */, maxBitlineCurrent, config);
    //senseAmpMuxLev2->CalculateRC();

    senseAmpMuxLev1->Initialize(muxOutputLev1, numColumn / muxSenseAmp / muxOutputLev1,
            senseAmpMuxLev2->capForPreviousDelayCalculation, senseAmpMuxLev2->capForPreviousPowerCalculation, maxBitlineCurrent, config);
    //senseAmpMuxLev1->CalculateRC();

    if (internalSenseAmp) {
        if (!invalid) {
            senseAmp->Initialize(numSenseAmp, !voltageSense, senseVoltage, lenWordline / numColumn * muxSenseAmp, config);
            if (senseAmp->invalid){
                std::cout << "SA invalid" << std::endl;
                invalid = true;
            }else{
                //senseAmp->CalculateRC();
            }

        }
        if (!invalid) {
            bitlineMux->Initialize(muxSenseAmp, numColumn / muxSenseAmp, senseAmp->capLoad, senseAmp->capLoad, maxBitlineCurrent, config);
        }
    } else {
        if (!invalid) {
            bitlineMux->Initialize(muxSenseAmp, numColumn / muxSenseAmp,
                    senseAmpMuxLev1->capForPreviousDelayCalculation, senseAmpMuxLev1->capForPreviousPowerCalculation, maxBitlineCurrent, config);
        }
    }

    if (!invalid) {
        //bitlineMux->CalculateRC();
    }

    initialized = true;
    CalculateArea();
}

void SubArray::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[Subarray]");
    } else if (invalid) {
        height = width = area = 1e41;
    } else {
        double addWidth = 0, addHeight = 0;

        width = lenWordline;
        height = lenBitline;

        rowDecoder->CalculateArea();
        if (rowDecoder->height > height) {
            /* assume magic folding */
            addWidth = rowDecoder->area / height;
        } else {
            /* allow white space */
            addWidth = rowDecoder->width;
        }

        precharger->CalculateArea();
        if (precharger->width > width) {
            /* assume magic folding */
            addHeight = precharger->area / precharger->width;
        } else {
            /* allow white space */
            addHeight = precharger->height;
        }

        bitlineMux->CalculateArea();
        addHeight += bitlineMux->height;

        if (internalSenseAmp) {
            senseAmp->CalculateArea();
            if (senseAmp->width > width * 1.001) {
                /* should never happen */
                std::cout << "[ERROR] Sense Amplifier area calculation is wrong!" << std::endl;
            } else {
                addHeight += senseAmp->height;
            }
        }

        senseAmpMuxLev1->CalculateArea();
        addHeight += senseAmpMuxLev1->height;

        senseAmpMuxLev2->CalculateArea();
        addHeight += senseAmpMuxLev2->height;

        bitlineMuxDecoder->CalculateArea();
        addWidth = MAX(addWidth, bitlineMuxDecoder->width);
        senseAmpMuxLev1Decoder->CalculateArea();
        addWidth = MAX(addWidth, senseAmpMuxLev1Decoder->width);
        senseAmpMuxLev2Decoder->CalculateArea();
        addWidth = MAX(addWidth, senseAmpMuxLev2Decoder->width);

        width += addWidth;
        height += addHeight;
        area = width * height;
    }
}

void SubArray::CalculateLatency(double _rampInput) {
    if (!initialized) {
        ThrowInitializationError("[Subarray]");
    } else if (invalid) {
        readLatency = writeLatency = 1e41;
    } else {
        precharger->CalculateLatency(_rampInput);
        rowDecoder->CalculateLatency(_rampInput);
        bitlineMuxDecoder->CalculateLatency(_rampInput);
        senseAmpMuxLev1Decoder->CalculateLatency(_rampInput);
        senseAmpMuxLev2Decoder->CalculateLatency(_rampInput);
        columnDecoderLatency = MAX(MAX(bitlineMuxDecoder->readLatency, senseAmpMuxLev1Decoder->readLatency), senseAmpMuxLev2Decoder->readLatency);
        double decoderLatency = MAX(rowDecoder->readLatency, columnDecoderLatency);
        /*need a second thought on this equation*/
        double capPassTransistor = bitlineMux->capNMOSPassTransistor +
            senseAmpMuxLev1->capNMOSPassTransistor + senseAmpMuxLev2->capNMOSPassTransistor;
        double resPassTransistor = bitlineMux->resNMOSPassTransistor +
            senseAmpMuxLev1->resNMOSPassTransistor + senseAmpMuxLev2->resNMOSPassTransistor;
        double tauChargeLatency = resPassTransistor * (capPassTransistor + capBitline) + resBitline * capBitline / 2;
        chargeLatency = horowitz(tauChargeLatency, 0, 1e20, NULL);

        if (config->technology.cell->memCellType == SRAM) {
            /* Codes below calculate the bitline latency */
            double resPullDown = CalculateOnResistance(config->technology.cell->widthSRAMCellNMOS * config->technology.tech->featureSize(), NMOS,
                    config->input.temperature, *config->technology.tech);
            double tau = (resCellAccess + resPullDown) * (capCellAccess + capBitline + bitlineMux->capForPreviousDelayCalculation)
                + resBitline * (bitlineMux->capForPreviousDelayCalculation + capBitline / 2);
            tau *= log(voltagePrecharge / (voltagePrecharge - senseVoltage / 2));	/* one signal raises and the other drops, so senseVoltage/2 is enough */
            double gm = CalculateTransconductance(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), NMOS, *config->technology.tech);
            double beta = 1 / (resPullDown * gm);
            double bitlineRamp = 0;
            bitlineDelay = horowitz(tau, beta, rowDecoder->rampOutput, &bitlineRamp);
            bitlineMux->CalculateLatency(bitlineRamp);
            if (internalSenseAmp) {
                senseAmp->CalculateLatency(bitlineMuxDecoder->rampOutput);
                senseAmpMuxLev1->CalculateLatency(1e20);
                senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
            } else {
                senseAmpMuxLev1->CalculateLatency(bitlineMux->rampOutput);
                senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
            }
            readLatency = decoderLatency + bitlineDelay + bitlineMux->readLatency + senseAmp->readLatency
                + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency;
            /* assume symmetric read/write for SRAM bitline delay */
            writeLatency = readLatency;
        } else if (config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
            double cap = (capCellAccess + config->technology.cell->capDRAMCell) * (capBitline + bitlineMux->capForPreviousDelayCalculation)
                / (capCellAccess + config->technology.cell->capDRAMCell + capBitline + bitlineMux->capForPreviousDelayCalculation);
            double res = resBitline + resCellAccess;
            double tau = 2.3 * res * cap;
            double bitlineRamp = 0;
            bitlineDelay = horowitz(tau, 0, rowDecoder->rampOutput, &bitlineRamp);
            senseAmp->CalculateLatency(bitlineRamp);
            senseAmpMuxLev1->CalculateLatency(1e20);
            senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);

            readLatency = decoderLatency + bitlineDelay + senseAmp->readLatency
                + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency;
            /* assume symmetric read/write for DRAM/eDRAM bitline delay */
            writeLatency = readLatency;
        } else if (config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == memristor || config->technology.cell->memCellType == FBRAM) {
            double bitlineRamp = 0;
            if (config->technology.cell->readMode == false) {	/* current-sensing */
                /* Use ICCAD 2009 model */
                double tau = resBitline * capBitline / 2 * (resMemCellOff + resBitline / 3) / (resMemCellOff + resBitline);
                bitlineDelay = horowitz(tau, 0, rowDecoder->rampOutput, &bitlineRamp);
            } else {						/* voltage-sensing */
                if (config->technology.cell->readVoltage == 0) {  /* Current-in voltage sensing */
                    double tau = resMemCellOn * (capCellAccess + capBitline + bitlineMux->capForPreviousDelayCalculation)
                        + resBitline * (bitlineMux->capForPreviousDelayCalculation + capBitline / 2); /* time constant of LRS */
                    bitlineDelayOn = tau * log((voltagePrecharge - voltageMemCellOn)/(voltagePrecharge - voltageMemCellOn - senseVoltage));  /* BitlineDelay of HRS */
                    tau = resMemCellOff * (capCellAccess + capBitline + bitlineMux->capForPreviousDelayCalculation)
                        + resBitline * (bitlineMux->capForPreviousDelayCalculation + capBitline / 2);  /* time constant of HRS */
                    bitlineDelayOff = tau * log((voltageMemCellOff - voltagePrecharge)/(voltageMemCellOff - voltagePrecharge - senseVoltage));
                    bitlineDelay = MAX(bitlineDelayOn, bitlineDelayOff);
                } else {   /*Voltage-in voltage sensing */
                    double tau = resEquivalentOn * (capCellAccess + capBitline + bitlineMux->capForPreviousDelayCalculation)
                        + resBitline * (bitlineMux->capForPreviousDelayCalculation + capBitline / 2); /* time constant of LRS */
                    bitlineDelayOn = tau * log((voltagePrecharge - voltageMemCellOn)/(voltagePrecharge - voltageMemCellOn - senseVoltage));  /* BitlineDelay of HRS */

                    tau = resEquivalentOff * (capCellAccess + capBitline + bitlineMux->capForPreviousDelayCalculation)
                        + resBitline * (bitlineMux->capForPreviousDelayCalculation + capBitline / 2);  /* time constant of HRS */
                    bitlineDelayOff = tau * log((voltageMemCellOff - voltagePrecharge)/(voltageMemCellOff - voltagePrecharge - senseVoltage));
                    bitlineDelay = MAX(bitlineDelayOn, bitlineDelayOff);
                }
            }
            bitlineMux->CalculateLatency(bitlineRamp);
            if (internalSenseAmp) {
                senseAmp->CalculateLatency(bitlineMuxDecoder->rampOutput);
                senseAmpMuxLev1->CalculateLatency(1e20);
                senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
            } else {
                senseAmpMuxLev1->CalculateLatency(bitlineMux->rampOutput);
                senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
            }
            readLatency = decoderLatency + bitlineDelay + bitlineMux->readLatency + senseAmp->readLatency
                + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency;

            if (config->technology.cell->memCellType == PCRAM) {
                if (config->input.writeScheme == write_and_verify) {
                    /*TODO: write and verify programming */
                } else {
                    writeLatency = MAX(rowDecoder->writeLatency, columnDecoderLatency + chargeLatency);	/* TODO: why not directly use precharger latency? */
                    resetLatency = writeLatency + config->technology.cell->resetPulse;
                    setLatency = writeLatency + config->technology.cell->setPulse;
                    writeLatency += MAX(config->technology.cell->resetPulse, config->technology.cell->setPulse);
                }
            } else if (config->technology.cell->memCellType == FBRAM) {
                writeLatency = MAX(rowDecoder->writeLatency, columnDecoderLatency + chargeLatency);
                resetLatency = writeLatency + config->technology.cell->resetPulse;
                setLatency = writeLatency + config->technology.cell->setPulse;
                writeLatency += MAX(config->technology.cell->resetPulse, config->technology.cell->setPulse);
            } else { //memristor and MRAM
                if (config->technology.cell->accessType == diode_access || config->technology.cell->accessType == none_access) {
                    if (config->input.writeScheme == erase_before_reset || config->input.writeScheme == erase_before_set)
                        writeLatency = MAX(rowDecoder->writeLatency, chargeLatency);
                    else
                        writeLatency = MAX(rowDecoder->writeLatency, columnDecoderLatency + chargeLatency);
                    writeLatency += chargeLatency;
                    writeLatency += config->technology.cell->resetPulse + config->technology.cell->setPulse;
                } else { // CMOS or Bipolar access
                    writeLatency = MAX(rowDecoder->writeLatency, columnDecoderLatency + chargeLatency);
                    resetLatency = writeLatency + config->technology.cell->resetPulse;
                    setLatency = writeLatency + config->technology.cell->setPulse;
                    writeLatency += MAX(config->technology.cell->resetPulse, config->technology.cell->setPulse);
                }
            }
        } else if (config->technology.cell->memCellType == SLCNAND) {
            /* Calculate the NAND flash string length, which is the page count per block plus 2 (two select transistors) */
            int pageCount = config->input.flashBlockSize / config->input.pageSize;
            int stringLength = pageCount + 2;
            /* Codes below calculate the bitline latency */
            double resPullDown = CalculateOnResistance(config->technology.tech->featureSize(), NMOS, config->input.temperature, *config->technology.tech)
                * stringLength;
            double tau = resPullDown * (capCellAccess + capBitline + bitlineMux->capForPreviousDelayCalculation)
                + resBitline * (bitlineMux->capForPreviousDelayCalculation + capBitline / 2);
            /* in one case the bitline is unchanged, and in the other case the bitline drops from 0.6V to 0.4V */
            tau *= log((voltagePrecharge)/ (voltagePrecharge - senseVoltage));
            double gm = CalculateTransconductance(config->technology.tech->featureSize(), NMOS, *config->technology.tech);	/* minimum size transistor */
            double beta = 1 / (resPullDown * gm);
            double bitlineRamp = 0;
            bitlineDelay = horowitz(tau, beta, rowDecoder->rampOutput, &bitlineRamp);
            /* to correct unnecessary horowitz calculation, TODO: need to revisit */
            bitlineDelay = MAX(bitlineDelay, tau * 20);
            bitlineMux->CalculateLatency(bitlineRamp);
            if (internalSenseAmp) {
                senseAmp->CalculateLatency(bitlineMuxDecoder->rampOutput);
                senseAmpMuxLev1->CalculateLatency(1e20);
                senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
            } else {
                senseAmpMuxLev1->CalculateLatency(bitlineMux->rampOutput);
                senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
            }
            readLatency = decoderLatency + bitlineDelay + bitlineMux->readLatency + senseAmp->readLatency
                + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency;
            /* calculate the erase time, a.k.a. reset here */
            resetLatency = MAX(rowDecoder->readLatency, columnDecoderLatency + chargeLatency) + config->technology.cell->flashEraseTime;
            /* calculate the programming time, a.k.a. set here */
            setLatency = MAX(rowDecoder->readLatency, columnDecoderLatency + chargeLatency) + config->technology.cell->flashProgramTime;
            /* use the programming latency as the write latency for SLC NAND*/
            writeLatency = setLatency;
        } else {	/* MLC NAND */
            invalid = true;
            config->logger.Verbose() << "[Subarray] MLC NAND latency modeling is not implemented.";
            return;
        }
    }
}

void SubArray::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[Subarray]");
    } else if (invalid) {
        readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
    } else {
        precharger->CalculatePower();
        rowDecoder->CalculatePower();
        bitlineMuxDecoder->CalculatePower();
        senseAmpMuxLev1Decoder->CalculatePower();
        senseAmpMuxLev2Decoder->CalculatePower();
        bitlineMux->CalculatePower();
        if (internalSenseAmp) {
            senseAmp->CalculatePower();
        }
        senseAmpMuxLev1->CalculatePower();
        senseAmpMuxLev2->CalculatePower();
        double rowDecoderReadEnergy = rowDecoder->readDynamicEnergy;
        double rowDecoderWriteEnergy = rowDecoder->writeDynamicEnergy;
        double rowDecoderSetEnergy = rowDecoder->setDynamicEnergy;
        double rowDecoderResetEnergy = rowDecoder->resetDynamicEnergy;

        if (config->technology.cell->memCellType == SRAM) {
            /* Codes below calculate the SRAM bitline power */
            readDynamicEnergy = (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation)
                * voltagePrecharge * voltagePrecharge * numColumn;
            writeDynamicEnergy = (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation)
                * voltagePrecharge * voltagePrecharge * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
            leakage = CalculateGateLeakage(INV, 1, config->technology.cell->widthSRAMCellNMOS * config->technology.tech->featureSize(),
                    config->technology.cell->widthSRAMCellPMOS * config->technology.tech->featureSize(), config->input.temperature, *config->technology.tech)
                * config->technology.tech->vdd() * 2;	/* two inverters per SRAM cell */
            leakage += CalculateGateLeakage(INV, 1, config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), 0,
                    config->input.temperature, *config->technology.tech) * config->technology.tech->vdd();	/* two accesses NMOS, but combined as one with vdd crossed */
            leakage *= numRow * numColumn;
        } else if (config->technology.cell->memCellType == DRAM || config->technology.cell->memCellType == eDRAM) {
            /* Codes below calculate the DRAM bitline power */
            readDynamicEnergy = (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation) * senseVoltage * config->technology.tech->vdd() * numColumn;
            double writeVoltage = config->technology.cell->resetVoltage;	/* should also equal to setVoltage, for DRAM, it is Vdd */
            writeDynamicEnergy = (capBitline + bitlineMux->capForPreviousPowerCalculation) * writeVoltage * writeVoltage * numColumn;
            leakage = readDynamicEnergy / DRAM_REFRESH_PERIOD * numRow;
        } else if (config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == memristor || config->technology.cell->memCellType == FBRAM) {
            if (config->technology.cell->readMode == false) {	/* current-sensing */
                /* Use ICCAD 2009 model */
                double resBitlineMux = bitlineMux->resNMOSPassTransistor;
                double vpreMin = config->technology.cell->readVoltage * resBitlineMux / (resBitlineMux + resBitline +resMemCellOn);
                double vpreMax = config->technology.cell->readVoltage * (resBitlineMux + resBitline) / (resBitlineMux + resBitline + resMemCellOn);
                readDynamicEnergy = capCellAccess * vpreMax * vpreMax + bitlineMux->capForPreviousPowerCalculation
                    * vpreMin * vpreMin + capBitline * (vpreMax * vpreMax + vpreMin * vpreMin + vpreMax * vpreMin) / 3;
                readDynamicEnergy *= numColumn;
            } else {						/* voltage-sensing */
                readDynamicEnergy = (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation) *
                    (voltagePrecharge * voltagePrecharge - voltageMemCellOn * voltageMemCellOn ) * numColumn;
            }

            if (config->technology.cell->readPower == 0) 
                cellReadEnergy = 2 * config->technology.cell->CalculateReadPower() * senseAmp->readLatency; /* x2 is because of the reference cell */
            else
                cellReadEnergy = 2 * config->technology.cell->readPower * senseAmp->readLatency;
            cellReadEnergy *= numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;

            /* Ignore the dynamic transition during the SET/RESET operation */
            /* Assume that the cell resistance keeps high for worst-case power estimation */
            config->technology.cell->CalculateWriteEnergy();

            double resetEnergyPerBit = config->technology.cell->resetEnergy;
            double setEnergyPerBit = config->technology.cell->setEnergy;
            if (config->technology.cell->setMode)
                setEnergyPerBit += (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation) * config->technology.cell->setVoltage * config->technology.cell->setVoltage;
            else
                setEnergyPerBit += (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation) * config->technology.tech->vdd() * config->technology.tech->vdd();
            if (config->technology.cell->resetMode)
                resetEnergyPerBit += (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation) * config->technology.cell->resetVoltage * config->technology.cell->resetVoltage;
            else
                resetEnergyPerBit += (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation) * config->technology.tech->vdd() * config->technology.tech->vdd();

            if (config->technology.cell->memCellType == PCRAM) { //PCRAM write energy
                if (config->input.writeScheme == write_and_verify) {
                    /*TODO: write and verify programming */
                } else {
                    cellResetEnergy = resetEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                    cellSetEnergy = setEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                    cellResetEnergy /= SHAPER_EFFICIENCY_CONSERVATIVE;
                    cellSetEnergy /= SHAPER_EFFICIENCY_CONSERVATIVE;  /* Due to the shaper inefficiency */
                    writeDynamicEnergy = MAX(cellResetEnergy, cellSetEnergy);
                }
            } else if (config->technology.cell->memCellType == FBRAM){ //FBRAM write energy
                cellResetEnergy = resetEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                cellSetEnergy = setEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                cellResetEnergy /= SHAPER_EFFICIENCY_AGGRESSIVE;
                cellSetEnergy /= SHAPER_EFFICIENCY_AGGRESSIVE;  /* Due to the shaper inefficiency */
                writeDynamicEnergy = MAX(cellResetEnergy, cellSetEnergy);
            } else { //MRAM and memristor write energy
                if (config->technology.cell->accessType == diode_access || config->technology.cell->accessType == none_access) {
                    if (config->input.writeScheme == erase_before_reset || config->input.writeScheme == erase_before_set) {
                        cellResetEnergy = resetEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                        cellSetEnergy = setEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                        writeDynamicEnergy = cellResetEnergy + cellSetEnergy;	/* TODO: bug here, did you consider the write pattern? */
                    } else { /* write scheme = set_before_reset or reset_before_set */
                        cellResetEnergy = resetEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                        cellSetEnergy = setEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                        writeDynamicEnergy = MAX(cellResetEnergy, cellSetEnergy);
                    }
                } else {
                    cellResetEnergy = resetEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                    cellSetEnergy = setEnergyPerBit * numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
                    writeDynamicEnergy = MAX(cellResetEnergy, cellSetEnergy);
                }
                cellResetEnergy /= SHAPER_EFFICIENCY_AGGRESSIVE;
                cellSetEnergy /= SHAPER_EFFICIENCY_AGGRESSIVE;  /* Due to the shaper inefficiency */
                writeDynamicEnergy /= SHAPER_EFFICIENCY_AGGRESSIVE;
            }
            leakage = 0;                       //TODO: cell leaks during read/write operation
        } else if (config->technology.cell->memCellType == SLCNAND) {
            /* Calculate the NAND flash string length, which is the page count per block plus 2 (two select transistors) */
            int pageCount = config->input.flashBlockSize / config->input.pageSize;
            int stringLength = pageCount + 2;

            /* === READ energy === */
            /* only the selected bitline is charged during the read operation, bitline is charged to Vpre */
            readDynamicEnergy = (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation)
                * voltagePrecharge * voltagePrecharge * numColumn;
            /* tricky thing here!
             * In SLC NAND operation, SSL, GSL, and unselected wordlines in a block are charged to Vpass,
             * but the selected wordline is not charged, which is totally different from the other cases.
             */
            rowDecoderResetEnergy = rowDecoderReadEnergy;
            rowDecoderSetEnergy = rowDecoderReadEnergy;
            double actualWordlineReadEnergy = rowDecoderReadEnergy / config->technology.tech->vdd() / config->technology.tech->vdd()
                * config->technology.cell->flashPassVoltage * config->technology.cell->flashPassVoltage;	/* approximate calculate, the wordline is charged to Vpass instead of Vdd */
            actualWordlineReadEnergy = actualWordlineReadEnergy * (numRow / pageCount * stringLength - 1);	/* except the selected wordline itself */
            rowDecoderReadEnergy = actualWordlineReadEnergy;

            /* === Programming (SET) energy === */
            /* first calculate the source line energy (charged to Vdd), which is a part of "bitline" in this scenario */
            setDynamicEnergy = (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation)
                * config->technology.cell->flashProgramVoltage * config->technology.cell->flashProgramVoltage * numColumn;
            /* add tunneling current */
            /* originally it should be multiplied by numColumn/muxSenseAmp/muxOutputLev1/muxOutputLev2,
             * but it is multiplied by numColumn here because all the unselected bitlines also need to precharge to Vdd
             */
            setDynamicEnergy += DELTA_V_TH * TUNNEL_CURRENT_FLOW * config->technology.cell->area
                * config->technology.tech->featureSize() * config->technology.tech->featureSize() * config->technology.cell->flashProgramTime * numColumn;
            /* in programming, the SSL is precharged to Vdd, which is equal to the original value calculated
             * from row decoder
             */
            double actualWordlineSetEnergy = rowDecoderSetEnergy;
            /* however, the unselected wordlines in the same block have to precharge to Vpass */
            actualWordlineSetEnergy += rowDecoderSetEnergy / config->technology.tech->vdd() / config->technology.tech->vdd()
                * config->technology.cell->flashPassVoltage * config->technology.cell->flashPassVoltage * (numRow / pageCount * stringLength - 1);
            /* And the selected wordline is precharged to Vpgm */
            actualWordlineSetEnergy += rowDecoderSetEnergy / config->technology.tech->vdd() / config->technology.tech->vdd()
                * config->technology.cell->flashProgramVoltage * config->technology.cell->flashProgramVoltage;
            rowDecoderSetEnergy = actualWordlineSetEnergy;

            /* === Erase (RESET) energy === */
            /* in erase, all the bitlines (selected or unselected) and the sourceline are precharged to Vera-Vbi */

            resetDynamicEnergy = (capCellAccess + capBitline + bitlineMux->capForPreviousPowerCalculation)
                * (config->technology.cell->flashEraseVoltage - config->technology.tech->buildInPotential()) * (config->technology.cell->flashEraseVoltage - config->technology.tech->buildInPotential());
            resetDynamicEnergy *= (numColumn + 1);	/* plus 1 is due to the source line */
            /* the p-well shared by the selected block is precharged to Vera */
            double wellJunctionCap = config->technology.tech->capJunction() * config->technology.cell->area * config->technology.tech->featureSize() * config->technology.tech->featureSize();
            wellJunctionCap *= config->input.flashBlockSize;	/* one block shares the same well */
            resetDynamicEnergy += wellJunctionCap * config->technology.cell->flashEraseVoltage * config->technology.cell->flashEraseVoltage;
            /* in erase, all the wordlines, SSL, and GSL in unselected block are precharged to Vera * beta
             * in selected block, SSL and GSL are precharged to Vera * beta
             * here beta is fixed at 0.8
             */
            double beta = 0.8;
            double actualWordlineResetEnergy = rowDecoderResetEnergy / config->technology.tech->vdd() / config->technology.tech->vdd()
                * (config->technology.cell->flashEraseVoltage * beta) * (config->technology.cell->flashEraseVoltage * beta);
            actualWordlineResetEnergy *= (numRow / pageCount * stringLength - pageCount);
            rowDecoderResetEnergy = actualWordlineResetEnergy;

            /* let write energy to be the average energy per page*/
            rowDecoderWriteEnergy = (rowDecoderSetEnergy + rowDecoderResetEnergy / pageCount) / 2;
            writeDynamicEnergy = (setDynamicEnergy + resetDynamicEnergy / pageCount) / 2;

            /* Assume NAND flash cell does not consume any leakage */
            leakage = 0;
        } else {	/* MLC NAND */
            invalid = true;
            config->logger.Verbose() << "[Subarray] MLC NAND power modeling is not implemented.";
            return;
        }

        readDynamicEnergy += cellReadEnergy + rowDecoderReadEnergy + bitlineMuxDecoder->readDynamicEnergy + senseAmpMuxLev1Decoder->readDynamicEnergy
            + senseAmpMuxLev2Decoder->readDynamicEnergy + precharger->readDynamicEnergy + bitlineMux->readDynamicEnergy
            + senseAmp->readDynamicEnergy + senseAmpMuxLev1->readDynamicEnergy + senseAmpMuxLev2->readDynamicEnergy;
        writeDynamicEnergy += rowDecoderWriteEnergy + bitlineMuxDecoder->writeDynamicEnergy + senseAmpMuxLev1Decoder->writeDynamicEnergy
            + senseAmpMuxLev2Decoder->writeDynamicEnergy + bitlineMux->writeDynamicEnergy
            + senseAmp->writeDynamicEnergy + senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy;
        /* for assymetric RESET and SET latency calculation only */
        setDynamicEnergy += cellSetEnergy + rowDecoderSetEnergy + bitlineMuxDecoder->writeDynamicEnergy + senseAmpMuxLev1Decoder->writeDynamicEnergy
            + senseAmpMuxLev2Decoder->writeDynamicEnergy + bitlineMux->writeDynamicEnergy
            + senseAmp->writeDynamicEnergy + senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy;
        resetDynamicEnergy += setDynamicEnergy + rowDecoderResetEnergy + bitlineMuxDecoder->writeDynamicEnergy + senseAmpMuxLev1Decoder->writeDynamicEnergy
            + senseAmpMuxLev2Decoder->writeDynamicEnergy + bitlineMux->writeDynamicEnergy
            + senseAmp->writeDynamicEnergy + senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy;

        if (config->technology.cell->accessType == diode_access || config->technology.cell->accessType == none_access) {
            writeDynamicEnergy += bitlineMux->writeDynamicEnergy + senseAmp->writeDynamicEnergy
                + senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy;
        }
        leakage += rowDecoder->leakage + bitlineMuxDecoder->leakage + senseAmpMuxLev1Decoder->leakage
            + senseAmpMuxLev2Decoder->leakage + precharger->leakage + bitlineMux->leakage
            + senseAmp->leakage + senseAmpMuxLev1->leakage + senseAmpMuxLev2->leakage;
    }
}

void SubArray::PrintProperty() {
    std::cout << "Subarray Properties:" << std::endl;
    FunctionUnit::PrintProperty();
    std::cout << "numRow:" << numRow << " numColumn:" << numColumn << std::endl;
    std::cout << "lenWordline * lenBitline = " << lenWordline*1e6 << "um * " << lenBitline*1e6 << "um = " << lenWordline * lenBitline * 1e6 << "mm^2" << std::endl;
    std::cout << "Row Decoder Area:" << rowDecoder->height*1e6 << "um x " << rowDecoder->width*1e6 << "um = " << rowDecoder->area*1e6 << "mm^2" << std::endl;
    std::cout << "Sense Amplifier Area:" << senseAmp->height*1e6 << "um x " << senseAmp->width*1e6 << "um = " << senseAmp->area*1e6 << "mm^2" << std::endl;
    std::cout << "Subarray Area Efficiency = " << lenWordline * lenBitline / area * 100 <<"%" << std::endl;
    std::cout << "bitlineDelay: " << bitlineDelay*1e12 << "ps" << std::endl;
    std::cout << "chargeLatency: " << chargeLatency*1e12 << "ps" << std::endl;
    std::cout << "columnDecoderLatency: " << columnDecoderLatency*1e12 << "ps" << std::endl;
}
