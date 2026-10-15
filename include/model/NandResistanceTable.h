#ifndef MODEL_NANDRESISTANCETABLE_H_
#define MODEL_NANDRESISTANCETABLE_H_

#include <utility>
#include <vector>

#include "app/EvaCAMMcamAnalysis.h"

// Per physical transistor: rows are programmed states, columns are query
// voltages. Symbols use complementary states (s, N-1-s) and biases (q, N-1-q).
struct NandResistanceTable {
    std::vector<std::vector<double>> resistance;
    std::vector<double> queryVoltages;

    void Validate(double passVoltage, double passResistance) const;
    int BitsPerCell() const;
    std::pair<double, double> Pair(int stored, int query, double passResistance) const;
    double RechargeVoltageSum(int query, double passVoltage) const;
    int WorstEnergyQuery() const;
};

// The first moment is affine in each series resistance. Enumerating each
// symbol's possibilities and combining match/nonmatch classes gives global
// bounds without enumerating complete words. Weights include downstream C.
std::pair<double, double> NandTableTimeBounds(const NandResistanceTable &table,
        double passResistance, const std::vector<double> &weights, int firstKey,
        int symbols, double baseTimeConstant, double invalidTimeIncrement);

// Fixed geometry and bias for nominal distance analysis. Baselines include
// valid binary validity devices, pass-biased data/dummies, selects and wires.
struct NandDistanceContext {
    NandResistanceTable table;
    std::vector<double> weights;
    int firstKey = 0;
    int symbols = 0;
    double baseTimeConstant = 0;
    double baseResistance = 0;
    double invalidTimeIncrement = 0;
    double prechargeVoltage = 0;
    double passResistance = 0;
    double configuredReference = 0;
    double senseOffset = 0;
    double requiredMargin = 0;
};

double NandSquaredDistance(const NandDistanceContext &context,
        const std::vector<int> &stored, const std::vector<int> &query);
std::vector<EvaCAMDistanceVoltageBounds> NandTableDistanceBounds(
        const NandDistanceContext &context, const std::vector<int> &query,
        double decisionTime, double searchLatency);
void SetNandThresholdDiagnostics(EvaCAMMatchResult &result,
        const NandDistanceContext &context,
        const std::vector<EvaCAMDistanceVoltageBounds> &bounds,
        double maximumSquaredDistance, double decisionTime);

#endif
