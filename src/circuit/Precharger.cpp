#include "Precharger.h"
#include "formula.h"
/*
   Precharger::Precharger() {
initialized = false;
enableLatency = 0;
}

Precharger::~Precharger() {
}
 */
void Precharger::Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline, std::shared_ptr<EvaCamConfig> _config, const Wire &_localWire) {
    if (initialized)
        _config->logger.Verbose() << "[Precharger] Warning: Already initialized!";

    outputDriver = std::make_unique<OutputDriver>();
    voltagePrecharge = _voltagePrecharge;
    numColumn  = _numColumn;
    capBitline = _capBitline;
    resBitline = _resBitline;
    config = _config;
    localWire = _localWire;

    capWireLoadPerColumn = config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize() * localWire.capWirePerUnit;
    resWireLoadPerColumn = config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize() * localWire.resWirePerUnit;
    widthInvNmos = MIN_NMOS_SIZE * config->technology.tech->featureSize();
    widthInvPmos = widthInvNmos * config->technology.tech->pnSizeRatio();
    widthPMOSBitlineEqual      = MIN_NMOS_SIZE * config->technology.tech->featureSize();
    widthPMOSBitlinePrecharger = 6 * config->technology.tech->featureSize();
    capLoadInv  = CalculateGateCap(widthPMOSBitlineEqual, config->technology.tech) + 2 * CalculateGateCap(widthPMOSBitlinePrecharger, config->technology.tech)
        + CalculateDrainCap(widthInvNmos, NMOS, config->technology.tech->featureSize()*40, config->technology.tech)
        + CalculateDrainCap(widthInvPmos, PMOS, config->technology.tech->featureSize()*40, config->technology.tech);
    capOutputBitlinePrecharger = CalculateDrainCap(widthPMOSBitlinePrecharger, PMOS, config->technology.tech->featureSize()*40, config->technology.tech) + CalculateDrainCap(widthPMOSBitlineEqual, PMOS, config->technology.tech->featureSize()*40, config->technology.tech);
    double capInputInv         = CalculateGateCap(widthInvNmos, config->technology.tech) + CalculateGateCap(widthInvPmos, config->technology.tech);
    capLoadPerColumn           = capInputInv + capWireLoadPerColumn;
    double capLoadOutputDriver = numColumn * capLoadPerColumn;
    outputDriver->Initialize(1, capInputInv, capLoadOutputDriver, 0 /* TODO */, true, latency_first, 0, config);  /* Always Latency First */

    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void Precharger::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[Precharger]");
    } else {
        //outputDriver->CalculateArea();
        double hBitlinePrechareger, wBitlinePrechareger;
        double hBitlineEqual, wBitlineEqual;
        double hInverter, wInverter;
        CalculateGateArea(INV, 1, 0, widthPMOSBitlinePrecharger, config->technology.tech->featureSize()*40, config->technology.tech, &hBitlinePrechareger, &wBitlinePrechareger, config->peripherals.useUpdatedLib);
        CalculateGateArea(INV, 1, 0, widthPMOSBitlineEqual, config->technology.tech->featureSize()*40, config->technology.tech, &hBitlineEqual, &wBitlineEqual, config->peripherals.useUpdatedLib);
        CalculateGateArea(INV, 1, widthInvNmos, widthInvPmos, config->technology.tech->featureSize()*40, config->technology.tech, &hInverter, &wInverter, config->peripherals.useUpdatedLib);
        width = 2 * wBitlinePrechareger + wBitlineEqual;
        width = MAX(width, wInverter);
        width *= numColumn;
        width = MAX(width, outputDriver->width);
        height = MAX(hBitlinePrechareger, hBitlineEqual);
        height += hInverter;
        height = MAX(height, outputDriver->height);
        area = height * width;
    }
}

void Precharger::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[Precharger]");
    } else {
        //outputDriver->CalculateRC();
        //more accurate RC model would include drain Capacitances of Precharger and Equalization PMOS transistors
        // TODO: the above
    }
}

void Precharger::CalculateLatency(double _rampInput){
    if (!initialized) {
        ThrowInitializationError("[Precharger]");
    } else {
        rampInput= _rampInput;
        outputDriver->CalculateLatency(rampInput);
        enableLatency = outputDriver->readLatency;
        double resPullDown;
        double tr;	/* time constant */
        double gm;	/* transconductance */
        double beta;	/* for horowitz calculation */
        double temp;
        resPullDown = CalculateOnResistance(widthInvNmos, NMOS, config->input.temperature, config->technology.tech);
        tr = resPullDown * capLoadInv;
        gm = CalculateTransconductance(widthInvNmos, NMOS, config->technology.tech);
        beta = 1 / (resPullDown * gm);
        enableLatency += horowitz(tr, beta, outputDriver->rampOutput, &temp);
        readLatency = 0;
        double resPullUp = CalculateOnResistance(widthPMOSBitlinePrecharger, PMOS,
                config->input.temperature, config->technology.tech);
        double tau = resPullUp * (capBitline + capOutputBitlinePrecharger) + resBitline * capBitline / 2;
        gm = CalculateTransconductance(widthPMOSBitlinePrecharger, PMOS, config->technology.tech);
        beta = 1 / (resPullUp * gm);
        readLatency += horowitz(tau, beta, temp, &rampOutput);
        writeLatency = readLatency;
    }
}

void Precharger::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[Precharger]");
    } else {
        //outputDriver->CalculatePower();
        /* Leakage power */
        leakage = outputDriver->leakage;
        leakage += numColumn * config->technology.tech->vdd() * CalculateGateLeakage(INV, 1, widthInvNmos, widthInvPmos, config->input.temperature, config->technology.tech);
        leakage += numColumn * voltagePrecharge * CalculateGateLeakage(INV, 1, 0, widthPMOSBitlinePrecharger,
                config->input.temperature, config->technology.tech);

        /* Dynamic energy */
        /* We don't count bitline precharge energy into account because it is a charging process */
        readDynamicEnergy = outputDriver->readDynamicEnergy;
        readDynamicEnergy += capLoadInv * config->technology.tech->vdd() * config->technology.tech->vdd() * numColumn;
        writeDynamicEnergy = 0;		/* No precharging is needed during the write operation */
    }
}

void Precharger::PrintProperty() {
    std::cout << "Precharger Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
