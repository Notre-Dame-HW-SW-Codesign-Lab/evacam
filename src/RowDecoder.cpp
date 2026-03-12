#include "../include/RowDecoder.h"
#include "../include/formula.h"
void RowDecoder::Initialize(int _numRow, double _capLoad, double _resLoad,
        bool _multipleRowPerSet, BufferDesignTarget _areaOptimizationLevel, double _minDriverCurrent,
        std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[Row Decoder] Warning: Already initialized!";

    outputDriver = std::make_shared<OutputDriver>();
    numRow = _numRow;
    capLoad = _capLoad;
    resLoad = _resLoad;
    multipleRowPerSet = _multipleRowPerSet;
    areaOptimizationLevel = _areaOptimizationLevel;
    minDriverCurrent = _minDriverCurrent;
    config = _config;

    if (numRow <= 8) {	/* The predecoder output is used directly */
        if (multipleRowPerSet)
            numNandInput = 2;	/* NAND way-select with predecoder output */
        else
            numNandInput = 0;	/* no circuit needed */
    } else {
        if (multipleRowPerSet)
            numNandInput = 3;	/* NAND way-select with two predecoder outputs */
        else
            numNandInput = 2;	/* just NAND two predecoder outputs */
    }

    if (numNandInput > 0) {
        double logicEffortNand;
        double capNand;
        if (numNandInput == 2) {	/* NAND2 */
            widthNandN = 2 * MIN_NMOS_SIZE * config->tech->featureSize;
            logicEffortNand = (2+config->tech->pnSizeRatio) / (1+config->tech->pnSizeRatio);
        } else {					/* NAND3 */
            widthNandN = 3 * MIN_NMOS_SIZE * config->tech->featureSize;
            logicEffortNand = (3+config->tech->pnSizeRatio) / (1+config->tech->pnSizeRatio);
        }
        widthNandP = config->tech->pnSizeRatio * MIN_NMOS_SIZE * config->tech->featureSize;
        capNand = CalculateGateCap(widthNandN, config->tech) + CalculateGateCap(widthNandP, config->tech);
        outputDriver->Initialize(logicEffortNand, capNand, capLoad, resLoad, true, 
                areaOptimizationLevel, minDriverCurrent, config);
    } else {
        /* we only need an 1-level output buffer to driver the wordline */
        double capInv;
        widthNandN = MIN_NMOS_SIZE * config->tech->featureSize;
        widthNandP = config->tech->pnSizeRatio * MIN_NMOS_SIZE * config->tech->featureSize;
        capInv = CalculateGateCap(widthNandN, config->tech) + CalculateGateCap(widthNandP, config->tech);
        outputDriver->Initialize(1, capInv, capLoad, resLoad, true, areaOptimizationLevel, 
                minDriverCurrent, config);
    }

    if (outputDriver->invalid) {
        invalid = true;
        std::cout << "invalid outputDriver" << std::endl;
        return;
    }

    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void RowDecoder::CalculateArea() {
    if (!initialized) {
        std::cout << "[Row Decoder Area] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculateArea();
        if (numNandInput == 0) {	/* no circuit needed, use predecoder outputs directly */
            height = outputDriver->height;
            width = outputDriver->width;
        } else {
            double hNand, wNand;
            CalculateGateArea(NAND, numNandInput, widthNandN, widthNandP, config->tech->featureSize*40, config->tech, 
                    &hNand, &wNand, config->UseUpdatedLib);
            height = MAX(hNand, outputDriver->height);
            width = wNand + outputDriver->width;
        }
        height *= numRow;
        area = height * width;
    }
}

void RowDecoder::CalculateRC() {
    if (!initialized) {
        std::cout << "[Row Decoder RC] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculateRC();
        if (numNandInput == 0) {	/* no circuit needed, use predecoder outputs directly */
            capNandInput = capNandOutput = 0;
        } else {
            CalculateGateCapacitance(NAND, numNandInput, widthNandN, widthNandP, 
                    config->tech->featureSize * MAX_TRANSISTOR_HEIGHT, config->tech, &capNandInput, &capNandOutput);
        }
    }
}

void RowDecoder::CalculateLatency(double _rampInput) {
    if (!initialized) {
        std::cout << "[Row Decoder Latency] Error: Require initialization first!" << std::endl;
    } else {
        if (numNandInput == 0) {	/* no circuit needed, use predecoder outputs directly */
            outputDriver->CalculateLatency(_rampInput);
            readLatency = writeLatency = outputDriver->readLatency;
            rampOutput = outputDriver->rampOutput;
        } else {
            rampInput = _rampInput;

            double resPullDown;
            double capLoad;
            double tr;	/* time constant */
            double gm;	/* transconductance */
            double beta;	/* for horowitz calculation */
            double rampInputForDriver;

            resPullDown = CalculateOnResistance(widthNandN, NMOS, config->temperature, config->tech) * numNandInput;
            capLoad = capNandOutput + outputDriver->capInput[0];
            tr = resPullDown * capLoad;
            gm = CalculateTransconductance(widthNandN, NMOS, config->tech);
            beta = 1 / (resPullDown * gm);
            readLatency = horowitz(tr, beta, rampInput, &rampInputForDriver);

            //std::cout << "rampInputForDriver = " << rampInputForDriver << std::endl;

            outputDriver->CalculateLatency(rampInputForDriver);
            readLatency += outputDriver->readLatency;
            writeLatency = readLatency;

            rampOutput = outputDriver->rampOutput;
            //rampOutput = 0;

            //std::cout << "OD RO = " << outputDriver->rampOutput << std::endl;

            //std::cout << "rampOutput = " << rampOutput << std::endl;
        }
    }
}

void RowDecoder::CalculatePower() {
    if (!initialized) {
        std::cout << "[Row Decoder Power] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculatePower();
        leakage = outputDriver->leakage;
        if (numNandInput == 0) {	/* no circuit needed, use predecoder outputs directly */
            readDynamicEnergy = writeDynamicEnergy = outputDriver->readDynamicEnergy;
        } else {
            /* Leakage power */
            leakage += CalculateGateLeakage(NAND, numNandInput, widthNandN, widthNandP,
                    config->temperature, config->tech) * config->tech->vdd;
            /* Dynamic energy */
            double capLoad = capNandOutput + outputDriver->capInput[0];
            readDynamicEnergy = capLoad * config->tech->vdd * config->tech->vdd;
            readDynamicEnergy += outputDriver->readDynamicEnergy;
            readDynamicEnergy *= 1;	/* only one row is activated each time */
            writeDynamicEnergy = readDynamicEnergy;
        }
        leakage *= numRow;
    }
}

void RowDecoder::PrintProperty() {
    std::cout << "Row Decoder Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

RowDecoder & RowDecoder::operator=(const RowDecoder &rhs) {
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
    outputDriver = rhs.outputDriver;
    numRow = rhs.numRow;
    multipleRowPerSet = rhs.multipleRowPerSet;
    numNandInput = rhs.numNandInput;
    capLoad = rhs.capLoad;
    resLoad = rhs.resLoad;
    areaOptimizationLevel = rhs.areaOptimizationLevel;
    minDriverCurrent = rhs.minDriverCurrent;

    widthNandN = rhs.widthNandN;
    widthNandP = rhs.widthNandP;
    capNandInput = rhs.capNandInput;
    capNandOutput = rhs.capNandOutput;
    rampInput = rhs.rampInput;
    rampOutput = rhs.rampOutput;

    return *this;
}
