#include "../include/CAM_DataBuffer.h"
#include "../include/formula.h"

CAM_DataBuffer::CAM_DataBuffer() {
    // TODO Auto-generated constructor stub
    initialized = false;
    capLoad = 0;
    resLoad = 0;
    capNandIn = 0;
    capNandOut = 0;
    widthNandN = 0;
    widthNandP = 0;
    rampInput = 0;
    rampOutput = 0;
    differential = false;
}
/*
   CAM_DataBuffer::~CAM_DataBuffer() {
// TODO Auto-generated destructor stub
}
 */
void CAM_DataBuffer::Initialize(bool _differential, double _capLoad, double _resLoad, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[CAM_DataBuffer] Warning: Already initialized!";
    /* structure: D-latch, contains 4 nand */
    capLoad = _capLoad;
    resLoad = _resLoad;
    differential  = _differential;
    config = _config;
    /* gate sizing: built up with 2-input nand */
    widthNandN = 2 * MIN_NMOS_SIZE * config->tech->featureSize;
    widthNandP = config->tech->pnSizeRatio * MIN_NMOS_SIZE * config->tech->featureSize;
    /* logic effort: two stages of nand and then to the driver */
    double logicEffort = (2+config->tech->pnSizeRatio) / (1+config->tech->pnSizeRatio);
    logicEffort *= logicEffort;

    outputDriver = std::make_shared<OutputDriver>();

    /* driver's cap-in: both the input and output of nand */
    CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, config->tech->featureSize*MAX_TRANSISTOR_HEIGHT, config->tech, &capNandIn, &capNandOut);
    outputDriver->Initialize(logicEffort, capNandIn+capNandOut, capLoad, resLoad, false, latency_first, 0, config);
    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void CAM_DataBuffer::CalculateArea(){
    if (!initialized) {
        std::cout << "[CAM_DataBuffer] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculateArea();
        height = outputDriver->height * (1+differential);
        width = outputDriver->width * (1+differential);
        double hNand, wNand;
        CalculateGateArea(NAND, 2, widthNandN, widthNandP, config->tech->featureSize*MAX_TRANSISTOR_HEIGHT, config->tech, &hNand, &wNand, config->UseUpdatedLib);
        width += wNand*2;
        height = MAX(height, hNand*2);
        area = height * width;
    }
}

void CAM_DataBuffer::CalculateRC() {
    if (!initialized) {
        std::cout << "[CAM_DataBuffer] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculateRC();
        CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, config->tech->featureSize*MAX_TRANSISTOR_HEIGHT, config->tech, &capNandIn, &capNandOut);
    }
}

void CAM_DataBuffer::CalculateLatency(double _rampInput) {
    if (!initialized) {
        std::cout << "[DataBuffer] Error: Require initialization first!" << std::endl;
    } else {
        rampInput = _rampInput;
        double resPullDown;
        double cap;
        double tr;		/* time constant */
        double gm;		/* transconductance */
        double beta;	/* for horowitz calculation */
        double rampInternal;

        /* 2 stage nand and the driver */
        resPullDown = CalculateOnResistance(widthNandN, NMOS, config->temperature, config->tech);
        cap = capNandOut + capNandIn;
        tr = resPullDown * cap;
        gm = CalculateTransconductance(widthNandN, NMOS, config->tech);
        beta = 1 / (resPullDown * gm);
        readLatency = horowitz(tr, beta, rampInput, &rampInternal);

        cap = capNandOut + outputDriver->capInput[0];
        tr = resPullDown * cap;
        readLatency += horowitz(tr, beta, rampInternal, &rampInternal);

        rampOutput = rampInternal;
        if (outputDriver->numStage > 0) {
            outputDriver->CalculateLatency(rampInternal);
            readLatency += outputDriver->readLatency;
            rampOutput = outputDriver->rampOutput;
        }
        writeLatency = readLatency;
    }
}

void CAM_DataBuffer::CalculatePower() {
    if (!initialized) {
        std::cout << "[CAM_DataBuffer] Error: Require initialization first!" << std::endl;
    } else {
        //outputDriver->CalculatePower();
        leakage = CalculateGateLeakage(NAND, 2, widthNandN, widthNandP, config->temperature, config->tech) * config->tech->vdd *4;
        leakage += ( outputDriver->leakage * (1+differential) );

        /* 2 stage nand and the driver */
        double cap;
        cap = outputDriver->capInput[0] + capNandOut * 4 + capNandIn * 5;
        readDynamicEnergy = cap * config->tech->vdd * config->tech->vdd;
        readDynamicEnergy += ( outputDriver->readDynamicEnergy * (1+differential) );
        writeDynamicEnergy = readDynamicEnergy;
    }
}

void CAM_DataBuffer::PrintProperty() {
    std::cout << "CAM_DataBuffer Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

CAM_DataBuffer & CAM_DataBuffer::operator=(const CAM_DataBuffer &rhs) {
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
    return *this;
}

