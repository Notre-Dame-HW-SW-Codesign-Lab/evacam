#ifndef MODEL_NAND3DCAMMODEL_H_
#define MODEL_NAND3DCAMMODEL_H_

#include <memory>
#include <vector>

#include "model/NandCamModel.h"
#include "model/NandRcLadder.h"
#include "technology/Nand3dMemoryDevice.h"

class Nand3dCamModel : public NandCamBackend {
    public:
        void Initialize(std::shared_ptr<EvaCamConfig> config, long long entries,
                long keyWidth, int muxSenseAmp, const Wire &wire) override;
        const NandCamMetrics &Metrics() const override;
        EvaCAMMatchResult Evaluate(const std::vector<int> &stored,
                const std::vector<int> &query, bool valid = true) const override;

    private:
        struct PatternResponse {
            double minimumVoltage = 0;
            double maximumVoltage = 0;
            double prechargeEnergy = 0; // Per string, summed over sense-mux rounds.
            double finalResetVoltage = 0;
            double maximumLocalError = 0;
            long long steps = 0;
        };
        std::vector<double> Encode(const std::vector<int> &stored,
                const std::vector<int> &query, bool valid) const;
        PatternResponse Simulate(const std::vector<double> &resistances) const;
        double QueryGateEnergy(const std::vector<int> &query) const;
        double QueryDriverEnergy(const std::vector<int> &query) const;
        bool initialized = false;
        Nand3dMemoryDevice device;
        NandCamMetrics metrics;
        NandRcOptions options;
        NandRcResult initialPrecharge;
        int mux = 1;
        int sourceDummyLayers = 0;
        int totalGateLayers = 0;
        double gateCapacitancePerWordline = 0;
        std::vector<double> capacitances;
        std::vector<double> allPassResistances;
};

#endif  // MODEL_NAND3DCAMMODEL_H_
