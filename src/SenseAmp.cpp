#include "../include/SenseAmp.h"
#include "../include/formula.h"
#include "../include/global.h"
/*
SenseAmp::SenseAmp() {
	// TODO Auto-generated constructor stub
	initialized = false;
	invalid = false;
}

SenseAmp::~SenseAmp() {
	// TODO Auto-generated destructor stub
}
*/
void SenseAmp::Initialize(long long _numColumn, bool _currentSense, double _senseVoltage, double _pitchSenseAmp,
        std::shared_ptr<InputParameter> _inputParameter) {
	if (initialized)
		std::cout << "[Sense Amp] Warning: Already initialized!" << std::endl;

	numColumn = _numColumn;
	currentSense = _currentSense;
	senseVoltage = _senseVoltage;
	pitchSenseAmp = _pitchSenseAmp;
        inputParameter = _inputParameter;

	if (pitchSenseAmp <= inputParameter->tech->featureSize * 2) {
		/* too small, cannot do the layout */
		invalid = true;
		std::cout << "Sense Amp too small, cannot do the layout" << std::endl;
	}

	initialized = true;
        CalculateArea();
        CalculateRC();
        CalculatePower();
}

void SenseAmp::CalculateArea() {
	if (!initialized) {
		std::cout << "[Sense Amp] Error: Require initialization first!" << std::endl;
	} else if (invalid) {
		height = width = area = 1e41;
	} else {
		height = width = area = 0;
		double tempHeight = 0;
		double tempWidth = 0;

		if (currentSense) {	/* current-sensing needs IV converter */
			area += IV_CONVERTER_AREA * inputParameter->tech->featureSize * inputParameter->tech->featureSize;
		}
		/* the following codes are transformed from CACTI 6.5 */


		CalculateGateArea(INV, 1, 0, W_SENSE_P * inputParameter->tech->featureSize,
				pitchSenseAmp, inputParameter->tech, &tempWidth, &tempHeight, inputParameter->UseUpdatedLib);	/* exchange width and height for senseamp layout */
		width = MAX(width, tempWidth);
		height += 2 * tempHeight;
		CalculateGateArea(INV, 1, 0, W_SENSE_ISO * inputParameter->tech->featureSize,
				pitchSenseAmp, inputParameter->tech, &tempWidth, &tempHeight, inputParameter->UseUpdatedLib);	/* exchange width and height for senseamp layout */
		width = MAX(width, tempWidth);
		height += tempHeight;
		height += 2 * MIN_GAP_BET_SAME_TYPE_DIFFS * inputParameter->tech->featureSize;

		CalculateGateArea(INV, 1, W_SENSE_N * inputParameter->tech->featureSize, 0,
				pitchSenseAmp, inputParameter->tech, &tempWidth, &tempHeight, inputParameter->UseUpdatedLib);	/* exchange width and height for senseamp layout */
		width = MAX(width, tempWidth);
		height += 2 * tempHeight;
		CalculateGateArea(INV, 1, W_SENSE_EN * inputParameter->tech->featureSize, 0,
				pitchSenseAmp, inputParameter->tech, &tempWidth, &tempHeight, inputParameter->UseUpdatedLib);	/* exchange width and height for senseamp layout */
		width = MAX(width, tempWidth);
		height += tempHeight;
		height += 2 * MIN_GAP_BET_SAME_TYPE_DIFFS * inputParameter->tech->featureSize;

		height += MIN_GAP_BET_P_AND_N_DIFFS * inputParameter->tech->featureSize;

		/* transformation so that width meets the pitch */
		height = height * width / pitchSenseAmp;
		width = pitchSenseAmp;

		/* Add additional area if IV converter exists */
		height += area / width;
		width *= numColumn;

		area = height * width;
	}
}

void SenseAmp::CalculateRC() {
	if (!initialized) {
		std::cout << "[Sense Amp] Error: Require initialization first!" << std::endl;
	} else if (invalid) {
		readLatency = writeLatency = 1e41;
	} else {
		capLoad = CalculateGateCap((W_SENSE_P + W_SENSE_N) * inputParameter->tech->featureSize, inputParameter->tech)
				+ CalculateDrainCap(W_SENSE_N * inputParameter->tech->featureSize, NMOS, pitchSenseAmp, inputParameter->tech)
				+ CalculateDrainCap(W_SENSE_P * inputParameter->tech->featureSize, PMOS, pitchSenseAmp, inputParameter->tech)
				+ CalculateDrainCap(W_SENSE_ISO * inputParameter->tech->featureSize, PMOS, pitchSenseAmp, inputParameter->tech)
				+ CalculateDrainCap(W_SENSE_MUX * inputParameter->tech->featureSize, NMOS, pitchSenseAmp, inputParameter->tech);
	}
}

void SenseAmp::CalculateLatency(double _rampInput) {	/* TODO: _rampInput is actually no use in SenseAmp */
	if (!initialized) {
		std::cout << "[Sense Amp] Error: Require initialization first!" << std::endl;
	} else {
		readLatency = writeLatency = 0;
		if (currentSense) {	/* current-sensing needs IV converter */
			/* all the following values achieved from HSPICE */
			if (inputParameter->tech->featureSize >= 119e-9)
				readLatency += 0.49e-9;		/* 120nm */
			else if (inputParameter->tech->featureSize >= 89e-9)
				readLatency += 0.53e-9;		/* 90nm */
			else if (inputParameter->tech->featureSize >= 64e-9)
				readLatency += 0.62e-9;		/* 65nm */
			else if (inputParameter->tech->featureSize >= 44e-9)
				readLatency += 0.80e-9;		/* 45nm */
			else if (inputParameter->tech->featureSize >= 31e-9)
				readLatency += 1.07e-9;		/* 32nm */
			else
				readLatency += 1.45e-9;     /* below 22nm */
		}

		/* Voltage sense amplifier */
		double gm = CalculateTransconductance(W_SENSE_N * inputParameter->tech->featureSize, NMOS, inputParameter->tech)
				+ CalculateTransconductance(W_SENSE_P * inputParameter->tech->featureSize, PMOS, inputParameter->tech);
		double tau = capLoad / gm;
		readLatency += tau * log(inputParameter->tech->vdd / senseVoltage);
	}
}

void SenseAmp::CalculatePower() {
	if (!initialized) {
		std::cout << "[Sense Amp] Error: Require initialization first!" << std::endl;
	} else if (invalid) {
		readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
	} else {
		readDynamicEnergy = writeDynamicEnergy = 0;
		leakage = 0;
		if (currentSense) {	/* current-sensing needs IV converter */
			/* all the following values achieved from HSPICE */
			if (inputParameter->tech->featureSize >= 119e-9) {			/* 120nm */
				readDynamicEnergy += 8.52e-14;	/* Unit: J */
				leakage += 1.40e-8;				/* Unit: W */
			} else if (inputParameter->tech->featureSize >= 89e-9) {	/* 90nm */
				readDynamicEnergy += 8.72e-14;
				leakage += 1.87e-8;
			} else if (inputParameter->tech->featureSize >= 64e-9) {	/* 65nm */
				readDynamicEnergy += 9.00e-14;
				leakage += 2.57e-8;
			} else if (inputParameter->tech->featureSize >= 44e-9) {	/* 45nm */
				readDynamicEnergy += 10.26e-14;
				leakage += 4.41e-9;
			} else if (inputParameter->tech->featureSize >= 31e-9) {	/* 32nm */
				readDynamicEnergy += 12.56e-14;
				leakage += 12.54e-8;
			} else {                                    /* TODO, need calibration below 22nm */
				readDynamicEnergy += 15e-14;
				leakage += 15e-8;
			}
		}

		/* Voltage sense amplifier */
		readDynamicEnergy += capLoad * inputParameter->tech->vdd * inputParameter->tech->vdd;
		double idleCurrent =  CalculateGateLeakage(INV, 1, W_SENSE_EN * inputParameter->tech->featureSize, 0,
				inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd;
		leakage += idleCurrent * inputParameter->tech->vdd;

		readDynamicEnergy *= numColumn;
		leakage *= numColumn;
	}
}

void SenseAmp::PrintProperty() {
	std::cout << "Sense Amplifier Properties:" << std::endl;
	FunctionUnit::PrintProperty();
}

SenseAmp & SenseAmp::operator=(const SenseAmp &rhs) {
	height = rhs.height;
	width = rhs.width;
	area = rhs.area;
	readLatency = rhs.readLatency;
	writeLatency = rhs.writeLatency;
	readDynamicEnergy = rhs.readDynamicEnergy;
	writeDynamicEnergy = rhs.writeDynamicEnergy;
	resetLatency = rhs.resetLatency;
	setLatency = rhs.setLatency;
	resetDynamicEnergy = rhs.resetDynamicEnergy;
	setDynamicEnergy = rhs.setDynamicEnergy;
	cellReadEnergy = rhs.cellReadEnergy;
	cellSetEnergy = rhs.cellSetEnergy;
	cellResetEnergy = rhs.cellResetEnergy;
	leakage = rhs.leakage;
	initialized = rhs.initialized;
	invalid = rhs.invalid;
	numColumn = rhs.numColumn;
	currentSense = rhs.currentSense;
	senseVoltage = rhs.senseVoltage;
	capLoad = rhs.capLoad;
	pitchSenseAmp = rhs.pitchSenseAmp;

	return *this;
}
