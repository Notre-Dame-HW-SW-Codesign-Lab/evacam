#include "CAM_Line.h"
#include "formula.h"

void CAM_Line::Initialize(bool _isRow, int _index, double _len, long long _numCell, 
        std::shared_ptr<EvaCamConfig> _config, const Wire &_localWire) {

    config= _config;
    localWire = _localWire;
    auto& cell = *config->technology.cell;
    auto& tech = *config->technology.tech;
    auto& fefetTech = *config->technology.fefetTech;

    if (initialized) {
        config->logger.Verbose() << "[CAM_Line 1st] Warning: Already initialized!";
    } else {
        len = _len;
        isRow = _isRow;
        index = _index;
        CellPort = cell.camPort[!isRow][index];
        numCell = _numCell;
        cap = len * localWire.capWirePerUnit;
        res = len * localWire.resWirePerUnit;

        if (CellPort.widthWire > 1.0) {
            cap = len * CalculateWireCapacitance(
                    PERMITTIVITY, 
                    localWire.wireWidth * CellPort.widthWire,
                    localWire.wireThickness, 
                    localWire.wireSpacing, 
                    localWire.ildThickness, 
                    1.5,
                    localWire.horizontalDielectric, 
                    3.9, 
                    1.15e-10);
            res = len * CalculateWireResistance(
                    localWire.copper_resistivity, 
                    localWire.wireWidth * CellPort.widthWire,
                    localWire.wireThickness, 
                    localWire.barrierThickness, 
                    0, 
                    1);
        }

        const Technology &capTech = (cell.memCellType == FEFETRAM) ? fefetTech : tech;
        const double cmosWidth = CellPort.widthCmos * capTech.featureSize();
        const double cellWidth = cell.widthInFeatureSize * capTech.featureSize();
        const double portMultiplier = numCell * CellPort.numCmos;

        const auto gateCap = [&]() {
            return CalculateGateCap(cmosWidth, capTech);
        };

        const auto drainCap = [&]() {
            return CalculateDrainCap(cmosWidth, NMOS, cellWidth, capTech);
        };

        switch (CellPort.ConnectedRegion) {
            case gate:
                cap += gateCap() * portMultiplier;
                break;
            case diode:
                cap += (gateCap() * numCell + drainCap()) * portMultiplier;
                break;
            case drain:
            case source: // TODO: source is the weird case in ISSCC'15 3t1r, double check this is right
            default:
                cap += drainCap() * portMultiplier;
                break;
        }

        const int temperature = config->input.temperature;
        const double featureSize = tech.featureSize();

        const double accessOnResistance = CalculateOnResistance(
                cell.widthAccessCMOS * featureSize,
                NMOS,
                temperature,
                tech) + cell.resistanceOn;

        const double onCurrent = tech.currentOnNmos()[temperature - 300];
        const double offCurrent = tech.currentOffNmos()[temperature - 300];

        const auto bitlineCurrent = [&]() {
            double current = cell.setMode ? cell.setVoltage / cell.resistanceOff : cell.setCurrent;
            if (cell.resetMode) {
                current = MAX(current, cell.resetVoltage / cell.resistanceOn);
            } else {
                current = MAX(current, cell.setCurrent);
            }
            current += cell.leakageCurrentAccessDevice * (numCell - 1);
            return current;
        };

        const auto sourcelineCurrent = [&]() {
            double current = bitlineCurrent();
            config->logger.Log() << "[CAM_Line] Warning: Make sure you are using Meng-Fan Chang's design";
            // it is special coded for Dr. Chang's design
            // for search operation: worst case, search all-0 or all-1
            // TODO: replace Vp to Vdd for coding convenience
            // TODO: the current is toooo large, have no idea, make it smaller
            current = MAX(current, tech.vdd() / accessOnResistance);
            current = MAX(current, MAX(CellPort.volSearch0, CellPort.volSearch1) / accessOnResistance);
            return current;
        };

        maxCurrent = 0;
        switch (CellPort.Type) {
            case Wordline:
                if (CellPort.ConnectedRegion != gate) {
                    invalid = true;
                    config->logger.Verbose() << "[CAM_Line] Weird wordline description.";
                    return;
                }
                break;
            case Bitline:
            case Sourceline:
                if (CellPort.ConnectedRegion != drain && CellPort.ConnectedRegion != none) {
                    invalid = true;
                    config->logger.Verbose() << "[CAM_Line] Weird bitline/sourceline description.";
                    return;
                }
                maxCurrent = (CellPort.Type == Sourceline) ? sourcelineCurrent() : bitlineCurrent();
                break;
            case Matchline:
            case Matchline_Bitline:
                // calc all miss
                if (cell.readMode && cell.readVoltage == 0) { // voltage sensing, current-in
                    maxCurrent = cell.readCurrent;
                } else if (cell.accessType == CMOS_access) {
                    maxCurrent = CellPort.widthCmos * featureSize * (onCurrent + offCurrent * (numCell - 1));
                } else if (cell.accessType == diode_access) {
                    maxCurrent = tech.vdd() / accessOnResistance
                        + CellPort.widthCmos * featureSize * offCurrent * (numCell - 1);
                } else if (cell.accessType == none_access) {
                    // TODO: too lazy to consider encoding here
                } else {
                    // TODO
                    // Note that no such a large current, just large enough to recognize it is miss
                    maxCurrent = tech.vdd() / accessOnResistance * numCell;
                }
                break;
            default:
                // Design for 2FeFET TCAM
                if (cell.memCellType == FEFETRAM || CellPort.Type == Searchline_Bitline) {
                    if (cell.setMode) {
                        maxCurrent = cell.setCurrent;
                    }
                    if (cell.resetMode) {
                        maxCurrent = MAX(maxCurrent, cell.resetVoltage / cell.resistanceOn);
                    }
                }
                break;
        }

        const double maxCurrentBitline = (CellPort.Type == Matchline_Bitline) ? bitlineCurrent() : 0;
        maxCurrent = MAX(maxCurrentBitline, maxCurrent);
    }

    minMuxWidth = isRow ? 0 : maxCurrent/ tech.currentOnNmos()[config->input.temperature - 300];

    initialized = true;
}


void CAM_Line::Initialize(double _len, long long _numCell, double _MuxWidth, 
        std::shared_ptr<EvaCamConfig> _config, const Wire &_localWire) {

    if (initialized) {
        config->logger.Verbose() << "[CAM_Line 2nd] Warning: Already initialized!";
    } else {
        auto& tech = *_config->technology.tech;

        localWire = _localWire;
        len = _len;
        config = _config;
        isRow = true;
        numCell = _numCell;
        cap = len * localWire.capWirePerUnit;
        res = len * localWire.resWirePerUnit;

        cap += CalculateGateCap(_MuxWidth * tech.featureSize(), tech) * numCell;
        maxCurrent = 1e11;
        minMuxWidth = 0;
        initialized = true;
    }
}
