#include "Mux.h"
#include "formula.h"

namespace {

bool HasMux(int numInput, long long numMux) {
    return numInput > 1 && numMux > 0;
}

}  // namespace

Mux::Mux() {
    initialized = false;
    capForPreviousPowerCalculation = 0;
    capForPreviousDelayCalculation = 0;
    capNMOSPassTransistor = 0;
    resNMOSPassTransistor = 0;
}

void Mux::Initialize(int _numInput, long long _numMux, double _capLoad, double _capInputNextStage, 
        double _minDriverCurrent, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized) {
        _config->logger.Verbose() << "[Mux] Warning: Already initialized!";
    }

    numInput = _numInput;
    numMux = _numMux;
    capLoad = _capLoad;
    capInputNextStage = _capInputNextStage;
    minDriverCurrent = _minDriverCurrent;
    config = _config;

    const bool hasMux = HasMux(numInput, numMux);
    if (hasMux) {
        const auto& input = config->input;
        const auto& technology = *config->technology.tech;
        const auto& cell = *config->technology.cell;
        const double featureSize = technology.featureSize();
        const double minNMOSWidth = minDriverCurrent / technology.currentOnNmos()[input.temperature - 300];
        const bool needsIrDropSizing = cell.memCellType == MRAM
            || cell.memCellType == PCRAM
            || cell.memCellType == memristor
            || cell.memCellType == FEFETRAM;

        if (needsIrDropSizing) {
            // Mux resistance should be small enough for voltage dividing.
            const double maxResNMOSPassTransistor = cell.resistanceOn * IR_DROP_TOLERANCE;
            const double maxNMOSWidth = input.maxNmosSize * featureSize;
            widthNMOSPassTransistor = CalculateOnResistance(
                    featureSize,
                    NMOS,
                    input.temperature,
                    technology)
                * featureSize / maxResNMOSPassTransistor;

            // Change the transistor size to avoid severe IR drop.
            if (widthNMOSPassTransistor > maxNMOSWidth) {
                widthNMOSPassTransistor = maxNMOSWidth;
            }
            widthNMOSPassTransistor = MAX(
                    MAX(widthNMOSPassTransistor, minNMOSWidth),
                    6 * MIN_NMOS_SIZE * featureSize);
        } else {
            widthNMOSPassTransistor = MAX(6 * MIN_NMOS_SIZE * featureSize, minNMOSWidth);
        }
    }

    initialized = true;
    CalculateArea();
    CalculateRC();
}

void Mux::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[Mux]");
        return;
    }

    const bool hasMux = HasMux(numInput, numMux);
    if (hasMux) {
        double h, w;
        const auto& technology = *config->technology.tech;
        CalculateGateArea(
                INV,
                1,
                widthNMOSPassTransistor,
                0,
                technology.featureSize() * 40,
                technology,
                &h,
                &w,
                config->peripherals.useUpdatedLib);
        width = numMux * numInput * w;
        height = h;
        area = width * height;
    } else {
        height = width = area = 0;
    }
}

void Mux::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[Mux]");
        return;
    }

    const bool hasMux = HasMux(numInput, numMux);
    if (hasMux) {
        const auto& input = config->input;
        const auto& technology = *config->technology.tech;
        capNMOSPassTransistor = CalculateDrainCap(
                widthNMOSPassTransistor,
                NMOS,
                technology.featureSize() * 40,
                technology);
        capForPreviousPowerCalculation = capNMOSPassTransistor;
        capOutput = numInput * capNMOSPassTransistor;
        capForPreviousDelayCalculation = capOutput + capNMOSPassTransistor + capLoad;
        resNMOSPassTransistor = CalculateOnResistance(
                widthNMOSPassTransistor,
                NMOS,
                input.temperature,
                technology);
    }
}

void Mux::CalculateLatency(double _rampInput) {  //rampInput is actually useless in Mux module
    if (!initialized) {
        ThrowInitializationError("[Mux]");
        return;
    }

    const bool hasMux = HasMux(numInput, numMux);
    rampInput = _rampInput;
    rampOutput = _rampInput;

    if (hasMux) {
        double tr = resNMOSPassTransistor * (capOutput + capLoad);
        readLatency = 2.3 * tr;
        writeLatency = readLatency;
    } else {
        readLatency = writeLatency = 0;
    }
}

void Mux::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[Mux]");
        return;
    }

    const bool hasMux = HasMux(numInput, numMux);
    if (hasMux) {
        const auto& technology = *config->technology.tech;
        leakage = 0; // TODO: add mux leakage model
        readDynamicEnergy = (capOutput + capInputNextStage)
            * technology.vdd()
            * (technology.vdd() - technology.vth());
        readDynamicEnergy *= numMux;  //worst-case dynamic power analysis
        writeDynamicEnergy = readDynamicEnergy;
    } else {
        readDynamicEnergy = writeDynamicEnergy = leakage = 0;
    }
}

void Mux::PrintProperty() {
    std::cout << "Mux Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
