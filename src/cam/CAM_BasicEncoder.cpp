#include "CAM_BasicEncoder.h"
#include "formula.h"

CAM_BasicEncoder::CAM_BasicEncoder() {
    initialized = false;
    capLoad = resLoad = 0;
    numInputBit = numNorInput = numNorGate = 0;
    widthNorN = widthNorP = 0;
    capNorInput = capNorOutput = 0;
    rampInput = rampOutput = 0;
    widthN = widthP = 0;
    widthNandN = widthNandP = 0;
    capDyn = 0;
    capNandInput = capNandOutput = 0;
    capInvInput = capInvOutput = 0;
}
void CAM_BasicEncoder::Initialize(int _numInputBit, double _capLoad, double _resLoad, 
        std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[CAM_BasicEncoder] Warning: Already initialized!";
    numInputBit = _numInputBit;
    capLoad = _capLoad;
    resLoad = _resLoad;
    config = _config;
    if (numInputBit == 8) {
        //TODO: Assuming we only have drivers at carry-in, z-output has no drivers
        widthNorN = MIN_NMOS_SIZE * config->technology.tech->featureSize();
        widthNorP = 2 * config->technology.tech->pnSizeRatio() * MIN_NMOS_SIZE * config->technology.tech->featureSize();
        widthNandN = 2 * MIN_NMOS_SIZE * config->technology.tech->featureSize();
        widthNandP = config->technology.tech->pnSizeRatio() * MIN_NMOS_SIZE * config->technology.tech->featureSize();
        widthN = MIN_NMOS_SIZE * config->technology.tech->featureSize();
        widthP = config->technology.tech->pnSizeRatio() * MIN_NMOS_SIZE * config->technology.tech->featureSize();
        double logicEffortCarry = 2 / (1+config->technology.tech->pnSizeRatio());
        double tmp;
        CalculateGateCapacitance(NOR, 8, widthN*2, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &tmp, &capDyn);
        outputDriver.Initialize(logicEffortCarry, capDyn, capLoad, resLoad, true, latency_first, 0, config);
    }
    else {
        // TODO 4-to-2 and 2-to-1 encoder
        throw std::runtime_error("[CAM_BasicEncoder] Error: only 8-to-3 encoder blocks are supported.");
    }
    initialized = true;
    CalculateArea();
    CalculateRC();
}

void CAM_BasicEncoder::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[CAM_BasicEncoder]");
    } else {
        outputDriver.CalculateArea();
        if (numInputBit == 8) {
            outputDriver.CalculateArea();
            // 4-input OR is 2 NOR2 + 1 NAND2
            // and the tre-state output by trans-gate
            double hNOR, wNOR, hNAND, wNAND, hTRI, wTRI;
            CalculateGateArea(NOR, 2, widthNorN, widthNorP, config->technology.tech->featureSize()*40, *config->technology.tech, &hNOR, &wNOR, 
                    config->peripherals.useUpdatedLib);
            // assuming the second stage NAND is twice as large as the first stage NOR
            CalculateGateArea(NAND, 2, widthNandN*2, widthNandP*2, config->technology.tech->featureSize()*40, *config->technology.tech, &hNAND, 
                    &wNAND, config->peripherals.useUpdatedLib);
            CalculateGateArea(INV, 1, widthN, widthP, config->technology.tech->featureSize()*40, *config->technology.tech, &hTRI, &wTRI, 
                    config->peripherals.useUpdatedLib);
            // TODO: a better layout
            width = std::max(std::max(wNOR, wNAND), wTRI);
            height = hTRI + hNAND + hNOR * 2;
            height *= 3;
            area = height * width;

            // dynamic circuit for carry in
            double hPullDown, wPullDown, hCLK, wCLK;
            // the clock part
            CalculateGateArea(INV, 1, widthN * 2, widthP, config->technology.tech->featureSize()*40, *config->technology.tech, &hCLK, &wCLK,
                    config->peripherals.useUpdatedLib);
            // the pull down NMOS part
            CalculateGateArea(INV, 8, widthN * 2, 0, config->technology.tech->featureSize()*40, *config->technology.tech, &hPullDown, &wPullDown,
                    config->peripherals.useUpdatedLib);
            // TODO: a better layout
            area += ( hCLK*wCLK + hPullDown*wPullDown );
            height = area / width;
        } else {
            // TODO 4-to-2 and 2-to-1 encoder
            throw std::runtime_error("[CAM_BasicEncoder] Error: only 8-to-3 encoder blocks are supported.");
        }
    }
}

void CAM_BasicEncoder::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[CAM_BasicEncoder]");
    } else {
        outputDriver.CalculateRC();
        CalculateGateCapacitance(NOR, 2, widthNorN, widthNorP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &capNorInput, &capNorOutput);
        CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &capNandInput, &capNandOutput);
        CalculateGateCapacitance(INV, 2, widthN, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &capInvInput, &capInvOutput);
    }
}

void CAM_BasicEncoder::CalculateLatency(double _rampInput) {
    if (!initialized) {
        ThrowInitializationError("[CAM_BasicEncoder]");
    } else {
        rampInput = _rampInput;
        if (numInputBit == 8) {
            // TODO: the output mem_data latency is not considered, since the carry in signal is much slower when array size larger than 8
            double resPullDown;
            double capLoad;
            double tr;	/* time constant */
            double gm;	/* transconductance */
            double beta;	/* for horowitz calculation */
            double rampInputForDriver;

            resPullDown = CalculateOnResistance(widthN*2, NMOS, config->input.temperature, *config->technology.tech);
            // carry in also gives to the tri-state gate
            capLoad = capDyn + outputDriver.capInput[0] + capInvInput;
            tr = resPullDown * capLoad;
            gm = CalculateTransconductance(widthNandN, NMOS, *config->technology.tech);
            beta = 1 / (resPullDown * gm);
            readLatency = horowitz(tr, beta, rampInput, &rampInputForDriver);
            outputDriver.CalculateLatency(rampInputForDriver);
            readLatency += outputDriver.readLatency;
            writeLatency = readLatency;
            rampOutput = outputDriver.rampOutput;
        } else {
            // TODO 4-to-2 and 2-to-1 encoder
            throw std::runtime_error("[CAM_BasicEncoder] Error: only 8-to-3 encoder blocks are supported.");
        }
    }
}

void CAM_BasicEncoder::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[CAM_BasicEncoder]");
    } else {
        outputDriver.CalculatePower();
        if (numInputBit == 8) {
            double cap;
            leakage = outputDriver.leakage;
            readDynamicEnergy = 0;
            // leakage for OR gates and tri-state output
            leakage += ( CalculateGateLeakage(NOR, 2, widthNorN, widthNorP, config->input.temperature, *config->technology.tech) * config->technology.tech->vdd() *2*3);
            leakage += ( CalculateGateLeakage(NAND, 2, widthNandN, widthNandP, config->input.temperature, *config->technology.tech) * config->technology.tech->vdd() *3);
            leakage += ( CalculateGateLeakage(INV, 1, widthN, widthP, config->input.temperature, *config->technology.tech) * config->technology.tech->vdd() *3);
            // leakage for the dynamic logic
            int tempIndex = (int)config->input.temperature - 300;
            if ((tempIndex > 100) || (tempIndex < 0)) {
                throw std::runtime_error("Error: Temperature is out of range");
            }
            const double *leakP = config->technology.tech->currentOffPmos().data();
            double leakageP = widthP * leakP[tempIndex];
            leakage += ( leakageP * config->technology.tech->vdd() );

            // dynamic for the dynamic logic
            cap = outputDriver.capInput[0] + capDyn + capInvInput;
            readDynamicEnergy += (cap * config->technology.tech->vdd() * config->technology.tech->vdd());
            // dynamic power for the OR gates and the tri-state output

            cap = capNorOutput *2 + capNandInput + capInvInput;
            readDynamicEnergy += (cap * config->technology.tech->vdd() * config->technology.tech->vdd() * 3);
            writeDynamicEnergy = readDynamicEnergy;
        }  else {
            // TODO 4-to-2 and 2-to-1 encoder
            throw std::runtime_error("[CAM_BasicEncoder] Error: only 8-to-3 encoder blocks are supported.");
        }
    }
}

void CAM_BasicEncoder::PrintProperty() {
    std::cout << "8 to 3 CAM_BasicEncoder Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
