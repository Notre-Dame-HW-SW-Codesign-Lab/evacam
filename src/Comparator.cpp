#include "Comparator.h"
#include "formula.h"
void Comparator::Initialize(int _numTagBits, double _capLoad, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[Comparator] Warning: Already initialized!";

    numTagBits = _numTagBits / 4;  /* Assuming there are 4 quarter comparators. input tagbits is already a multiple of 4 */
    capLoad = _capLoad;
    widthNMOSInv[0] = 7.5 * config->tech->featureSize();
    widthPMOSInv[0] = 12.5 * config->tech->featureSize();
    widthNMOSInv[1] = 15 * config->tech->featureSize();
    widthPMOSInv[1] = 25 * config->tech->featureSize();
    widthNMOSInv[2] = 30 * config->tech->featureSize();
    widthPMOSInv[2] = 50 * config->tech->featureSize();
    widthNMOSInv[3] = 50 * config->tech->featureSize();
    widthPMOSInv[3] = 100 * config->tech->featureSize();
    widthNMOSComp = 12.5 * config->tech->featureSize();
    widthPMOSComp = 37.5 * config->tech->featureSize();

    config = _config;

    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void Comparator::CalculateArea() {
    if (!initialized) {
        std::cout << "[Comparator] Error: Require initialization first!" << std::endl;
    } else {
        double totalHeight = 0;
        double totalWidth = 0;
        double h, w;
        for (int i = 0; i < COMPARATOR_INV_CHAIN_LEN; i++) {
            CalculateGateArea(INV, 1, widthNMOSInv[i], widthPMOSInv[i], config->tech->featureSize()*40, config->tech, &h, &w,
                    config->UseUpdatedLib);
            totalHeight = MAX(totalHeight, h);
            totalWidth += w;
        }
        CalculateGateArea(NAND, 2, widthNMOSComp, 0, config->tech->featureSize()*40, config->tech, &h, &w,
                config->UseUpdatedLib);
        totalHeight += h;
        totalWidth = MAX(totalWidth, numTagBits * w);
        height = totalHeight * 1; // 4 quarter comparators can have different placement, here assumes 1*4
        width = totalWidth * 4;
        area = height * width;
    }
}

void Comparator::CalculateRC() {
    if (!initialized) {
        std::cout << "[Comparator] Error: Require initialization first!" << std::endl;
    } else {
        for (int i = 0; i < COMPARATOR_INV_CHAIN_LEN; i++) {
            CalculateGateCapacitance(INV, 1, widthNMOSInv[i], widthPMOSInv[i], config->tech->featureSize() * MAX_TRANSISTOR_HEIGHT, config->tech, &(capInput[i]), &(capOutput[i]));
        }
        double capComp, capTemp;
        CalculateGateCapacitance(NAND, 2, widthNMOSComp, 0, config->tech->featureSize()*40, config->tech, &capTemp, &capComp);
        capBottom = capOutput[COMPARATOR_INV_CHAIN_LEN-1] + numTagBits * capComp;
        capTop = numTagBits * capComp + CalculateDrainCap(widthPMOSComp, PMOS, config->tech->featureSize() * MAX_TRANSISTOR_HEIGHT, config->tech) + capLoad;
        resBottom = CalculateOnResistance(widthNMOSInv[COMPARATOR_INV_CHAIN_LEN-1], NMOS, config->temperature, config->tech);
        resTop = 2 * CalculateOnResistance(widthNMOSComp, NMOS, config->temperature, config->tech);
    }
}

void Comparator::CalculateLatency(double _rampInput) {
    if (!initialized) {
        std::cout << "[Comparator] Error: Require initialization first!" << std::endl;
    } else {
        rampInput = _rampInput;
        double resPullDown;
        double capNode;
        double tr;	/* time constant */
        double gm;	/* transconductance */
        double beta;	/* for horowitz calculation */
        double temp;
        readLatency = 0;
        for (int i = 0; i < COMPARATOR_INV_CHAIN_LEN - 1; i++) {
            resPullDown = CalculateOnResistance(widthNMOSInv[i], NMOS, config->temperature, config->tech);
            capNode = capOutput[i] + capInput[i+1];
            tr = resPullDown * capNode;
            gm = CalculateTransconductance(widthNMOSInv[i], NMOS, config->tech);
            beta = 1 / (resPullDown * gm);
            readLatency += horowitz(tr, beta, rampInput, &temp);
            rampInput = temp;	/* for next stage */
        }
        tr = resBottom * capBottom + (resBottom + resTop) * capTop;
        readLatency += horowitz(tr, 0, rampInput, &rampOutput);
        rampInput = _rampInput;
        writeLatency = readLatency;
    }
}

void Comparator::CalculatePower() {
    if (!initialized) {
        std::cout << "[Comparator] Error: Require initialization first!" << std::endl;
    } else {
        /* Leakage power */
        leakage = 0;
        for (int i = 0; i < COMPARATOR_INV_CHAIN_LEN; i++) {
            leakage += CalculateGateLeakage(INV, 1, widthNMOSInv[i], widthPMOSInv[i], config->temperature, config->tech)
                * config->tech->vdd();
        }
        leakage += numTagBits * CalculateGateLeakage(NAND, 2, widthNMOSComp, 0, config->temperature, config->tech)
            * config->tech->vdd();
        leakage *= 4;
        /* Dynamic energy */
        readDynamicEnergy = 0;
        double capNode;
        for (int i = 0; i < COMPARATOR_INV_CHAIN_LEN - 1; i++) {
            capNode = capOutput[i] + capInput[i+1];
            readDynamicEnergy += capNode * config->tech->vdd() * config->tech->vdd();
        }
        readDynamicEnergy += (capBottom + capTop) * config->tech->vdd() * config->tech->vdd();
        readDynamicEnergy *= 4;
        writeDynamicEnergy = readDynamicEnergy;
    }
}

void Comparator::PrintProperty() {
    std::cout << "Comparator Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

Comparator & Comparator::operator=(const Comparator &rhs) {
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
    numTagBits = rhs.numTagBits;
    capLoad = rhs.capLoad;
    widthNMOSComp = rhs.widthNMOSComp;
    widthPMOSComp = rhs.widthPMOSComp;
    capBottom = rhs.capBottom;
    capTop = rhs.capTop;
    resBottom = rhs.resBottom;
    resTop = rhs.resTop;
    for (int i = 0; i < COMPARATOR_INV_CHAIN_LEN; i++) {
        widthNMOSInv[i] = rhs.widthNMOSInv[i];
        widthPMOSInv[i] = rhs.widthPMOSInv[i];
        capInput[i] = rhs.capInput[i];
        capOutput[i] =rhs.capOutput[i];
    }
    rampInput = rhs.rampInput;
    rampOutput = rhs.rampOutput;

    return *this;
}
