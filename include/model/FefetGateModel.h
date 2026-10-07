#ifndef FEFET_GATE_MODEL_H_
#define FEFET_GATE_MODEL_H_

struct FefetGateResponse {
    double matchVoltage = 0;
    double mismatchVoltage = 0;
    double timeConstant = 0;
    double delay = 0;
    double ramp = 0; // Normalized voltage slope, 1/s.
    double chargingEnergy = 0; // Worst-case zero-to-mismatch transition, J/cell.
    double staticPower = 0; // Complementary searchline divider, W/cell.
};

// Two resistive FeFET branches drive the gate of a separate CMOS pull-down.
// This is a first-order control-node model, not a compact FeFET transistor model.
FefetGateResponse EvaluateFefetGate(double resistanceOn, double resistanceOff,
        double capacitance, double supplyVoltage, double switchingVoltage);

#endif
