#ifndef TECHNOLOGY_NANDCELLCURRENTPARAMETERS_H_
#define TECHNOLOGY_NANDCELLCURRENTPARAMETERS_H_

// Experimental reduced-law parameters, not a BSIM card or calibrated NAND data.
// No implicit defaults for electrical parameters or admissible terminal biases.
struct NandCellCurrentParameters {
    double beta = 0;              // A/V^2 at the supplied geometry; strictly positive.
    double slopeFactor = 0;       // Dimensionless subthreshold smoothing factor.
    double leakageConductance = 0; // S, physical parallel leakage (may be zero).
    double temperature = 0;       // K; fixed temperature, no mobility temperature law.
    double channelDiameter = 0;   // m; provenance only, no inferred geometry scaling.
    double gateLength = 0;        // m; provenance only.
    double minimumVoltage = 0;    // V; all three terminals must lie in this domain.
    double maximumVoltage = 0;
    double minimumThreshold = 0;  // V; explicit threshold/charge-state domain.
    double maximumThreshold = 0;
};

#endif
