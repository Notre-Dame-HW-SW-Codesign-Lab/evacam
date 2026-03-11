#include "../include/CAM_Precharger.h"
#include "../include/formula.h"
#include "../include/global.h"
#include "../include/constant.h"
/*
CAM_Precharger::CAM_Precharger() {
	// TODO Auto-generated constructor stub
	initialized = false;
	enableLatency = 0;
}

CAM_Precharger::~CAM_Precharger() {
	// TODO Auto-generated destructor stub
}
*/
void CAM_Precharger::Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline, 
        std::shared_ptr<InputParameter> _inputParameter, std::shared_ptr<Wire> _localWire) {
	if (initialized)
		std::cout << "[Precharger] Warning: Already initialized!" << std::endl;

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
	widthPMOSBitlinePrecharger = PRECHARGER_SIZE * inputParameter->tech->featureSize;
	capLoadInv  = CalculateGateCap(widthPMOSBitlineEqual, inputParameter->tech) + 2 * CalculateGateCap(widthPMOSBitlinePrecharger, inputParameter->tech)
			+ CalculateDrainCap(widthInvNmos, NMOS, inputParameter->tech->featureSize*40, inputParameter->tech)
			+ CalculateDrainCap(widthInvPmos, PMOS, inputParameter->tech->featureSize*40, inputParameter->tech);
	capOutputBitlinePrecharger = CalculateDrainCap(widthPMOSBitlinePrecharger, PMOS, inputParameter->tech->featureSize*40, inputParameter->tech) + CalculateDrainCap(widthPMOSBitlineEqual, PMOS, inputParameter->tech->featureSize*40, inputParameter->tech);
	double capInputInv         = CalculateGateCap(widthInvNmos, inputParameter->tech) + CalculateGateCap(widthInvPmos, inputParameter->tech);
	capLoadPerColumn           = capInputInv + capWireLoadPerColumn;
	double capLoadOutputDriver = numColumn * capLoadPerColumn;
        outputDriver = std::make_shared<OutputDriver>();
	outputDriver->Initialize(1, capInputInv, capLoadOutputDriver, 0 /* TODO */, true, latency_first, 0, inputParameter);  /* Always Latency First */

	initialized = true;
        CalculateArea();
        CalculateRC();
        CalculatePower();
}

void CAM_Precharger::CalculateArea() {
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
		// begin_change
		//width = 2 * wBitlinePrechareger + wBitlineEqual;
		width = 2 * wBitlinePrechareger;
		// end_change
		width = MAX(width, wInverter);
		width *= numColumn;
		width = MAX(width, outputDriver->width);
		// begin_charge
		//height = MAX(hBitlinePrechareger, hBitlineEqual);
		height = hBitlinePrechareger;
		// end_change
		height += hInverter;
		height += outputDriver->height;
		area = height * width;
	}
}

CAM_Precharger & CAM_Precharger::operator=(const CAM_Precharger &rhs) {
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


