#include "OutputDriver.h"
#include "formula.h"
#include "typedef.h"
#include "macros.h"
#include <math.h>
/*
   OutputDriver::OutputDriver() : FunctionUnit(){
   initialized = false;
   invalid = false;
   }

   OutputDriver::~OutputDriver() {
}
 */
void OutputDriver::Initialize(double _logicEffort, double _inputCap, double _outputCap, double _outputRes,
        bool _inv, BufferDesignTarget _areaOptimizationLevel, double _minDriverCurrent, 
        std::shared_ptr<EvaCamConfig> _config) {

    if (initialized)
        _config->logger.Verbose() << "[Output Driver] Warning: Already initialized!";

    logicEffort = _logicEffort;
    inputCap = _inputCap;
    outputCap = _outputCap;
    outputRes = _outputRes;
    inv = _inv;
    areaOptimizationLevel = _areaOptimizationLevel;
    minDriverCurrent = _minDriverCurrent;
    config = _config;

    double minNMOSDriverWidth = minDriverCurrent / config->tech->currentOnNmos()[config->temperature - 300];
    minNMOSDriverWidth = MAX(MIN_NMOS_SIZE * config->tech->featureSize(), minNMOSDriverWidth);

    if (minNMOSDriverWidth > config->maxNmosSize * config->tech->featureSize()) {
        invalid = true;
        std::cout << "Too large minNMOSDriverWidth" << std::endl;
        return;
    }

    int optimalNumStage;

    if (areaOptimizationLevel == latency_first) {
        double F = MAX(1, logicEffort * outputCap / inputCap);	/* Total logic effort */
        optimalNumStage = MAX(0, (int)(log(F) / log(OPT_F) + 0.5) - 1);

        if ((optimalNumStage % 2) ^ inv)	/* If odd, add 1 */
            optimalNumStage += 1;

        if (optimalNumStage > MAX_INV_CHAIN_LEN) {/* Exceed maximum stages */
            if (WARNING)
                        config->logger.Verbose() << "[WARNING] Exceed maximum inverter chain length!";
            optimalNumStage = MAX_INV_CHAIN_LEN;
        }

        numStage = optimalNumStage;

        double f = pow(F, 1.0 / (optimalNumStage + 1));	/* Logic effort per stage */
        double inputCapLast = outputCap / f;

        widthNMOS[optimalNumStage-1] = MAX(MIN_NMOS_SIZE * config->tech->featureSize(),
                inputCapLast / CalculateGateCap(1/*meter*/, config->tech) / (1.0 + config->tech->pnSizeRatio()));

        if (widthNMOS[optimalNumStage-1] > config->maxNmosSize * config->tech->featureSize()) {
            if (WARNING)
                std::cout << "[WARNING] Exceed maximum NMOS size!" << std::endl;
            widthNMOS[optimalNumStage-1] = config->maxNmosSize * config->tech->featureSize();
            /* re-Calculate the logic effort */
            double capLastStage = CalculateGateCap((1 + config->tech->pnSizeRatio()) * config->maxNmosSize * config->tech->featureSize(), config->tech);
            F = logicEffort * capLastStage / inputCap;
            f =	pow(F, 1.0 / (optimalNumStage));
        }

        if (widthNMOS[optimalNumStage-1] < minNMOSDriverWidth) {
            /* the last level Inv can not provide minimum current so that the Inv chain can't only decided by Logic Effort */
            areaOptimizationLevel = latency_area_trade_off;
        } else {
            widthPMOS[optimalNumStage-1] = widthNMOS[optimalNumStage-1] * config->tech->pnSizeRatio();

            for (int i = optimalNumStage-2; i >= 0; i--) {
                widthNMOS[i] = widthNMOS[i+1] / f;
                if (widthNMOS[i] < MIN_NMOS_SIZE * config->tech->featureSize()) {
                    if (WARNING)
                        config->logger.Verbose() << "[WARNING] Exceed minimum NMOS size!";
                    widthNMOS[i] = MIN_NMOS_SIZE * config->tech->featureSize();
                }
                widthPMOS[i] = widthNMOS[i] * config->tech->pnSizeRatio();
            }
        }
    }

    if (areaOptimizationLevel == latency_area_trade_off){
        double newOutputCap = CalculateGateCap(minNMOSDriverWidth, config->tech) * (1.0 + config->tech->pnSizeRatio());
        double F = MAX(1, logicEffort * newOutputCap / inputCap);	/* Total logic effort */
        optimalNumStage = MAX(0, (int)(log(F) / log(OPT_F) + 0.5) - 1);

        if (!((optimalNumStage % 2) ^ inv))	/* If even, add 1 */
            optimalNumStage += 1;

        if (optimalNumStage > MAX_INV_CHAIN_LEN) {/* Exceed maximum stages */
            if (WARNING)
                std::cout << "[WARNING] Exceed maximum inverter chain length!" << std::endl;
            optimalNumStage = MAX_INV_CHAIN_LEN;
        }

        numStage = optimalNumStage + 1;

        widthNMOS[optimalNumStage] = minNMOSDriverWidth;
        widthPMOS[optimalNumStage] = widthNMOS[optimalNumStage] * config->tech->pnSizeRatio();

        double f = pow(F, 1.0 / (optimalNumStage + 1));	/* Logic effort per stage */

        for (int i = optimalNumStage - 1; i >= 0; i--) {
            widthNMOS[i] = widthNMOS[i+1] / f;
            if (widthNMOS[i] < MIN_NMOS_SIZE * config->tech->featureSize()) {
                if (WARNING)
                    std::cout << "[WARNING] Exceed minimum NMOS size!" << std::endl;
                widthNMOS[i] = MIN_NMOS_SIZE * config->tech->featureSize();
            }
            widthPMOS[i] = widthNMOS[i] * config->tech->pnSizeRatio();
        }
    } else if (areaOptimizationLevel == area_first) {
        optimalNumStage = 1;
        numStage = 1;
        widthNMOS[optimalNumStage - 1] = MAX(MIN_NMOS_SIZE * config->tech->featureSize(), minNMOSDriverWidth);
        if (widthNMOS[optimalNumStage - 1] > AREA_OPT_CONSTRAIN * config->maxNmosSize * config->tech->featureSize()) {
            invalid = true;
            std::cout << "invalid widthNMOS" << std::endl;
            return;
        }
        widthPMOS[optimalNumStage - 1] = widthNMOS[optimalNumStage - 1] * config->tech->pnSizeRatio();
    } else {
        //TODO: Added this else to prevent unitialized value errors, actually calculate things
        //for now picked to ingore and hard code if it doesn't exist
        //widthNMOS[0] = config->tech->featureSize();
    }

    /* Restore the original buffer design style */
    areaOptimizationLevel = _areaOptimizationLevel;

    initialized = true;
    CalculateRC();
    CalculateArea();
    CalculatePower();
}

void OutputDriver::CalculateArea() {
    if (!initialized) {
        std::cout << "[Output Driver] Error: Require initialization first!" << std::endl;
    } else if (invalid) {
        height = width = area = 1e41;
    } else {
        double totalHeight = 0;
        double totalWidth = 0;
        double h, w;
        for (int i = 0; i < numStage; i++) {
            CalculateGateArea(INV, 1, widthNMOS[i], widthPMOS[i], config->tech->featureSize()*40, config->tech, &h, &w, config->UseUpdatedLib);
            totalHeight = MAX(totalHeight, h);
            totalWidth += w;
        }
        height = totalHeight;
        width = totalWidth;
        area = height * width;
    }
}

void OutputDriver::CalculateRC() {
    if (!initialized) {
        std::cout << "[Output Driver] Error: Require initialization first!" << std::endl;
    } else if (invalid) {
        ;  // nothing to do if invalid
    } else if (numStage == 0) {
        capInput[0] = 0;
        capOutput[0] = 0;
    } else {
        for (int i = 0; i < numStage; i++) {
            CalculateGateCapacitance(INV, 1, widthNMOS[i], widthPMOS[i], config->tech->featureSize() * MAX_TRANSISTOR_HEIGHT, config->tech, &(capInput[i]), &(capOutput[i]));
        }
    }
}

void OutputDriver::CalculateLatency(double _rampInput) {
    if (!initialized) {
        // TODO: Decide whether these are runtime errors or warnings
        std::cout << "[Output Driver] Error: Require initialization first!" << std::endl;
    } else if (invalid) {
        readLatency = writeLatency = 1e41;
    } else {
        rampInput = _rampInput;
        double resPullDown;
        double capLoad;
        double tr;	/* time constant */
        double gm;	/* transconductance */
        double beta;	/* for horowitz calculation */
        double temp;
        readLatency = 0;

        //std::cout << "numStage = " << numStage << std::endl;

        for (int i = 0; i < numStage - 1; i++) {
            resPullDown = CalculateOnResistance(widthNMOS[i], NMOS, config->temperature, config->tech);
            capLoad = capOutput[i] + capInput[i+1];
            tr = resPullDown * capLoad;
            gm = CalculateTransconductance(widthNMOS[i], NMOS, config->tech);
            beta = 1 / (resPullDown * gm);
            readLatency += horowitz(tr, beta, rampInput, &temp);
            rampInput = temp;	/* for next stage */
        }
        /* Last level inverter */

        //int tmpNumStage = numStage ? numStage > 0 : 1;
        if (numStage != 0) {
            resPullDown = CalculateOnResistance(widthNMOS[/*tmpN*/numStage-1], NMOS, config->temperature, config->tech);
            capLoad = capOutput[/*tmpN*/numStage-1] + outputCap;

            //std::cout << capLoad << std::endl;

            //std::cout << resPullDown << std::endl;
            //std::cout << outputCap << std::endl;
            //std::cout << outputRes << std::endl;

            tr = resPullDown * capLoad + outputCap * outputRes / 2;
            gm = CalculateTransconductance(widthNMOS[/*tmpN*/numStage-1], NMOS, config->tech);
            beta = 1 / (resPullDown * gm);
            readLatency += horowitz(tr, beta, rampInput, &rampOutput);
            rampInput = _rampInput;

            //std::cout << "OD rampOutput " << rampOutput << std::endl;
        }

        writeLatency = readLatency;
    }
}

void OutputDriver::CalculatePower() {
    if (!initialized) {
        std::cout << "[Output Driver] Error: Require initialization first!" << std::endl;
    } else if (invalid) {
        readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
    } else {
        /* Leakage power */
        leakage = 0;

        for (int i = 0; i < numStage; i++) {
            leakage += CalculateGateLeakage(INV, 1, widthNMOS[i], widthPMOS[i], 
                    config->temperature, config->tech) * config->tech->vdd();
        }
        /* Dynamic energy */
        readDynamicEnergy = 0;
        double capLoad;
        for (int i = 0; i < numStage - 1; i++) {
            capLoad = capOutput[i] + capInput[i+1];
            readDynamicEnergy += capLoad * config->tech->vdd() * config->tech->vdd();
        }

        int tmpNumStage = numStage ? numStage > 0 : 1;

        capLoad = capOutput[tmpNumStage-1] + outputCap;	/* outputCap here means the final load capacitance */
        readDynamicEnergy += capLoad * config->tech->vdd() * config->tech->vdd();
        writeDynamicEnergy = readDynamicEnergy;
    }
}

void OutputDriver::PrintProperty() {
    std::cout << "Output Driver Properties:" << std::endl;
    FunctionUnit::PrintProperty();
    std::cout << "Number of inverter stage: " << numStage << std::endl;
}

OutputDriver & OutputDriver::operator=(const OutputDriver &rhs) {
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
    logicEffort = rhs.logicEffort;
    inputCap = rhs.inputCap;
    outputCap = rhs.outputCap;
    outputRes = rhs.outputRes;
    inv = rhs.inv;
    numStage = rhs.numStage;
    areaOptimizationLevel = rhs.areaOptimizationLevel;
    minDriverCurrent = rhs.minDriverCurrent;
    rampInput = rhs.rampInput;
    rampOutput = rhs.rampOutput;

    return *this;
}
