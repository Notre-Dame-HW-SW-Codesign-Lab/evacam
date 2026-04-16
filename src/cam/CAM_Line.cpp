#include "CAM_Line.h"
#include "formula.h"

void CAM_Line::Initialize(bool _isRow, int _index, double _len, long long _numCell, 
        std::shared_ptr<EvaCamConfig> _config, const Wire &_localWire) {

    config= _config;
    localWire = _localWire;

    if (initialized) {
        config->logger.Verbose() << "[CAM_Line 1st] Warning: Already initialized!";
    } else {
        len = _len;
        isRow = _isRow;
        index = _index;
        CellPort.IsCol = config->technology.cell->camPort[!isRow][index].IsCol;
        CellPort.ConnectedRegion = config->technology.cell->camPort[!isRow][index].ConnectedRegion;
        CellPort.Type = config->technology.cell->camPort[!isRow][index].Type;
        CellPort.isNMOS = config->technology.cell->camPort[!isRow][index].isNMOS;
        CellPort.leak = config->technology.cell->camPort[!isRow][index].leak;
        //CellPort.isNVMdischarge = config->technology.cell->camPort[!isRow][index].isNVMdischarge;
        CellPort.numCmos = config->technology.cell->camPort[!isRow][index].numCmos;
        CellPort.volReset = config->technology.cell->camPort[!isRow][index].volReset;
        CellPort.volSearch0 = config->technology.cell->camPort[!isRow][index].volSearch0;
        CellPort.volSearch1 = config->technology.cell->camPort[!isRow][index].volSearch1;
        CellPort.volSetLRS = config->technology.cell->camPort[!isRow][index].volSetLRS;
        CellPort.volSetMRS = config->technology.cell->camPort[!isRow][index].volSetMRS;
        CellPort.widthCmos = config->technology.cell->camPort[!isRow][index].widthCmos;
        CellPort.widthWire = config->technology.cell->camPort[!isRow][index].widthWire;
        numCell = _numCell;
        cap = len * localWire.capWirePerUnit;
        res = len * localWire.resWirePerUnit;

        if (CellPort.widthWire > 1.0) {
            cap = len * CalculateWireCapacitance(PERMITTIVITY, localWire.wireWidth*CellPort.widthWire,
                    localWire.wireThickness, localWire.wireSpacing, localWire.ildThickness, 1.5,
                    localWire.horizontalDielectric, 3.9, 1.15e-10);
            res = len * CalculateWireResistance(localWire.copper_resistivity, 
                    localWire.wireWidth*CellPort.widthWire,
                    localWire.wireThickness, localWire.barrierThickness, 0, 1);
        }

        if (CellPort.ConnectedRegion == gate) {
            if (config->technology.cell->memCellType == FEFETRAM) {
                cap += CalculateGateCap(CellPort.widthCmos * config->technology.fefetTech->featureSize(), config->technology.fefetTech) 
                    * numCell * CellPort.numCmos;
            } else {
                cap += CalculateGateCap(CellPort.widthCmos * config->technology.tech->featureSize(), config->technology.tech) 
                    * numCell * CellPort.numCmos;
            }			
        } else if (CellPort.ConnectedRegion == drain) {
            if (config->technology.cell->memCellType == FEFETRAM) {
                cap += CalculateDrainCap(CellPort.widthCmos * config->technology.fefetTech->featureSize(), NMOS, 
                        config->technology.cell->widthInFeatureSize * config->technology.fefetTech->featureSize(), config->technology.fefetTech)
                    * numCell * CellPort.numCmos;
            } else {
                cap += CalculateDrainCap(CellPort.widthCmos * config->technology.tech->featureSize(), NMOS, 
                        config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize(), config->technology.tech)
                    * numCell * CellPort.numCmos;
            }

        } else if (CellPort.ConnectedRegion == diode) {
            if (config->technology.cell->memCellType == FEFETRAM) {
                cap += ( CalculateGateCap(CellPort.widthCmos * config->technology.fefetTech->featureSize(), config->technology.fefetTech) 
                        * numCell + CalculateDrainCap(CellPort.widthCmos * config->technology.fefetTech->featureSize(), 
                            NMOS, config->technology.cell->widthInFeatureSize * config->technology.fefetTech->featureSize(), config->technology.fefetTech) )
                    * numCell * CellPort.numCmos;
            } else {
                cap += ( CalculateGateCap(CellPort.widthCmos * config->technology.tech->featureSize(), config->technology.tech) * numCell
                        + CalculateDrainCap(CellPort.widthCmos * config->technology.tech->featureSize(), NMOS, 
                            config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize(), config->technology.tech) )
                    * numCell * CellPort.numCmos;
            }
        } else if (CellPort.ConnectedRegion == source) { // it is source, which is the weird case in ISSCC'15 3t1r
                                                         // TODO
            if (config->technology.cell->memCellType == FEFETRAM){
                cap += CalculateDrainCap(CellPort.widthCmos * config->technology.fefetTech->featureSize(), NMOS, 
                        config->technology.cell->widthInFeatureSize * config->technology.fefetTech->featureSize(), config->technology.fefetTech)
                    * numCell * CellPort.numCmos;
            } else {
                cap += CalculateDrainCap(CellPort.widthCmos * config->technology.tech->featureSize(), NMOS,
                        config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize(), config->technology.tech)
                    * numCell * CellPort.numCmos;
            }
        } else {
            cap += CalculateDrainCap(CellPort.widthCmos * config->technology.tech->featureSize(), NMOS, 
                    config->technology.cell->widthInFeatureSize * config->technology.tech->featureSize(), config->technology.tech)
                * numCell * CellPort.numCmos;
        }

        maxCurrent = 0;
        if (CellPort.Type == Wordline) {
            if (CellPort.ConnectedRegion != gate) {
                invalid = true;
                config->logger.Verbose() << "[CAM_Line] Weird wordline description.";
                return;
            }
        } else if (CellPort.Type == Bitline || CellPort.Type == Sourceline) {
            if (CellPort.ConnectedRegion != drain && CellPort.ConnectedRegion != none) {
                invalid = true;
                config->logger.Verbose() << "[CAM_Line] Weird bitline/sourceline description.";
                return;
            }
            if (config->technology.cell->setMode) {
                maxCurrent = config->technology.cell->setVoltage / config->technology.cell->resistanceOff;
            } else {
                maxCurrent = config->technology.cell->setCurrent;
            }
            if (config->technology.cell->resetMode) {
                maxCurrent = MAX(maxCurrent, config->technology.cell->resetVoltage / config->technology.cell->resistanceOn);
            } else {
                maxCurrent = MAX(maxCurrent, config->technology.cell->setCurrent);
            }
            maxCurrent += config->technology.cell->leakageCurrentAccessDevice * (numCell - 1);
            if (CellPort.Type == Sourceline) { // the "column based resistor sourceline" in ISSCC'15 3t1r
                config->logger.Log() << "[CAM_Line] Warning: Make sure you are using Meng-Fan Chang's design";
                // it is special coded for Dr. Chang's design
                // for search operation: worst case, search all-0 or all-1
                // TODO: replace Vp to Vdd for coding convenience
                // TODO: the current is toooo large, have no idea, make it smaller
                double res = CalculateOnResistance(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), NMOS, 
                        config->input.temperature, config->technology.tech) + config->technology.cell->resistanceOn;
                maxCurrent = MAX(maxCurrent, config->technology.tech->vdd() / res);
                maxCurrent = MAX(maxCurrent, MAX(CellPort.volSearch0,CellPort.volSearch1) / res);
            }
        } else if (CellPort.Type == Matchline || CellPort.Type == Matchline_Bitline) {
            // calc all miss
            if (config->technology.cell->readMode && config->technology.cell->readVoltage == 0) { // voltage sensing, current-in
                maxCurrent = config->technology.cell->readCurrent;
            } else {
                // TODO
                // Note that no  such a large current, just large enough to recognize it is miss
                if (config->technology.cell->accessType == CMOS_access) {
                    maxCurrent = CellPort.widthCmos * config->technology.tech->featureSize() * 
                        (config->technology.tech->currentOnNmos()[config->input.temperature - 300]
                         + config->technology.tech->currentOffNmos()[config->input.temperature - 300] 
                         * (numCell - 1));
                } else if (config->technology.cell->accessType == diode_access) {
                    double res = CalculateOnResistance(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), 
                            NMOS, config->input.temperature, config->technology.tech) + config->technology.cell->resistanceOn;
                    maxCurrent = config->technology.tech->vdd() / res + CellPort.widthCmos * config->technology.tech->featureSize() * 
                        config->technology.tech->currentOffNmos()[config->input.temperature - 300] * (numCell - 1);
                } else if (config->technology.cell->accessType == none_access) {
                    // TODO: too lazy to consider encoding here
                } else {
                    double res = CalculateOnResistance(config->technology.cell->widthAccessCMOS * config->technology.tech->featureSize(), 
                            NMOS, config->input.temperature, config->technology.tech) + config->technology.cell->resistanceOn;
                    maxCurrent = config->technology.tech->vdd() / res * numCell;
                }
            }

            // Design for 2FeFET TCAM
        } else if (config->technology.cell-> memCellType == FEFETRAM ||CellPort.Type == Searchline_Bitline) { 			                        //std::cout << "[CAM_Line] Warning: Make sure you are using 2FEFET TCAM design" << std::endl;
            if (config->technology.cell->setMode) {
                maxCurrent = config->technology.cell->setCurrent;
            }
            if (config->technology.cell->resetMode) {
                maxCurrent = MAX(maxCurrent, config->technology.cell->resetVoltage / config->technology.cell->resistanceOn);
            } 
        }
        double maxCurrentBitline = 0;
        if (CellPort.Type == Matchline_Bitline) {
            // as bitline
            if (config->technology.cell->setMode) {
                maxCurrentBitline = config->technology.cell->setVoltage / config->technology.cell->resistanceOff;
            } else {
                maxCurrentBitline = config->technology.cell->setCurrent;
            }
            if (config->technology.cell->resetMode) {
                maxCurrentBitline = MAX(maxCurrentBitline, 
                        config->technology.cell->resetVoltage / config->technology.cell->resistanceOn);
            } else {
                maxCurrentBitline = MAX(maxCurrentBitline, config->technology.cell->setCurrent);
            }
            maxCurrentBitline += config->technology.cell->leakageCurrentAccessDevice * (numCell - 1);
        }
        maxCurrent = MAX(maxCurrentBitline, maxCurrent);
    }

    if (isRow) {
        minMuxWidth = 0;
    } else {
        minMuxWidth = maxCurrent/ config->technology.tech->currentOnNmos()[config->input.temperature - 300];
    }


    initialized = true;
}


void CAM_Line::Initialize(double _len, long long _numCell, double _MuxWidth, 
        std::shared_ptr<EvaCamConfig> _config, const Wire &_localWire) {

    if (initialized) {
        config->logger.Verbose() << "[CAM_Line 2nd] Warning: Already initialized!";
    } else {
        localWire = _localWire;
        len = _len;
        config = _config;
        isRow = true;
        numCell = _numCell;
        cap = len * localWire.capWirePerUnit;
        res = len * localWire.resWirePerUnit;

        cap += CalculateGateCap(_MuxWidth * config->technology.tech->featureSize(), config->technology.tech) * numCell;
        maxCurrent = 1e11;
        minMuxWidth = 0;
        initialized = true;
    }
}
