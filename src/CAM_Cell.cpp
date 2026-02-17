#include "../include/CAM_Cell.h"
#include "../include/formula.h"
#include "../include/global.h"
#include "../include/macros.h"
#include <math.h>
#include <fstream>

CAM_MemCell::CAM_MemCell() {
	// TODO Auto-generated constructor stub
	memCellType         = PCRAM;
	area                = 0;
	aspectRatio         = 0;
	resistanceOn        = 0;
	resistanceOff       = 0;
	readMode            = true;
	readVoltage         = 0;
	readCurrent         = 0;
	readPower           = 0;
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

	/* for CAM */
	camNumRow = 0;
	camNumCol = 0;
	camMLC = false;
	camWidthMatchTran = 0.0;

}
/*
CAM_MemCell::~CAM_MemCell() {
	// TODO Auto-generated destructor stub
}
*/
void CAM_MemCell::ReadCellFromFile(const std::string & inputFile, DesignTarget _designTarget, double _vdd) {
        designTarget = _designTarget;
        vdd = _vdd;

        (void)inputFile;
        throw std::runtime_error("Only YAML cell files are supported. Please provide a .yaml/.yml cell file.");
}

