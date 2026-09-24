#ifndef MODEL_NANDRCLADDER_H_
#define MODEL_NANDRCLADDER_H_

#include <vector>

// Boundary resistor connects the first/last dynamic node to an ideal source.
// An unconnected boundary is an open circuit; its resistance is ignored.
struct NandRcBoundary {
    bool connected = false;
    double resistance = 0;
    double voltage = 0;
};

struct NandRcOptions {
    double maxStep = 0;
    double tolerance = 1e-7; // Absolute voltage local error-control tolerance.
    int maxSteps = 100000;   // Includes rejected attempted steps.
};

struct NandRcResult {
    std::vector<double> voltages;
    double elapsed = 0;
    double capacitorChargeChange = 0;
    double initialStoredEnergy = 0;
    double finalStoredEnergy = 0;
    double leftSourceCharge = 0;
    double rightSourceCharge = 0;
    double maximumEstimatedLocalError = 0;
    int acceptedSteps = 0;
    int rejectedSteps = 0;
};

// Full linear nodal transient, not an Elmore/single-exponential approximation.
// C[i] is node i's shunt capacitance. R[i] joins nodes i and i+1.
// No transistor current/voltage or coupling-capacitance model is implied.
class NandRcLadder {
    public:
        static NandRcResult Solve(const std::vector<double> &capacitances,
                const std::vector<double> &seriesResistances,
                const std::vector<double> &initialVoltages, double duration,
                const NandRcBoundary &left, const NandRcBoundary &right,
                const NandRcOptions &options);
};

#endif  // MODEL_NANDRCLADDER_H_
