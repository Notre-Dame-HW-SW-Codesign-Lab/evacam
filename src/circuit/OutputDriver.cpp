#include "OutputDriver.h"
#include "formula.h"
#include "typedef.h"
#include <math.h>

void OutputDriver::Initialize(double _logicEffort, double _inputCap, double _outputCap,
        double _outputRes, bool _inv, BufferDesignTarget _areaOptimizationLevel,
        double _minDriverCurrent, std::shared_ptr<EvaCamConfig> _config) {

    if (initialized)
        _config->logger.Verbose() << "[Output Driver] Warning: Already initialized!";

    logicEffort = _logicEffort;
    inputCap = _inputCap;
    outputCap = _outputCap;
    outputRes = _outputRes;
    inv = _inv;
    areaOptimizationLevel = _areaOptimizationLevel;
    BufferDesignTarget effectiveDesignTarget = areaOptimizationLevel;
    minDriverCurrent = _minDriverCurrent;
    config = _config;

    double minNMOSDriverWidth = minDriverCurrent
            / config->technology.tech->currentOnNmos()[config->input.temperature - 300];
    minNMOSDriverWidth = std::max(MIN_NMOS_SIZE * config->technology.tech->featureSize(),
            minNMOSDriverWidth);

    if (minNMOSDriverWidth > config->input.maxNmosSize * config->technology.tech->featureSize()) {
        invalid = true;
        initialized = true;
        config->logger.Verbose()
                << "[Output Driver] Warning: minNMOSDriverWidth exceeds maximum NMOS size.";
        return;
    }

    int optimalNumStage;

    if (areaOptimizationLevel == latency_first) {
        double F = std::max(1.0, logicEffort * outputCap / inputCap);	/* Total logic effort */
        optimalNumStage = std::max(0, (int)(log(F) / log(OPT_F) + 0.5) - 1);

        if ((optimalNumStage % 2) ^ inv)	/* If odd, add 1 */
            optimalNumStage += 1;

        if (optimalNumStage > MAX_INV_CHAIN_LEN) {/* Exceed maximum stages */
            config->logger.Verbose()
                    << "[Output Driver] Warning: Exceed maximum inverter chain length!";
            optimalNumStage = MAX_INV_CHAIN_LEN;
        }

        numStage = optimalNumStage;

        /* A non-inverting latency-first driver can choose zero stages even when
         * minDriverCurrent is nonzero. Confirm whether the current requirement
         * should force a real final driver stage before changing this behavior. */
        if (numStage > 0) {
            double f = pow(F, 1.0 / (optimalNumStage + 1));	/* Logic effort per stage */
            double inputCapLast = outputCap / f;

            widthNMOS[optimalNumStage-1] = std::max(
                    MIN_NMOS_SIZE * config->technology.tech->featureSize(),
                    inputCapLast / CalculateGateCap(1/*meter*/, *config->technology.tech)
                            / (1.0 + config->technology.tech->pnSizeRatio()));

            if (widthNMOS[optimalNumStage-1]
                    > config->input.maxNmosSize * config->technology.tech->featureSize()) {
                config->logger.Verbose()
                        << "[Output Driver] Warning: Exceed maximum NMOS size!";
                widthNMOS[optimalNumStage-1] =
                        config->input.maxNmosSize * config->technology.tech->featureSize();
                /* re-Calculate the logic effort */
                double capLastStage = CalculateGateCap(
                        (1 + config->technology.tech->pnSizeRatio())
                                * config->input.maxNmosSize
                                * config->technology.tech->featureSize(),
                        *config->technology.tech);
                F = logicEffort * capLastStage / inputCap;
                f =	pow(F, 1.0 / (optimalNumStage));
            }

            if (widthNMOS[optimalNumStage-1] < minNMOSDriverWidth) {
                /* The last Inv cannot provide minimum current, so logic effort
                 * alone cannot determine the chain. */
                effectiveDesignTarget = latency_area_trade_off;
            } else {
                widthPMOS[optimalNumStage-1] = widthNMOS[optimalNumStage-1]
                        * config->technology.tech->pnSizeRatio();

                for (int i = optimalNumStage-2; i >= 0; i--) {
                    widthNMOS[i] = widthNMOS[i+1] / f;
                    if (widthNMOS[i] < MIN_NMOS_SIZE * config->technology.tech->featureSize()) {
                        config->logger.Verbose()
                                << "[Output Driver] Warning: Exceed minimum NMOS size!";
                        widthNMOS[i] = MIN_NMOS_SIZE * config->technology.tech->featureSize();
                    }
                    widthPMOS[i] = widthNMOS[i] * config->technology.tech->pnSizeRatio();
                }
            }
        }
    }

    if (effectiveDesignTarget == latency_area_trade_off) {
        double newOutputCap = CalculateGateCap(minNMOSDriverWidth, *config->technology.tech)
                * (1.0 + config->technology.tech->pnSizeRatio());
        double F = std::max(1.0, logicEffort * newOutputCap / inputCap);	/* Total logic effort */
        optimalNumStage = std::max(0, (int)(log(F) / log(OPT_F) + 0.5) - 1);

        if (!((optimalNumStage % 2) ^ inv))	/* If even, add 1 */
            optimalNumStage += 1;

        if (optimalNumStage >= MAX_INV_CHAIN_LEN) {/* Exceed maximum stages */
            config->logger.Verbose()
                    << "[Output Driver] Warning: Exceed maximum inverter chain length!";
            optimalNumStage = MAX_INV_CHAIN_LEN - 1;
        }

        numStage = optimalNumStage + 1;

        widthNMOS[optimalNumStage] = minNMOSDriverWidth;
        widthPMOS[optimalNumStage] = widthNMOS[optimalNumStage]
                * config->technology.tech->pnSizeRatio();

        double f = pow(F, 1.0 / (optimalNumStage + 1));	/* Logic effort per stage */

        for (int i = optimalNumStage - 1; i >= 0; i--) {
            widthNMOS[i] = widthNMOS[i+1] / f;
            if (widthNMOS[i] < MIN_NMOS_SIZE * config->technology.tech->featureSize()) {
                config->logger.Verbose()
                        << "[Output Driver] Warning: Exceed minimum NMOS size!";
                widthNMOS[i] = MIN_NMOS_SIZE * config->technology.tech->featureSize();
            }
            widthPMOS[i] = widthNMOS[i] * config->technology.tech->pnSizeRatio();
        }
    } else if (effectiveDesignTarget == area_first) {
        optimalNumStage = 1;
        numStage = 1;
        widthNMOS[optimalNumStage - 1] = std::max(
                MIN_NMOS_SIZE * config->technology.tech->featureSize(),
                minNMOSDriverWidth);
        if (widthNMOS[optimalNumStage - 1]
                > AREA_OPT_CONSTRAIN * config->input.maxNmosSize
                        * config->technology.tech->featureSize()) {
            invalid = true;
            initialized = true;
            config->logger.Verbose()
                    << "[Output Driver] Warning: widthNMOS exceeds area optimization constraint.";
            return;
        }
        widthPMOS[optimalNumStage - 1] = widthNMOS[optimalNumStage - 1]
                * config->technology.tech->pnSizeRatio();
    }

    initialized = true;
    CalculateRC();
    CalculateArea();
}

void OutputDriver::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[Output Driver]");
    } else if (invalid) {
        height = width = area = 1e41;
    } else {
        double totalHeight = 0;
        double totalWidth = 0;
        double h, w;
        for (int i = 0; i < numStage; i++) {
            CalculateGateArea(INV, 1, widthNMOS[i], widthPMOS[i],
                    config->technology.tech->featureSize()*40, *config->technology.tech,
                    &h, &w, config->peripherals.useUpdatedLib);
            totalHeight = std::max(totalHeight, h);
            totalWidth += w;
        }
        height = totalHeight;
        width = totalWidth;
        area = height * width;
    }
}

void OutputDriver::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[Output Driver]");
    } else if (invalid) {
        return;  // nothing to do if invalid
    } else if (numStage == 0) {
        capInput[0] = 0;
        capOutput[0] = 0;
    } else {
        for (int i = 0; i < numStage; i++) {
            CalculateGateCapacitance(INV, 1, widthNMOS[i], widthPMOS[i],
                    config->technology.tech->featureSize() * MAX_TRANSISTOR_HEIGHT,
                    *config->technology.tech, &(capInput[i]), &(capOutput[i]));
        }
    }
}

void OutputDriver::CalculateLatency(double _rampInput) {
    if (!initialized) {
        ThrowInitializationError("[Output Driver]");
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

        for (int i = 0; i < numStage - 1; i++) {
            resPullDown = CalculateOnResistance(widthNMOS[i], NMOS,
                    config->input.temperature, *config->technology.tech);
            capLoad = capOutput[i] + capInput[i+1];
            tr = resPullDown * capLoad;
            gm = CalculateTransconductance(widthNMOS[i], NMOS, *config->technology.tech);
            beta = 1 / (resPullDown * gm);
            readLatency += horowitz(tr, beta, rampInput, &temp);
            rampInput = temp;	/* for next stage */
        }
        /* Last level inverter */

        if (numStage != 0) {
            resPullDown = CalculateOnResistance(widthNMOS[numStage-1], NMOS,
                    config->input.temperature, *config->technology.tech);
            capLoad = capOutput[numStage-1] + outputCap;

            tr = resPullDown * capLoad + outputCap * outputRes / 2;
            gm = CalculateTransconductance(widthNMOS[numStage-1], NMOS, *config->technology.tech);
            beta = 1 / (resPullDown * gm);
            readLatency += horowitz(tr, beta, rampInput, &rampOutput);
            rampInput = _rampInput;
        }

        writeLatency = readLatency;
    }
}

void OutputDriver::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[Output Driver]");
    } else if (invalid) {
        readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
    } else {
        /* Leakage power */
        leakage = 0;

        for (int i = 0; i < numStage; i++) {
            leakage += CalculateGateLeakage(INV, 1, widthNMOS[i], widthPMOS[i], 
                    config->input.temperature, *config->technology.tech)
                    * config->technology.tech->vdd();
        }
        /* Dynamic energy */
        readDynamicEnergy = 0;
        double capLoad;
        for (int i = 0; i < numStage - 1; i++) {
            capLoad = capOutput[i] + capInput[i+1];
            readDynamicEnergy += capLoad * config->technology.tech->vdd()
                    * config->technology.tech->vdd();
        }

        if (numStage > 0) {
            capLoad = capOutput[numStage - 1] + outputCap;
        } else {
            capLoad = outputCap;
        }
        readDynamicEnergy += capLoad * config->technology.tech->vdd()
                * config->technology.tech->vdd();
        writeDynamicEnergy = readDynamicEnergy;
    }
}

void OutputDriver::PrintProperty() {
    std::cout << "Output Driver Properties:" << std::endl;
    FunctionUnit::PrintProperty();
    std::cout << "Number of inverter stage: " << numStage << std::endl;
}
