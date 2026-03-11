#include "../include/CAM_OutputAccumulator.h"
#include "../include/global.h"
#include "../include/formula.h"

CAM_OutputAccumulator::CAM_OutputAccumulator() {
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
}
/*
CAM_OutputAccumulator::~CAM_OutputAccumulator() {
	// TODO Auto-generated destructor stub
}
*/
void CAM_OutputAccumulator::Initialize(double _capLoad, double _resLoad, std::shared_ptr<InputParameter> _inputParameter) {
	if (initialized)
		std::cout << "[CAM_OutputAccumulator] Warning: Already initialized!" << std::endl;
	/* structure: nand for out/out_last, nand for initial, FF (6 nand) */
	capLoad = _capLoad;
	resLoad = _resLoad;
        inputParameter = _inputParameter;
	widthNandN = 2 * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
	widthNandP = inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
	CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &capNandIn, &capNandOut);

	/* logic effort: built up with 1+1+3 nand */
	double logicEffort = (2+inputParameter->tech->pnSizeRatio) / (1+inputParameter->tech->pnSizeRatio);
	logicEffort = pow(logicEffort,5.0);
	CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &capNandIn, &capNandOut);
	outputDriver.Initialize(logicEffort, capNandOut, capLoad, resLoad, true, latency_first, 0, inputParameter);
	initialized = true;
        CalculateArea();
        CalculateRC();
        CalculatePower();
}

void CAM_OutputAccumulator::CalculateArea(){
	if (!initialized) {
		std::cout << "[CAM_OutputAccumulator] Error: Require initialization first!" << std::endl;
	} else {
		// TODO: The exactly layout of accumulator is not described, just for magic folding
		outputDriver.CalculateArea();
		area = outputDriver.area;
		double hNand, wNand;
		CalculateGateArea(NAND, 2, widthNandN, widthNandP, inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &hNand, &wNand, inputParameter->UseUpdatedLib);
		area += (hNand * wNand * (1+1+6));
		height = hNand * 3;
		width = area / height;
	}
}

void CAM_OutputAccumulator::CalculateRC() {
	if (!initialized) {
		std::cout << "[CAM_OutputAccumulator] Error: Require initialization first!" << std::endl;
	} else {
		outputDriver.CalculateRC();
		CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &capNandIn, &capNandOut);
	}
}

void CAM_OutputAccumulator::CalculateLatency(double _rampInput) {
	if (!initialized) {
		std::cout << "[CAM_OutputAccumulator] Error: Require initialization first!" << std::endl;
	} else {
		rampInput = _rampInput;
    	double resPullDown;
    	double cap;
    	double tr;		/* time constant */
    	double gm;		/* transconductance */
    	double beta;	/* for horowitz calculation */
    	double rampInternal;
    	/* nand and the driver */
    	resPullDown = CalculateOnResistance(widthNandN, NMOS, inputParameter->temperature, inputParameter->tech);
    	gm = CalculateTransconductance(widthNandN, NMOS, inputParameter->tech);
    	beta = 1 / (resPullDown * gm);
    	cap = capNandOut;
    	tr = resPullDown * cap;
    	readLatency = horowitz(tr, beta, rampInput, &rampInternal);
    	cap = capNandOut + capNandIn;
    	tr = resPullDown * cap;
    	readLatency += horowitz(tr, beta, rampInternal, &rampInternal);
    	cap = capNandOut + capNandIn;
    	tr = resPullDown * cap;
    	readLatency += horowitz(tr, beta, rampInternal, &rampInternal);
    	cap = capNandOut + capNandIn * 2;
    	tr = resPullDown * cap;
    	readLatency += horowitz(tr, beta, rampInternal, &rampInternal);
    	cap = capNandOut + capNandIn * 2;
    	tr = resPullDown * cap;
    	readLatency += horowitz(tr, beta, rampInternal, &rampInternal);
    	cap = capNandOut + capNandIn + outputDriver.capInput[0];
    	tr = resPullDown * cap;
    	readLatency += horowitz(tr, beta, rampInternal, &rampInternal);
    	outputDriver.CalculateLatency(rampInternal);
    	readLatency += outputDriver.readLatency;
    	rampOutput = outputDriver.rampOutput;
    	writeLatency = readLatency;
	}
}

void CAM_OutputAccumulator::CalculatePower() {
	if (!initialized) {
		std::cout << "[CAM_OutputAccumulator] Error: Require initialization first!" << std::endl;
	} else {
		outputDriver.CalculatePower();
		leakage = CalculateGateLeakage(NAND, 2, widthNandN, widthNandP, inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd * (1+1+6);
		leakage += outputDriver.leakage;

		/* nand and the driver */
		double cap;
		cap = outputDriver.capInput[0] + capNandOut * 7 + capNandIn * 11;
		readDynamicEnergy = cap * inputParameter->tech->vdd * inputParameter->tech->vdd;
		readDynamicEnergy += outputDriver.readDynamicEnergy;
		writeDynamicEnergy = readDynamicEnergy;
	}
}

void CAM_OutputAccumulator::PrintProperty() {
	std::cout << "CAM_OutputAccumulator Properties:" << std::endl;
	FunctionUnit::PrintProperty();
}

CAM_OutputAccumulator & CAM_OutputAccumulator::operator=(const CAM_OutputAccumulator &rhs) {
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



