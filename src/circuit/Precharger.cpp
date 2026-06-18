#include "Precharger.h"
#include "formula.h"

void Precharger::Initialize(
        double _voltagePrecharge, 
        int _numColumn, 
        double _capBitline, 
        double _resBitline, 
        std::shared_ptr<EvaCamConfig> _config, 
        const Wire &_localWire) {

    config = _config;

    if (initialized)
        config->logger.Verbose() << "[Precharger] Warning: Already initialized!";

    voltagePrecharge = _voltagePrecharge;
    numColumn  = _numColumn;
    capBitline = _capBitline;
    resBitline = _resBitline;
    localWire = _localWire;
    const auto &tech = *config->technology.tech;
    const double transistorRegionHeight = tech.featureSize() * 40;

    capWireLoadPerColumn =
            config->technology.cell->widthInFeatureSize * tech.featureSize() * localWire.capWirePerUnit;
    resWireLoadPerColumn =
            config->technology.cell->widthInFeatureSize * tech.featureSize() * localWire.resWirePerUnit;

    widthInvNmos = MIN_NMOS_SIZE * tech.featureSize();
    widthInvPmos = widthInvNmos * tech.pnSizeRatio();
    widthPMOSBitlineEqual      = MIN_NMOS_SIZE * tech.featureSize();
    widthPMOSBitlinePrecharger = PRECHARGER_SIZE * tech.featureSize();

    capLoadInv =
            CalculateGateCap(widthPMOSBitlineEqual, tech)
            + 2 * CalculateGateCap(widthPMOSBitlinePrecharger, tech)
            + CalculateDrainCap(widthInvNmos, NMOS, transistorRegionHeight, tech)
            + CalculateDrainCap(widthInvPmos, PMOS, transistorRegionHeight, tech);

    capOutputBitlinePrecharger =
            CalculateDrainCap(widthPMOSBitlinePrecharger, PMOS, transistorRegionHeight, tech)
            + CalculateDrainCap(widthPMOSBitlineEqual, PMOS, transistorRegionHeight, tech);

    double capInputInv         = CalculateGateCap(widthInvNmos, tech) + CalculateGateCap(widthInvPmos, tech);
    capLoadPerColumn           = capInputInv + capWireLoadPerColumn;
    double capLoadOutputDriver = numColumn * capLoadPerColumn;

    outputDriver.Initialize(
            1,
            capInputInv,
            capLoadOutputDriver,
            0 /* TODO */,
            true,
            latency_first,
            0,
            config);  /* Always Latency First */

    initialized = true;
    CalculateArea();
    CalculateRC();
}

void Precharger::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[Precharger]");
    } else {
        const auto &tech = *config->technology.tech;
        const double transistorRegionHeight = tech.featureSize() * 40;

        double hBitlinePrecharger, wBitlinePrecharger;
        double hBitlineEqual, wBitlineEqual;
        double hInverter, wInverter;

        CalculateGateArea(
                INV,
                1,
                0,
                widthPMOSBitlinePrecharger,
                transistorRegionHeight,
                tech,
                &hBitlinePrecharger,
                &wBitlinePrecharger,
                config->peripherals.useUpdatedLib);

        CalculateGateArea(
                INV,
                1,
                0,
                widthPMOSBitlineEqual,
                transistorRegionHeight,
                tech,
                &hBitlineEqual,
                &wBitlineEqual,
                config->peripherals.useUpdatedLib);

        CalculateGateArea(
                INV,
                1,
                widthInvNmos,
                widthInvPmos,
                transistorRegionHeight,
                tech,
                &hInverter,
                &wInverter,
                config->peripherals.useUpdatedLib);

        // CAM precharge excludes the equalization device, so width tracks only the two precharge PMOS devices.
        width = 2 * wBitlinePrecharger;
        width = std::max(width, wInverter);
        width *= numColumn;
        width = std::max(width, outputDriver.width);

        // Height excludes equalization-device footprint for the same reason.
        height = hBitlinePrecharger;
        height += hInverter;
        height += outputDriver.height;
        area = height * width;
    }
}

void Precharger::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[Precharger]");
    } else {
        // more accurate RC model would include drain Capacitances of Precharger and Equalization PMOS transistors
        // TODO: the above
    }
}

void Precharger::CalculateLatency(double _rampInput){
    if (!initialized) {
        ThrowInitializationError("[Precharger]");
    } else {
        rampInput= _rampInput;
        outputDriver.CalculateLatency(rampInput);
        enableLatency = outputDriver.readLatency;
        double resPullDown;
        double tr;	/* time constant */
        double gm;	/* transconductance */
        double beta;	/* for horowitz calculation */
        double temp;
        resPullDown = CalculateOnResistance(widthInvNmos, NMOS, config->input.temperature, *config->technology.tech);
        tr = resPullDown * capLoadInv;
        gm = CalculateTransconductance(widthInvNmos, NMOS, *config->technology.tech);
        beta = 1 / (resPullDown * gm);
        enableLatency += horowitz(tr, beta, outputDriver.rampOutput, &temp);
        readLatency = 0;
        double resPullUp = CalculateOnResistance(widthPMOSBitlinePrecharger, PMOS,
                config->input.temperature, *config->technology.tech);
        double tau = resPullUp * (capBitline + capOutputBitlinePrecharger) + resBitline * capBitline / 2;
        gm = CalculateTransconductance(widthPMOSBitlinePrecharger, PMOS, *config->technology.tech);
        beta = 1 / (resPullUp * gm);
        readLatency += horowitz(tau, beta, temp, &rampOutput);
        writeLatency = readLatency;
    }
}

void Precharger::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[Precharger]");
    } else {
        outputDriver.CalculatePower();
        /* Leakage power */
        leakage = outputDriver.leakage;
        leakage += numColumn * config->technology.tech->vdd() * CalculateGateLeakage(INV, 1, widthInvNmos, widthInvPmos, config->input.temperature, *config->technology.tech);
        leakage += numColumn * voltagePrecharge * CalculateGateLeakage(INV, 1, 0, widthPMOSBitlinePrecharger,
                config->input.temperature, *config->technology.tech);

        /* Dynamic energy */
        /* We don't count bitline precharge energy into account because it is a charging process */
        readDynamicEnergy = outputDriver.readDynamicEnergy;
        readDynamicEnergy += capLoadInv * config->technology.tech->vdd() * config->technology.tech->vdd() * numColumn;
        writeDynamicEnergy = 0;		/* No precharging is needed during the write operation */
    }
}

void Precharger::PrintProperty() {
    std::cout << "Precharger Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
