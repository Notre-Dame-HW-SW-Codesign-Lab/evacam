#include "Wire.h"
#include "formula.h"
#include "SenseAmp.h"
#include "WireProcessTable.h"

namespace {

struct RepeaterTransistorWidths {
    double nmos;
    double pmos;
};

struct RepeaterElectricals {
    RepeaterTransistorWidths widths;
    double inputCap;
    double outputCap;
    double outputRes;
};

double RepeaterPenalty(WireRepeaterType repeaterType) {
    switch (repeaterType) {
        case repeated_5:
            return 0.05;
        case repeated_10:
            return 0.10;
        case repeated_20:
            return 0.20;
        case repeated_30:
            return 0.30;
        case repeated_40:
            return 0.40;
        default:	/* repeated_50 ? */
            return 0.50;
    }
}

RepeaterTransistorWidths CalculateRepeaterTransistorWidths(
        double repeaterSize,
        const Technology &tech) {
    RepeaterTransistorWidths widths;
    widths.nmos = MIN_NMOS_SIZE * tech.featureSize() * repeaterSize;
    widths.pmos = widths.nmos * tech.pnSizeRatio();
    return widths;
}

RepeaterElectricals CalculateRepeaterElectricals(
        double repeaterSize,
        const Technology &tech,
        double temperature) {
    RepeaterElectricals electricals;
    electricals.widths = CalculateRepeaterTransistorWidths(repeaterSize, tech);
    electricals.inputCap = CalculateGateCap(electricals.widths.nmos, tech)
        + CalculateGateCap(electricals.widths.pmos, tech);
    electricals.outputCap = CalculateDrainCap(
            electricals.widths.nmos, NMOS, 1 /*no limit*/, tech)
        + CalculateDrainCap(electricals.widths.pmos, PMOS, 1 /*no limit*/, tech);
    electricals.outputRes = CalculateOnResistance(
            electricals.widths.nmos, NMOS, temperature, tech)
        + CalculateOnResistance(electricals.widths.pmos, PMOS, temperature, tech);
    return electricals;
}

}  // namespace

void Wire::Initialize(
        int _featureSizeInNano,
        WireType _wireType,
        WireRepeaterType _wireRepeaterType,
        int _temperature,
        bool _isLowSwing,
        std::shared_ptr<EvaCamConfig> _config) {

    if (initialized) {
        /* reload the new input, clear the previous setting */
        initialized = false;
    }

    /* Store initialization inputs. */
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

    /* Load process-specific wire geometry. */
    WireProcessSpec processSpec;
    if (FindWireProcessSpec(_featureSizeInNano, wireType, &processSpec)) {
        featureSize = processSpec.featureSize;
        barrierThickness = processSpec.barrierThickness;
        horizontalDielectric = processSpec.horizontalDielectric;
        wirePitch = processSpec.wirePitch;
        aspectRatio = processSpec.aspectRatio;
        ildThickness = processSpec.ildThickness;
        copper_resistivity = processSpec.copperResistivity;
    } else {
        throw std::runtime_error("[Wire] Error: unsupported wire process specification.");
    }

    /* Derive wire dimensions. */
    wireWidth = wirePitch / 2;
    wireThickness = aspectRatio * wireWidth;
    wireSpacing = wirePitch - wireWidth;

    /* Calculate per-unit wire RC. */
    const double dishingThickness = 0;
    const double alphaScatter = 1;
    const double millerValue = 1.5;
    const double verticalDielectric = 3.9;
    const double fringeCapacitance = 1.15e-10;

    /* TODO: here we only support copper wire, aluminum is to be added */
    copper_resistivity = copper_resistivity
        * (1 + COPPER_RESISTIVITY_TEMPERATURE_COEFFICIENT * (temperature - 293));
    resWirePerUnit = CalculateWireResistance(
            copper_resistivity, wireWidth, wireThickness, barrierThickness,
            dishingThickness, alphaScatter);
    capWirePerUnit = CalculateWireCapacitance(
            PERMITTIVITY, wireWidth, wireThickness, wireSpacing,
            ildThickness, millerValue, horizontalDielectric,
            verticalDielectric, fringeCapacitance);

    /* Configure repeaters. */
    if (wireRepeaterType != repeated_none) {
        const Technology &tech = *config->technology.tech;

        findOptimalRepeater();
        if (wireRepeaterType != repeated_opt) {
            /* The repeated wire is not fully latency optimized */
            findPenalizedRepeater(RepeaterPenalty(wireRepeaterType));
        }

        const RepeaterTransistorWidths repeaterWidths =
            CalculateRepeaterTransistorWidths(repeaterSize, tech);

        CalculateGateArea(INV, 1, repeaterWidths.nmos, repeaterWidths.pmos, 1e41, tech,
                &repeaterHeight, &repeaterWidth, config->peripherals.useUpdatedLib);
        if (repeaterWidth < repeaterHeight) {
            double temp = repeaterWidth;
            repeaterWidth = repeaterHeight;
            repeaterHeight = temp;
        }
        repeatedWirePitch = wirePitch + repeaterWidth;
    }

    initialized = true;
}


void Wire::CalculateLatencyAndPower(
        double _wireLength,
        double *delay,
        double *dynamicEnergy,
        double *leakagePower) {
    if (!initialized) {
        throw std::runtime_error("[Wire] Error: Require initialization first!");
    }

    if (isLowSwing) {
        CalculateLowSwingLatencyAndPower(_wireLength, delay, dynamicEnergy, leakagePower);
        return;
    }

    if (wireRepeaterType == repeated_none) {
        CalculatePassiveLatencyAndPower(_wireLength, delay, dynamicEnergy, leakagePower);
        return;
    }

    CalculateRepeatedLatencyAndPower(_wireLength, delay, dynamicEnergy, leakagePower);
}

void Wire::CalculateLowSwingLatencyAndPower(
        double _wireLength,
        double *delay,
        double *dynamicEnergy,
        double *leakagePower) {
    if (wireRepeaterType != repeated_none) {
        throw std::runtime_error(
                "[Wire] Error: low swing wires with repeaters are not supported.");
    }

    const Technology &tech = *config->technology.tech;
    const double techFeatureSize = tech.featureSize();
    const double pnSizeRatio = tech.pnSizeRatio();
    const double vdd = tech.vdd();
    const double inputTemperature = config->input.temperature;
    const double maxTransistorHeight = techFeatureSize * MAX_TRANSISTOR_HEIGHT;
    const double driverTransistorHeight = techFeatureSize * 40;

    double widthNmos = MIN_NMOS_SIZE * techFeatureSize;
    double widthPmos = widthNmos * pnSizeRatio;
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
    CalculateGateCapacitance(INV, 1, widthNmos, widthPmos,
            maxTransistorHeight, tech, &capInput, &capOutput);

    capLoad = capInput + capOutput;
    resPullUp = CalculateOnResistance(widthNmos, NMOS, inputTemperature, tech);
    resPullUp = CalculateOnResistance(widthPmos, PMOS, inputTemperature, tech);

    tr = resPullUp * capLoad;
    gm = CalculateTransconductance(widthPmos, PMOS, tech);
    beta = 1 / (resPullUp * gm);
    horowitz(tr, beta, 1e20, &riseTime);

    resPullDown = CalculateOnResistance(widthNmos, NMOS, inputTemperature, tech);
    tr = resPullDown * capLoad;
    gm = CalculateTransconductance(widthNmos, NMOS, tech);
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
    widthNmosDriver = MIN(widthNmosDriver, MAX_NMOS_SIZE * techFeatureSize);
    widthNmosDriver = MAX(widthNmosDriver, MIN_NMOS_SIZE * techFeatureSize);

    if (resWire * capWire > 8 * delayFO4) {
        widthNmosDriver = config->input.maxNmosSize * techFeatureSize;
    }

    // size the inverter appropriately to minimize the transmitter delay
    // Note - In order to minimize leakage, we are not adding a set of inverters to
    // bring down delay. Instead, we are sizing the single gate
    // based on the logical effort.

    CalculateGateCapacitance(INV, 1, widthNmosDriver, 0,
            driverTransistorHeight, tech, &capGateDriver, &temp);

    CalculateGateCapacitance(INV, 1, 2 * widthNmos, 2 * widthPmos,
            driverTransistorHeight, tech, &capInput, &capOutput);

    double logicalEffort = (2 + pnSizeRatio) / (1 + pnSizeRatio);
    double stageEffort = sqrt(logicalEffort * capGateDriver / capInput);
    double reqCin = logicalEffort * capGateDriver / stageEffort;

    CalculateGateCapacitance(INV, 1, widthNmos, widthPmos,
            driverTransistorHeight, tech, &capInput, &capOutput);

    double sizeInverter = reqCin / capInput;
    sizeInverter = MAX(sizeInverter, 1);

    /* nand gate delay */

    resPullDown *= 2;
    beta = 1 / (resPullDown * gm);
    double capNandInput, capNandOutput;

    CalculateGateCapacitance(NAND, 2, 2 * widthNmos, widthPmos,
            driverTransistorHeight, tech, &capNandInput, &capNandOutput);

    CalculateGateCapacitance(INV, 1, sizeInverter * widthNmos,
            sizeInverter * widthPmos, driverTransistorHeight,
            tech, &capInput, &capOutput);

    capLoad = capNandOutput + capInput;
    tr = resPullDown * capLoad;
    *(delay) = horowitz(tr, beta, rampInput, &temp);
    *(dynamicEnergy) = capLoad * vdd * vdd;
    rampInput = temp; /* for the next stage */

    /* Inverter *(delay):
     *    * The load capacitance of this inv depends on
     *    * the gate capacitance of the final stage nmos
     *    * transistor which in turn depends on nsize
     *    */

    resPullDown = CalculateOnResistance(
            sizeInverter * widthNmos, NMOS, inputTemperature, tech);
    gm = CalculateTransconductance(widthNmos, NMOS, tech);
    beta = 1 / (resPullDown * gm);
    capLoad = capOutput + capGateDriver;
    tr = resPullDown * capLoad;
    *(delay) += horowitz(tr, beta, rampInput, &temp);
    *(dynamicEnergy) += capLoad * vdd * vdd;
    rampInput = temp; /* for the next stage */

    *(leakagePower) = 2 * vdd * CalculateGateLeakage(
            INV, 1, sizeInverter * widthNmos,
            sizeInverter * widthPmos, inputTemperature, tech);
    *(leakagePower) += 2 * vdd * CalculateGateLeakage(
            NAND, 2, 2 * widthNmos, widthPmos,
            inputTemperature, tech);
    *(leakagePower) *= 2;

    SenseAmp senseAmp;
    senseAmp.Initialize(
            1, false, config->technology.cell->minSenseVoltage,
            1, config /* for test */);
    senseAmp.CalculateRC();

    /* nmos *(delay) + wire *(delay) */
    /*
     * 			   * NOTE: nmos is used as both pull up and pull down transistor
     * 			   * in the transmitter. This is because for low voltage swing, drive
     *			   * resistance of nmos is less than pmos
     *			   * (for a detailed graph ref: On-Chip Wires: Scaling and Efficiency)
     */

    double drainCapDriver = CalculateDrainCap(
            widthNmosDriver, NMOS, driverTransistorHeight, tech);

    capLoad = capWire + drainCapDriver * 2 + senseAmp.capLoad;
    resPullDown = CalculateOnResistance(
            widthNmosDriver, NMOS, inputTemperature, tech);

    gm = CalculateTransconductance(widthNmosDriver, NMOS, tech);
    beta = 1 / (resPullDown * gm);
    tr = resPullDown * RES_ADJ * (capWire + drainCapDriver * 2)
        + capWire * resWire / 2
        + (resPullDown + resWire) * senseAmp.capLoad;

    if (delay)
        *(delay) += horowitz(
                tr, beta, rampInput,
                &temp); // TODO: inconsistent with Cacti 6.5
    if (dynamicEnergy) {
        /* .4v is the over drive voltage */
        *(dynamicEnergy) += capLoad * VOL_SWING * 0.4;
        *(dynamicEnergy) *= 2;
    }
    if (leakagePower)
        *(leakagePower) += 4 * vdd * CalculateGateLeakage(
                INV, 1, widthNmosDriver, 0, inputTemperature, tech);

    /* SA *(delay) and power */
    if (delay)
        *(delay) += senseAmp.readLatency;
    if (dynamicEnergy)
        *(dynamicEnergy) += senseAmp.readDynamicEnergy;
    if (leakagePower)
        *(leakagePower) += senseAmp.leakage;
}

void Wire::CalculatePassiveLatencyAndPower(
        double _wireLength,
        double *delay,
        double *dynamicEnergy,
        double *leakagePower) {
    const double vdd = config->technology.tech->vdd();

    if (delay)
        *(delay) = 2.3 * resWirePerUnit * capWirePerUnit
            * _wireLength * _wireLength / 2;
    if (dynamicEnergy)
        *(dynamicEnergy) = capWirePerUnit * _wireLength * vdd * vdd;
    if (leakagePower)
        *(leakagePower) = 0;
}

void Wire::CalculateRepeatedLatencyAndPower(
        double _wireLength,
        double *delay,
        double *dynamicEnergy,
        double *leakagePower) {
    if (delay)
        *(delay) = getRepeatedWireUnitDelay() * _wireLength;
    if (dynamicEnergy)
        *(dynamicEnergy) = getRepeatedWireUnitDynamicEnergy() * _wireLength;
    if (leakagePower)
        *(leakagePower) = getRepeatedWireUnitLeakage() * _wireLength;
}

void Wire::findOptimalRepeater() {
    const Technology &tech = *config->technology.tech;
    const double inputTemperature = config->input.temperature;
    const RepeaterElectricals minimumRepeater =
        CalculateRepeaterElectricals(1, tech, inputTemperature);

    repeaterSize = sqrt(
            minimumRepeater.outputRes * capWirePerUnit
            / minimumRepeater.inputCap / resWirePerUnit);
    repeaterSpacing = sqrt(
            2 * minimumRepeater.outputRes
            * (minimumRepeater.outputCap + minimumRepeater.inputCap)
            / (resWirePerUnit * capWirePerUnit));

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
            if (thisDelay <= targetDelay
                    && thisDynamicEnergy / currentDynamicEnergy
                        + thisLeakage / currentLeakage < 2) {
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
    const Technology &tech = *config->technology.tech;
    const double inputTemperature = config->input.temperature;
    const RepeaterElectricals repeater =
        CalculateRepeaterElectricals(repeaterSize, tech, inputTemperature);
    double wireCap = capWirePerUnit * repeaterSpacing;
    double wireRes = resWirePerUnit * repeaterSpacing;

    double tau = repeater.outputRes * (repeater.inputCap + repeater.outputCap)
        + repeater.outputRes * wireCap + wireRes * repeater.outputCap
        + 0.5 * wireRes * wireCap;

    /* Return as a unit value */
    return 0.693 * tau / repeaterSpacing;
}

double Wire::getRepeatedWireUnitDynamicEnergy() {
    const Technology &tech = *config->technology.tech;
    const double vdd = tech.vdd();
    const RepeaterElectricals repeater =
        CalculateRepeaterElectricals(repeaterSize, tech, config->input.temperature);
    double wireCap = capWirePerUnit * repeaterSpacing;

    double switchingEnergy = (repeater.inputCap + repeater.outputCap + wireCap)
        * vdd * vdd;
    double shortCircuitEnergy = 0;		/* TODO: no short circuit energy in this version */

    return (switchingEnergy + shortCircuitEnergy) / repeaterSpacing;
}

double Wire::getRepeatedWireUnitLeakage() {
    const Technology &tech = *config->technology.tech;
    const double inputTemperature = config->input.temperature;

    const RepeaterTransistorWidths repeaterWidths =
        CalculateRepeaterTransistorWidths(repeaterSize, tech);
    double leakagePerRepeater = CalculateGateLeakage(
            INV, 1, repeaterWidths.nmos, repeaterWidths.pmos,
            inputTemperature, tech)
        * tech.vdd();

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
        std::cout << "Dynamic Energy: "
            << getRepeatedWireUnitDynamicEnergy() * 1e6 << "nJ/mm" << std::endl;
        std::cout << "Subtheshold Leakage Power: "
            << getRepeatedWireUnitLeakage() << "mW/mm" << std::endl;
    }
}
