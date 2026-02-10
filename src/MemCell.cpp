#include "../include/MemCell.h"
#include "../include/formula.h"
#include "../include/global.h"
#include "../include/macros.h"
#include <math.h>
#include <bits/stdc++.h>

MemCell::MemCell() {
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

		
	/* for CAM */
	camNumRow = 0;
	camNumCol = 0;
	camWidthMatchTran = 0.0;

	withVariation = false;
	resistanceOnVariation = 0;
	resistanceOffVariation = 0;

}
/*
MemCell::~MemCell() {
	// TODO Auto-generated destructor stub
}
*/

void MemCell::ReadCellFromFile(const std::string & inputFile, DesignTarget _designTarget, double _vdd) {
        designTarget = _designTarget;
        vdd = _vdd;

	FILE *fp = fopen(inputFile.c_str(), "r");
	char line[5000];
	char tmp[5000];
	int index;

	if (!fp) {
		std::cout << inputFile << " cannot be found!\n";
		exit(-1);
	}

	while (fscanf(fp, "%[^\n]\n", line) != EOF) {
		if (!strncmp("-MemCellType", line, strlen("-MemCellType"))) {
			sscanf(line, "-MemCellType: %s", tmp);
			if (!strcmp(tmp, "SRAM"))
				memCellType = SRAM;
			else if (!strcmp(tmp, "DRAM"))
				memCellType = DRAM;
			else if (!strcmp(tmp, "eDRAM"))
				memCellType = eDRAM;
			else if (!strcmp(tmp, "MRAM"))
				memCellType = MRAM;
			else if (!strcmp(tmp, "PCRAM"))
				memCellType = PCRAM;
			else if (!strcmp(tmp, "FBRAM"))
				memCellType = FBRAM;
			else if (!strcmp(tmp, "memristor"))
				memCellType = memristor;
			else if (!strcmp(tmp, "SLCNAND"))
				memCellType = SLCNAND;
			else if (!strcmp(tmp, "MLCNAND"))
				memCellType = MLCNAND;
			else 
				memCellType = FEFETRAM;
			continue;
		}
		if (!strncmp("-ProcessNode", line, strlen("-ProcessNode"))) {
			sscanf(line, "-ProcessNode: %d", &processNode);
			continue;
		}
		if (!strncmp("-CellArea", line, strlen("-CellArea"))) {
			sscanf(line, "-CellArea (F^2): %lf", &area);
			continue;
		}
		if (!strncmp("-CellAspectRatio", line, strlen("-CellAspectRatio"))) {
			sscanf(line, "-CellAspectRatio: %lf", &aspectRatio);
			heightInFeatureSize = sqrt(area * aspectRatio);
			widthInFeatureSize = sqrt(area / aspectRatio);
			continue;
		}

		if (!strncmp("-CAMType", line, strlen("-CAMType"))) {
			sscanf(line, "-CAMType: %s", tmp);
			if (!strcmp(tmp, "ACAM")){
				camType = ACAM;
			} else if (!strcmp(tmp, "MCAM")){
				camType = MCAM;
			} else if(!strcmp(tmp, "TCAM")){
				camType = TCAM;
			} else {
				std::cout << "Unsupported CAM type." << std::endl;
				exit(1);
			}
			continue;
		}

		if (!strncmp("-ResistanceOnAtSetVoltage", line, strlen("-ResistanceOnAtSetVoltage"))) {
			sscanf(line, "-ResistanceOnAtSetVoltage (ohm): %lf", &resistanceOnAtSetVoltage);
			continue;
		}
		if (!strncmp("-ResistanceOffAtSetVoltage", line, strlen("-ResistanceOffAtSetVoltage"))) {
			sscanf(line, "-ResistanceOffAtSetVoltage (ohm): %lf", &resistanceOffAtSetVoltage);
			continue;
		}
		if (!strncmp("-ResistanceOnAtResetVoltage", line, strlen("-ResistanceOnAtResetVoltage"))) {
			sscanf(line, "-ResistanceOnAtResetVoltage (ohm): %lf", &resistanceOnAtResetVoltage);
			continue;
		}
		if (!strncmp("-ResistanceOffAtResetVoltage", line, strlen("-ResistanceOffAtResetVoltage"))) {
			sscanf(line, "-ResistanceOffAtResetVoltage (ohm): %lf", &resistanceOffAtResetVoltage);
			continue;
		}
		if (!strncmp("-ResistanceOnAtReadVoltage", line, strlen("-ResistanceOnAtReadVoltage"))) {
			sscanf(line, "-ResistanceOnAtReadVoltage (ohm): %lf", &resistanceOnAtReadVoltage);
			resistanceOn = resistanceOnAtReadVoltage;
			continue;
		}
		if (!strncmp("-ResistanceOffAtReadVoltage", line, strlen("-ResistanceOffAtReadVoltage"))) {
			sscanf(line, "-ResistanceOffAtReadVoltage (ohm): %lf", &resistanceOffAtReadVoltage);
			resistanceOff = resistanceOffAtReadVoltage;
			continue;
		}
		if (!strncmp("-ResistanceOnAtHalfReadVoltage", line, strlen("-ResistanceOnAtHalfReadVoltage"))) {
			sscanf(line, "-ResistanceOnAtHalfReadVoltage (ohm): %lf", &resistanceOnAtHalfReadVoltage);
			continue;
		}
		if (!strncmp("-ResistanceOffAtHalfReadVoltage", line, strlen("-ResistanceOffAtHalfReadVoltage"))) {
			sscanf(line, "-ResistanceOffAtHalfReadVoltage (ohm): %lf", &resistanceOffAtHalfReadVoltage);
			continue;
		}
		if (!strncmp("-ResistanceOnAtHalfResetVoltage", line, strlen("-ResistanceOnAtHalfResetVoltage"))) {
			sscanf(line, "-ResistanceOnAtHalfResetVoltage (ohm): %lf", &resistanceOnAtHalfResetVoltage);
			continue;
		}

		if (!strncmp("-ResistanceOn", line, strlen("-ResistanceOn"))) {
			sscanf(line, "-ResistanceOn (ohm): %lf", &resistanceOn);
			continue;
		}

		if (!strncmp("-ResistanceOff", line, strlen("-ResistanceOff"))) {
			sscanf(line, "-ResistanceOff (ohm): %lf", &resistanceOff);
			continue;
		}
		if (!strncmp("-CapacitanceOn", line, strlen("-CapacitanceOn"))) {
			sscanf(line, "-CapacitanceOn (F): %lf", &capacitanceOn);
			continue;
		}
		if (!strncmp("-CapacitanceOff", line, strlen("-CapacitanceOff"))) {
			sscanf(line, "-CapacitanceOff (F): %lf", &capacitanceOff);
			continue;
		}

		if (!strncmp("-GateOxThicknessFactor", line, strlen("-GateOxThicknessFactor"))) {
			sscanf(line, "-GateOxThicknessFactor: %lf", &gateOxThicknessFactor);
			continue;
		}

		if (!strncmp("-SOIDeviceWidth (F)", line, strlen("-SOIDeviceWidth (F)"))) {
			sscanf(line, "-SOIDeviceWidth (F): %lf", &widthSOIDevice);
			continue;
		}

		if (!strncmp("-ReadMode", line, strlen("-ReadMode"))) {
			sscanf(line, "-ReadMode: %s", tmp);
			if (!strcmp(tmp, "voltage"))
				readMode = true;
			else
				readMode = false;
			continue;
		}
		if (!strncmp("-ReadVoltage", line, strlen("-ReadVoltage"))) {
			sscanf(line, "-ReadVoltage (V): %lf", &readVoltage);
			continue;
		}
		if (!strncmp("-ReadCurrent", line, strlen("-ReadCurrent"))) {
			sscanf(line, "-ReadCurrent (uA): %lf", &readCurrent);
			readCurrent /= 1e6;
			continue;
		}
		if (!strncmp("-ReadPower", line, strlen("-ReadPower"))) {
			sscanf(line, "-ReadPower (uW): %lf", &readPower);
			readPower /= 1e6;
			continue;
		}
		if (!strncmp("-ReadEnergy", line, strlen("-ReadEnergy"))) {
			sscanf(line, "-ReadEnergy (fJ): %lf", &readEnergy);
			readEnergy /= 1e15;
			continue;
		}
		if (!strncmp("-WordlineBoostRatio", line, strlen("-WordlineBoostRatio"))) {
			sscanf(line, "-WordlineBoostRatio: %lf", &wordlineBoostRatio);
			continue;
		}
		if (!strncmp("-MinSenseVoltage", line, strlen("-MinSenseVoltage"))) {
			sscanf(line, "-MinSenseVoltage (mV): %lf", &minSenseVoltage);
			minSenseVoltage /= 1e3;
			continue;
		}


		if (!strncmp("-ResetMode", line, strlen("-ResetMode"))) {
			sscanf(line, "-ResetMode: %s", tmp);
			if (!strcmp(tmp, "voltage"))
				resetMode = true;
			else
				resetMode = false;
			continue;
		}
		if (!strncmp("-ResetVoltage", line, strlen("-ResetVoltage"))) {
			sscanf(line, "-ResetVoltage (V): %lf", &resetVoltage);
			continue;
		}
		if (!strncmp("-ResetCurrent", line, strlen("-ResetCurrent"))) {
			sscanf(line, "-ResetCurrent (uA): %lf", &resetCurrent);
			resetCurrent /= 1e6;
			continue;
		}
		if (!strncmp("-ResetVoltage", line, strlen("-ResetVoltage"))) {
			sscanf(line, "-ResetVoltage (V): %lf", &resetVoltage);
			continue;
		}
		if (!strncmp("-ResetPulse", line, strlen("-ResetPulse"))) {
			sscanf(line, "-ResetPulse (ns): %lf", &resetPulse);
			resetPulse /= 1e9;
			continue;
		}
		if (!strncmp("-ResetEnergy", line, strlen("-ResetEnergy"))) {
			sscanf(line, "-ResetEnergy (pJ): %lf", &resetEnergy);
			resetEnergy /= 1e12;
			continue;
		}

		if (!strncmp("-SetMode", line, strlen("-SetMode"))) {
			sscanf(line, "-SetMode: %s", tmp);
			if (!strcmp(tmp, "voltage"))
				setMode = true;
			else
				setMode = false;
			continue;
		}
		if (!strncmp("-SetVoltage", line, strlen("-SetVoltage"))) {
			sscanf(line, "-SetVoltage (V): %lf", &setVoltage);
			continue;
		}
		if (!strncmp("-SetCurrent", line, strlen("-SetCurrent"))) {
			sscanf(line, "-SetCurrent (uA): %lf", &setCurrent);
			setCurrent /= 1e6;
			continue;
		}
		if (!strncmp("-SetVoltage", line, strlen("-SetVoltage"))) {
			sscanf(line, "-SetVoltage (V): %lf", &setVoltage);
			continue;
		}
		if (!strncmp("-SetPulse", line, strlen("-SetPulse"))) {
			sscanf(line, "-SetPulse (ns): %lf", &setPulse);
			setPulse /= 1e9;
			continue;
		}
		if (!strncmp("-SetEnergy", line, strlen("-SetEnergy"))) {
			sscanf(line, "-SetEnergy (pJ): %lf", &setEnergy);
			setEnergy /= 1e12;
			continue;
		}

		if (!strncmp("-AccessType", line, strlen("-AccessType"))) {
			sscanf(line, "-AccessType: %s", tmp);
			if (!strcmp(tmp, "CMOS"))
				accessType = CMOS_access;
			else if (!strcmp(tmp, "BJT"))
				accessType = BJT_access;
			else if (!strcmp(tmp, "diode"))
				accessType = diode_access;
			else
				accessType = none_access;
			continue;
		}
		if (!strncmp("-AccessCMOSWidth", line, strlen("-AccessCMOSWidth"))) {
				sscanf(line, "-AccessCMOSWidth (F): %lf", &widthAccessCMOS);
			continue;
		}

		if (!strncmp("-VoltageDropAccessDevice", line, strlen("-VoltageDropAccessDevice"))) {
			sscanf(line, "-VoltageDropAccessDevice (V): %lf", &voltageDropAccessDevice);
			continue;
		}

		if (!strncmp("-LeakageCurrentAccessDevice", line, strlen("-LeakageCurrentAccessDevice"))) {
			sscanf(line, "-LeakageCurrentAccessDevice (uA): %lf", &leakageCurrentAccessDevice);
			leakageCurrentAccessDevice /= 1e6;
			continue;
		}

		if (!strncmp("-DRAMCellCapacitance", line, strlen("-DRAMCellCapacitance"))) {
			if (memCellType != DRAM && memCellType != eDRAM)
				std::cout << "Warning: The input of DRAM cell capacitance is ignored because the memory cell is not DRAM." << std::endl;
			else
				sscanf(line, "-DRAMCellCapacitance (F): %lf", &capDRAMCell);
			continue;
		}

		if (!strncmp("-SRAMCellNMOSWidth", line, strlen("-SRAMCellNMOSWidth"))) {
			if (memCellType != SRAM)
				std::cout << "Warning: The input of SRAM cell NMOS width is ignored because the memory cell is not SRAM." << std::endl;
			else
				sscanf(line, "-SRAMCellNMOSWidth (F): %lf", &widthSRAMCellNMOS);
			continue;
		}

		if (!strncmp("-SRAMCellPMOSWidth", line, strlen("-SRAMCellPMOSWidth"))) {
			if (memCellType != SRAM)
				std::cout << "Warning: The input of SRAM cell PMOS width is ignored because the memory cell is not SRAM." << std::endl;
			else
				sscanf(line, "-SRAMCellPMOSWidth (F): %lf", &widthSRAMCellPMOS);
			continue;
		}


		if (!strncmp("-ReadFloating", line, strlen("-ReadFloating"))) {
			sscanf(line, "-ReadFloating: %s", tmp);
			if (!strcmp(tmp, "true"))
				readFloating = true;
			else
				readFloating = false;
			continue;
		}

		if (!strncmp("-FlashEraseVoltage (V)", line, strlen("-FlashEraseVoltage (V)"))) {
			if (memCellType != SLCNAND && memCellType != MLCNAND)
				std::cout << "Warning: The input of programming/erase voltage is ignored because the memory cell is not flash." << std::endl;
			else
				sscanf(line, "-FlashEraseVoltage (V): %lf", &flashEraseVoltage);
			continue;
		}

		if (!strncmp("-FlashProgramVoltage (V)", line, strlen("-FlashProgramVoltage (V)"))) {
			if (memCellType != SLCNAND && memCellType != MLCNAND)
				std::cout << "Warning: The input of programming/program voltage is ignored because the memory cell is not flash." << std::endl;
			else
				sscanf(line, "-FlashProgramVoltage (V): %lf", &flashProgramVoltage);
			continue;
		}

		if (!strncmp("-FlashPassVoltage (V)", line, strlen("-FlashPassVoltage (V)"))) {
			if (memCellType != SLCNAND && memCellType != MLCNAND)
				std::cout << "Warning: The input of pass voltage is ignored because the memory cell is not flash." << std::endl;
			else
				sscanf(line, "-FlashPassVoltage (V): %lf", &flashPassVoltage);
			continue;
		}

		if (!strncmp("-FlashEraseTime", line, strlen("-FlashEraseTime"))) {
			if (memCellType != SLCNAND && memCellType != MLCNAND)
				std::cout << "Warning: The input of erase time is ignored because the memory cell is not flash." << std::endl;
			else {
				sscanf(line, "-FlashEraseTime (ms): %lf", &flashEraseTime);
				flashEraseTime /= 1e3;
			}
			continue;
		}

		if (!strncmp("-FlashProgramTime", line, strlen("-FlashProgramTime"))) {
			if (memCellType != SLCNAND && memCellType != MLCNAND)
				std::cout << "Warning: The input of erase time is ignored because the memory cell is not flash." << std::endl;
			else {
				sscanf(line, "-FlashProgramTime (us): %lf", &flashProgramTime);
				flashProgramTime /= 1e6;
			}
			continue;
		}

		if (!strncmp("-GateCouplingRatio", line, strlen("-GateCouplingRatio"))) {
			if (memCellType != SLCNAND && memCellType != MLCNAND)
				std::cout << "Warning: The input of gate coupling ratio (GCR) is ignored because the memory cell is not flash." << std::endl;
			else {
				sscanf(line, "-GateCouplingRatio: %lf", &gateCouplingRatio);
			}
			continue;
		}
		// if (!strncmp("-MatchCMOSWidth", line, strlen("-MatchCMOSWidth"))) {
		// 	if (inputParameter->designTarget != CAM_chip)
		// 		std::cout << "Warning: The input of MatchCMOSWidth is ignored because the memory is not CAM." << std::endl;
		// 	else {
		// 		sscanf(line, "-MatchCMOSWidth (F): %lf", &camWidthMatchTran);
		// 	}
		// 	continue;
		// }
		if (!strncmp("-NumRowPort", line, strlen("-NumRowPort"))) {
				sscanf(line, "-NumRowPort: %d", &camNumRow);
				for(int i=0;i<camNumRow;i++) {
					camPort[0][i].IsCol = false;
			}
			continue;
		}
		if (!strncmp("-RowPort:PortType", line, strlen("-RowPort:PortType"))) {
				sscanf(line, "-RowPort:PortType: %d:%s", &index, tmp);
				if (index > camNumRow) {
					std::cout << "Error: The input of RowPort index error." << std::endl;
					exit(1);
				}
				if (!strcmp(tmp, "Wordline"))
					camPort[0][index].Type = Wordline;
				else if (!strcmp(tmp, "Sourceline"))
					camPort[0][index].Type = Sourceline;
				else if (!strcmp(tmp, "Searchline"))
					camPort[0][index].Type = Searchline;
				else if (!strcmp(tmp, "Bitline"))
					camPort[0][index].Type = Bitline;
				else if (!strcmp(tmp, "Dataline"))
					camPort[0][index].Type = Dataline;
				else if (!strcmp(tmp, "Matchline"))
					camPort[0][index].Type = Matchline;
				else if (!strcmp(tmp, "Matchline_Bitline"))
					camPort[0][index].Type = Matchline_Bitline;
				else if (!strcmp(tmp, "Searchline_Bitline"))
					camPort[0][index].Type = Searchline_Bitline;
				else {
					std::cout << "Error: The input of RowPort:Type error." << std::endl;
					exit(1);
				}
			continue;
		}
		if (!strncmp("-RowPort:CmosRegion", line, strlen("-RowPort:CmosRegion"))) {
				sscanf(line, "-RowPort:CmosRegion: %d:%s", &index, tmp);
				if (index > camNumRow) {
					std::cout << "Error: The input of RowPort index error." << std::endl;
					exit(1);
				}
				if (!strcmp(tmp, "gate"))
					camPort[0][index].ConnectedRegion = gate;
				else if (!strcmp(tmp, "source"))
					camPort[0][index].ConnectedRegion = source;
				else if (!strcmp(tmp, "drain"))
					camPort[0][index].ConnectedRegion = drain;
				else if (!strcmp(tmp, "diode"))
					camPort[0][index].ConnectedRegion = diode;
				else if (!strcmp(tmp, "none"))
					camPort[0][index].ConnectedRegion = none;
				else {
					std::cout << "Error: The input of RowPort:CmosRegion error." << std::endl;
					exit(1);
				}
			continue;
		}
		if (!strncmp("-RowPort:numCmos", line, strlen("-RowPort:numCmos"))) {
			if (designTarget != CAM_chip)
				std::cout << "Warning: The input of RowPort:numCmos is ignored because the memory is not CAM." << std::endl;
			else {
				int temp;
				sscanf(line, "-RowPort:numCmos: %d:%d", &index, &temp);
				camPort[0][index].numCmos = temp;
			}
			continue;
		}
		if (!strncmp("-RowPort:widthCmos", line, strlen("-RowPort:widthCmos"))) {
				double temp;
				sscanf(line, "-RowPort:widthCmos (F): %d:%lf", &index, &temp);
				camPort[0][index].widthCmos = temp;
		}
		if (!strncmp("-RowPort:isNMOS", line, strlen("-RowPort:isNMOS"))) {
				sscanf(line, "-RowPort:isNMOS: %d:%s", &index, tmp);
				if (!strcmp(tmp, "true")) {
					camPort[0][index].isNMOS = true;
				} else {
					camPort[0][index].isNMOS = false;
			}
			continue;
		}
		if (!strncmp("-RowPort:volSetLRS", line, strlen("-RowPort:volSetLRS"))) {
				double temp;
				sscanf(line, "-RowPort:volSetLRS (V): %d:%lf", &index, &temp);
				camPort[0][index].volSetLRS = temp;
			continue;
		
		}
		if (!strncmp("-RowPort:volReset", line, strlen("-RowPort:volReset"))) {
				double temp;
				sscanf(line, "-RowPort:volReset (V): %d:%lf", &index, &temp);
				camPort[0][index].volReset = temp;
			continue;
		}
		if (!strncmp("-RowPort:volSetMRS", line, strlen("-RowPort:volSetMRS"))) {
				double temp;
				sscanf(line, "-RowPort:volSetMRS (V): %d:%lf", &index, &temp);
				camPort[0][index].volSetMRS = temp;
			continue;
		}
		if (!strncmp("-RowPort:volSearch0", line, strlen("-RowPort:volSearch0"))) {
				double temp;
				sscanf(line, "-RowPort:volSearch0 (V): %d:%lf", &index, &temp);
				camPort[0][index].volSearch0 = temp;
			continue;
		}
		if (!strncmp("-RowPort:volSearch1", line, strlen("-RowPort:volSearch1"))) {
				double temp;
				sscanf(line, "-RowPort:volSearch1 (V): %d:%lf", &index, &temp);
				camPort[0][index].volSearch1 = temp;
			continue;
		}
		if (!strncmp("-RowPort:widthWire", line, strlen("-RowPort:widthWire"))) {
				double temp;
				sscanf(line, "-RowPort:widthWire: %d:%lf", &index, &temp);
				camPort[0][index].widthWire = temp;
			continue;
		}
		if (!strncmp("-RowPort:leak", line, strlen("-RowPort:leak"))) {
				sscanf(line, "-RowPort:leak: %d:%s", &index, tmp);
				if (!strcmp(tmp, "true")) {
					camPort[0][index].leak = true;
				} else {
					camPort[0][index].leak = false;
				}
			continue;
		}
		if (!strncmp("-NumColPort", line, strlen("-NumColPort"))) {
				sscanf(line, "-NumColPort: %d", &camNumCol);
				for(int i=0;i<camNumCol;i++) {
					camPort[1][i].IsCol = true;
				}
			continue;
		}
		if (!strncmp("-ColPort:PortType", line, strlen("-ColPort:PortType"))) {
				sscanf(line, "-ColPort:PortType: %d:%s", &index, tmp);
				if (index > camNumCol) {
					std::cout << "Error: The input of ColPort index error." << std::endl;
					exit(1);
				}
				if (!strcmp(tmp, "Wordline"))
					camPort[1][index].Type = Wordline;
				else if (!strcmp(tmp, "Sourceline"))
					camPort[1][index].Type = Sourceline;
				else if (!strcmp(tmp, "Searchline"))
					camPort[1][index].Type = Searchline;
				else if (!strcmp(tmp, "Bitline"))
					camPort[1][index].Type = Bitline;
				else if (!strcmp(tmp, "Matchline"))
					camPort[1][index].Type = Matchline;
				else if (!strcmp(tmp, "Matchline_Bitline"))
					camPort[1][index].Type = Matchline_Bitline;
				else if (!strcmp(tmp, "Searchline_Bitline"))
					camPort[1][index].Type = Searchline_Bitline;
				else {
					std::cout << "Error: The input of ColPort:Type error." << std::endl;
					exit(1);
				}
			continue;
		}
		if (!strncmp("-ColPort:CmosRegion", line, strlen("-ColPort:CmosRegion"))) {
				sscanf(line, "-ColPort:CmosRegion: %d:%s", &index, tmp);
				if (index > camNumCol) {
					std::cout << "Error: The input of ColPort index error." << std::endl;
					exit(1);
				}
				if (!strcmp(tmp, "gate"))
					camPort[1][index].ConnectedRegion = gate;
				else if (!strcmp(tmp, "source"))
					camPort[1][index].ConnectedRegion = source;
				else if (!strcmp(tmp, "drain"))
					camPort[1][index].ConnectedRegion = drain;
				else if (!strcmp(tmp, "diode"))
					camPort[1][index].ConnectedRegion = diode;
				else if (!strcmp(tmp, "none"))
					camPort[1][index].ConnectedRegion = none;
				else {
					std::cout << "Error: The input of ColPort:CmosRegion error." << std::endl;
					exit(1);
				}
			continue;
		}
		if (!strncmp("-ColPort:numCmos", line, strlen("-ColPort:numCmos"))) {
				int temp;
				sscanf(line, "-ColPort:numCmos: %d:%d", &index, &temp);
				camPort[1][index].numCmos = temp;
			continue;
		}
		if (!strncmp("-ColPort:widthCmos", line, strlen("-ColPort:widthCmos"))) {
				double temp;
				sscanf(line, "-ColPort:widthCmos (F): %d:%lf", &index, &temp);
				camPort[1][index].widthCmos = temp;
			continue;
		}
		if (!strncmp("-ColPort:isNMOS", line, strlen("-ColPort:isNMOS"))) {
				sscanf(line, "-ColPort:isNMOS: %d:%s", &index, tmp);
				if (!strcmp(tmp, "true")) {
					camPort[1][index].isNMOS = true;
				} else {
					camPort[1][index].isNMOS = false;
				}
			continue;
		}
		if (!strncmp("-isNVMdischarge", line, strlen("-isNVMdischarge"))) {
				sscanf(line, "-isNVMdischarge: %s", tmp);
				if (!strcmp(tmp, "true")) {
					isNVMdischarge = true;
				} else if(!strcmp(tmp, "false")){
					isNVMdischarge = false;
				} else{
					std::cout << "[Input Error]: No NVM discharge specification." << std::endl;
				}
			continue;
		}
		if (!strncmp("-ColPort:volSetLRS", line, strlen("-ColPort:volSetLRS"))) {
				double temp;
				sscanf(line, "-ColPort:volSetLRS (V): %d:%lf", &index, &temp);
				camPort[1][index].volSetLRS = temp;
			continue;
		}
		if (!strncmp("-ColPort:volReset", line, strlen("-ColPort:volReset"))) {
				double temp;
				sscanf(line, "-ColPort:volReset (V): %d:%lf", &index, &temp);
				camPort[1][index].volReset = temp;
			continue;
		}
		if (!strncmp("-ColPort:volSetMRS", line, strlen("-ColPort:volSetMRS"))) {
				double temp;
				sscanf(line, "-ColPort:volSetMRS (V): %d:%lf", &index, &temp);
				camPort[1][index].volSetMRS = temp;
			continue;
		}
		if (!strncmp("-ColPort:volSearch0", line, strlen("-ColPort:volSearch0"))) {
				double temp;
				sscanf(line, "-ColPort:volSearch0 (V): %d:%lf", &index, &temp);
				camPort[1][index].volSearch0 = temp;
			continue;
		}
		if (!strncmp("-ColPort:volSearch1", line, strlen("-ColPort:volSearch1"))) {
				double temp;
				sscanf(line, "-ColPort:volSearch1 (V): %d:%lf", &index, &temp);
				camPort[1][index].volSearch1 = temp;
			continue;
		}
		if (!strncmp("-ColPort:widthWire", line, strlen("-ColPort:widthWire"))) {
				double temp;
				sscanf(line, "-ColPort:widthWire: %d:%lf", &index, &temp);
				camPort[1][index].widthWire = temp;
			continue;
		}
		if (!strncmp("-ColPort:leak", line, strlen("-ColPort:leak"))) {
				sscanf(line, "-ColPort:leak: %d:%s", &index, tmp);
				if (!strcmp(tmp, "true")) {
					camPort[1][index].leak = true;
				} else {
					camPort[1][index].leak = false;
				}
			continue;
		}

		//MCAM resistance states input
		if (!strncmp("-NumResistanceState", line, strlen("-NumResistanceState"))) {
			sscanf(line, "-NumResistanceState: %d", &numResistanceState);
			continue;
		}

		if (!strncmp("-MCAM:ResistanceState", line, strlen("-MCAM:ResistanceState"))) {
				double res;
				sscanf(line, "-MCAM:ResistanceState: %d:%lf", &index, &res);
				ResistanceState[index] = res;
			}
			continue;

		// Variation model input

		// Consider device variation or not
		if (!strncmp("-withVariation", line, strlen("-withVariation"))) {
			sscanf(line, "-withVariation: %s", tmp);
			if (!strcmp(tmp, "true")) {
				withVariation = true;
			} else {
				withVariation = false;
			}
		continue;
		}

		// Variation for TCAM
		if (!strncmp("-ResistanceOffVariation", line, strlen("-ResistanceOffVariation"))) {
			double variation;
			sscanf(line, "-ResistanceOffVariation (%%): %lf", &variation);
			resistanceOffVariation =variation/100;
			continue;
		}
		if (!strncmp("-ResistanceOnVariation", line, strlen("-ResistanceOnVariation"))) {
			double variation;
			sscanf(line, "-ResistanceOnVariation (%%): %lf", &variation);
			resistanceOnVariation = variation/100;
			continue;
		}

		//Variation for MCAM
		if (!strncmp("-MCAM:StateVariation", line, strlen("-MCAM:StateVariation"))) {
				double variation;
				sscanf(line, "-MCAM:StateVariation (%%): %d:%lf", &index, &variation);
				resStateVariation[index] = variation/100;
			}
			continue;		

	}

	fclose(fp);
}

// void MemCell::CellScaling(int _targetProcessNode) {
// 	if ((processNode > 0) && (processNode != _targetProcessNode)) {
// 		double scalingFactor = (double)processNode / _targetProcessNode;
// 		if (memCellType == PCRAM) {
// 			resistanceOn *= scalingFactor;
// 			resistanceOff *= scalingFactor;
// 			if (!setMode) {
// 				setCurrent /= scalingFactor;
// 			} else {
// 				setVoltage *= 1;
// 			}
// 			if (!resetMode) {
// 				resetCurrent /= scalingFactor;
// 			} else {
// 				resetVoltage *= 1;
// 			}
// 			if (accessType == diode_access) {
// 			//	capacitanceOn /= scalingFactor; //TODO
// 			//	capacitanceOff /= scalingFactor; //TODO
// 			}
// 		} else if (memCellType == MRAM){ //TODO: MRAM
// 			resistanceOn *= scalingFactor * scalingFactor;
// 			resistanceOff *= scalingFactor * scalingFactor;
// 			if (!setMode) {
// 				setCurrent /= scalingFactor;
// 			} else {
// 				setVoltage *= scalingFactor;
// 			}
// 			if (!resetMode) {
// 				resetCurrent /= scalingFactor;
// 			} else {
// 				resetVoltage *= scalingFactor;
// 			}
// 			if (accessType == diode_access) {
// 			//	capacitanceOn /= scalingFactor; //TODO
// 			//	capacitanceOff /= scalingFactor; //TODO
// 			}
// 		} else if (memCellType == memristor) { //TODO: memristor

// 		} else { //TODO: other RAMs

// 		}
// 		processNode = _targetProcessNode;
// 	}
// }

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

// void MemCell::PrintCell()
// {
// 	switch (memCellType) {
// 	case SRAM:
// 		std::cout << "Memory Cell: SRAM" << std::endl;
// 		break;
// 	case DRAM:
// 		std::cout << "Memory Cell: DRAM" << std::endl;
// 		break;
// 	case eDRAM:
// 		std::cout << "Memory Cell: Embedded DRAM" << std::endl;
// 		break;
// 	case MRAM:
// 		std::cout << "Memory Cell: MRAM (Magnetoresistive)" << std::endl;
// 		break;
// 	case PCRAM:
// 		std::cout << "Memory Cell: PCRAM (Phase-Change)" << std::endl;
// 		break;
// 	case memristor:
// 		std::cout << "Memory Cell: RRAM (Memristor)" << std::endl;
// 		break;
// 	case FBRAM:
// 		std::cout << "Memory Cell: FBRAM (Floating Body)" <<std::endl;
// 		break;
// 	case SLCNAND:
// 		std::cout << "Memory Cell: Single-Level Cell NAND Flash" << std::endl;
// 		break;
// 	case MLCNAND:
// 		std::cout << "Memory Cell: Multi-Level Cell NAND Flash" << std::endl;
// 		break;
// 	case FEFETRAM:
// 		std::cout << "Memory Cell: FeFET" << std::endl;
// 		break;
// 	default:
// 		std::cout << "Memory Cell: Unknown" << std::endl;
// 	}
// 	std::cout << "Cell Area (F^2)    : " << area << " (" << heightInFeatureSize << "Fx" << widthInFeatureSize << "F)" << std::endl;
// 	std::cout << "Cell Aspect Ratio  : " << aspectRatio << std::endl;

// 	if (memCellType == PCRAM || memCellType == MRAM || memCellType == memristor || memCellType == FBRAM || memCellType == FEFETRAM) {
// 		if (resistanceOn < 1e3 )
// 			std::cout << "Cell Turned-On Resistance : " << resistanceOn << "ohm" << std::endl;
// 		else if (resistanceOn < 1e6)
// 			std::cout << "Cell Turned-On Resistance : " << resistanceOn / 1e3 << "Kohm" << std::endl;
// 		else
// 			std::cout << "Cell Turned-On Resistance : " << resistanceOn / 1e6 << "Mohm" << std::endl;
// 		if (resistanceOff < 1e3 )
// 			std::cout << "Cell Turned-Off Resistance: "<< resistanceOff << "ohm" << std::endl;
// 		else if (resistanceOff < 1e6)
// 			std::cout << "Cell Turned-Off Resistance: "<< resistanceOff / 1e3 << "Kohm" << std::endl;
// 		else
// 			std::cout << "Cell Turned-Off Resistance: "<< resistanceOff / 1e6 << "Mohm" << std::endl;

// 		if (readMode) {
// 			std::cout << "Read Mode: Voltage-Sensing" << std::endl;
// 			if (readCurrent > 0)
// 				std::cout << "  - Read Current: " << readCurrent * 1e6 << "uA" << std::endl;
// 			if (readVoltage > 0)
// 				std::cout << "  - Read Voltage: " << readVoltage << "V" << std::endl;
// 		} else {
// 			std::cout << "Read Mode: Current-Sensing" << std::endl;
// 			if (readCurrent > 0)
// 				std::cout << "  - Read Current: " << readCurrent * 1e6 << "uA" << std::endl;
// 			if (readVoltage > 0)
// 				std::cout << "  - Read Voltage: " << readVoltage << "V" << std::endl;
// 		}

// 		if (resetMode) {
// 			std::cout << "Reset Mode: Voltage" << std::endl;
// 			std::cout << "  - Reset Voltage: " << resetVoltage << "V" << std::endl;
// 		} else {
// 			std::cout << "Reset Mode: Current" << std::endl;
// 			std::cout << "  - Reset Current: " << resetCurrent * 1e6 << "uA" << std::endl;
// 		}
// 		std::cout << "  - Reset Pulse: " << TO_SECOND(resetPulse) << std::endl;

// 		if (setMode) {
// 			std::cout << "Set Mode: Voltage" << std::endl;
// 			std::cout << "  - Set Voltage: " << setVoltage << "V" << std::endl;
// 		} else {
// 			std::cout << "Set Mode: Current" << std::endl;
// 			std::cout << "  - Set Current: " << setCurrent * 1e6 << "uA" << std::endl;
// 		}
// 		std::cout << "  - Set Pulse: " << TO_SECOND(setPulse) << std::endl;

// 		switch (accessType) {
// 		case CMOS_access:
// 			std::cout << "Access Type: CMOS" << std::endl;
// 			break;
// 		case BJT_access:
// 			std::cout << "Access Type: BJT" << std::endl;
// 			break;
// 		case diode_access:
// 			std::cout << "Access Type: Diode" << std::endl;
// 			break;
// 		default:
// 			std::cout << "Access Type: None Access Device" << std::endl;
// 		}
// 	} else if (memCellType == SRAM) {
// 		std::cout << "SRAM Cell Access Transistor Width: " << widthAccessCMOS << "F" << std::endl;
// 		std::cout << "SRAM Cell NMOS Width: " << widthSRAMCellNMOS << "F" << std::endl;
// 		std::cout << "SRAM Cell PMOS Width: " << widthSRAMCellPMOS << "F" << std::endl;
// 	} else if (memCellType == SLCNAND) {
// 		std::cout << "Pass Voltage       : " << flashPassVoltage << "V" << std::endl;
// 		std::cout << "Programming Voltage: " << flashProgramVoltage << "V" << std::endl;
// 		std::cout << "Erase Voltage      : " << flashEraseVoltage << "V" << std::endl;
// 		std::cout << "Programming Time   : " << TO_SECOND(flashProgramTime) << std::endl;
// 		std::cout << "Erase Time         : " << TO_SECOND(flashEraseTime) << std::endl;
// 		std::cout << "Gate Coupling Ratio: " << gateCouplingRatio << std::endl;
// 	}
// }

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
