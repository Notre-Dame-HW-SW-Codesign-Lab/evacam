#include "../include/Precharger.h"
#include "../include/formula.h"
#include "../include/global.h"
/*
Precharger::Precharger() {
	// TODO Auto-generated constructor stub
	initialized = false;
	enableLatency = 0;
}

Precharger::~Precharger() {
	// TODO Auto-generated destructor stub
}
*/
void Precharger::Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline, std::shared_ptr<InputParameter> _inputParameter, std::shared_ptr<Wire> _localWire) {
	if (initialized)
		std::cout << "[Precharger] Warning: Already initialized!" << std::endl;

        outputDriver = std::make_shared<OutputDriver>();
	voltagePrecharge = _voltagePrecharge;
	numColumn  = _numColumn;
	capBitline = _capBitline;
	resBitline = _resBitline;
        inputParameter = _inputParameter;
        localWire = _localWire;

	capWireLoadPerColumn = inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize * localWire->capWirePerUnit;
	resWireLoadPerColumn = inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize * localWire->resWirePerUnit;
	widthInvNmos = MIN_NMOS_SIZE * inputParameter->tech->featureSize;
	widthInvPmos = widthInvNmos * inputParameter->tech->pnSizeRatio;
	widthPMOSBitlineEqual      = MIN_NMOS_SIZE * inputParameter->tech->featureSize;
	widthPMOSBitlinePrecharger = 6 * inputParameter->tech->featureSize;
	capLoadInv  = CalculateGateCap(widthPMOSBitlineEqual, inputParameter->tech) + 2 * CalculateGateCap(widthPMOSBitlinePrecharger, inputParameter->tech)
			+ CalculateDrainCap(widthInvNmos, NMOS, inputParameter->tech->featureSize*40, inputParameter->tech)
			+ CalculateDrainCap(widthInvPmos, PMOS, inputParameter->tech->featureSize*40, inputParameter->tech);
	capOutputBitlinePrecharger = CalculateDrainCap(widthPMOSBitlinePrecharger, PMOS, inputParameter->tech->featureSize*40, inputParameter->tech) + CalculateDrainCap(widthPMOSBitlineEqual, PMOS, inputParameter->tech->featureSize*40, inputParameter->tech);
	double capInputInv         = CalculateGateCap(widthInvNmos, inputParameter->tech) + CalculateGateCap(widthInvPmos, inputParameter->tech);
	capLoadPerColumn           = capInputInv + capWireLoadPerColumn;
	double capLoadOutputDriver = numColumn * capLoadPerColumn;
	outputDriver->Initialize(1, capInputInv, capLoadOutputDriver, 0 /* TODO */, true, latency_first, 0, inputParameter);  /* Always Latency First */

	initialized = true;
        CalculateArea();
        CalculateRC();
        CalculatePower();
}

void Precharger::CalculateArea() {
	if (!initialized) {
		std::cout << "[Precharger] Error: Require initialization first!" << std::endl;
	} else {
		//outputDriver->CalculateArea();
		double hBitlinePrechareger, wBitlinePrechareger;
		double hBitlineEqual, wBitlineEqual;
		double hInverter, wInverter;
		CalculateGateArea(INV, 1, 0, widthPMOSBitlinePrecharger, inputParameter->tech->featureSize*40, inputParameter->tech, &hBitlinePrechareger, &wBitlinePrechareger, inputParameter->UseUpdatedLib);
		CalculateGateArea(INV, 1, 0, widthPMOSBitlineEqual, inputParameter->tech->featureSize*40, inputParameter->tech, &hBitlineEqual, &wBitlineEqual, inputParameter->UseUpdatedLib);
		CalculateGateArea(INV, 1, widthInvNmos, widthInvPmos, inputParameter->tech->featureSize*40, inputParameter->tech, &hInverter, &wInverter, inputParameter->UseUpdatedLib);
		width = 2 * wBitlinePrechareger + wBitlineEqual;
		width = MAX(width, wInverter);
		width *= numColumn;
		width = MAX(width, outputDriver->width);
		height = MAX(hBitlinePrechareger, hBitlineEqual);
		height += hInverter;
		height = MAX(height, outputDriver->height);
		area = height * width;
	}
}

void Precharger::CalculateRC() {
	if (!initialized) {
		std::cout << "[Precharger] Error: Require initialization first!" << std::endl;
	} else {
		//outputDriver->CalculateRC();
		//more accurate RC model would include drain Capacitances of Precharger and Equalization PMOS transistors
                // TODO: the above
	}
}

void Precharger::CalculateLatency(double _rampInput){
	if (!initialized) {
		std::cout << "[Precharger] Error: Require initialization first!" << std::endl;
	} else {
		rampInput= _rampInput;
		outputDriver->CalculateLatency(rampInput);
		enableLatency = outputDriver->readLatency;
		double resPullDown;
		double tr;	/* time constant */
		double gm;	/* transconductance */
		double beta;	/* for horowitz calculation */
		double temp;
		resPullDown = CalculateOnResistance(widthInvNmos, NMOS, inputParameter->temperature, inputParameter->tech);
		tr = resPullDown * capLoadInv;
		gm = CalculateTransconductance(widthInvNmos, NMOS, inputParameter->tech);
		beta = 1 / (resPullDown * gm);
		enableLatency += horowitz(tr, beta, outputDriver->rampOutput, &temp);
		readLatency = 0;
		double resPullUp = CalculateOnResistance(widthPMOSBitlinePrecharger, PMOS,
				inputParameter->temperature, inputParameter->tech);
		double tau = resPullUp * (capBitline + capOutputBitlinePrecharger) + resBitline * capBitline / 2;
		gm = CalculateTransconductance(widthPMOSBitlinePrecharger, PMOS, inputParameter->tech);
		beta = 1 / (resPullUp * gm);
		readLatency += horowitz(tau, beta, temp, &rampOutput);
		writeLatency = readLatency;
	}
}

void Precharger::CalculatePower() {
	if (!initialized) {
		std::cout << "[Precharger] Error: Require initialization first!" << std::endl;
	} else {
		//outputDriver->CalculatePower();
		/* Leakage power */
		leakage = outputDriver->leakage;
		leakage += numColumn * inputParameter->tech->vdd * CalculateGateLeakage(INV, 1, widthInvNmos, widthInvPmos, inputParameter->temperature, inputParameter->tech);
		leakage += numColumn * voltagePrecharge * CalculateGateLeakage(INV, 1, 0, widthPMOSBitlinePrecharger,
				inputParameter->temperature, inputParameter->tech);

		/* Dynamic energy */
		/* We don't count bitline precharge energy into account because it is a charging process */
		readDynamicEnergy = outputDriver->readDynamicEnergy;
		readDynamicEnergy += capLoadInv * inputParameter->tech->vdd * inputParameter->tech->vdd * numColumn;
		writeDynamicEnergy = 0;		/* No precharging is needed during the write operation */
	}
}

void Precharger::PrintProperty() {
	std::cout << "Precharger Properties:" << std::endl;
	FunctionUnit::PrintProperty();
}

Precharger & Precharger::operator=(const Precharger &rhs) {
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
	outputDriver = rhs.outputDriver;
	capBitline = rhs.capBitline;
	resBitline = rhs.resBitline;
	capLoadInv = rhs.capLoadInv;
	capOutputBitlinePrecharger = rhs.capOutputBitlinePrecharger;
	capWireLoadPerColumn = rhs.capWireLoadPerColumn;
	resWireLoadPerColumn = rhs.resWireLoadPerColumn;
	enableLatency = rhs.enableLatency;
	numColumn = rhs.numColumn;
	widthPMOSBitlinePrecharger = rhs.widthPMOSBitlinePrecharger;
	widthPMOSBitlineEqual = rhs.widthPMOSBitlineEqual;
	capLoadPerColumn = rhs.capLoadPerColumn;
	rampInput = rhs.rampInput;
	rampOutput = rhs.rampOutput;

	return *this;
}
