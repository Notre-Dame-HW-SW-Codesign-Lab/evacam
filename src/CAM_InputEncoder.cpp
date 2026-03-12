#include "../include/CAM_InputEncoder.h"
#include "../include/formula.h"

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
/*
   CAM_InputEncoder::~CAM_InputEncoder() {
}
 */
void CAM_InputEncoder::Initialize(TypeOfInputEncoder _typeEncoder, bool _isCustom, double _capLoad, 
        double _resLoad, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[CAM_InputEncoder] Warning: Already initialized!";

    outputDriver = std::make_shared<OutputDriver>();

    capLoad = _capLoad;
    resLoad = _resLoad;
    typeEncoder = _typeEncoder;
    isCustom = _isCustom;
    config = _config;
    if(isCustom) {
        // TODO Customizable Input Encoder
        std::cout<<"[CAM_InputEncoder] Error: Customized input encoder is under development."<<std::endl;
        return;
    }
    else if(typeEncoder == encoding_two_bit) {
        numInputBits = 2;
        /* gate sizing: built up with 2-input nand */
        widthNandN = 2 * MIN_NMOS_SIZE * config->tech->featureSize;
        widthNandP = config->tech->pnSizeRatio * MIN_NMOS_SIZE * config->tech->featureSize;
        double logicEffort = (2+config->tech->pnSizeRatio) / (1+config->tech->pnSizeRatio);
        CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, config->tech->featureSize*MAX_TRANSISTOR_HEIGHT, config->tech, &capNandIn, &capNandOut);
        outputDriver->Initialize(logicEffort, capNandOut, capLoad, resLoad, true, latency_first, 0, config);
        initialized = true;
    }
    else {
        // TODO Encoding scheme look up table
        std::cout<<"[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version."<<std::endl;
        return;
    }
}

void CAM_InputEncoder::CalculateArea(){
    if (!initialized) {
        std::cout << "[CAM_InputEncoder] Error: Require initialization first!" << std::endl;
    } else {
        if(isCustom) {
            // TODO Customizable Input Encoder
            std::cout<<"[CAM_InputEncoder] Error: Customized input encoder is under development."<<std::endl;
            return;
        }
        else if(typeEncoder == encoding_two_bit) {
            //outputDriver->CalculateArea();
            height = outputDriver->height * numInputBits * 2;
            width = outputDriver->width;
            double hNand, wNand;
            CalculateGateArea(NAND, 2, widthNandN, widthNandP, config->tech->featureSize*MAX_TRANSISTOR_HEIGHT, config->tech, &hNand, &wNand, config->UseUpdatedLib);
            width += wNand;
            height = MAX(height, hNand * numInputBits * 2);
            area = height * width;
        }
        else {
            // TODO Encoding scheme look up table
            std::cout<<"[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version."<<std::endl;
            return;
        }
    }
}

void CAM_InputEncoder::CalculateRC() {
    if (!initialized) {
        std::cout << "[CAM_InputEncoder] Error: Require initialization first!" << std::endl;
    } else {
        if(isCustom) {
            // TODO Customizable Input Encoder
            std::cout<<"[CAM_InputEncoder] Error: Customized input encoder is under development."<<std::endl;
            return;
        }
        else if(typeEncoder == encoding_two_bit) {
            //outputDriver->CalculateRC();
            CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, config->tech->featureSize*MAX_TRANSISTOR_HEIGHT, config->tech, &capNandIn, &capNandOut);
        }
        else {
            // TODO Encoding scheme look up table
            std::cout<<"[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version."<<std::endl;
            return;
        }
    }
}

void CAM_InputEncoder::CalculateLatency(double _rampInput) {
    if (!initialized) {
        std::cout << "[CAM_InputEncoder] Error: Require initialization first!" << std::endl;
    } else {

        if(isCustom) {
            // TODO Customizable Input Encoder
            std::cout<<"[CAM_InputEncoder] Error: Customized input encoder is under development."<<std::endl;
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
            resPullDown = CalculateOnResistance(widthNandN, NMOS, config->temperature, config->tech);
            gm = CalculateTransconductance(widthNandN, NMOS, config->tech);
            beta = 1 / (resPullDown * gm);
            cap = capNandOut + outputDriver->capInput[0];
            tr = resPullDown * cap;
            readLatency = horowitz(tr, beta, rampInput, &rampInternal);
            outputDriver->CalculateLatency(rampInternal);
            readLatency += outputDriver->readLatency;
            writeLatency = readLatency;
            rampOutput = outputDriver->rampOutput;
        }
        else {
            // TODO Encoding scheme look up table
            std::cout<<"[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version."<<std::endl;
            return;
        }
    }
}

void CAM_InputEncoder::CalculatePower() {
    if (!initialized) {
        std::cout << "[CAM_InputEncoder] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculatePower();
        if(isCustom) {
            // TODO Customizable Input Encoder
            std::cout<<"[CAM_InputEncoder] Error: Customized input encoder is under development."<<std::endl;
            return;
        }
        else if(typeEncoder == encoding_two_bit) {
            leakage = CalculateGateLeakage(NAND, 2, widthNandN, widthNandP, config->temperature, config->tech) * config->tech->vdd * numInputBits * 2;
            leakage += outputDriver->leakage * numInputBits * 2;

            /* nand and the driver */
            double cap;
            cap = outputDriver->capInput[0] + capNandOut;
            readDynamicEnergy = numInputBits * 2 * cap * config->tech->vdd * config->tech->vdd;
            readDynamicEnergy += (numInputBits * 2 * outputDriver->readDynamicEnergy);
            writeDynamicEnergy = readDynamicEnergy;
        }
        else {
            // TODO Encoding scheme look up table
            std::cout<<"[CAM_InputEncoder] Error: Two-bit encoder in JSSC11 is the only scheme supported in this version."<<std::endl;
            return;
        }
    }
}

void CAM_InputEncoder::PrintProperty() {
    std::cout << "CAM_InputEncoder Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

CAM_InputEncoder & CAM_InputEncoder::operator=(const CAM_InputEncoder &rhs) {
    height = rhs.height;
    width = rhs.width;
    area = rhs.area;
    readLatency = rhs.readLatency;
    writeLatency = rhs.writeLatency;
    readDynamicEnergy = rhs.readDynamicEnergy;
    writeDynamicEnergy = rhs.writeDynamicEnergy;
    leakage = rhs.leakage;
    initialized = rhs.initialized;
    capLoad = rhs.capLoad;
    rampInput = rhs.rampInput;
    rampOutput = rhs.rampOutput;
    resLoad = rhs.resLoad;
    capNandIn = rhs.capNandIn;
    capNandOut = rhs.capNandOut;
    widthNandN = rhs.widthNandN;
    widthNandP = rhs.widthNandP;
    rampInput = rhs.rampInput;
    rampOutput = rhs.rampOutput;
    isCustom = rhs.isCustom;
    typeEncoder = rhs.typeEncoder;
    numInputBits = rhs.numInputBits;
    return *this;
}
