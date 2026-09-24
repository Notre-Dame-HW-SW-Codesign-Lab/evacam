#ifndef TECHNOLOGY_NANDDEVICESPEC_H_
#define TECHNOLOGY_NANDDEVICESPEC_H_

#include <string>

// All values use SI units. Peripheral costs are per physical wordline or string;
// operation costs apply to one selected block unless named page/block explicitly.
struct NandPeripheralSpec {
    double area = 0;
    double latency = 0;
    double energy = 0;
    double leakage = 0;
};

struct NandOperationSpec {
    double latency = 0;
    double energy = 0;
};

struct NandDeviceSpec {
    bool configured = false;
    std::string model;
    std::string calibrationStatus;
    std::string source;
    double resistanceReadOn = 0;
    double resistancePass = 0;
    double resistanceOff = 0;
    double resistanceSelect = 0;
    double capacitanceGate = 0;
    double capacitanceInternal = 0;
    double capacitanceBitline = 0;
    double capacitanceSource = 0;
    double capacitanceSelect = 0;
    double thresholdLow = 0;
    double thresholdHigh = 0;
    double voltageRead = 0;
    double voltagePass = 0;
    double voltagePrecharge = 0;
    double decisionTime = 0;
    double minSenseMargin = 0;
    double referenceVoltage = 0; // Zero selects the midpoint of worst-case signals.
    double senseOffset = 0;
    double supplyEfficiency = 1;
    NandPeripheralSpec wordlineDriver;
    NandPeripheralSpec sense;
    NandPeripheralSpec pageBuffer;
    NandOperationSpec query;
    NandOperationSpec setup;
    NandOperationSpec precharge;
    NandOperationSpec recovery;
    NandOperationSpec programPage;
    NandOperationSpec eraseBlock;
};

#endif  // TECHNOLOGY_NANDDEVICESPEC_H_
