#include "CAM_LevelShifter.h"
#include "formula.h"

CAM_LevelShifter::CAM_LevelShifter() {
    initialized = false;
    capLoad = 0;
    resLoad = 0;
    capNandIn = 0;
    capNandOut = 0;
    widthNandN = 0;
    widthNandP = 0;
    rampInput = 0;
    rampOutput = 0;
}
/*
   CAM_LevelShifter::~CAM_LevelShifter() {
}
 */
void CAM_LevelShifter::Initialize(int _numInputBit, double _capLoad, double _resLoad, std::shared_ptr<EvaCamConfig> _config){
    if (initialized)
        _config->logger.Verbose() << "[CAM_LevelShifter] Warning: Already initialized!";
    /* structure: D-latch, contains 4 nand */
    config = _config;
    numInputBit = _numInputBit;
    capLoad = _capLoad;
    resLoad = _resLoad;
    /* gate sizing: built up with 2-input nand */
    widthNandN = 2 * MIN_NMOS_SIZE * config->technology.tech->featureSize();
    widthNandP = config->technology.tech->pnSizeRatio() * MIN_NMOS_SIZE * config->technology.tech->featureSize();
    /* logic effort: two stages of nand and then to the driver */
    double logicEffort = (2+config->technology.tech->pnSizeRatio()) / (1+config->technology.tech->pnSizeRatio());
    logicEffort *= logicEffort;
    /* driver's cap-in: both the input and output of nand */
    CalculateGateCapacitance(NAND, 2, widthNandN*3, widthNandP*3, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &capNandIn, &capNandOut);


    initialized = true;
    CalculateArea();
    CalculateRC();
}

void CAM_LevelShifter::CalculateArea(){
    if (!initialized) {
        ThrowInitializationError("[CAM_LevelShifter]");
    } else {
        area = 0;
        height = 0;
        width = 0;

        double hlow, /* hlatch,*/  hhigh, wlow, wlatch, whigh; // TODO: why is hlatch unused?
        CalculateGateArea(NAND, 2, widthNandN*3, widthNandP*3, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &hlow, &wlow, config->peripherals.useUpdatedLib);
        CalculateGateArea(NAND, 2, widthNandN*3, widthNandP*3, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &hhigh, &whigh, config->peripherals.useUpdatedLib);
        double hLS = MAX(hlow, hhigh);

        //TODO: figure out what this is supposed to be initialized to
        wlatch = 0; 

        double wLS = wlow + 2*wlatch + whigh;
        area = hLS * wLS * numInputBit;
    }
}

void CAM_LevelShifter::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[CAM_LevelShifter]");
    } else {

        CalculateGateCapacitance(NAND, 2, widthNandN*3, widthNandP*3, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &capNandIn, &capNandOut);
    }
}

void CAM_LevelShifter::CalculateLatency(double _rampInput) {
    if (!initialized) {
        ThrowInitializationError("[CAM_LevelShifter]");
    } else {
        rampInput = _rampInput;
        double resPullDown;
        double cap;
        double tr;		/* time constant */
        double gm;		/* transconductance */
        double beta;	/* for horowitz calculation */
        double rampInternal;

        /* 2 stage nand and the driver */
        resPullDown = CalculateOnResistance(widthNandP*10, PMOS, config->input.temperature, *config->technology.tech);
        cap = capNandOut + capNandIn;
        tr = resPullDown * cap;
        gm = CalculateTransconductance(widthNandN, NMOS, *config->technology.tech);
        beta = 1 / (resPullDown * gm);
        readLatency = horowitz(tr, beta, rampInput, &rampInternal);

        cap = capNandOut;
        tr = resPullDown * cap;
        readLatency += horowitz(tr, beta, rampInternal, &rampInternal);

        rampOutput = rampInternal;
    }
    writeLatency = readLatency;
}

void CAM_LevelShifter::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[CAM_LevelShifter]");
    } else {
        leakage = 0;
        readDynamicEnergy = 0;
        writeDynamicEnergy = 0;
        double cap;
        cap = capNandOut + capNandIn;

        // Read dynamic energy
        // during read, high voltage is not triggered
        readDynamicEnergy += cap * config->technology.tech->vdd() * config->technology.tech->vdd() * numInputBit; 

        //std::cout << "[CAM_LevelShifter] readDynamicEnergy: " << readDynamicEnergy << std::endl;


        // Write dynamic energy (2-step write and average case half SET and half RESET)
        // 1T1R
        writeDynamicEnergy += cap * config->technology.tech->vdd() * config->technology.tech->vdd()*9;    
    }
}

void CAM_LevelShifter::PrintProperty() {
    std::cout << "CAM_LevelShifter Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
