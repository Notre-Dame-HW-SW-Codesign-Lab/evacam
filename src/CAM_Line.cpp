#include "../include/CAM_Line.h"
#include "../include/global.h"
#include "../include/typedef.h"
#include "../include/constant.h"
#include "../include/formula.h"
#include "../include/global.h"
#include "../include/MemCell.h"
#include "../include/macros.h"

void CAM_Line::Initialize(bool _isRow, int _index, double _len, long long _numCell, 
        std::shared_ptr<InputParameter> _inputParameter, std::shared_ptr<Wire> _localWire) {
                
        inputParameter= _inputParameter;
        localWire = _localWire;

	if (initialized) {
		PRINT_VERBOSE("[CAM_Line 1st] Warning: Already initialized!");
        } else {
		len = _len;
		isRow = _isRow;
		index = _index;
		CellPort.IsCol = inputParameter->cell->camPort[!isRow][index].IsCol;
		CellPort.ConnectedRegion = inputParameter->cell->camPort[!isRow][index].ConnectedRegion;
		CellPort.Type = inputParameter->cell->camPort[!isRow][index].Type;
		CellPort.isNMOS = inputParameter->cell->camPort[!isRow][index].isNMOS;
		//CellPort.isNVMdischarge = inputParameter->cell->camPort[!isRow][index].isNVMdischarge;
		CellPort.numCmos = inputParameter->cell->camPort[!isRow][index].numCmos;
		CellPort.volReset = inputParameter->cell->camPort[!isRow][index].volReset;
		CellPort.volSearch0 = inputParameter->cell->camPort[!isRow][index].volSearch0;
		CellPort.volSearch1 = inputParameter->cell->camPort[!isRow][index].volSearch1;
		CellPort.volSetLRS = inputParameter->cell->camPort[!isRow][index].volSetLRS;
		CellPort.volSetMRS = inputParameter->cell->camPort[!isRow][index].volSetMRS;
		CellPort.widthCmos = inputParameter->cell->camPort[!isRow][index].widthCmos;
		CellPort.widthWire = inputParameter->cell->camPort[!isRow][index].widthWire;
		numCell = _numCell;
		cap = len * localWire->capWirePerUnit;
		res = len * localWire->resWirePerUnit;

		if (CellPort.widthWire > 1.0) {
			cap = len * CalculateWireCapacitance(PERMITTIVITY, localWire->wireWidth*CellPort.widthWire,
					localWire->wireThickness, localWire->wireSpacing, localWire->ildThickness, 1.5,
					localWire->horizontalDielectric, 3.9, 1.15e-10);
			res = len * CalculateWireResistance(localWire->copper_resistivity, 
                                        localWire->wireWidth*CellPort.widthWire,
                                        localWire->wireThickness, localWire->barrierThickness, 0, 1);
		}

		if (CellPort.ConnectedRegion == gate) {
			if (inputParameter->cell->memCellType == FEFETRAM) {
				cap += CalculateGateCap(CellPort.widthCmos * inputParameter->FEFET_tech->featureSize, inputParameter->FEFET_tech) 
                                    * numCell * CellPort.numCmos;
			} else {
				cap += CalculateGateCap(CellPort.widthCmos * inputParameter->tech->featureSize, inputParameter->tech) 
                                    * numCell * CellPort.numCmos;
			}			
		} else if (CellPort.ConnectedRegion == drain) {
			if (inputParameter->cell->memCellType == FEFETRAM) {
				cap += CalculateDrainCap(CellPort.widthCmos * inputParameter->FEFET_tech->featureSize, NMOS, 
                                        inputParameter->cell->widthInFeatureSize * inputParameter->FEFET_tech->featureSize, inputParameter->FEFET_tech)
					* numCell * CellPort.numCmos;
			} else {
				cap += CalculateDrainCap(CellPort.widthCmos * inputParameter->tech->featureSize, NMOS, 
                                        inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)
				 * numCell * CellPort.numCmos;
			}
				
		} else if (CellPort.ConnectedRegion == diode) {
			if (inputParameter->cell->memCellType == FEFETRAM) {
				cap += ( CalculateGateCap(CellPort.widthCmos * inputParameter->FEFET_tech->featureSize, inputParameter->FEFET_tech) 
                                        * numCell + CalculateDrainCap(CellPort.widthCmos * inputParameter->FEFET_tech->featureSize, 
                                            NMOS, inputParameter->cell->widthInFeatureSize * inputParameter->FEFET_tech->featureSize, inputParameter->FEFET_tech) )
						* numCell * CellPort.numCmos;
			} else {
			cap += ( CalculateGateCap(CellPort.widthCmos * inputParameter->tech->featureSize, inputParameter->tech) * numCell
					+ CalculateDrainCap(CellPort.widthCmos * inputParameter->tech->featureSize, NMOS, 
                                            inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech) )
					* numCell * CellPort.numCmos;
			}
		} else if (CellPort.ConnectedRegion == source) { // it is source, which is the weird case in ISSCC'15 3t1r
			// TODO
			if (inputParameter->cell->memCellType == FEFETRAM){
				cap += CalculateDrainCap(CellPort.widthCmos * inputParameter->FEFET_tech->featureSize, NMOS, 
                                        inputParameter->cell->widthInFeatureSize * inputParameter->FEFET_tech->featureSize, inputParameter->FEFET_tech)
					* numCell * CellPort.numCmos;
			} else {
			cap += CalculateDrainCap(CellPort.widthCmos * inputParameter->tech->featureSize, NMOS,
                                inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)
				 * numCell * CellPort.numCmos;
			}
		} else {
			cap += CalculateDrainCap(CellPort.widthCmos * inputParameter->tech->featureSize, NMOS, 
                                inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)
				 * numCell * CellPort.numCmos;
		}

		maxCurrent = 0;
		if (CellPort.Type == Wordline) {
			if (CellPort.ConnectedRegion != gate) {
				invalid = true;
				std::cout << "[CAM_Line] Error: Weird Wordline description!" << std::endl;
				return;
			}
		} else if (CellPort.Type == Bitline || CellPort.Type == Sourceline) {
			if (CellPort.ConnectedRegion != drain && CellPort.ConnectedRegion != none) {
				invalid = true;
				std::cout << "[CAM_Line] Error: Weird Bitline/Sourceline description!" << std::endl;
				return;
			}
			if (inputParameter->cell->setMode) {
				maxCurrent = inputParameter->cell->setVoltage / inputParameter->cell->resistanceOff;
			} else {
				maxCurrent = inputParameter->cell->setCurrent;
			}
			if (inputParameter->cell->resetMode) {
				maxCurrent = MAX(maxCurrent, inputParameter->cell->resetVoltage / inputParameter->cell->resistanceOn);
			} else {
				maxCurrent = MAX(maxCurrent, inputParameter->cell->setCurrent);
			}
			maxCurrent += inputParameter->cell->leakageCurrentAccessDevice * (numCell - 1);
			if (CellPort.Type == Sourceline) { // the "column based resistor sourceline" in ISSCC'15 3t1r
				std::cout << "[CAM_Line] Warning: Make sure you are using Meng-Fan Chang's design" << std::endl;
				// it is special coded for Dr. Chang's design
				// for search operation: worst case, search all-0 or all-1
				// TODO: replace Vp to Vdd for coding convenience
				// TODO: the current is toooo large, have no idea, make it smaller
				double res = CalculateOnResistance(inputParameter->cell->widthAccessCMOS * inputParameter->tech->featureSize, NMOS, 
                                        inputParameter->temperature, inputParameter->tech) + inputParameter->cell->resistanceOn;
				maxCurrent = MAX(maxCurrent, inputParameter->tech->vdd / res);
				maxCurrent = MAX(maxCurrent, MAX(CellPort.volSearch0,CellPort.volSearch1) / res);
			}
		} else if (CellPort.Type == Matchline || CellPort.Type == Matchline_Bitline) {
			// calc all miss
			if (inputParameter->cell->readMode && inputParameter->cell->readVoltage == 0) { // voltage sensing, current-in
				maxCurrent = inputParameter->cell->readCurrent;
			} else {
				// TODO
				// Note that no  such a large current, just large enough to recognize it is miss
				if (inputParameter->cell->accessType == CMOS_access) {
					maxCurrent = CellPort.widthCmos * inputParameter->tech->featureSize * 
                                            (inputParameter->tech->currentOnNmos[inputParameter->temperature - 300]
					              + inputParameter->tech->currentOffNmos[inputParameter->temperature - 300] 
                                                      * (numCell - 1));
				} else if (inputParameter->cell->accessType == diode_access) {
					double res = CalculateOnResistance(inputParameter->cell->widthAccessCMOS * inputParameter->tech->featureSize, 
                                                NMOS, inputParameter->temperature, inputParameter->tech) + inputParameter->cell->resistanceOn;
					maxCurrent = inputParameter->tech->vdd / res + CellPort.widthCmos * inputParameter->tech->featureSize * 
                                            inputParameter->tech->currentOffNmos[inputParameter->temperature - 300] * (numCell - 1);
				} else if (inputParameter->cell->accessType == none_access) {
					// TODO: too lazy to consider encoding here
				} else {
					double res = CalculateOnResistance(inputParameter->cell->widthAccessCMOS * inputParameter->tech->featureSize, 
                                                NMOS, inputParameter->temperature, inputParameter->tech) + inputParameter->cell->resistanceOn;
					maxCurrent = inputParameter->tech->vdd / res * numCell;
				}
			}

                // Design for 2FeFET TCAM
		} else if (inputParameter->cell-> memCellType == FEFETRAM ||CellPort.Type == Searchline_Bitline) { 			                        //std::cout << "[CAM_Line] Warning: Make sure you are using 2FEFET TCAM design" << std::endl;
			if (inputParameter->cell->setMode) {
				maxCurrent = inputParameter->cell->setCurrent;
			}
			if (inputParameter->cell->resetMode) {
				maxCurrent = MAX(maxCurrent, inputParameter->cell->resetVoltage / inputParameter->cell->resistanceOn);
			} 
		}
			double maxCurrentBitline = 0;
			if (CellPort.Type == Matchline_Bitline) {
				// as bitline
				if (inputParameter->cell->setMode) {
					maxCurrentBitline = inputParameter->cell->setVoltage / inputParameter->cell->resistanceOff;
				} else {
					maxCurrentBitline = inputParameter->cell->setCurrent;
				}
				if (inputParameter->cell->resetMode) {
					maxCurrentBitline = MAX(maxCurrentBitline, 
                                                inputParameter->cell->resetVoltage / inputParameter->cell->resistanceOn);
				} else {
					maxCurrentBitline = MAX(maxCurrentBitline, inputParameter->cell->setCurrent);
				}
				maxCurrentBitline += inputParameter->cell->leakageCurrentAccessDevice * (numCell - 1);
			}
			maxCurrent = MAX(maxCurrentBitline, maxCurrent);
		}

		if (isRow) {
			minMuxWidth = 0;
		} else {
			minMuxWidth = maxCurrent/ inputParameter->tech->currentOnNmos[inputParameter->temperature - 300];
		}
	

			initialized = true;
	}


void CAM_Line::Initialize(double _len, long long _numCell, double _MuxWidth, 
        std::shared_ptr<InputParameter> _inputParameter, std::shared_ptr<Wire> _localWire) {

	if (initialized) {
		PRINT_VERBOSE("[CAM_Line 2nd] Warning: Already initialized!");
        } else {
                localWire = _localWire;
		len = _len;
                inputParameter = _inputParameter;
		isRow = true;
		numCell = _numCell;
		cap = len * localWire->capWirePerUnit;
		res = len * localWire->resWirePerUnit;

		cap += CalculateGateCap(_MuxWidth * inputParameter->tech->featureSize, inputParameter->tech) * numCell;
		maxCurrent = 1e11;
		minMuxWidth = 0;
		initialized = true;
	}
}
