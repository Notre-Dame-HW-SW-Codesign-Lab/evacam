#include "CAM_InputEncoder.h"
#include "formula.h"

CAM_InputEncoder::CAM_InputEncoder() {
    initialized = false;
    capLoad = 0;
    resLoad = 0;
    typeEncoder = encoding_two_bit;
    isCustom = false;
    numInputBits = 2;
    capNandIn = 0;
    capNandOut = 0;
    widthNandN = 0;
    widthNandP = 0;
    rampInput = 0;
    rampOutput = 0;
}
void CAM_InputEncoder::Initialize(TypeOfInputEncoder _typeEncoder, bool _isCustom, double _capLoad, 
        double _resLoad, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[CAM_InputEncoder] Warning: Already initialized!";

    capLoad = _capLoad;
    resLoad = _resLoad;
    typeEncoder = _typeEncoder;
    isCustom = _isCustom;
    config = _config;
    if(isCustom) {
        // TODO Customizable Input Encoder
        config->logger.Log()
            << "[CAM_InputEncoder] Error: Customized input encoder is under development.";
        return;
    }
    else if(typeEncoder == encoding_two_bit) {
        numInputBits = 2;
        /* gate sizing: built up with 2-input nand */
        widthNandN = 2 * MIN_NMOS_SIZE * config->technology.tech->featureSize();
        widthNandP = config->technology.tech->pnSizeRatio() * MIN_NMOS_SIZE * config->technology.tech->featureSize();
        double logicEffort = (2+config->technology.tech->pnSizeRatio()) / (1+config->technology.tech->pnSizeRatio());
        CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &capNandIn, &capNandOut);
        outputDriver.Initialize(logicEffort, capNandOut, capLoad, resLoad, true, latency_first, 0, config);
        initialized = true;
    }
    else {
        // TODO Encoding scheme look up table
        config->logger.Log()
            << "[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version.";
        return;
    }
}

void CAM_InputEncoder::CalculateArea(){
    if (!initialized) {
        ThrowInitializationError("[CAM_InputEncoder]");
    } else {
        if(isCustom) {
            // TODO Customizable Input Encoder
            config->logger.Log()
                << "[CAM_InputEncoder] Error: Customized input encoder is under development.";
            return;
        }
        else if(typeEncoder == encoding_two_bit) {
            //outputDriver.CalculateArea();
            height = outputDriver.height * numInputBits * 2;
            width = outputDriver.width;
            double hNand, wNand;
            CalculateGateArea(NAND, 2, widthNandN, widthNandP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &hNand, &wNand, config->peripherals.useUpdatedLib);
            width += wNand;
            height = std::max(height, hNand * numInputBits * 2);
            area = height * width;
        }
        else {
            // TODO Encoding scheme look up table
            config->logger.Log()
                << "[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version.";
            return;
        }
    }
}

void CAM_InputEncoder::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[CAM_InputEncoder]");
    } else {
        if(isCustom) {
            // TODO Customizable Input Encoder
            config->logger.Log()
                << "[CAM_InputEncoder] Error: Customized input encoder is under development.";
            return;
        }
        else if(typeEncoder == encoding_two_bit) {
            //outputDriver.CalculateRC();
            CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &capNandIn, &capNandOut);
        }
        else {
            // TODO Encoding scheme look up table
            config->logger.Log()
                << "[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version.";
            return;
        }
    }
}

void CAM_InputEncoder::CalculateLatency(double _rampInput) {
    if (!initialized) {
        ThrowInitializationError("[CAM_InputEncoder]");
    } else {

        if(isCustom) {
            // TODO Customizable Input Encoder
            config->logger.Log()
                << "[CAM_InputEncoder] Error: Customized input encoder is under development.";
            return;
        }
        else if(typeEncoder == encoding_two_bit) {
            rampInput = _rampInput;
            double resPullDown;
            double cap;
            double tr;		/* time constant */
            double gm;		/* transconductance */
            double beta;	/* for horowitz calculation */
            double rampInternal;

            /* nand and the driver */
            resPullDown = CalculateOnResistance(widthNandN, NMOS, config->input.temperature, *config->technology.tech);
            gm = CalculateTransconductance(widthNandN, NMOS, *config->technology.tech);
            beta = 1 / (resPullDown * gm);
            cap = capNandOut + outputDriver.capInput[0];
            tr = resPullDown * cap;
            readLatency = horowitz(tr, beta, rampInput, &rampInternal);
            outputDriver.CalculateLatency(rampInternal);
            readLatency += outputDriver.readLatency;
            writeLatency = readLatency;
            rampOutput = outputDriver.rampOutput;
        }
        else {
            // TODO Encoding scheme look up table
            config->logger.Log()
                << "[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version.";
            return;
        }
    }
}

void CAM_InputEncoder::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[CAM_InputEncoder]");
    } else {
        if(isCustom) {
            // TODO Customizable Input Encoder
            config->logger.Log()
                << "[CAM_InputEncoder] Error: Customized input encoder is under development.";
            return;
        }
        else if(typeEncoder == encoding_two_bit) {
            outputDriver.CalculatePower();
            leakage = CalculateGateLeakage(NAND, 2, widthNandN, widthNandP, config->input.temperature, *config->technology.tech) * config->technology.tech->vdd() * numInputBits * 2;
            leakage += outputDriver.leakage * numInputBits * 2;

            /* nand and the driver */
            double cap;
            cap = outputDriver.capInput[0] + capNandOut;
            readDynamicEnergy = numInputBits * 2 * cap * config->technology.tech->vdd() * config->technology.tech->vdd();
            readDynamicEnergy += (numInputBits * 2 * outputDriver.readDynamicEnergy);
            writeDynamicEnergy = readDynamicEnergy;
        }
        else {
            // TODO Encoding scheme look up table
            config->logger.Log()
                << "[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version.";
            return;
        }
    }
}

void CAM_InputEncoder::PrintProperty() {
    std::cout << "CAM_InputEncoder Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
