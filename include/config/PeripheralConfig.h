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
    bool noPrechargeInc = false;
    bool includeLeakage = false;
    double scaledVoltage = 0;
    bool useUpdatedLib = false;
    double addCapOnML = 0;
};

#endif /* CONFIG_PERIPHERALCONFIG_H_ */
