#include "../include/Precharger.h"
#include "../include/formula.h"
/*
   Precharger::Precharger() {
// TODO Auto-generated constructor stub
initialized = false;
enableLatency = 0;
}

Precharger::~Precharger() {
// TODO Auto-generated destructor stub
}
 */
void Precharger::Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline, std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire) {
    if (initialized)
        _config->logger.Verbose() << "[Precharger] Warning: Already initialized!";

    outputDriver = std::make_shared<OutputDriver>();
    voltagePrecharge = _voltagePrecharge;
    numColumn  = _numColumn;
    capBitline = _capBitline;
    resBitline = _resBitline;
    config = _config;
    localWire = _localWire;

    capWireLoadPerColumn = config->cell->widthInFeatureSize * config->tech->featureSize * localWire->capWirePerUnit;
    resWireLoadPerColumn = config->cell->widthInFeatureSize * config->tech->featureSize * localWire->resWirePerUnit;
    widthInvNmos = MIN_NMOS_SIZE * config->tech->featureSize;
    widthInvPmos = widthInvNmos * config->tech->pnSizeRatio;
    widthPMOSBitlineEqual      = MIN_NMOS_SIZE * config->tech->featureSize;
    widthPMOSBitlinePrecharger = 6 * config->tech->featureSize;
    capLoadInv  = CalculateGateCap(widthPMOSBitlineEqual, config->tech) + 2 * CalculateGateCap(widthPMOSBitlinePrecharger, config->tech)
        + CalculateDrainCap(widthInvNmos, NMOS, config->tech->featureSize*40, config->tech)
        + CalculateDrainCap(widthInvPmos, PMOS, config->tech->featureSize*40, config->tech);
    capOutputBitlinePrecharger = CalculateDrainCap(widthPMOSBitlinePrecharger, PMOS, config->tech->featureSize*40, config->tech) + CalculateDrainCap(widthPMOSBitlineEqual, PMOS, config->tech->featureSize*40, config->tech);
    double capInputInv         = CalculateGateCap(widthInvNmos, config->tech) + CalculateGateCap(widthInvPmos, config->tech);
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
        std::cout << "[Precharger] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculateArea();
        double hBitlinePrechareger, wBitlinePrechareger;
        double hBitlineEqual, wBitlineEqual;
        double hInverter, wInverter;
        CalculateGateArea(INV, 1, 0, widthPMOSBitlinePrecharger, config->tech->featureSize*40, config->tech, &hBitlinePrechareger, &wBitlinePrechareger, config->UseUpdatedLib);
        CalculateGateArea(INV, 1, 0, widthPMOSBitlineEqual, config->tech->featureSize*40, config->tech, &hBitlineEqual, &wBitlineEqual, config->UseUpdatedLib);
        CalculateGateArea(INV, 1, widthInvNmos, widthInvPmos, config->tech->featureSize*40, config->tech, &hInverter, &wInverter, config->UseUpdatedLib);
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
        std::cout << "[Precharger] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculateRC();
        //more accurate RC model would include drain Capacitances of Precharger and Equalization PMOS transistors
        // TODO: the above
    }
}

void Precharger::CalculateLatency(double _rampInput){
    if (!initialized) {
        std::cout << "[Precharger] Error: Require initialization first!" << std::endl;
    } else {
        rampInput= _rampInput;
        outputDriver->CalculateLatency(rampInput);
        enableLatency = outputDriver->readLatency;
        double resPullDown;
        double tr;	/* time constant */
        double gm;	/* transconductance */
        double beta;	/* for horowitz calculation */
        double temp;
        resPullDown = CalculateOnResistance(widthInvNmos, NMOS, config->temperature, config->tech);
        tr = resPullDown * capLoadInv;
        gm = CalculateTransconductance(widthInvNmos, NMOS, config->tech);
        beta = 1 / (resPullDown * gm);
        enableLatency += horowitz(tr, beta, outputDriver->rampOutput, &temp);
        readLatency = 0;
        double resPullUp = CalculateOnResistance(widthPMOSBitlinePrecharger, PMOS,
                config->temperature, config->tech);
        double tau = resPullUp * (capBitline + capOutputBitlinePrecharger) + resBitline * capBitline / 2;
        gm = CalculateTransconductance(widthPMOSBitlinePrecharger, PMOS, config->tech);
        beta = 1 / (resPullUp * gm);
        readLatency += horowitz(tau, beta, temp, &rampOutput);
        writeLatency = readLatency;
    }
}

void Precharger::CalculatePower() {
    if (!initialized) {
        std::cout << "[Precharger] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculatePower();
        /* Leakage power */
        leakage = outputDriver->leakage;
        leakage += numColumn * config->tech->vdd * CalculateGateLeakage(INV, 1, widthInvNmos, widthInvPmos, config->temperature, config->tech);
        leakage += numColumn * voltagePrecharge * CalculateGateLeakage(INV, 1, 0, widthPMOSBitlinePrecharger,
                config->temperature, config->tech);

        /* Dynamic energy */
        /* We don't count bitline precharge energy into account because it is a charging process */
        readDynamicEnergy = outputDriver->readDynamicEnergy;
        readDynamicEnergy += capLoadInv * config->tech->vdd * config->tech->vdd * numColumn;
        writeDynamicEnergy = 0;		/* No precharging is needed during the write operation */
    }
}

void Precharger::PrintProperty() {
    std::cout << "Precharger Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

Precharger & Precharger::operator=(const Precharger &rhs) {
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
    outputDriver = rhs.outputDriver;
    capBitline = rhs.capBitline;
    resBitline = rhs.resBitline;
    capLoadInv = rhs.capLoadInv;
    capOutputBitlinePrecharger = rhs.capOutputBitlinePrecharger;
    capWireLoadPerColumn = rhs.capWireLoadPerColumn;
    resWireLoadPerColumn = rhs.resWireLoadPerColumn;
    enableLatency = rhs.enableLatency;
    numColumn = rhs.numColumn;
    widthPMOSBitlinePrecharger = rhs.widthPMOSBitlinePrecharger;
    widthPMOSBitlineEqual = rhs.widthPMOSBitlineEqual;
    capLoadPerColumn = rhs.capLoadPerColumn;
    rampInput = rhs.rampInput;
    rampOutput = rhs.rampOutput;

    return *this;
}
