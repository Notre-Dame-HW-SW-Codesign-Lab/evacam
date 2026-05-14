#include "CAM_Precharger.h"
#include "formula.h"

void CAM_Precharger::Initialize(
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

void CAM_Precharger::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[CAM_Precharger]");
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
        width = MAX(width, wInverter);
        width *= numColumn;
        width = MAX(width, outputDriver.width);

        // Height excludes equalization-device footprint for the same reason.
        height = hBitlinePrecharger;
        height += hInverter;
        height += outputDriver.height;
        area = height * width;
    }
}
