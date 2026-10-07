#ifndef ANALYTICAL_CAM_TIMING_H_
#define ANALYTICAL_CAM_TIMING_H_

enum class CamDecisionMode { LegacyHorowitz, FixedThreshold, Differential };

struct CamDecisionResult {
    bool feasible = false;
    double time = 0;
    double matchVoltage = 0;
    double missVoltage = 0;
    double margin = 0;
    double ramp = 0;
};

// Closed-form RC response with a linear activation of discharge conductance.
// activationTime=0 selects an ideal step. No transient time stepping is used.
double CamDischargeVoltage(double precharge, double tau, double activationTime, double time);
// Parallel diode keepers are linear resistors above their clamp voltage and
// open circuits below it. The two segments are solved in closed form.
double CamKeeperVoltage(double precharge, double highClamp, double lowClamp,
        double branchTau, int paths, int misses, double activationTime, double time, double wireTau = 0);
CamDecisionResult EvaluateCamKeeperDecision(double precharge, double highClamp,
        double lowClamp, double branchTau, int paths, double activationTime,
        double requiredMargin, double threshold, double wireTau = 0);
double CamSupplyRechargeEnergy(double capacitance, double supply,
        double precharge, double finalVoltage);
double CamInverterTripVoltage(double supply, double threshold, double pToNStrength);
// Diode-connected MOS keeper, I = k * max(V - clamp, 0)^2.
// rate = k / C. Clamps include the source voltage plus MOS threshold.
double CamDiodeKeeperVoltage(double precharge, double highClamp, double lowClamp,
        double rate, int paths, int misses, double activationTime, double time);
CamDecisionResult EvaluateCamDiodeKeeperDecision(double precharge, double highClamp,
        double lowClamp, double rate, int paths, double activationTime,
        double requiredMargin, double threshold);
CamDecisionResult EvaluateCamDecision(CamDecisionMode mode, double precharge,
        double matchTau, double missTau, double activationTime,
        double requiredMargin, double threshold = 0);

struct CamSearchPhases {
    double queryReady = 0;
    double evaluationStart = 0;
    double resultReady = 0;
    double cycleTime = 0;
};

CamSearchPhases EvaluateCamSearchPhases(double input, double control, double precharge,
        double evaluation, double output, double recovery, bool overlapPrecharge);

#endif
