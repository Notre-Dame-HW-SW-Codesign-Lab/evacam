#include "../include/Mux.h"
#include "../include/formula.h"

Mux::Mux() {
    initialized = false;
    capForPreviousPowerCalculation = 0;
    capForPreviousDelayCalculation = 0;
    capNMOSPassTransistor = 0;
    resNMOSPassTransistor = 0;
}
/*
   Mux::~Mux() {
}
 */
void Mux::Initialize(int _numInput, long long _numMux, double _capLoad, double _capInputNextStage, 
        double _minDriverCurrent, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[Mux] Warning: Already initialized!";

    numInput = _numInput;
    numMux = _numMux;
    capLoad = _capLoad;
    capInputNextStage = _capInputNextStage;
    minDriverCurrent = _minDriverCurrent;
    config = _config;

    if ((numInput > 1) && (numMux > 0 )) {
        double minNMOSWidth = minDriverCurrent / config->tech->currentOnNmos[config->temperature - 300];
        if (config->cell->memCellType == MRAM || config->cell->memCellType == PCRAM || config->cell->memCellType == memristor || config->cell->memCellType == FEFETRAM) {
            /* Mux resistance should be small enough for voltage dividing */
            double maxResNMOSPassTransistor = config->cell->resistanceOn * IR_DROP_TOLERANCE;
            widthNMOSPassTransistor = CalculateOnResistance(config->tech->featureSize, NMOS, config->temperature, config->tech)
                * config->tech->featureSize / maxResNMOSPassTransistor;
            if (widthNMOSPassTransistor > config->maxNmosSize * config->tech->featureSize) {	// Change the transistor size to avoid severe IR drop
                widthNMOSPassTransistor = config->maxNmosSize * config->tech->featureSize;
            }
            widthNMOSPassTransistor = MAX(MAX(widthNMOSPassTransistor,minNMOSWidth), 6 * MIN_NMOS_SIZE * config->tech->featureSize);
        } else {
            widthNMOSPassTransistor = MAX(6 * MIN_NMOS_SIZE * config->tech->featureSize, minNMOSWidth);
        }
    }

    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void Mux::CalculateArea(){
    if (!initialized) {
        std::cout << "[Mux] Error: Require initialization first!" << std::endl;
    } else {
        if ((numInput > 1) && (numMux > 0 )) {
            double h,w;
            CalculateGateArea(INV, 1, widthNMOSPassTransistor, 0, config->tech->featureSize*40, config->tech, &h, &w, config->UseUpdatedLib);
            width = numMux * numInput * w;
            height = h;
            area = width * height;
        } else {
            height = width = area = 0;
        }
    }
}

void Mux::CalculateRC() {
    if (!initialized) {
        std::cout << "[Mux] Error: Require initialization first!" << std::endl;
    } else {
        if ((numInput > 1) && (numMux > 0 )) {
            capNMOSPassTransistor = CalculateDrainCap(widthNMOSPassTransistor, NMOS, config->tech->featureSize*40, config->tech);
            capForPreviousPowerCalculation = capNMOSPassTransistor;
            capOutput = numInput * capNMOSPassTransistor;
            capForPreviousDelayCalculation = capOutput + capNMOSPassTransistor + capLoad;
            resNMOSPassTransistor = CalculateOnResistance(widthNMOSPassTransistor, NMOS, config->temperature, config->tech);
        } else {
            ;	/* nothing to do */
        }
    }
}

void Mux::CalculateLatency(double _rampInput) {  //rampInput is actually useless in Mux module
    if (!initialized) {
        std::cout << "[Mux] Error: Require initialization first!" << std::endl;
    } else {
        if ((numInput > 1) && (numMux > 0 )) {
            rampInput = _rampInput;
            double tr;
            tr = resNMOSPassTransistor * (capOutput + capLoad);
            readLatency = 2.3 * tr;
            writeLatency = readLatency;
        } else {
            readLatency = writeLatency = 0;
        }
    }
}

void Mux::CalculatePower() {
    if (!initialized) {
        std::cout << "[Mux] Error: Require initialization first!" << std::endl;
    } else {
        if ((numInput > 1) && (numMux > 0 )) {
            leakage = 0; //TODO
            readDynamicEnergy = (capOutput + capInputNextStage) * config->tech->vdd * (config->tech->vdd - config->tech->vth);
            readDynamicEnergy *= numMux;  //worst-case dynamic power analysis
            writeDynamicEnergy = readDynamicEnergy;
        } else {
            readDynamicEnergy = writeDynamicEnergy = leakage = 0;
        }
    }
}

void Mux::PrintProperty() {
    std::cout << "Mux Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

Mux & Mux::operator=(const Mux &rhs) {
    height = rhs.height;
    width = rhs.width;
    area = rhs.area;
    readLatency = rhs.readLatency;
    writeLatency = rhs.writeLatency;
    readDynamicEnergy = rhs.readDynamicEnergy;
    writeDynamicEnergy = rhs.writeDynamicEnergy;
    leakage = rhs.leakage;
    initialized = rhs.initialized;
    numInput = rhs.numInput;
    numMux = rhs.numMux;
    capLoad = rhs.capLoad;
    capInputNextStage = rhs.capInputNextStage;
    minDriverCurrent = rhs.minDriverCurrent;
    capOutput = rhs.capOutput;
    widthNMOSPassTransistor = rhs.widthNMOSPassTransistor;
    resNMOSPassTransistor = rhs.resNMOSPassTransistor;
    capNMOSPassTransistor = rhs.capNMOSPassTransistor;
    capForPreviousDelayCalculation = rhs.capForPreviousDelayCalculation;
    capForPreviousPowerCalculation = rhs.capForPreviousPowerCalculation;
    rampInput = rhs.rampInput;
    rampOutput = rhs.rampOutput;

    return *this;
}
