#include "../include/CAM_BasicEncoder.h"
#include "../include/formula.h"
#include "../include/global.h"

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
/*
CAM_BasicEncoder::~CAM_BasicEncoder() {
	// TODO Auto-generated destructor stub
}
*/
void CAM_BasicEncoder::Initialize(int _numInputBit, double _capLoad, double _resLoad, 
        std::shared_ptr<InputParameter> _inputParameter) {
	if (initialized)
		std::cout << "[CAM_BasicEncoder] Warning: Already initialized!" << std::endl;
	numInputBit = _numInputBit;
	capLoad = _capLoad;
	resLoad = _resLoad;
	inputParameter = _inputParameter;
        if (numInputBit == 8) {
		//TODO: Assuming we only have drivers at carry-in, z-output has no drivers
		widthNorN = MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		widthNorP = 2 * inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		widthNandN = 2 * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		widthNandP = inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		widthN = MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		widthP = inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		double logicEffortCarry = 2 / (1+inputParameter->tech->pnSizeRatio);
		double tmp;
		CalculateGateCapacitance(NOR, 8, widthN*2, widthP, inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &tmp, &capDyn);
		outputDriver.Initialize(logicEffortCarry, capDyn, capLoad, resLoad, true, latency_first, 0, inputParameter);
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
			CalculateGateArea(NOR, 2, widthNorN, widthNorP, inputParameter->tech->featureSize*40, inputParameter->tech, &hNOR, &wNOR, 
                                inputParameter->UseUpdatedLib);
			// assuming the second stage NAND is twice as large as the first stage NOR
			CalculateGateArea(NAND, 2, widthNandN*2, widthNandP*2, inputParameter->tech->featureSize*40, inputParameter->tech, &hNAND, 
                                &wNAND, inputParameter->UseUpdatedLib);
			CalculateGateArea(INV, 1, widthN, widthP, inputParameter->tech->featureSize*40, inputParameter->tech, &hTRI, &wTRI, 
                                inputParameter->UseUpdatedLib);
			// TODO: a better layout
			width = MAX(MAX(wNOR, wNAND), wTRI);
			height = hTRI + hNAND + hNOR * 2;
			height *= 3;
			area = height * width;

			// dynamic circuit for carry in
			double hPullDown, wPullDown, hCLK, wCLK;
			// the clock part
			CalculateGateArea(INV, 1, widthN * 2, widthP, inputParameter->tech->featureSize*40, inputParameter->tech, &hCLK, &wCLK,
                                inputParameter->UseUpdatedLib);
			// the pull down NMOS part
			CalculateGateArea(INV, 8, widthN * 2, 0, inputParameter->tech->featureSize*40, inputParameter->tech, &hPullDown, &wPullDown,
                                inputParameter->UseUpdatedLib);
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
		CalculateGateCapacitance(NOR, 2, widthNorN, widthNorP, inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &capNorInput, &capNorOutput);
		CalculateGateCapacitance(NAND, 2, widthNandN, widthNandP, inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &capNandInput, &capNandOutput);
		CalculateGateCapacitance(INV, 2, widthN, widthP, inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &capInvInput, &capInvOutput);
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

        	resPullDown = CalculateOnResistance(widthN*2, NMOS, inputParameter->temperature, inputParameter->tech);
        	// carry in also gives to the tri-state gate
        	capLoad = capDyn + outputDriver.capInput[0] + capInvInput;
        	tr = resPullDown * capLoad;
        	gm = CalculateTransconductance(widthNandN, NMOS, inputParameter->tech);
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
			leakage += ( CalculateGateLeakage(NOR, 2, widthNorN, widthNorP, inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd *2*3);
			leakage += ( CalculateGateLeakage(NAND, 2, widthNandN, widthNandP, inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd *3);
			leakage += ( CalculateGateLeakage(INV, 1, widthN, widthP, inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd *3);
			// leakage for the dynamic logic
			int tempIndex = (int)inputParameter->temperature - 300;
			if ((tempIndex > 100) || (tempIndex < 0)) {
				throw std::runtime_error("Error: Temperature is out of range");
			}
			double *leakP = inputParameter->tech->currentOffPmos;
			double leakageP = widthP * leakP[tempIndex];
			leakage += ( leakageP * inputParameter->tech->vdd );

			// dynamic for the dynamic logic
			cap = outputDriver.capInput[0] + capDyn + capInvInput;
			readDynamicEnergy += (cap * inputParameter->tech->vdd * inputParameter->tech->vdd);
			// dynamic power for the OR gates and the tri-state output

			cap = capNorOutput *2 + capNandInput + capInvInput;
			readDynamicEnergy += (cap * inputParameter->tech->vdd * inputParameter->tech->vdd * 3);
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


