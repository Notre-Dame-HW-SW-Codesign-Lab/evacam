#ifndef MODEL_NANDNONLINEARSTRING_H_
#define MODEL_NANDNONLINEARSTRING_H_

#include <vector>

#include "model/NandCellCurrentModel.h"

// In source-to-drain order. Include select/dummy devices explicitly when needed.
struct NandNonlinearDevice {
    NandCellCurrentModel model;
    double gateVoltage;
    double threshold;
};

struct NandNonlinearDcOptions {
    double absoluteCurrentTolerance = 1e-14; // A, internal KCL residual.
    double relativeCurrentTolerance = 1e-8;
    double voltageTolerance = 1e-10; // V, full Newton correction, not damped step.
    int maxIterations = 100;
    int maxBacktracks = 40;
};

struct NandNonlinearDcResult {
    std::vector<double> voltages; // Includes fixed source and drain endpoints.
    double current = 0;          // A, drain to source at drain endpoint.
    double maximumKclResidual = 0;
    double maximumVoltageCorrection = 0;
    int iterations = 0;
    int backtracks = 0;
};

class NandNonlinearString {
    public:
        // Experimental DC only; 1..512 devices, pivoted dense Newton solve.
        // Throws on invalid input, singular Jacobian, or exhausted work budget.
        // No artificial conductance, extrapolation, or fixed-resistance fallback.
        static NandNonlinearDcResult Solve(const std::vector<NandNonlinearDevice> &devices,
                double sourceVoltage, double drainVoltage,
                const NandNonlinearDcOptions &options = {});
};

#endif
