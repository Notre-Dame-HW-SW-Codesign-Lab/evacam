#include "../include/Mux.h"
#include "../include/global.h"
#include "../include/formula.h"

Mux::Mux() {
	// TODO Auto-generated constructor stub
	initialized = false;
	capForPreviousPowerCalculation = 0;
	capForPreviousDelayCalculation = 0;
	capNMOSPassTransistor = 0;
	resNMOSPassTransistor = 0;
}
/*
Mux::~Mux() {
	// TODO Auto-generated destructor stub
}
*/
void Mux::Initialize(int _numInput, long long _numMux, double _capLoad, double _capInputNextStage, 
        double _minDriverCurrent, std::shared_ptr<InputParameter> _inputParameter) {
	if (initialized)
		std::cout << "[Mux] Warning: Already initialized!" << std::endl;

	numInput = _numInput;
	numMux = _numMux;
	capLoad = _capLoad;
	capInputNextStage = _capInputNextStage;
	minDriverCurrent = _minDriverCurrent;
        inputParameter = _inputParameter;

	if ((numInput > 1) && (numMux > 0 )) {
		double minNMOSWidth = minDriverCurrent / inputParameter->tech->currentOnNmos[inputParameter->temperature - 300];
		if (inputParameter->cell->memCellType == MRAM || inputParameter->cell->memCellType == PCRAM || inputParameter->cell->memCellType == memristor || inputParameter->cell->memCellType == FEFETRAM) {
			/* Mux resistance should be small enough for voltage dividing */
			double maxResNMOSPassTransistor = inputParameter->cell->resistanceOn * IR_DROP_TOLERANCE;
	    	widthNMOSPassTransistor = CalculateOnResistance(inputParameter->tech->featureSize, NMOS, inputParameter->temperature, inputParameter->tech)
					* inputParameter->tech->featureSize / maxResNMOSPassTransistor;
	    	if (widthNMOSPassTransistor > inputParameter->maxNmosSize * inputParameter->tech->featureSize) {	// Change the transistor size to avoid severe IR drop
	    		widthNMOSPassTransistor = inputParameter->maxNmosSize * inputParameter->tech->featureSize;
	    	}
			widthNMOSPassTransistor = MAX(MAX(widthNMOSPassTransistor,minNMOSWidth), 6 * MIN_NMOS_SIZE * inputParameter->tech->featureSize);
		} else {
			widthNMOSPassTransistor = MAX(6 * MIN_NMOS_SIZE * inputParameter->tech->featureSize, minNMOSWidth);
		}
	}

	initialized = true;
        CalculateArea();
        CalculateRC();
        CalculatePower();
}

void Mux::CalculateArea(){
	if (!initialized) {
		std::cout << "[Mux] Error: Require initialization first!" << std::endl;
	} else {
		if ((numInput > 1) && (numMux > 0 )) {
			double h,w;
			CalculateGateArea(INV, 1, widthNMOSPassTransistor, 0, inputParameter->tech->featureSize*40, inputParameter->tech, &h, &w, inputParameter->UseUpdatedLib);
			width = numMux * numInput * w;
			height = h;
			area = width * height;
		} else {
			height = width = area = 0;
		}
	}
}

void Mux::CalculateRC() {
	if (!initialized) {
		std::cout << "[Mux] Error: Require initialization first!" << std::endl;
	} else {
		if ((numInput > 1) && (numMux > 0 )) {
			capNMOSPassTransistor = CalculateDrainCap(widthNMOSPassTransistor, NMOS, inputParameter->tech->featureSize*40, inputParameter->tech);
			capForPreviousPowerCalculation = capNMOSPassTransistor;
			capOutput = numInput * capNMOSPassTransistor;
			capForPreviousDelayCalculation = capOutput + capNMOSPassTransistor + capLoad;
			resNMOSPassTransistor = CalculateOnResistance(widthNMOSPassTransistor, NMOS, inputParameter->temperature, inputParameter->tech);
		} else {
			;	/* nothing to do */
		}
	}
}

void Mux::CalculateLatency(double _rampInput) {  //rampInput is actually useless in Mux module
	if (!initialized) {
		std::cout << "[Mux] Error: Require initialization first!" << std::endl;
	} else {
		if ((numInput > 1) && (numMux > 0 )) {
			rampInput = _rampInput;
			double tr;
			tr = resNMOSPassTransistor * (capOutput + capLoad);
			readLatency = 2.3 * tr;
			writeLatency = readLatency;
		} else {
			readLatency = writeLatency = 0;
		}
	}
}

void Mux::CalculatePower() {
	if (!initialized) {
		std::cout << "[Mux] Error: Require initialization first!" << std::endl;
	} else {
		if ((numInput > 1) && (numMux > 0 )) {
			leakage = 0; //TODO
			readDynamicEnergy = (capOutput + capInputNextStage) * inputParameter->tech->vdd * (inputParameter->tech->vdd - inputParameter->tech->vth);
			readDynamicEnergy *= numMux;  //worst-case dynamic power analysis
			writeDynamicEnergy = readDynamicEnergy;
		} else {
			readDynamicEnergy = writeDynamicEnergy = leakage = 0;
		}
	}
}

void Mux::PrintProperty() {
	std::cout << "Mux Properties:" << std::endl;
	FunctionUnit::PrintProperty();
}

Mux & Mux::operator=(const Mux &rhs) {
	height = rhs.height;
	width = rhs.width;
	area = rhs.area;
	readLatency = rhs.readLatency;
	writeLatency = rhs.writeLatency;
	readDynamicEnergy = rhs.readDynamicEnergy;
	writeDynamicEnergy = rhs.writeDynamicEnergy;
	leakage = rhs.leakage;
	initialized = rhs.initialized;
	numInput = rhs.numInput;
	numMux = rhs.numMux;
	capLoad = rhs.capLoad;
	capInputNextStage = rhs.capInputNextStage;
	minDriverCurrent = rhs.minDriverCurrent;
    capOutput = rhs.capOutput;
	widthNMOSPassTransistor = rhs.widthNMOSPassTransistor;
	resNMOSPassTransistor = rhs.resNMOSPassTransistor;
	capNMOSPassTransistor = rhs.capNMOSPassTransistor;
	capForPreviousDelayCalculation = rhs.capForPreviousDelayCalculation;
	capForPreviousPowerCalculation = rhs.capForPreviousPowerCalculation;
	rampInput = rhs.rampInput;
	rampOutput = rhs.rampOutput;

	return *this;
}
