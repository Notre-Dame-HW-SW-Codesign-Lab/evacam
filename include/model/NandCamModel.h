#ifndef MODEL_NANDCAMMODEL_H_
#define MODEL_NANDCAMMODEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/EvaCAMMatchResult.h"
#include "technology/NandDeviceSpec.h"

class EvaCamConfig;
class Wire;

// One physical erase block, with one stored logical entry per NAND string.
// All dimensional results are SI; capacities/counts are physical bits/devices.
struct NandCamMetrics {
    std::string modelBackend;
    std::string calibrationStatus;
    std::string modelSource;
    long long entries = 0;
    long keyWidth = 0;
    long dataWordlines = 0;
    long paddingWordlines = 0;
    long long physicalCells = 0;
    long long physicalPageBits = 0;
    long long physicalBlockBits = 0;
    long long senseAmplifiers = 0;
    int searchRounds = 0;
    long long queryBitCount = 0; // Zero retains the legacy complementary-pair count.
    double area = 0;
    double width = 0;
    double height = 0;
    double searchLatency = 0;
    double searchEnergy = 0;
    double leakage = 0;
    double programPageLatency = 0;
    double programPageEnergy = 0;
    double eraseBlockLatency = 0;
    double eraseBlockEnergy = 0;
    double decisionTime = 0;
    double matchVoltage = 0;
    double mismatchVoltage = 0;
    double referenceVoltage = 0;
    // Conservative within the single-exponential approximation, not a bound
    // on the full RC transient. Per-class reference margin after comparator
    // offset. A negative margin is infeasible and is never made absolute.
    double senseMargin = 0;
    double requiredSenseMargin = 0;
    bool senseMarginPass = false;
    double slowestMatchTimeConstant = 0;
    double fastestMismatchTimeConstant = 0;
    std::map<std::string, double> areaBreakdown;
    std::map<std::string, double> searchEnergyBreakdown;
    std::map<std::string, double> latencyBreakdown;
    // Backend-specific, explicitly named SI metrics. Legacy outputs omit them.
    std::map<std::string, std::string> metadata;
    std::map<std::string, double> geometryMetrics;
    std::map<std::string, double> diagnosticMetrics;
};

class NandCamBackend {
    public:
        virtual ~NandCamBackend() = default;
        virtual void Initialize(std::shared_ptr<EvaCamConfig> config, long long entries,
                long keyWidth, int muxSenseAmp, const Wire &wire) = 0;
        virtual const NandCamMetrics &Metrics() const = 0;
        virtual EvaCAMMatchResult Evaluate(const std::vector<int> &stored,
                const std::vector<int> &query, bool valid = true) const = 0;
};

// Explicit exploratory first-moment RC approximation, not a calibrated NAND
// device simulator. Complementary pairs encode 0=(L,H), 1=(H,L), X=(L,L).
// Query 0=(read,pass), 1=(pass,read), X=(pass,pass). Match discharges fastest.
class NandCamModel : public NandCamBackend {
    public:
        void Initialize(std::shared_ptr<EvaCamConfig> config, long long entries,
                long keyWidth, int muxSenseAmp, const Wire &wire) override;
        const NandCamMetrics &Metrics() const override;
        // hit is the ideal ternary truth-table decision; senseMarginPass
        // independently says whether this electrical decision has margin.
        EvaCAMMatchResult Evaluate(const std::vector<int> &stored,
                const std::vector<int> &query, bool valid = true) const override;

    private:
        double StringTimeConstant(const std::vector<double> &resistances) const;
        double StringResistance(const std::vector<double> &resistances) const;
        std::vector<double> EncodeResistances(const std::vector<int> &stored,
                const std::vector<int> &query, bool valid) const;
        double QueryGateEnergy(const std::vector<int> &query) const;
        double QueryDriverEnergy(const std::vector<int> &query) const;
        bool initialized = false;
        NandDeviceSpec device;
        NandCamMetrics metrics;
        double bitlineCapacitance = 0;
        double bitlineWireCapacitance = 0;
        double bitlineWireResistance = 0;
        double gateCapacitancePerWordline = 0;
};

#endif  // MODEL_NANDCAMMODEL_H_
