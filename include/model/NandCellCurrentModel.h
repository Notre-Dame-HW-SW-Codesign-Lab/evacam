#ifndef MODEL_NANDCELLCURRENTMODEL_H_
#define MODEL_NANDCELLCURRENTMODEL_H_

#include "technology/NandCellCurrentParameters.h"

struct NandCellCurrentResult {
    double current = 0; // A, positive drain to source. Gate current is zero.
    double gateDerivative = 0; // A/V, other absolute terminal voltages fixed.
    double sourceDerivative = 0;
    double drainDerivative = 0;
};

// Symmetric smooth potential-difference approximation. Supports either Vds sign.
// Parameters are copied and immutable; threshold is supplied for every evaluation.
class NandCellCurrentModel {
    public:
        explicit NandCellCurrentModel(const NandCellCurrentParameters &parameters);
        NandCellCurrentResult Evaluate(double gate, double source, double drain,
                double threshold) const;
    private:
        const NandCellCurrentParameters parameters;
};

#endif
