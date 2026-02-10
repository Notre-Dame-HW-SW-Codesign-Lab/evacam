#ifndef BASIC_DECODER_CPP
#define BASIC_DECODER_CPP

#include "../include/BasicDecoder.h"
#include "../include/formula.h"
#include "../include/global.h"

void BasicDecoder::Initialize(int _numAddressBit, double _capLoad, double _resLoad, 
        std::shared_ptr<InputParameter> _inputParameter) {
	/*if (initialized)
			std::cout << "Warning: Already initialized!" << std::endl;*/
	/* might be re-initialized by predecodeblock */
	if (_numAddressBit == 1) {
		numNandInput = 0;
	}
	else {
		numNandInput = _numAddressBit;
	}
	capLoad = _capLoad;
	resLoad = _resLoad;
        inputParameter = _inputParameter;

	if (numNandInput == 0) {
		numNandGate = 0;
		double logicEffortInv = 1;
		double widthInvN = MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		double widthInvP = inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		double capInv = CalculateGateCap(widthInvN, inputParameter->tech) + CalculateGateCap(widthInvP, inputParameter->tech);
		outputDriver->Initialize(logicEffortInv, capInv, capLoad, resLoad, true, latency_first, 0, inputParameter);  /* Always Latency First */
	}
	else{
		double logicEffortNand;
		double capNand;
		if (numNandInput == 2) {	/* NAND2 */
			numNandGate = 4;
			widthNandN = 2 * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
			logicEffortNand = (2+inputParameter->tech->pnSizeRatio) / (1+inputParameter->tech->pnSizeRatio);
		} else {					/* NAND3 */
			numNandGate = 8;
			widthNandN = 3 * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
			logicEffortNand = (3+inputParameter->tech->pnSizeRatio) / (1+inputParameter->tech->pnSizeRatio);
		}
		widthNandP = inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize;
		capNand = CalculateGateCap(widthNandN, inputParameter->tech) + CalculateGateCap(widthNandP, inputParameter->tech);
		outputDriver->Initialize(logicEffortNand, capNand, capLoad, resLoad, true, latency_first, 0, inputParameter);  /* Always Latency First */
	}
	initialized = true;
        CalculateArea();
        CalculateRC();
        CalculatePower();
}

void BasicDecoder::CalculateArea() {
	if (!initialized) {
		std::cout << "[Basic Decoder] Error: Require initialization first!" << std::endl;
	} else {
		//outputDriver->CalculateArea();
		if (numNandInput == 0){
			height = 2 * outputDriver->height;
			width = outputDriver->width;
		}
		else {
			double hNand, wNand;
			CalculateGateArea(NAND, numNandInput, widthNandN, widthNandP, inputParameter->tech->featureSize*40, inputParameter->tech, &hNand, &wNand, inputParameter->UseUpdatedLib);
			height = MAX(hNand, outputDriver->height);
			width = wNand + outputDriver->width;
			height *= numNandGate;
		}
		area = height * width;
	}
}

void BasicDecoder::CalculateRC() {
	if (!initialized) {
		std::cout << "[Basic Decoder] Error: Require initialization first!" << std::endl;
	} else {
		//outputDriver->CalculateRC();
		if (numNandInput > 0) {
			CalculateGateCapacitance(NAND, numNandInput, widthNandN, widthNandP, inputParameter->tech->featureSize * MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &capNandInput, &capNandOutput);
		}
	}
}

void BasicDecoder::CalculateLatency(double _rampInput) {
	if (!initialized) {
		std::cout << "[Basic Decoder] Error: Require initialization first!" << std::endl;
	} else {
		rampInput = _rampInput;
        if (numNandInput == 0) {
        	outputDriver->CalculateLatency(rampInput);
        	readLatency  = outputDriver->readLatency;
        	writeLatency = readLatency;
        } else {
        	double resPullDown;
        	double capLoad;
        	double tr;	/* time constant */
        	double gm;	/* transconductance */
        	double beta;	/* for horowitz calculation */
        	double rampInputForDriver;

        	resPullDown = CalculateOnResistance(widthNandN, NMOS, inputParameter->temperature, inputParameter->tech) * numNandInput;
        	capLoad = capNandOutput + outputDriver->capInput[0];
        	tr = resPullDown * capLoad;
        	gm = CalculateTransconductance(widthNandN, NMOS, inputParameter->tech);
        	beta = 1 / (resPullDown * gm);
        	readLatency = horowitz(tr, beta, rampInput, &rampInputForDriver);

        	outputDriver->CalculateLatency(rampInputForDriver);
        	readLatency += outputDriver->readLatency;
        	writeLatency = readLatency;
        }
        rampOutput = outputDriver->rampOutput;
	}
}

void BasicDecoder::CalculatePower() {
	if (!initialized) {
		std::cout << "[Basic Decoder] Error: Require initialization first!" << std::endl;
	} else {
		//outputDriver->CalculatePower();
		double capLoad;
		if (numNandInput == 0) {
			leakage = 2 * outputDriver->leakage;
			capLoad = outputDriver->capInput[0] + outputDriver->capOutput[0];
			readDynamicEnergy = capLoad * inputParameter->tech->vdd * inputParameter->tech->vdd;
			readDynamicEnergy += outputDriver->readDynamicEnergy;
			readDynamicEnergy *= 1;	/* only one row is activated each time */
			writeDynamicEnergy = readDynamicEnergy;

		} else {
			/* Leakage power */
			leakage = CalculateGateLeakage(NAND, numNandInput, widthNandN, widthNandP,
					inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd;
			leakage += outputDriver->leakage;
			leakage *= numNandGate;
			/* Dynamic energy */
			capLoad = capNandOutput + outputDriver->capInput[0];
			readDynamicEnergy = capLoad * inputParameter->tech->vdd * inputParameter->tech->vdd;
			readDynamicEnergy += outputDriver->readDynamicEnergy;
			readDynamicEnergy *= 1;	/* only one row is activated each time */
			writeDynamicEnergy = readDynamicEnergy;
		}
	}
}

void BasicDecoder::PrintProperty() {
	std::cout << numNandInput << " to " << numNandGate << " Decoder Properties:" << std::endl;
	FunctionUnit::PrintProperty();
}

#endif
