#ifndef MEMCELL_H_
#define MEMCELL_H_

#include <stdint.h>
#include <string>

#include "typedef.h"
#include "constant.h"

class MemCell {
    public:
        MemCell();
        MemCell(const MemCell&) {}
        virtual ~MemCell() {}

        /* Functions */
        void ReadCellFromFile(const std::string & inputFile, DesignTarget _designTarget, double _vdd);
        void PrintCell();
        // void CellScaling(int _targetProcessNode);
        double GetMemristance(double _relativeReadVoltage);  /* Get the LRS resistance of memristor at log-linear region of I-V curve */
        void CalculateWriteEnergy();
        double CalculateReadPower();

        /* Properties */
        MemCellType memCellType;	/* Memory cell type (like MRAM, PCRAM, etc.) */
        int processNode;        /* Cell original process technology node, Unit: nm*/
        double area;			/* Cell area, Unit: F^2 */
        double aspectRatio;		/* Cell aspect ratio, H/W */
        double widthInFeatureSize;	/* Cell width, Unit: F */
        double heightInFeatureSize;	/* Cell height, Unit: F */
        double resistanceOn;	/* Turn-on resistance */
        double resistanceOff;	/* Turn-off resistance */
        double capacitanceOn;   /* Cell capacitance when memristor is on */
        double capacitanceOff;  /* Cell capacitance when memristor is off */
        bool   readMode;		/* true = voltage-mode, false = current-mode */
        double readVoltage;		/* Read voltage */
        double readCurrent;		/* Read current */
        double minSenseVoltage; /* Minimum sense voltage */
        double wordlineBoostRatio; /*TODO: function not realized: ratio of boost wordline voltage to vdd */
        double readPower;       /* Read power per cell (uW)*/
        double readEnergy;      /* Read Energy per cell (fJ) */
        bool   resetMode;		/* true = voltage-mode, false = current-mode */
        double resetVoltage;	/* Reset voltage */
        double resetCurrent;	/* Reset current */
        double resetPulse;		/* Reset pulse duration (ns) */
        double resetEnergy;     /* Reset energy per cell (pJ) */
        bool   setMode;			/* true = voltage-mode, false = current-mode */
        double setVoltage;		/* Set voltage */
        double setCurrent;		/* Set current */
        double setPulse;		/* Set pulse duration (ns) */
        double setEnergy;       /* Set energy per cell (pJ) */
        CellAccessType accessType;	/* Cell access type: CMOS, BJT, or diode */

        int camNumRow;
        int camNumCol;
        CAMPort camPort[2][MAX_PORT];

        // No longer global
        DesignTarget designTarget;
        double vdd;


        double camWidthMatchTran;		/* The gate width of CMOS access transistor, Unit: F */
        CAMType camType; /* Ternary CAM, Multi-bit CAM, or Analog CAM */
        bool isNVMdischarge;
        int numResistanceState; // # of state of multi-bit CAM
                                // double ResistanceValues[64]; // corresponding resistance values
        double ResistanceState[64];

        bool withVariation;
        bool hasVariationSeed;
        uint32_t variationSeed;
        std::string variationMode;
        std::string variationLutFile;
        int variationSamples;
        double resistanceOnVariation;
        double resistanceOffVariation;
        double matchlineWireResistanceVariation;
        double deviceAccessResistanceVariation;
        double deviceMatchResistanceVariation;
        double resStateVariation[64];

        /* Optional properties */
        int stitching;			/* If non-zero, add stitching overhead for every x cells */
        double gateOxThicknessFactor; /* The oxide thickness of FBRAM could be larger than the traditional SOI MOS */
        double widthSOIDevice; /* The gate width of SOI device as FBRAM element, Unit: F*/
        double widthAccessCMOS;	/* The gate width of CMOS access transistor, Unit: F */
        double voltageDropAccessDevice;  /* The voltage drop on the access device, Unit: V */
        double leakageCurrentAccessDevice;  /* Reverse current of access device, Unit: uA */
        double capDRAMCell;		/* The DRAM cell capacitance if the memory cell is DRAM, Unit: F */
        double widthSRAMCellNMOS;	/* The gate width of NMOS in SRAM cells, Unit: F */
        double widthSRAMCellPMOS;	/* The gate width of PMOS in SRAM cells, Unit: F */

        /* For memristor */
        bool readFloating;      /* If unselected wordlines/bitlines are floating to reduce total leakage */
        double resistanceOnAtSetVoltage; /* Low resistance state when set voltage is applied */
        double resistanceOffAtSetVoltage; /* High resistance state when set voltage is applied */
        double resistanceOnAtResetVoltage; /* Low resistance state when reset voltage is applied */
        double resistanceOffAtResetVoltage; /* High resistance state when reset voltage is applied */
        double resistanceOnAtReadVoltage; /* Low resistance state when read voltage is applied */
        double resistanceOffAtReadVoltage; /* High resistance state when read voltage is applied */
        double resistanceOnAtHalfReadVoltage; /* Low resistance state when 1/2 read voltage is applied */
        double resistanceOffAtHalfReadVoltage; /* High resistance state when 1/2 read voltage is applied */
        double resistanceOnAtHalfResetVoltage; /* Low resistance state when 1/2 reset voltage is applied */

        /* For NAND flash */
        double flashEraseVoltage;		/* The erase voltage, Unit: V, highest W/E voltage in ITRS sheet */
        double flashPassVoltage;		/* The voltage applied on the unselected wordline within the same block during programming, Unit: V */
        double flashProgramVoltage;		/* The program voltage, Unit: V */
        double flashEraseTime;			/* The flash erase time, Unit: s */
        double flashProgramTime;		/* The SLC flash program time, Unit: s */
        double gateCouplingRatio;		/* The ratio of control gate to total floating gate capacitance */
};

#endif /* MEMCELL_H_ */
