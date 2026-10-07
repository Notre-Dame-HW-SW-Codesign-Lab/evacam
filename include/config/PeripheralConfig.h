#ifndef CONFIG_PERIPHERALCONFIG_H_
#define CONFIG_PERIPHERALCONFIG_H_

#include <string>

#include "typedef.h"
#include "model/AnalyticalCamTiming.h"

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
    CamDecisionMode decisionMode = CamDecisionMode::LegacyHorowitz;
    double decisionThreshold = 0;
    bool inverterTripDecision = false;
    bool keeperMidpointDecision = false;
    bool explicitSearchTiming = false;
    bool searchBroadcast = false;
    bool overlapSearchPrecharge = true;
    bool usePhysicalDriverLoad = false;
    double searchRecovery = 0;
    // Opt-in circuit reductions; legacy examples keep their historical behavior.
    std::string matchlineCircuit = "legacy";
    double prechargeVoltage = 0;
    int bitsPerDischargePath = 1;
    double keeperHighClamp = 0;
    double keeperLowClamp = 0;
};

#endif /* CONFIG_PERIPHERALCONFIG_H_ */
