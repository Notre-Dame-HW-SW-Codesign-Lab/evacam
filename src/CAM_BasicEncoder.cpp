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
        widthNorN = MIN_NMOS_SIZE * config->tech->featureSize();
        widthNorP = 2 * config->tech->pnSizeRatio() * MIN_NMOS_SIZE * config->tech->featureSize();
        widthNandN = 2 * MIN_NMOS_SIZE * config->tech->featureSize();
        widthNandP = config->tech->pnSizeRatio() * MIN_NMOS_SIZE * config->tech->featureSize();
        widthN = MIN_NMOS_SIZE * config->tech->featureSize();
        widthP = config->tech->pnSizeRatio() * MIN_NMOS_SIZE * config->tech->featureSize();
        double logicEffortCarry = 2 / (1+config->tech->pnSizeRatio());
        double tmp;
        CalculateGateCapacitance(NOR, 8, widthN*2, widthP, config->tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->tech, &tmp, &capDyn);
        outputDriver.Initialize(logicEffortCarry, capDyn, capLoad, resLoad, true, latency_first, 0, config);
    }
    else {
        // TODO 4-to-2 and 2-to-1 encoder
        std::cout << "[CAM_BasicEncoder] Error: Only support 8-to-3 Encoder block by now!" << std::endl;
        return;
    }
    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void CAM_BasicEncoder::CalculateArea() {
    if (!initialized) {
        std::cout << "[CAM_BasicEncoder] Error: Require initialization first!" << std::endl;
    } else {
        outputDriver.CalculateArea();
        if (numInputBit == 8) {
            outputDriver.CalculateArea();
            // 4-input OR is 2 NOR2 + 1 NAND2
            // and the tre-state output by trans-gate
            double hNOR, wNOR, hNAND, wNAND, hTRI, wTRI;
            CalculateGateArea(NOR, 2, widthNorN, widthNorP, config->tech->featureSize()*40, config->tech, &hNOR, &wNOR, 
                    config->UseUpdatedLib);
            // assuming the second stage NAND is twice as large as the first stage NOR
            CalculateGateArea(NAND, 2, widthNandN*2, widthNandP*2, config->tech->featureSize()*40, config->tech, &hNAND, 
                    &wNAND, config->UseUpdatedLib);
            CalculateGateArea(INV, 1, widthN, widthP, config->tech->featureSize()*40, config->tech, &hTRI, &wTRI, 
                    config->UseUpdatedLib);
            // TODO: a better layout
            width = MAX(MAX(wNOR, wNAND), wTRI);
            height = hTRI + hNAND + hNOR * 2;
            height *= 3;
            area = height * width;

            // dynamic circuit for carry in
            double hPullDown, wPullDown, hCLK, wCLK;
            // the clock part
            CalculateGateArea(INV, 1, widthN * 2, widthP, config->tech->featureSize()*40, config->tech, &hCLK, &wCLK,
                    config->UseUpdatedLib);
            // the pull down NMOS part
            CalculateGateArea(INV, 8, widthN * 2, 0, config->tech->featureSize()*40, config->tech, &hPullDown, &wPullDown,
                    config->UseUpdatedLib);
            // TODO: a better layout
            area += ( hCLK*wCLK + hPullDown*wPullDown );
            height = area / width;
        } else {
            // TODO 4-to-2 and 2-to-1 encoder
            std::cout << "[CAM_BasicEncoder] Error: Only support 8-to-3 Encoder block by now!" << std::endl;
            return;
        }
    }
}

void CAM_BasicEncoder::CalculateRC() {
    if (!initialized) {
        std::cout << "[CAM_BasicEncoder] Error: Require initialization first!" << std::endl;
    } else {
        outputDriver.CalculateRC();
        CalculateGateCapacitance(NOR, 2, widthNorN, widthNorP, config->tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->tech, &capNorInput, &capNorOutput);
        CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, config->tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->tech, &capNandInput, &capNandOutput);
        CalculateGateCapacitance(INV, 2, widthN, widthP, config->tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->tech, &capInvInput, &capInvOutput);
    }
}

void CAM_BasicEncoder::CalculateLatency(double _rampInput) {
    if (!initialized) {
        std::cout << "[CAM_BasicEncoder] Error: Require initialization first!" << std::endl;
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

            resPullDown = CalculateOnResistance(widthN*2, NMOS, config->temperature, config->tech);
            // carry in also gives to the tri-state gate
            capLoad = capDyn + outputDriver.capInput[0] + capInvInput;
            tr = resPullDown * capLoad;
            gm = CalculateTransconductance(widthNandN, NMOS, config->tech);
            beta = 1 / (resPullDown * gm);
            readLatency = horowitz(tr, beta, rampInput, &rampInputForDriver);
            outputDriver.CalculateLatency(rampInputForDriver);
            readLatency += outputDriver.readLatency;
            writeLatency = readLatency;
            rampOutput = outputDriver.rampOutput;
        } else {
            // TODO 4-to-2 and 2-to-1 encoder
            std::cout << "[CAM_BasicEncoder] Error: Only support 8-to-3 Encoder block by now!" << std::endl;
            return;
        }
    }
}

void CAM_BasicEncoder::CalculatePower() {
    if (!initialized) {
        std::cout << "[CAM_BasicEncoder] Error: Require initialization first!" << std::endl;
    } else {
        outputDriver.CalculatePower();
        if (numInputBit == 8) {
            double cap;
            leakage = outputDriver.leakage;
            readDynamicEnergy = 0;
            // leakage for OR gates and tri-state output
            leakage += ( CalculateGateLeakage(NOR, 2, widthNorN, widthNorP, config->temperature, config->tech) * config->tech->vdd() *2*3);
            leakage += ( CalculateGateLeakage(NAND, 2, widthNandN, widthNandP, config->temperature, config->tech) * config->tech->vdd() *3);
            leakage += ( CalculateGateLeakage(INV, 1, widthN, widthP, config->temperature, config->tech) * config->tech->vdd() *3);
            // leakage for the dynamic logic
            int tempIndex = (int)config->temperature - 300;
            if ((tempIndex > 100) || (tempIndex < 0)) {
                throw std::runtime_error("Error: Temperature is out of range");
            }
            const double *leakP = config->tech->currentOffPmos().data();
            double leakageP = widthP * leakP[tempIndex];
            leakage += ( leakageP * config->tech->vdd() );

            // dynamic for the dynamic logic
            cap = outputDriver.capInput[0] + capDyn + capInvInput;
            readDynamicEnergy += (cap * config->tech->vdd() * config->tech->vdd());
            // dynamic power for the OR gates and the tri-state output

            cap = capNorOutput *2 + capNandInput + capInvInput;
            readDynamicEnergy += (cap * config->tech->vdd() * config->tech->vdd() * 3);
            writeDynamicEnergy = readDynamicEnergy;
        }  else {
            // TODO 4-to-2 and 2-to-1 encoder
            std::cout << "[CAM_BasicEncoder] Error: Only support 8-to-3 Encoder block by now!" << std::endl;
            return;
        }
    }
}

void CAM_BasicEncoder::PrintProperty() {
    std::cout << "8 to 3 CAM_BasicEncoder Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
