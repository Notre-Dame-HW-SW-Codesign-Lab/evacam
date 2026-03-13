#include "CAM_Encoder.h"
#include "formula.h"

CAM_Encoder::CAM_Encoder() {
    initialized = false;
    capLoad = resLoad = 0;
    numInputBit = 0;
    widthNorN = widthNorP = 0;
    capNorInput = capNorOutput = 0;
    rampInput = rampOutput = 0;
    widthN = widthP = 0;
    capInvInput = capInvOutput = 0;
    numStage = numBasicEncoder = numAdr = 0;
    areaOptimizationLevel = latency_first;
}
void CAM_Encoder::Initialize(int _numInputBit, BufferDesignTarget _areaOptimizationLevel, 
        double _capLoad, double _resLoad, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[CAM_Encoder] Warning: Already initialized!";
    numInputBit = _numInputBit;
    capLoad = _capLoad;
    resLoad = _resLoad;
    areaOptimizationLevel = _areaOptimizationLevel;
    config= _config;
    if (numInputBit > pow(2,27)) {
        std::cout << "[CAM_Encoder] Error: Invalid number of subarray bits" <<std::endl;
        exit(-1);
    }
    numBasicEncoder = (int)(numInputBit/8) + ( (numInputBit%8)>0 );
    numAdr = (int)log2(numInputBit);
    double tmp = numBasicEncoder;
    numStage = 0;
    while(tmp >= 1) {
        numStage++;
        tmp /= 8;
        numBasicEncoder += ( (int)tmp + ( (int)(tmp)%8)>0 );
    }
    widthN = MIN_NMOS_SIZE * config->tech->featureSize();
    widthP = config->tech->pnSizeRatio() * MIN_NMOS_SIZE * config->tech->featureSize();
    CalculateGateCapacitance(INV, 2, widthN, widthP, config->tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->tech, &capInvInput, &capInvOutput);
    double logicEffort = (2+config->tech->pnSizeRatio()) / (1+config->tech->pnSizeRatio());
    outputDriver.Initialize(logicEffort, capInvOutput, capLoad, resLoad, false, areaOptimizationLevel, 0, config);

    widthNorN = MIN_NMOS_SIZE * config->tech->featureSize();
    widthNorP = config->tech->pnSizeRatio() * MIN_NMOS_SIZE * config->tech->featureSize();
    CalculateGateCapacitance(NOR, 2, widthNorN, widthNorP, config->tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->tech, &capNorInput, &capNorOutput);
    BasicEncoder.Initialize(8, capNorInput, 0, config /*TODO gate resistance */);
    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void CAM_Encoder::CalculateArea() {
    if (!initialized) {
        std::cout << "[CAM_Encoder] Error: Require initialization first!" << std::endl;
    } else {
        outputDriver.CalculateArea();
        BasicEncoder.CalculateArea();

        area = outputDriver.area * numAdr + BasicEncoder.area * numBasicEncoder;

        // TODO: a better layout
        height = BasicEncoder.height * (int)(numInputBit/8);
        width = area / height;
    }
}

void CAM_Encoder::CalculateRC() {
    if (!initialized) {
        std::cout << "[CAM_Encoder] Error: Require initialization first!" << std::endl;
    } else {
        outputDriver.CalculateRC();
        BasicEncoder.CalculateRC();
    }
}

void CAM_Encoder::CalculateLatency(double _rampInput) {
    if (!initialized) {
        std::cout << "[CAM_Encoder] Error: Require initialization first!" << std::endl;
    } else {
        rampInput = _rampInput;
        BasicEncoder.CalculateLatency(rampInput);
        outputDriver.CalculateLatency(rampInput /* TODO not exactly */);
        double lastRamp = rampInput;
        for(int i=0;i<numStage;i++){
            BasicEncoder.CalculateLatency(lastRamp);
            lastRamp = BasicEncoder.rampOutput;
            readLatency += BasicEncoder.readLatency;
        }
        writeLatency = readLatency;
        rampOutput = outputDriver.rampOutput;
    }
}

void CAM_Encoder::CalculatePower() {
    if (!initialized) {
        std::cout << "[CAM_Encoder] Error: Require initialization first!" << std::endl;
    } else {
        outputDriver.CalculatePower();
        BasicEncoder.CalculatePower();
        leakage = outputDriver.leakage * numAdr + BasicEncoder.leakage * numBasicEncoder;
        readDynamicEnergy = outputDriver.readDynamicEnergy * numAdr + BasicEncoder.readDynamicEnergy * numBasicEncoder;
        writeDynamicEnergy = readDynamicEnergy;
    }
}

void CAM_Encoder::PrintProperty() {
    std::cout << "8 to 3 CAM_Encoder Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

