#ifndef EVACAMMATCHRESULT_H_
#define EVACAMMATCHRESULT_H_

struct EvaCAMMatchResult {
    bool hit = false;
    double searchLatency = 0;
    double searchDynamicEnergy = 0;
    double matchlineDelay = 0;
    // The margin used for this decision. For MCAM best-match arrays this is
    // the actual best/runner-up voltage gap, not an ideal distance proxy.
    double senseMargin = 0;
    double requiredSenseMargin = 0;
    double senseMarginSlack = 0;
    bool senseMarginPass = false;
    bool senseMarginApplicable = true;
    // MCAM-only observables. squaredEuclideanDistance is the ideal symbol
    // distance; matchlineConductance and matchlineVoltage are the modeled
    // electrical score and sampled voltage at the common sensing instant.
    double squaredEuclideanDistance = 0;
    double matchlineConductance = 0;
    double matchlineVoltage = 0;
};

#endif /* EVACAMMATCHRESULT_H_ */
