#ifndef CONFIG_PERIPHERALCONFIG_H_
#define CONFIG_PERIPHERALCONFIG_H_

#include <string>

#include "typedef.h"

struct PeripheralConfig {
    bool withInputEnc = false;
    TypeOfInputEncoder typeInputEnc = encoding_two_bit;
    bool customInputEnc = false;
    TypeOfSenseAmp typeSenseAmp = nvsim_voltage_sense;
    bool customSenseAmp = false;
    bool withOutputAcc = false;
    bool withPriorityEnc = false;
    bool withWriteDriver = false;
    bool withInputBuffer = false;
    bool withOutputBuffer = false;
    double matchlineSenseMargin = 3e-8;
    std::string fileCustomSA;
    std::string fileSenseAmp;
    bool noPrechargeInc = false;
    bool includeLeakage = false;
    // MCAM sensing is diagnostic by default so data-dependent margins can be
    // inspected even when they miss the configured hardware requirement.
    bool strictSenseMargin = false;
    double scaledVoltage = 0;
    bool useUpdatedLib = false;
    double addCapOnML = 0;
};

#endif /* CONFIG_PERIPHERALCONFIG_H_ */
