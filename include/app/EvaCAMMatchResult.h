#ifndef EVACAMMATCHRESULT_H_
#define EVACAMMATCHRESULT_H_

struct EvaCAMMatchResult {
    bool hit = false;
    double searchLatency = 0;
    double searchDynamicEnergy = 0;
    double matchlineDelay = 0;
    // The margin used for this decision. For MCAM best-match and k-NN arrays
    // this is the actual selected/rejected voltage gap, not an ideal distance
    // proxy.
    double senseMargin = 0;
    double requiredSenseMargin = 0;
    double senseMarginSlack = 0;
    bool senseMarginPass = false;
    bool senseMarginApplicable = true;
    // squaredEuclideanDistance is the MCAM ideal symbol distance. Conductance
    // and voltage describe the modeled MCAM matchline or NAND series string
    // at the common decision time; NAND hit remains the ideal logical result.
    double squaredEuclideanDistance = 0;
    double matchlineConductance = 0;
    double matchlineVoltage = 0;
};

#endif /* EVACAMMATCHRESULT_H_ */
