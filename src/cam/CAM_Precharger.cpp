#include "CAM_Precharger.h"
#include "formula.h"
void CAM_Precharger::Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline, 
        std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire) {
    if (initialized)
        _config->logger.Verbose() << "[Precharger] Warning: Already initialized!";

    voltagePrecharge = _voltagePrecharge;
    numColumn  = _numColumn;
    capBitline = _capBitline;
    resBitline = _resBitline;
    config = _config;
    localWire = _localWire;
    capWireLoadPerColumn = config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize() * localWire->capWirePerUnit;
    resWireLoadPerColumn = config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize() * localWire->resWirePerUnit;
    widthInvNmos = MIN_NMOS_SIZE * config->technology.tech->featureSize();
    widthInvPmos = widthInvNmos * config->technology.tech->pnSizeRatio();
    widthPMOSBitlineEqual      = MIN_NMOS_SIZE * config->technology.tech->featureSize();
    widthPMOSBitlinePrecharger = PRECHARGER_SIZE * config->technology.tech->featureSize();
    capLoadInv  = CalculateGateCap(widthPMOSBitlineEqual, config->technology.tech) + 2 * CalculateGateCap(widthPMOSBitlinePrecharger, config->technology.tech)
        + CalculateDrainCap(widthInvNmos, NMOS, config->technology.tech->featureSize()*40, config->technology.tech)
        + CalculateDrainCap(widthInvPmos, PMOS, config->technology.tech->featureSize()*40, config->technology.tech);
    capOutputBitlinePrecharger = CalculateDrainCap(widthPMOSBitlinePrecharger, PMOS, config->technology.tech->featureSize()*40, config->technology.tech) + CalculateDrainCap(widthPMOSBitlineEqual, PMOS, config->technology.tech->featureSize()*40, config->technology.tech);
    double capInputInv         = CalculateGateCap(widthInvNmos, config->technology.tech) + CalculateGateCap(widthInvPmos, config->technology.tech);
    capLoadPerColumn           = capInputInv + capWireLoadPerColumn;
    double capLoadOutputDriver = numColumn * capLoadPerColumn;
    outputDriver = std::make_unique<OutputDriver>();
    outputDriver->Initialize(1, capInputInv, capLoadOutputDriver, 0 /* TODO */, true, latency_first, 0, config);  /* Always Latency First */

    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void CAM_Precharger::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[CAM_Precharger]");
    } else {
        //outputDriver->CalculateArea();
        double hBitlinePrechareger, wBitlinePrechareger;
        double hBitlineEqual, wBitlineEqual;
        double hInverter, wInverter;
        CalculateGateArea(INV, 1, 0, widthPMOSBitlinePrecharger, config->technology.tech->featureSize()*40, config->technology.tech, &hBitlinePrechareger, &wBitlinePrechareger, config->peripherals.useUpdatedLib);
        CalculateGateArea(INV, 1, 0, widthPMOSBitlineEqual, config->technology.tech->featureSize()*40, config->technology.tech, &hBitlineEqual, &wBitlineEqual, config->peripherals.useUpdatedLib);
        CalculateGateArea(INV, 1, widthInvNmos, widthInvPmos, config->technology.tech->featureSize()*40, config->technology.tech, &hInverter, &wInverter, config->peripherals.useUpdatedLib);
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
