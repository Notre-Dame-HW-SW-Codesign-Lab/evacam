#include "Wire.h"
#include "formula.h"
#include "SenseAmp.h"
/*
   Wire::Wire() {
initialized = false;
senseAmp = NULL;
}

Wire::~Wire() {
// smart pointers make this delete no longer needed
//if (senseAmp)
//delete senseAmp;
}
 */
void Wire::Initialize(int _featureSizeInNano, WireType _wireType, WireRepeaterType _wireRepeaterType,
        int _temperature, bool _isLowSwing, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized) {
        /* reload the new input, clear the previous setting */
        initialized = false;
    }

    featureSizeInNano = _featureSizeInNano;
    featureSize = _featureSizeInNano * 1e-9;
    wireType = _wireType;
    wireRepeaterType = _wireRepeaterType;
    temperature = _temperature;
    isLowSwing = _isLowSwing;
    config = _config;

    if (wireRepeaterType != repeated_none && isLowSwing) {
        throw std::runtime_error("[Wire] Error: low swing is not supported for repeated wires.");
    }

    copper_resistivity = COPPER_RESISTIVITY;
    /* Initialize copper resistivity */

    if (_featureSizeInNano <= 22) {
        featureSize = 22e-9;
        switch (wireType) {
            case local_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.55;
                wirePitch = 2 * featureSize;
                aspectRatio = 1.9;
                ildThickness = aspectRatio * featureSize;
                copper_resistivity =  6.0e-8;
                break;
            case local_conservative:
                barrierThickness = 0.0021e-6;
                horizontalDielectric = 3;
                wirePitch = 2 * featureSize;
                aspectRatio = 1.9;
                ildThickness = aspectRatio * featureSize;
                copper_resistivity =  6.0e-8;
                break;
            case semi_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.55;
                wirePitch = 4 * featureSize;
                aspectRatio = 1.9;
                ildThickness = 2 * aspectRatio * featureSize;
                copper_resistivity =  6.0e-8;
                break;
            case semi_conservative:
                barrierThickness = 0.0021e-6;
                horizontalDielectric = 3;
                wirePitch = 4 * featureSize;
                aspectRatio = 1.9;
                ildThickness = 2 * aspectRatio * featureSize;
                copper_resistivity =  6.0e-8;
                break;
            case global_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.55;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.34;
                ildThickness = 0.42e-6 * 22 / 32;
                copper_resistivity =  3.0e-8; /* TODO confusing mem_data in ITRS */
                break;
            case global_conservative:
                barrierThickness = 0.0063e-6; /* TODO No mem_data in ITRS */
                horizontalDielectric = 3;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.34;
                ildThickness = 0.385e-6 * 22 / 32;
                copper_resistivity =  3.0e-8; /* TODO confusing mem_data in ITRS */
                break;
            default:	/* dram_wordline */
                /* TODO CACTI does not have a detailed model for it */
                barrierThickness = 0e-6;
                horizontalDielectric = 0;
                wirePitch = 2 * featureSize;
                aspectRatio = 0;
                ildThickness = 0e-6;
        }
    } else if (_featureSizeInNano <= 32) {
        featureSize = 32e-9;
        switch (wireType) {
            case local_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.82;
                wirePitch = 2 * featureSize;
                aspectRatio = 1.8;
                ildThickness = aspectRatio * featureSize;
                copper_resistivity =  5.0e-8;
                break;
            case local_conservative:
                barrierThickness = 0.0026e-6;
                horizontalDielectric = 3.16;
                wirePitch = 2 * featureSize;
                aspectRatio = 1.8;
                ildThickness = aspectRatio * featureSize;
                copper_resistivity =  5.0e-8;
                break;
            case semi_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.82;
                wirePitch = 4 * featureSize;
                aspectRatio = 1.9;
                ildThickness = 2 * aspectRatio * featureSize;
                copper_resistivity =  5.0e-8;
                break;
            case semi_conservative:
                barrierThickness = 0.0026e-6;
                horizontalDielectric = 3.16;
                wirePitch = 4 * featureSize;
                aspectRatio = 1.9;
                ildThickness = 2 * aspectRatio * featureSize;
                copper_resistivity =  5.0e-8;
                break;
            case global_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.82;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.34;
                ildThickness = 0.42e-6;
                copper_resistivity =  2.5e-8; /* TODO confusing mem_data in ITRS */
                break;
            case global_conservative:
                barrierThickness = 0.0078e-6; /* TODO No mem_data in ITRS */
                horizontalDielectric = 3.16;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.34;
                ildThickness = 0.385e-6;
                copper_resistivity =  2.5e-8; /* TODO confusing mem_data in ITRS */
                break;
            default:	/* dram_wordline */
                /* TODO CACTI does not have a detailed model for it */
                barrierThickness = 0e-6;
                horizontalDielectric = 0;
                wirePitch = 2 * featureSize;
                aspectRatio = 0;
                ildThickness = 0e-6;
        }
    } else if (_featureSizeInNano <= 45) {
        featureSize = 45e-9;
        switch (wireType) {
            case local_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.6;
                wirePitch = 0.102e-6;
                aspectRatio = 1.8;
                ildThickness = 0.0918e-6;
                copper_resistivity =  4.08e-8;
                break;
            case local_conservative:
                barrierThickness = 0.0033e-6;
                horizontalDielectric = 2.9;
                wirePitch = 0.102e-6;
                aspectRatio = 1.8;
                ildThickness = 0.0918e-6;
                copper_resistivity =  4.08e-8;
                break;
            case semi_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.6;
                wirePitch = 4 * featureSize;
                aspectRatio = 1.8;
                ildThickness = 2 * aspectRatio * featureSize;
                copper_resistivity =  4.08e-8;
                break;
            case semi_conservative:
                barrierThickness = 0.0033e-6;
                horizontalDielectric = 2.9;
                wirePitch = 4 * featureSize;
                aspectRatio = 1.8;
                ildThickness = 2 * aspectRatio * featureSize;
                copper_resistivity =  4.08e-8;
                break;
            case global_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.6;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.34;
                ildThickness = 0.63e-6;
                copper_resistivity =  2.06e-8;
                break;
            case global_conservative:
                barrierThickness = 0.01e-6;
                horizontalDielectric = 2.9;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.34;
                ildThickness = 0.55e-6;
                copper_resistivity =  2.06e-8;
                break;
            default:	/* dram_wordline */
                /* TODO CACTI does not have a detailed model for it */
                barrierThickness = 0e-6;
                horizontalDielectric = 0;
                wirePitch = 2 * featureSize;
                aspectRatio = 0;
                ildThickness = 0e-6;
        }
    } else if (_featureSizeInNano <= 65) {
        featureSize = 65e-9;
        switch (wireType) {
            case local_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.303;
                wirePitch = 2.5* featureSize;
                aspectRatio = 2.7;
                ildThickness = 0.405e-6;
                break;
            case local_conservative:
                barrierThickness = 0.006e-6;
                horizontalDielectric = 2.734;
                wirePitch = 2.5* featureSize;
                aspectRatio = 2.0;
                ildThickness = 0.405e-6;
                break;
            case semi_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.303;
                wirePitch = 4 * featureSize;
                aspectRatio = 2.7;
                ildThickness = 0.405e-6;
                break;
            case semi_conservative:
                barrierThickness = 0.006e-6;
                horizontalDielectric = 2.734;
                wirePitch = 4 * featureSize;
                aspectRatio = 2.0;
                ildThickness = 0.405e-6;
                break;
            case global_aggressive:
                barrierThickness = 0.00e-6;
                horizontalDielectric = 2.303;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.8;
                ildThickness = 0.81e-6;
                break;
            case global_conservative:
                barrierThickness = 0.006e-6;
                horizontalDielectric = 2.734;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.2;
                ildThickness = 0.77e-6;
                break;
            default:	/* dram_wordline */
                /* TODO CACTI does not have a detailed model for it */
                barrierThickness = 0e-6;
                horizontalDielectric = 0;
                wirePitch = 2 * featureSize;
                aspectRatio = 0;
                ildThickness = 0e-6;
        }
    } else if (_featureSizeInNano <= 90) {
        featureSize = 90e-9;
        switch (wireType) {
            case local_aggressive:
                barrierThickness = 0.01e-6;
                horizontalDielectric = 2.709;
                wirePitch = 2.5* featureSize;
                aspectRatio = 2.4;
                ildThickness = 0.48e-6;
                break;
            case local_conservative:
                barrierThickness = 0.008e-6;
                horizontalDielectric = 3.038;
                wirePitch = 2.5* featureSize;
                aspectRatio = 2.0;
                ildThickness = 0.48e-6;
                break;
            case semi_aggressive:
                barrierThickness = 0.01e-6;
                horizontalDielectric = 2.709;
                wirePitch = 4 * featureSize;
                aspectRatio = 2.4;
                ildThickness = 0.48e-6;
                break;
            case semi_conservative:
                barrierThickness = 0.008e-6;
                horizontalDielectric = 3.038;
                wirePitch = 4 * featureSize;
                aspectRatio = 2.0;
                ildThickness = 0.48e-6;
                break;
            case global_aggressive:
                barrierThickness = 0.01e-6;
                horizontalDielectric = 2.709;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.7;
                ildThickness = 0.96e-6;
                break;
            case global_conservative:
                barrierThickness = 0.008e-6;
                horizontalDielectric = 3.038;
                wirePitch = 8 * featureSize;
                aspectRatio = 2.2;
                ildThickness = 1.1e-6;
                break;
            default:	/* dram_wordline */
                /* TODO CACTI does not have a detailed model for it */
                barrierThickness = 0e-6;
                horizontalDielectric = 0;
                wirePitch = 2 * featureSize;
                aspectRatio = 0;
                ildThickness = 0e-6;
        }
    } else if (_featureSizeInNano <= 120) {
        featureSize = 120e-9;
        switch (wireType) {
            case local_aggressive:
                barrierThickness = 0.012e-6;
                horizontalDielectric = 3.3;
                wirePitch = 240e-9;
                aspectRatio = 1.6;
                ildThickness = 0.48e-6;
                break;
            case local_conservative:
                barrierThickness = 0.01e-6;
                horizontalDielectric = 3.6;
                wirePitch = 240e-9;
                aspectRatio = 1.4;
                ildThickness = 0.48e-6;
                break;
            case semi_aggressive:
                barrierThickness = 0.012e-6;
                horizontalDielectric = 3.3;
                wirePitch = 320e-9;
                aspectRatio = 1.7;
                ildThickness = 0.48e-6;
                break;
            case semi_conservative:
                barrierThickness = 0.01e-6;
                horizontalDielectric = 3.6;
                wirePitch = 320e-9;
                aspectRatio = 1.5;
                ildThickness = 0.48e-6;
                break;
            case global_aggressive:
                barrierThickness = 0.012e-6;
                horizontalDielectric = 3.3;
                wirePitch = 475e-9;
                aspectRatio = 2.1;
                ildThickness = 0.96e-6;
                break;
            case global_conservative:
                barrierThickness = 0.01e-6;
                horizontalDielectric = 3.6;
                wirePitch = 475e-9;
                aspectRatio = 1.9;
                ildThickness = 1.1e-6;
                break;
            default:	/* dram_wordline */
                /* TODO CACTI does not have a detailed model for it */
                barrierThickness = 0e-6;
                horizontalDielectric = 0;
                wirePitch = 2 * featureSize;
                aspectRatio = 0;
                ildThickness = 0e-6;
        }
    } else if (_featureSizeInNano <= 200) {
        featureSize = 200e-9;
        switch (wireType) {
            case local_aggressive:
                barrierThickness = 0.016e-6;
                horizontalDielectric = 3.75;
                wirePitch = 0.45e-6;
                aspectRatio = 2.4;
                ildThickness = 1e-6; /* TODO: for test */
                break;
            case local_conservative:
                barrierThickness = 0.016e-6 * 0.8;
                horizontalDielectric = 3.75 * 3.038 / 2.709;
                wirePitch = 0.45e-6;
                aspectRatio = 1.2;
                ildThickness = 1e-6; /* TODO: for test */
                break;
            case semi_aggressive:
                barrierThickness = 0.016e-6;
                horizontalDielectric = 3.75;
                wirePitch = 0.575e-6;
                aspectRatio = 2.1;
                ildThickness = 1e-6; /* TODO: for test */
                break;
            case semi_conservative:
                barrierThickness = 0.016e-6 * 0.8;
                horizontalDielectric = 3.75 * 3.038 / 2.709;
                wirePitch = 0.575e-6;
                aspectRatio = 2.1 * 2.0 / 2.4;
                ildThickness = 1e-6; /* TODO: for test */
                break;
            case global_aggressive:
                barrierThickness = 0.016e-6;
                horizontalDielectric = 3.75;
                wirePitch = 0.945e-6;
                aspectRatio = 2.1;
                ildThickness = 2e-6; /* TODO: for test */
                break;
            case global_conservative:
                barrierThickness = 0.016e-6 * 0.8;
                horizontalDielectric = 3.75 * 3.038 / 2.709;
                wirePitch = 0.945e-6;
                aspectRatio = 2.1 * 2.2 / 2.7;
                ildThickness = 2.2e-6; /* TODO: for test */
                break;
            default:	/* dram_wordline */
                /* TODO CACTI does not have a detailed model for it */
                barrierThickness = 0e-6;
                horizontalDielectric = 0;
                wirePitch = 2 * featureSize;
                aspectRatio = 0;
                ildThickness = 0e-6;
        }
    }

    wireWidth = wirePitch / 2;
    wireThickness = aspectRatio * wireWidth;
    wireSpacing = wirePitch - wireWidth;

    /* TODO: here we only support copper wire, aluminum is to be added */
    copper_resistivity = copper_resistivity
        * (1 + COPPER_RESISTIVITY_TEMPERATURE_COEFFICIENT * (temperature - 293));
    resWirePerUnit = CalculateWireResistance(copper_resistivity, wireWidth, wireThickness, barrierThickness,
            0 /* Dishing Thickness */, 1 /* Alpha Scatter */);
    capWirePerUnit = CalculateWireCapacitance(PERMITTIVITY, wireWidth, wireThickness, wireSpacing,
            ildThickness, 1.5 /* miller value */, horizontalDielectric, 3.9 /* Vertical Dielectric */,
            1.15e-10 /* Fringe Capacitance (Unit: F/m), TODO: CACTI assumes a fixed number here */);

    if (wireRepeaterType != repeated_none) {
        /* If the repeaters are inserted in the wire */
        findOptimalRepeater();
        if (wireRepeaterType != repeated_opt) {
            /* The repeated wire is not fully latency optimized */
            double penalty;
            switch (wireRepeaterType) {
                case repeated_5:
                    penalty = 0.05;
                    break;
                case repeated_10:
                    penalty = 0.10;
                    break;
                case repeated_20:
                    penalty = 0.20;
                    break;
                case repeated_30:
                    penalty = 0.30;
                    break;
                case repeated_40:
                    penalty = 0.40;
                    break;
                default:	/* repeated_50 ? */
                    penalty = 0.50;
            }
            findPenalizedRepeater(penalty);
        }
        /* calculate repeated wire pitch */
        CalculateGateArea(INV, 1, repeaterSize * MIN_NMOS_SIZE * config->technology.tech->featureSize(),
                repeaterSize * MIN_NMOS_SIZE * config->technology.tech->featureSize() * config->technology.tech->pnSizeRatio(), 1e41, config->technology.tech,
                &repeaterHeight, &repeaterWidth, config->peripherals.useUpdatedLib);
        if (repeaterWidth < repeaterHeight) {
            double temp = repeaterWidth;
            repeaterWidth = repeaterHeight;
            repeaterHeight = temp;
        }
        repeatedWirePitch = wirePitch + repeaterWidth;
    }

    initialized =true;
}


void Wire::CalculateLatencyAndPower(double _wireLength, double *delay, double *dynamicEnergy, double *leakagePower) {
    if (!initialized) {
        throw std::runtime_error("[Wire] Error: Require initialization first!");
    } else {
        if (isLowSwing) {
            /* When it is low-swing */
            if (wireRepeaterType == repeated_none) {
                double widthNmos = MIN_NMOS_SIZE * config->technology.tech->featureSize();
                double widthPmos = widthNmos * config->technology.tech->pnSizeRatio();
                double capInput, capOutput;
                double tr;
                double gm;
                double beta;
                double resPullUp;
                double resPullDown;
                double capLoad;
                double temp;
                double riseTime, fallTime;
                double rampInput;

                /* Calculate rampInput */
                CalculateGateCapacitance(INV, 1, widthNmos, widthPmos, config->technology.tech->featureSize() * MAX_TRANSISTOR_HEIGHT, config->technology.tech, &capInput, &capOutput);
                capLoad = capInput + capOutput;
                resPullUp = CalculateOnResistance(widthNmos, NMOS, config->input.temperature, config->technology.tech);
                resPullUp = CalculateOnResistance(widthPmos, PMOS, config->input.temperature, config->technology.tech);
                tr = resPullUp * capLoad;
                gm = CalculateTransconductance(widthPmos, PMOS, config->technology.tech);
                beta = 1 / (resPullUp * gm);
                horowitz(tr, beta, 1e20, &riseTime);
                resPullDown = CalculateOnResistance(widthNmos, NMOS, config->input.temperature, config->technology.tech);
                tr = resPullDown * capLoad;
                gm = CalculateTransconductance(widthNmos, NMOS, config->technology.tech);
                beta = 1 / (resPullDown * gm);
                horowitz(tr, beta, riseTime, &fallTime);
                rampInput = fallTime;

                /* Calculate FO4 delay */
                double delayFO4;
                capLoad = capOutput + 4 * capInput;
                tr = resPullDown * capLoad;
                delayFO4 = horowitz(tr, beta, 1e20, &temp);

                /* Caluculate the size of driver */
                double wireLength = _wireLength;
                double capGateDriver;
                double capWire = capWirePerUnit * wireLength;
                double resWire = resWirePerUnit * wireLength;
                double resDriver = ((-8) * delayFO4 / ( log(0.5) * capWire)) / RES_ADJ;
                double widthNmosDriver = resPullDown * widthNmos / resDriver;
                widthNmosDriver = MIN(widthNmosDriver, MAX_NMOS_SIZE * config->technology.tech->featureSize());
                widthNmosDriver = MAX(widthNmosDriver, MIN_NMOS_SIZE * config->technology.tech->featureSize());

                if(resWire * capWire > 8 * delayFO4)
                {
                    widthNmosDriver = config->input.maxNmosSize * config->technology.tech->featureSize();
                }

                // size the inverter appropriately to minimize the transmitter delay
                // Note - In order to minimize leakage, we are not adding a set of inverters to
                // bring down delay. Instead, we are sizing the single gate
                // based on the logical effort.
                CalculateGateCapacitance(INV, 1, widthNmosDriver, 0, config->technology.tech->featureSize()*40, config->technology.tech, &capGateDriver, &temp);
                CalculateGateCapacitance(INV, 1, 2 * widthNmos, 2 * widthPmos, config->technology.tech->featureSize()*40, config->technology.tech, &capInput, &capOutput);
                double stageEffort   = sqrt(((2 + config->technology.tech->pnSizeRatio()) / (1 + config->technology.tech->pnSizeRatio())) * capGateDriver / capInput);
                double reqCin  = (((2 + config->technology.tech->pnSizeRatio()) / (1 + config->technology.tech->pnSizeRatio())) * capGateDriver) / stageEffort;
                CalculateGateCapacitance(INV, 1, widthNmos, widthPmos, config->technology.tech->featureSize()*40, config->technology.tech, &capInput, &capOutput);
                double sizeInverter = reqCin / capInput;
                sizeInverter = MAX(sizeInverter, 1);

                /* nand gate delay */
                resPullDown *= 2;
                beta = 1 / (resPullDown * gm);
                double capNandInput, capNandOutput;
                CalculateGateCapacitance(NAND, 2, 2 * widthNmos, widthPmos, config->technology.tech->featureSize()*40, config->technology.tech, &capNandInput, &capNandOutput);
                CalculateGateCapacitance(INV, 1, sizeInverter * widthNmos, sizeInverter * widthPmos, config->technology.tech->featureSize()*40, config->technology.tech, &capInput, &capOutput);
                capLoad = capNandOutput + capInput;
                tr = resPullDown * capLoad;
                *(delay) = horowitz(tr, beta, rampInput, &temp);
                *(dynamicEnergy) = capLoad * config->technology.tech->vdd() * config->technology.tech->vdd();
                rampInput = temp; /* for the next stage */

                /* Inverter *(delay):
                 *    * The load capacitance of this inv depends on
                 *    * the gate capacitance of the final stage nmos
                 *    * transistor which in turn depends on nsize
                 *    */
                resPullDown = CalculateOnResistance(sizeInverter * widthNmos, NMOS, config->input.temperature, config->technology.tech);
                gm = CalculateTransconductance(widthNmos, NMOS, config->technology.tech);
                beta = 1 / (resPullDown * gm);
                capLoad = capOutput + capGateDriver;
                tr = resPullDown * capLoad;
                *(delay) += horowitz(tr, beta, rampInput, &temp);
                *(dynamicEnergy) += capLoad * config->technology.tech->vdd() * config->technology.tech->vdd();
                rampInput = temp; /* for the next stage */

                *(leakagePower) = 2 * config->technology.tech->vdd() * CalculateGateLeakage(INV, 1, sizeInverter * widthNmos, sizeInverter * widthPmos, config->input.temperature, config->technology.tech);
                *(leakagePower) += 2 * config->technology.tech->vdd() * CalculateGateLeakage(NAND, 2, 2 * widthNmos, widthPmos, config->input.temperature, config->technology.tech);
                *(leakagePower) *= 2;

                SenseAmp senseAmp;
                senseAmp.Initialize(1, false, config->technology.cell->minSenseVoltage, 1, config /* for test */);
                senseAmp.CalculateRC();

                /* nmos *(delay) + wire *(delay) */
                /*
                 * 			   * NOTE: nmos is used as both pull up and pull down transistor
                 * 			   * in the transmitter. This is because for low voltage swing, drive
                 *			   * resistance of nmos is less than pmos
                 *			   * (for a detailed graph ref: On-Chip Wires: Scaling and Efficiency)
                 */
                double drainCapDriver = CalculateDrainCap(widthNmosDriver, NMOS, config->technology.tech->featureSize()*40, config->technology.tech);
                capLoad = capWire + drainCapDriver * 2 + senseAmp.capLoad;
                resPullDown = CalculateOnResistance(widthNmosDriver, NMOS, config->input.temperature, config->technology.tech);
                gm = CalculateTransconductance(widthNmosDriver, NMOS, config->technology.tech);
                beta = 1 / (resPullDown * gm);
                tr = resPullDown * RES_ADJ *(capWire + drainCapDriver * 2) + capWire * resWire / 2 + (resPullDown + resWire) * senseAmp.capLoad;
                if (delay)
                    *(delay) += horowitz(tr, beta, rampInput, &temp); //TODO: inconsistent with Cacti 6.5
                if (dynamicEnergy) {
                    *(dynamicEnergy) += capLoad * VOL_SWING * 0.4; /* .4v is the over drive voltage */
                    *(dynamicEnergy) *=2;
                }
                if (leakagePower)
                    *(leakagePower) += 4 * config->technology.tech->vdd() * CalculateGateLeakage(INV, 1, widthNmosDriver, 0, config->input.temperature, config->technology.tech);

                /* SA *(delay) and power */
                if (delay)
                    *(delay) += senseAmp.readLatency;
                if (dynamicEnergy)
                    *(dynamicEnergy) += senseAmp.readDynamicEnergy;
                if (leakagePower)
                    *(leakagePower) += senseAmp.leakage;

            } else {
                throw std::runtime_error("[Wire] Error: low swing wires with repeaters are not supported.");
            }
        } else {
            /* When it is not a low-swing */
            if (wireRepeaterType == repeated_none) {
                if (delay)
                    *(delay) = 2.3 * resWirePerUnit * capWirePerUnit * _wireLength * _wireLength / 2;
                if (dynamicEnergy)
                    *(dynamicEnergy) = capWirePerUnit * _wireLength * config->technology.tech->vdd() * config->technology.tech->vdd();
                if (leakagePower)
                    *(leakagePower) = 0;
            } else {		/* with repeaters */
                if (delay)
                    *(delay) = getRepeatedWireUnitDelay() * _wireLength;
                if (dynamicEnergy)
                    *(dynamicEnergy) = getRepeatedWireUnitDynamicEnergy() * _wireLength;
                if (leakagePower)
                    *(leakagePower) = getRepeatedWireUnitLeakage() * _wireLength;
            }
        }
    }
}

void Wire::findOptimalRepeater() {
    /* Use minimum sized inverter */
    double nmosSize = MIN_NMOS_SIZE * config->technology.tech->featureSize();
    double pmosSize = nmosSize * config->technology.tech->pnSizeRatio();
    double inputCap = CalculateGateCap(nmosSize, config->technology.tech) + CalculateGateCap(pmosSize, config->technology.tech);
    double outputCap = CalculateDrainCap(nmosSize, NMOS, 1 /*no limit*/, config->technology.tech)
        + CalculateDrainCap(pmosSize, PMOS, 1 /*no limit*/, config->technology.tech);
    double outputRes = CalculateOnResistance(nmosSize, NMOS, config->input.temperature, config->technology.tech)
        + CalculateOnResistance(pmosSize, PMOS, config->input.temperature, config->technology.tech);

    repeaterSize = sqrt(outputRes * capWirePerUnit / inputCap / resWirePerUnit);
    repeaterSpacing = sqrt(2 * outputRes * (outputCap + inputCap) / (resWirePerUnit * capWirePerUnit));

    //double tau = outputRes * (inputCap + outputCap) + outputRes * capWirePerUnit * repeaterSpacing
    //		+ resWirePerUnit * repeaterSpacing * inputCap * repeaterSize
}

void Wire::findPenalizedRepeater(double _penalty) {
    double targetDelay = getRepeatedWireUnitDelay() * (1 + _penalty);
    double currentDynamicEnergy = getRepeatedWireUnitDynamicEnergy();
    double currentLeakage = getRepeatedWireUnitLeakage();

    double targetRepeaterSpacing = repeaterSpacing;
    double targetRepeaterSize = repeaterSize;
    double stepSpacing = 100e-6;	/* 100um */
    double endSpacing = 4 * repeaterSpacing;
    double stepSize = 1;			/* minimum buffer size */
    double endSize = 1;

    double thisDelay, thisDynamicEnergy, thisLeakage;
    /* Start finding the target repeated wire */
    for (; repeaterSpacing <= endSpacing; repeaterSpacing += stepSpacing) {
        for (; repeaterSize >= endSize; repeaterSize -= stepSize) {
            thisDelay = getRepeatedWireUnitDelay();
            thisDynamicEnergy = getRepeatedWireUnitDynamicEnergy();
            thisLeakage = getRepeatedWireUnitLeakage();
            if (thisDelay <= targetDelay && thisDynamicEnergy / currentDynamicEnergy + thisLeakage / currentLeakage < 2) {
                currentDynamicEnergy = thisDynamicEnergy;
                currentLeakage = thisLeakage;
                targetRepeaterSpacing = repeaterSpacing;
                targetRepeaterSize = repeaterSize;
            }
        }
    }
    repeaterSpacing = targetRepeaterSpacing;
    repeaterSize = targetRepeaterSize;
}

double Wire::getRepeatedWireUnitDelay() {
    /* Use the scaled size of the repeater */
    double nmosSize = MIN_NMOS_SIZE * config->technology.tech->featureSize() * repeaterSize;
    double pmosSize = nmosSize * config->technology.tech->pnSizeRatio();
    double inputCap = CalculateGateCap(nmosSize, config->technology.tech) + CalculateGateCap(pmosSize, config->technology.tech);
    double outputCap = CalculateDrainCap(nmosSize, NMOS, 1 /*no limit*/, config->technology.tech)
        + CalculateDrainCap(pmosSize, PMOS, 1 /*no limit*/, config->technology.tech);
    double outputRes = CalculateOnResistance(nmosSize, NMOS, config->input.temperature, config->technology.tech)
        + CalculateOnResistance(pmosSize, PMOS, config->input.temperature, config->technology.tech);
    double wireCap = capWirePerUnit * repeaterSpacing;
    double wireRes = resWirePerUnit * repeaterSpacing;

    double tau = outputRes * (inputCap + outputCap) + outputRes * wireCap + wireRes * outputCap
        + 0.5 * wireRes * wireCap;

    /* Return as a unit value */
    return 0.693 * tau / repeaterSpacing;
}

double Wire::getRepeatedWireUnitDynamicEnergy() {
    /* Use the scaled size of the repeater */
    double nmosSize = MIN_NMOS_SIZE * config->technology.tech->featureSize() * repeaterSize;
    double pmosSize = nmosSize * config->technology.tech->pnSizeRatio();
    double inputCap = CalculateGateCap(nmosSize, config->technology.tech) + CalculateGateCap(pmosSize, config->technology.tech);
    double outputCap = CalculateDrainCap(nmosSize, NMOS, 1 /*no limit*/, config->technology.tech)
        + CalculateDrainCap(pmosSize, PMOS, 1 /*no limit*/, config->technology.tech);
    double wireCap = capWirePerUnit * repeaterSpacing;

    double switchingEnergy = (inputCap + outputCap + wireCap) * config->technology.tech->vdd() * config->technology.tech->vdd();
    double shortCircuitEnergy = 0;		/* TODO: no short circuit energy in this version */

    return (switchingEnergy + shortCircuitEnergy) / repeaterSpacing;
}

double Wire::getRepeatedWireUnitLeakage() {
    double nmosSize = MIN_NMOS_SIZE * config->technology.tech->featureSize() * repeaterSize;
    double pmosSize = nmosSize * config->technology.tech->pnSizeRatio();
    double leakagePerRepeater = CalculateGateLeakage(INV, 1, nmosSize, pmosSize, config->input.temperature, config->technology.tech)
        * config->technology.tech->vdd();

    return leakagePerRepeater / repeaterSpacing;
}

void Wire::PrintProperty() {
    if (wireRepeaterType == repeated_none) {
        std::cout << "Wire Type: passive (without repeaters)";
        if (isLowSwing) {
            std::cout << " Low Swing";
        }
        std::cout << std::endl;
        std::cout << "Wire Resistance: " << resWirePerUnit / 1e6 << "ohm/um" << std::endl;
        std::cout << "Wire Capacitance: " << capWirePerUnit / 1e6 << "F/um" << std::endl;
    } else {
        std::cout << "Wire type: active (with repeaters)" << std::endl;
        std::cout << "Repeater Size: " << repeaterSize << std::endl;
        std::cout << "Repeater Spacing: " << repeaterSpacing * 1e3 << "mm" <<std::endl;
        std::cout << "Delay: " << getRepeatedWireUnitDelay() * 1e6 << "ns/mm" <<std::endl;
        std::cout << "Dynamic Energy: " << getRepeatedWireUnitDynamicEnergy() * 1e6 << "nJ/mm" <<std::endl;
        std::cout << "Subtheshold Leakage Power: " << getRepeatedWireUnitLeakage() << "mW/mm" << std::endl;
    }
}

Wire & Wire::operator=(const Wire &rhs) {
    initialized = rhs.initialized;
    featureSizeInNano = rhs.featureSizeInNano;
    featureSize = rhs.featureSize;
    wireType = rhs.wireType;
    wireRepeaterType = rhs.wireRepeaterType;
    temperature = rhs.temperature;
    isLowSwing = rhs.isLowSwing;
    barrierThickness = rhs.barrierThickness;
    horizontalDielectric = rhs.horizontalDielectric;
    wirePitch = rhs.wirePitch;
    aspectRatio = rhs.aspectRatio;
    ildThickness = rhs.ildThickness;
    wireWidth = rhs.wireWidth;
    wireThickness = rhs.wireThickness;
    wireSpacing = rhs.wireSpacing;
    repeaterSize = rhs.repeaterSize;
    repeaterSpacing = rhs.repeaterSpacing;
    repeaterHeight = rhs.repeaterHeight;
    repeaterWidth = rhs.repeaterWidth;
    repeatedWirePitch = rhs.repeatedWirePitch;
    resWirePerUnit = rhs.resWirePerUnit;
    capWirePerUnit = rhs.capWirePerUnit;
    copper_resistivity = rhs.copper_resistivity;

    return *this;
}
