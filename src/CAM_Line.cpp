#include "CAM_Line.h"
#include "formula.h"

void CAM_Line::Initialize(bool _isRow, int _index, double _len, long long _numCell, 
        std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire) {

    config= _config;
    localWire = _localWire;

    if (initialized) {
        config->logger.Verbose() << "[CAM_Line 1st] Warning: Already initialized!";
    } else {
        len = _len;
        isRow = _isRow;
        index = _index;
        CellPort.IsCol = config->cell->camPort[!isRow][index].IsCol;
        CellPort.ConnectedRegion = config->cell->camPort[!isRow][index].ConnectedRegion;
        CellPort.Type = config->cell->camPort[!isRow][index].Type;
        CellPort.isNMOS = config->cell->camPort[!isRow][index].isNMOS;
        //CellPort.isNVMdischarge = config->cell->camPort[!isRow][index].isNVMdischarge;
        CellPort.numCmos = config->cell->camPort[!isRow][index].numCmos;
        CellPort.volReset = config->cell->camPort[!isRow][index].volReset;
        CellPort.volSearch0 = config->cell->camPort[!isRow][index].volSearch0;
        CellPort.volSearch1 = config->cell->camPort[!isRow][index].volSearch1;
        CellPort.volSetLRS = config->cell->camPort[!isRow][index].volSetLRS;
        CellPort.volSetMRS = config->cell->camPort[!isRow][index].volSetMRS;
        CellPort.widthCmos = config->cell->camPort[!isRow][index].widthCmos;
        CellPort.widthWire = config->cell->camPort[!isRow][index].widthWire;
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
            if (config->cell->memCellType == FEFETRAM) {
                cap += CalculateGateCap(CellPort.widthCmos * config->FEFET_tech->featureSize(), config->FEFET_tech) 
                    * numCell * CellPort.numCmos;
            } else {
                cap += CalculateGateCap(CellPort.widthCmos * config->tech->featureSize(), config->tech) 
                    * numCell * CellPort.numCmos;
            }			
        } else if (CellPort.ConnectedRegion == drain) {
            if (config->cell->memCellType == FEFETRAM) {
                cap += CalculateDrainCap(CellPort.widthCmos * config->FEFET_tech->featureSize(), NMOS, 
                        config->cell->widthInFeatureSize * config->FEFET_tech->featureSize(), config->FEFET_tech)
                    * numCell * CellPort.numCmos;
            } else {
                cap += CalculateDrainCap(CellPort.widthCmos * config->tech->featureSize(), NMOS, 
                        config->cell->widthInFeatureSize * config->tech->featureSize(), config->tech)
                    * numCell * CellPort.numCmos;
            }

        } else if (CellPort.ConnectedRegion == diode) {
            if (config->cell->memCellType == FEFETRAM) {
                cap += ( CalculateGateCap(CellPort.widthCmos * config->FEFET_tech->featureSize(), config->FEFET_tech) 
                        * numCell + CalculateDrainCap(CellPort.widthCmos * config->FEFET_tech->featureSize(), 
                            NMOS, config->cell->widthInFeatureSize * config->FEFET_tech->featureSize(), config->FEFET_tech) )
                    * numCell * CellPort.numCmos;
            } else {
                cap += ( CalculateGateCap(CellPort.widthCmos * config->tech->featureSize(), config->tech) * numCell
                        + CalculateDrainCap(CellPort.widthCmos * config->tech->featureSize(), NMOS, 
                            config->cell->widthInFeatureSize * config->tech->featureSize(), config->tech) )
                    * numCell * CellPort.numCmos;
            }
        } else if (CellPort.ConnectedRegion == source) { // it is source, which is the weird case in ISSCC'15 3t1r
                                                         // TODO
            if (config->cell->memCellType == FEFETRAM){
                cap += CalculateDrainCap(CellPort.widthCmos * config->FEFET_tech->featureSize(), NMOS, 
                        config->cell->widthInFeatureSize * config->FEFET_tech->featureSize(), config->FEFET_tech)
                    * numCell * CellPort.numCmos;
            } else {
                cap += CalculateDrainCap(CellPort.widthCmos * config->tech->featureSize(), NMOS,
                        config->cell->widthInFeatureSize * config->tech->featureSize(), config->tech)
                    * numCell * CellPort.numCmos;
            }
        } else {
            cap += CalculateDrainCap(CellPort.widthCmos * config->tech->featureSize(), NMOS, 
                    config->cell->widthInFeatureSize * config->tech->featureSize(), config->tech)
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
            if (config->cell->setMode) {
                maxCurrent = config->cell->setVoltage / config->cell->resistanceOff;
            } else {
                maxCurrent = config->cell->setCurrent;
            }
            if (config->cell->resetMode) {
                maxCurrent = MAX(maxCurrent, config->cell->resetVoltage / config->cell->resistanceOn);
            } else {
                maxCurrent = MAX(maxCurrent, config->cell->setCurrent);
            }
            maxCurrent += config->cell->leakageCurrentAccessDevice * (numCell - 1);
            if (CellPort.Type == Sourceline) { // the "column based resistor sourceline" in ISSCC'15 3t1r
                config->logger.Log() << "[CAM_Line] Warning: Make sure you are using Meng-Fan Chang's design";
                // it is special coded for Dr. Chang's design
                // for search operation: worst case, search all-0 or all-1
                // TODO: replace Vp to Vdd for coding convenience
                // TODO: the current is toooo large, have no idea, make it smaller
                double res = CalculateOnResistance(config->cell->widthAccessCMOS * config->tech->featureSize(), NMOS, 
                        config->temperature, config->tech) + config->cell->resistanceOn;
                maxCurrent = MAX(maxCurrent, config->tech->vdd() / res);
                maxCurrent = MAX(maxCurrent, MAX(CellPort.volSearch0,CellPort.volSearch1) / res);
            }
        } else if (CellPort.Type == Matchline || CellPort.Type == Matchline_Bitline) {
            // calc all miss
            if (config->cell->readMode && config->cell->readVoltage == 0) { // voltage sensing, current-in
                maxCurrent = config->cell->readCurrent;
            } else {
                // TODO
                // Note that no  such a large current, just large enough to recognize it is miss
                if (config->cell->accessType == CMOS_access) {
                    maxCurrent = CellPort.widthCmos * config->tech->featureSize() * 
                        (config->tech->currentOnNmos()[config->temperature - 300]
                         + config->tech->currentOffNmos()[config->temperature - 300] 
                         * (numCell - 1));
                } else if (config->cell->accessType == diode_access) {
                    double res = CalculateOnResistance(config->cell->widthAccessCMOS * config->tech->featureSize(), 
                            NMOS, config->temperature, config->tech) + config->cell->resistanceOn;
                    maxCurrent = config->tech->vdd() / res + CellPort.widthCmos * config->tech->featureSize() * 
                        config->tech->currentOffNmos()[config->temperature - 300] * (numCell - 1);
                } else if (config->cell->accessType == none_access) {
                    // TODO: too lazy to consider encoding here
                } else {
                    double res = CalculateOnResistance(config->cell->widthAccessCMOS * config->tech->featureSize(), 
                            NMOS, config->temperature, config->tech) + config->cell->resistanceOn;
                    maxCurrent = config->tech->vdd() / res * numCell;
                }
            }

            // Design for 2FeFET TCAM
        } else if (config->cell-> memCellType == FEFETRAM ||CellPort.Type == Searchline_Bitline) { 			                        //std::cout << "[CAM_Line] Warning: Make sure you are using 2FEFET TCAM design" << std::endl;
            if (config->cell->setMode) {
                maxCurrent = config->cell->setCurrent;
            }
            if (config->cell->resetMode) {
                maxCurrent = MAX(maxCurrent, config->cell->resetVoltage / config->cell->resistanceOn);
            } 
        }
        double maxCurrentBitline = 0;
        if (CellPort.Type == Matchline_Bitline) {
            // as bitline
            if (config->cell->setMode) {
                maxCurrentBitline = config->cell->setVoltage / config->cell->resistanceOff;
            } else {
                maxCurrentBitline = config->cell->setCurrent;
            }
            if (config->cell->resetMode) {
                maxCurrentBitline = MAX(maxCurrentBitline, 
                        config->cell->resetVoltage / config->cell->resistanceOn);
            } else {
                maxCurrentBitline = MAX(maxCurrentBitline, config->cell->setCurrent);
            }
            maxCurrentBitline += config->cell->leakageCurrentAccessDevice * (numCell - 1);
        }
        maxCurrent = MAX(maxCurrentBitline, maxCurrent);
    }

    if (isRow) {
        minMuxWidth = 0;
    } else {
        minMuxWidth = maxCurrent/ config->tech->currentOnNmos()[config->temperature - 300];
    }


    initialized = true;
}


void CAM_Line::Initialize(double _len, long long _numCell, double _MuxWidth, 
        std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire) {

    if (initialized) {
        config->logger.Verbose() << "[CAM_Line 2nd] Warning: Already initialized!";
    } else {
        localWire = _localWire;
        len = _len;
        config = _config;
        isRow = true;
        numCell = _numCell;
        cap = len * localWire->capWirePerUnit;
        res = len * localWire->resWirePerUnit;

        cap += CalculateGateCap(_MuxWidth * config->tech->featureSize(), config->tech) * numCell;
        maxCurrent = 1e11;
        minMuxWidth = 0;
        initialized = true;
    }
}
