#include "MemCell.h"
#include "formula.h"
#include "macros.h"
#include "YamlHelpers.h"
#include <math.h>
#include <bits/stdc++.h>

MemCell::MemCell() {
    memCellType         = PCRAM;
    area                = 0;
    aspectRatio         = 0;
    resistanceOn        = 0;
    resistanceOff       = 0;
    readMode            = true;
    readVoltage         = 0;
    readCurrent         = 0;
    readPower           = 0;
    readEnergy	    = 0;
    wordlineBoostRatio  = 1.0;
    resetMode           = true;
    resetVoltage        = 0;
    resetCurrent        = 0;
    minSenseVoltage     = 0.08;
    resetPulse          = 0;
    resetEnergy         = 0;
    setMode             = true;
    setVoltage          = 0;
    setCurrent          = 0;
    setPulse            = 0;
    accessType          = CMOS_access;
    processNode         = 0;
    setEnergy           = 0;

    /* Needs to be here for error free reasons */
    isNVMdischarge = false;

    /* Optional */
    stitching         = 0;
    gateOxThicknessFactor = 2;
    widthSOIDevice = 0;
    widthAccessCMOS   = 0;
    voltageDropAccessDevice = 0;
    leakageCurrentAccessDevice = 0;
    capDRAMCell		  = 0;
    widthSRAMCellNMOS = 2.08;	/* Default NMOS width in SRAM cells is 2.08 (from CACTI) */
    widthSRAMCellPMOS = 1.23;	/* Default PMOS width in SRAM cells is 1.23 (from CACTI) */

    /*For memristors */
    readFloating = false;
    resistanceOnAtSetVoltage = 0;
    resistanceOffAtSetVoltage = 0;
    resistanceOnAtResetVoltage = 0;
    resistanceOffAtResetVoltage = 0;
    resistanceOnAtReadVoltage = 0;
    resistanceOffAtReadVoltage = 0;
    resistanceOnAtHalfReadVoltage = 0;
    resistanceOffAtHalfReadVoltage = 0;
    resistanceOnAtHalfResetVoltage = 0;


    /* for CAM */
    camNumRow = 0;
    camNumCol = 0;
    camWidthMatchTran = 0.0;
    numResistanceState = 0;
    for (int i = 0; i < 64; i++) {
        ResistanceState[i] = 0;
        resStateVariation[i] = 0;
    }

    withVariation = false;
    resistanceOnVariation = 0;
    resistanceOffVariation = 0;

    /* For NAND flash */
    flashEraseVoltage = 0;
    flashPassVoltage = 0;
    flashProgramVoltage = 0;
    flashEraseTime = 0;
    flashProgramTime = 0;
    gateCouplingRatio = 0;

}
/*
MemCell::~MemCell() {
}
 */

void MemCell::ReadCellFromFile(const std::string & inputFile, DesignTarget _designTarget, double _vdd) {
    designTarget = _designTarget;
    vdd = _vdd;

    if (YamlHelpers::is_yaml_file(inputFile)) {
        YamlHelpers::ReadMemCellFromYaml(*this, inputFile);
        return;
    }

    throw std::runtime_error("Only YAML cell files are supported. Please provide a .yaml/.yml cell file.");
}

double MemCell::GetMemristance(double _relativeReadVoltage) { /* Get the LRS resistance of memristor at log-linera region of I-V curve */
    if (memCellType == memristor) {
        double x1, x2, x3;  // x1: read voltage, x2: half voltage, x3: applied voltage
        if (readVoltage == 0) {
            x1 = readCurrent * resistanceOnAtReadVoltage;
        } else {
            x1 = readVoltage;
        }
        x2 = readVoltage / 2;
        x3 = _relativeReadVoltage * readVoltage;
        double y1, y2 ,y3; // y1:log(read current), y2: log(leakage current at half read voltage
        y1 = log2(x1/resistanceOnAtReadVoltage);
        y2 = log2(x2/resistanceOnAtHalfReadVoltage);
        y3 = (y2 - y1) / (x2 -x1) * x3 + (x2 * y1 - x1 * y2) / (x2 - x1);  //insertion
        return x3 / pow(2, y3);
    } else {  // not memristor, can't call the function
        std::cout <<"Warning[MemCell] : Try to get memristance from a non-memristor memory cell" << std::endl;
        return -1;
    }
}

void MemCell::CalculateWriteEnergy() {
    if (resetEnergy == 0) {
        if (resetMode) {
            if (memCellType == memristor)
                if (accessType == none_access)
                    resetEnergy = fabs(resetVoltage) * (fabs(resetVoltage) - voltageDropAccessDevice) / resistanceOnAtResetVoltage * resetPulse;
                else
                    resetEnergy = fabs(resetVoltage) * (fabs(resetVoltage) - voltageDropAccessDevice) / resistanceOn * resetPulse;
            else if (memCellType == PCRAM)
                resetEnergy = fabs(resetVoltage) * (fabs(resetVoltage) - voltageDropAccessDevice) / resistanceOn * resetPulse;	// PCM cells shows low resistance during most time of the switching
            else if (memCellType == FBRAM)
                resetEnergy = fabs(resetVoltage) * fabs(resetCurrent) * resetPulse;
            else if (memCellType == FEFETRAM)
                resetEnergy = fabs(resetVoltage) * fabs(resetCurrent) * resetPulse;
            else
                resetEnergy = fabs(resetVoltage) * (fabs(resetVoltage) - voltageDropAccessDevice) / resistanceOn * resetPulse;
        } else {
            if (resetVoltage == 0){
                resetEnergy = vdd * fabs(resetCurrent) * resetPulse; /*TODO consider charge pump*/
            } else {
                resetEnergy = fabs(resetVoltage) * fabs(resetCurrent) * resetPulse;
            }
            /* previous model seems to be problematic
               if (memCellType == memristor)
               if (accessType == none_access)
               resetEnergy = resetCurrent * (resetCurrent * resistanceOffAtResetVoltage + voltageDropAccessDevice) * resetPulse;
               else
               resetEnergy = resetCurrent * (resetCurrent * resistanceOff + voltageDropAccessDevice) * resetPulse;
               else if (memCellType == PCRAM)
               resetEnergy = resetCurrent * (resetCurrent * resistanceOn + voltageDropAccessDevice) * resetPulse;		// PCM cells shows low resistance during most time of the switching
               else if (memCellType == FBRAM)
               resetEnergy = fabs(resetVoltage) * fabs(resetCurrent) * resetPulse;
               else
               resetEnergy = resetCurrent * (resetCurrent * resistanceOff + voltageDropAccessDevice) * resetPulse;
             */
        }
    }
    if (setEnergy == 0) {
        if (setMode) {
            if (memCellType == memristor)
                if (accessType == none_access)
                    setEnergy = fabs(setVoltage) * (fabs(setVoltage) - voltageDropAccessDevice) / resistanceOnAtSetVoltage * setPulse;
                else
                    setEnergy = fabs(setVoltage) * (fabs(setVoltage) - voltageDropAccessDevice) / resistanceOn * setPulse;
            else if (memCellType == PCRAM)
                setEnergy = fabs(setVoltage) * (fabs(setVoltage) - voltageDropAccessDevice) / resistanceOn * setPulse;			// PCM cells shows low resistance during most time of the switching
            else if (memCellType == FBRAM)
                setEnergy = fabs(setVoltage) * fabs(setCurrent) * setPulse;
            else if (memCellType == FEFETRAM)
                setEnergy = fabs(setVoltage) * fabs(setCurrent) * setPulse;
            else
                setEnergy = fabs(setVoltage) * (fabs(setVoltage) - voltageDropAccessDevice) / resistanceOn * setPulse;
        } else {
            if (resetVoltage == 0){
                setEnergy = vdd * fabs(setCurrent) * setPulse; /*TODO consider charge pump*/
            } else {
                setEnergy = fabs(setVoltage) * fabs(setCurrent) * setPulse;
            }
            /* previous model seems to be problematic
               if (memCellType == memristor)
               if (accessType == none_access)
               setEnergy = setCurrent * (setCurrent * resistanceOffAtSetVoltage + voltageDropAccessDevice) * setPulse;
               else
               setEnergy = setCurrent * (setCurrent * resistanceOff + voltageDropAccessDevice) * setPulse;
               else if (memCellType == PCRAM)
               setEnergy = setCurrent * (setCurrent * resistanceOn + voltageDropAccessDevice) * setPulse;		// PCM cells shows low resistance during most time of the switching
               else if (memCellType == FBRAM)
               setEnergy = fabs(setVoltage) * fabs(setCurrent) * setPulse;
               else
               setEnergy = setCurrent * (setCurrent * resistanceOff + voltageDropAccessDevice) * setPulse;
             */
        }
    }
}
double MemCell::CalculateReadPower() { /* TODO consider charge pumped read voltage */
    if (readPower == 0) {
        if (readMode) {	/* voltage-sensing */
            if (readVoltage == 0) { /* Current-in voltage sensing */
                return vdd * readCurrent;
            }
            if (readCurrent == 0) { /*Voltage-divider sensing */
                double resInSerialForSenseAmp, maxMatchlineCurrent;
                resInSerialForSenseAmp = sqrt(resistanceOn * resistanceOff);
                maxMatchlineCurrent = (readVoltage - voltageDropAccessDevice) / (resistanceOn + resInSerialForSenseAmp);
                return vdd * maxMatchlineCurrent;
            }
        } else { /* current-sensing */
            double maxMatchlineCurrent = (readVoltage - voltageDropAccessDevice) / resistanceOn;
            return vdd * maxMatchlineCurrent;
        }
    } else {
        return -1.0; /* should not call the function if read energy exists */
    }
    return -1.0;
}

void MemCell::PrintCell()
{
    char *type[6];
    type[0] = (char*)"Wordline";
    type[1] = (char*)"Searchline";
    type[2] = (char*)"Bitline";
    type[3] = (char*)"Sourceline";
    type[4] = (char*)"Matchline";
    type[5] = (char*)"Matchline_Bitline";
    type[6] = (char*)"Searchline_Bitline";

    char *region[5];
    region[0] = (char*)"gate";
    region[1] = (char*)"source";
    region[2] = (char*)"drain";
    region[3] = (char*)"diode";
    region[4] = (char*)"none";
    char *is[2];
    is[1] = (char*)"Yes";
    is[0] = (char*)"No";
    switch (memCellType) {
        case SRAM:
            std::cout << "Memory Cell: SRAM" << std::endl;
            break;
        case DRAM:
            std::cout << "Memory Cell: DRAM" << std::endl;
            break;
        case eDRAM:
            std::cout << "Memory Cell: Embedded DRAM" << std::endl;
            break;
        case MRAM:
            std::cout << "Memory Cell: MRAM (Magnetoresistive)" << std::endl;
            break;
        case PCRAM:
            std::cout << "Memory Cell: PCRAM (Phase-Change)" << std::endl;
            break;
        case memristor:
            std::cout << "Memory Cell: RRAM (Memristor)" << std::endl;
            break;
        case FBRAM:
            std::cout << "Memory Cell: FBRAM (Floating Body)" <<std::endl;
            break;
        case SLCNAND:
            std::cout << "Memory Cell: Single-Level Cell NAND Flash" << std::endl;
            break;
        case MLCNAND:
            std::cout << "Memory Cell: Multi-Level Cell NAND Flash" << std::endl;
            break;
        case FEFETRAM:
            std::cout << "Memory Cell: FeFET" << std::endl;
            break;
        default:
            std::cout << "Memory Cell: Unknown" << std::endl;
    }
    std::cout << "Cell Area (F^2)    : " << area << " (" << heightInFeatureSize << "Fx" << widthInFeatureSize << "F)" << std::endl;
    std::cout << "Cell Aspect Ratio  : " << aspectRatio << std::endl;

    if (memCellType == PCRAM || memCellType == MRAM || memCellType == memristor || memCellType == FBRAM || memCellType == FEFETRAM) {
        if (resistanceOn < 1e3 )
            std::cout << "Cell Turned-On Resistance : " << resistanceOn << "ohm" << std::endl;
        else if (resistanceOn < 1e6)
            std::cout << "Cell Turned-On Resistance : " << resistanceOn / 1e3 << "Kohm" << std::endl;
        else
            std::cout << "Cell Turned-On Resistance : " << resistanceOn / 1e6 << "Mohm" << std::endl;
        if (resistanceOff < 1e3 )
            std::cout << "Cell Turned-Off Resistance: "<< resistanceOff << "ohm" << std::endl;
        else if (resistanceOff < 1e6)
            std::cout << "Cell Turned-Off Resistance: "<< resistanceOff / 1e3 << "Kohm" << std::endl;
        else
            std::cout << "Cell Turned-Off Resistance: "<< resistanceOff / 1e6 << "Mohm" << std::endl;

        if (readMode) {
            std::cout << "Read Mode: Voltage-Sensing" << std::endl;
            if (readCurrent > 0)
                std::cout << "  - Read Current: " << readCurrent * 1e6 << "uA" << std::endl;
            if (readVoltage > 0)
                std::cout << "  - Read Voltage: " << readVoltage << "V" << std::endl;
        } else {
            std::cout << "Read Mode: Current-Sensing" << std::endl;
            if (readCurrent > 0)
                std::cout << "  - Read Current: " << readCurrent * 1e6 << "uA" << std::endl;
            if (readVoltage > 0)
                std::cout << "  - Read Voltage: " << readVoltage << "V" << std::endl;
        }

        if (resetMode) {
            std::cout << "Reset Mode: Voltage" << std::endl;
            std::cout << "  - Reset Voltage: " << resetVoltage << "V" << std::endl;
        } else {
            std::cout << "Reset Mode: Current" << std::endl;
            std::cout << "  - Reset Current: " << resetCurrent * 1e6 << "uA" << std::endl;
        }
        std::cout << "  - Reset Pulse: " << TO_SECOND(resetPulse) << std::endl;

        if (setMode) {
            std::cout << "Set Mode: Voltage" << std::endl;
            std::cout << "  - Set Voltage: " << setVoltage << "V" << std::endl;
        } else {
            std::cout << "Set Mode: Current" << std::endl;
            std::cout << "  - Set Current: " << setCurrent * 1e6 << "uA" << std::endl;
        }
        std::cout << "  - Set Pulse: " << TO_SECOND(setPulse) << std::endl;

        switch (accessType) {
            case CMOS_access:
                std::cout << "Access Type: CMOS" << std::endl;
                break;
            case BJT_access:
                std::cout << "Access Type: BJT" << std::endl;
                break;
            case diode_access:
                std::cout << "Access Type: Diode" << std::endl;
                break;
            default:
                std::cout << "Access Type: None Access Device" << std::endl;
        }
    } else if (memCellType == SRAM) {
        std::cout << "SRAM Cell Access Transistor Width: " << widthAccessCMOS << "F" << std::endl;
        std::cout << "SRAM Cell NMOS Width: " << widthSRAMCellNMOS << "F" << std::endl;
        std::cout << "SRAM Cell PMOS Width: " << widthSRAMCellPMOS << "F" << std::endl;
    } else if (memCellType == SLCNAND) {
        std::cout << "Pass Voltage       : " << flashPassVoltage << "V" << std::endl;
        std::cout << "Programming Voltage: " << flashProgramVoltage << "V" << std::endl;
        std::cout << "Erase Voltage      : " << flashEraseVoltage << "V" << std::endl;
        std::cout << "Programming Time   : " << TO_SECOND(flashProgramTime) << std::endl;
        std::cout << "Erase Time         : " << TO_SECOND(flashEraseTime) << std::endl;
        std::cout << "Gate Coupling Ratio: " << gateCouplingRatio << std::endl;
    }
    std::cout << "===========   For CAM  ==============" << std::endl;
    // std::cout << "MLC used             : " << camMLC << std::endl;
    std::cout << "  -Match CMOS Width (F) : " << camWidthMatchTran << std::endl;
    std::cout << "  -NVM discharge	  : " << isNVMdischarge << std::endl;
    for(int i=0;i<camNumRow;i++) {
        std::cout << "**** Row Port " << i << " ******" << std::endl;
        std::cout << "  -PortType          : " << type[camPort[0][i].Type] << std::endl;
        std::cout << "  -CMOS Region       : " << region[camPort[0][i].ConnectedRegion] << std::endl;
        std::cout << "  -CMOS Number       : " << camPort[0][i].numCmos << std::endl;
        std::cout << "  -CMOS Width        : " << camPort[0][i].widthCmos << std::endl;
        std::cout << "  -NMOS              : " << is[camPort[0][i].isNMOS] << std::endl;
        std::cout << "  -Set LRS Voltage   : " << camPort[0][i].volSetLRS << std::endl;
        std::cout << "  -Set MRS Voltage   : " << camPort[0][i].volSetMRS << std::endl;
        std::cout << "  -Reset Voltage     : " << camPort[0][i].volReset << std::endl;
        std::cout << "  -Search 0 Voltage  : " << camPort[0][i].volSearch0 << std::endl;
        std::cout << "  -Search 1 Voltage  : " << camPort[0][i].volSearch1 << std::endl;
        std::cout << "  -wire width        : " << camPort[0][i].widthWire << std::endl;
    }

    for(int i=0;i<camNumCol;i++) {
        std::cout << "**** Col Port " << i << " ******" << std::endl;
        std::cout << "  -PortType          : " << type[camPort[1][i].Type] << std::endl;
        std::cout << "  -CMOS Region       : " << region[camPort[1][i].ConnectedRegion] << std::endl;
        std::cout << "  -CMOS Number       : " << camPort[1][i].numCmos << std::endl;
        std::cout << "  -CMOS Width        : " << camPort[1][i].widthCmos << std::endl;
        std::cout << "  -NMOS              : " << is[camPort[1][i].isNMOS] << std::endl;
        std::cout << "  -Set LRS Voltage   : " << camPort[1][i].volSetLRS << std::endl;
        std::cout << "  -Set MRS Voltage   : " << camPort[1][i].volSetMRS << std::endl;
        std::cout << "  -Reset Voltage     : " << camPort[1][i].volReset << std::endl;
        std::cout << "  -Search 0 Voltage  : " << camPort[1][i].volSearch0 << std::endl;
        std::cout << "  -Search 1 Voltage  : " << camPort[1][i].volSearch1 << std::endl;
        std::cout << "  -wire width        : " << camPort[1][i].widthWire << std::endl;
    }
}
