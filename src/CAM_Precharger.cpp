#include "../include/CAM_Precharger.h"
#include "../include/formula.h"
void CAM_Precharger::Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline, 
        std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire) {
    if (initialized)
        std::cout << "[Precharger] Warning: Already initialized!" << std::endl;

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
    widthPMOSBitlinePrecharger = PRECHARGER_SIZE * config->tech->featureSize;
    capLoadInv  = CalculateGateCap(widthPMOSBitlineEqual, config->tech) + 2 * CalculateGateCap(widthPMOSBitlinePrecharger, config->tech)
        + CalculateDrainCap(widthInvNmos, NMOS, config->tech->featureSize*40, config->tech)
        + CalculateDrainCap(widthInvPmos, PMOS, config->tech->featureSize*40, config->tech);
    capOutputBitlinePrecharger = CalculateDrainCap(widthPMOSBitlinePrecharger, PMOS, config->tech->featureSize*40, config->tech) + CalculateDrainCap(widthPMOSBitlineEqual, PMOS, config->tech->featureSize*40, config->tech);
    double capInputInv         = CalculateGateCap(widthInvNmos, config->tech) + CalculateGateCap(widthInvPmos, config->tech);
    capLoadPerColumn           = capInputInv + capWireLoadPerColumn;
    double capLoadOutputDriver = numColumn * capLoadPerColumn;
    outputDriver = std::make_shared<OutputDriver>();
    outputDriver->Initialize(1, capInputInv, capLoadOutputDriver, 0 /* TODO */, true, latency_first, 0, config);  /* Always Latency First */

    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void CAM_Precharger::CalculateArea() {
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
        // begin_change
        //width = 2 * wBitlinePrechareger + wBitlineEqual;
        width = 2 * wBitlinePrechareger;
        // end_change
        width = MAX(width, wInverter);
        width *= numColumn;
        width = MAX(width, outputDriver->width);
        // begin_charge
        //height = MAX(hBitlinePrechareger, hBitlineEqual);
        height = hBitlinePrechareger;
        // end_change
        height += hInverter;
        height += outputDriver->height;
        area = height * width;
    }
}

CAM_Precharger & CAM_Precharger::operator=(const CAM_Precharger &rhs) {
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
