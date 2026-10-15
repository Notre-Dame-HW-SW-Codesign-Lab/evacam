#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "model/NandResistanceTable.h"

void NandResistanceTable::Validate(double passVoltage, double passResistance) const {
    const auto states = resistance.size();
    if (states < 2 || states > 16 || (states & (states - 1)) != 0
            || queryVoltages.size() != states) {
        throw std::invalid_argument("NAND resistance_table requires 2, 4, 8, or 16 states and one query voltage per state");
    }
    if (!std::isfinite(passVoltage) || passVoltage <= 0
            || !std::isfinite(passResistance) || passResistance <= 0) {
        throw std::invalid_argument("NAND resistance_table requires positive finite pass bias and resistance");
    }
    for (std::size_t state = 0; state < states; ++state) {
        if (resistance[state].size() != states) {
            throw std::invalid_argument("NAND resistance_table must be square");
        }
        if (!std::isfinite(queryVoltages[state]) || queryVoltages[state] < 0
                || queryVoltages[state] >= passVoltage
                || (state > 0 && queryVoltages[state] <= queryVoltages[state - 1])) {
            throw std::invalid_argument("NAND query_voltages must increase strictly within [0, bias.pass)");
        }
        for (double value : resistance[state]) {
            if (!std::isfinite(value) || value < passResistance) {
                throw std::invalid_argument("NAND resistance_table entries must be finite and >= resistance.pass");
            }
        }
    }
}

int NandResistanceTable::BitsPerCell() const {
    if (resistance.empty()) return 1;
    const auto states = resistance.size();
    if (states < 2 || states > 16 || (states & (states - 1)) != 0) {
        throw std::invalid_argument("NAND resistance_table state count must be 2, 4, 8, or 16");
    }
    int bits = 0;
    for (auto count = states; count > 1; count >>= 1) ++bits;
    return bits;
}

std::pair<double, double> NandResistanceTable::Pair(int stored, int query, double passResistance) const {
    const int states = static_cast<int>(resistance.size());
    if (states == 0 || stored < -1 || stored >= states || query < -1 || query >= states) {
        throw std::invalid_argument("NAND table symbols must be -1 (wildcard) or a configured state index");
    }
    if (query == -1) return {passResistance, passResistance};
    // A stored wildcard is two lowest-threshold states, not an ideal bypass.
    const int first = stored == -1 ? 0 : stored;
    const int second = stored == -1 ? 0 : states - 1 - stored;
    return {resistance.at(first).at(query), resistance.at(second).at(states - 1 - query)};
}

double NandResistanceTable::RechargeVoltageSum(int query, double passVoltage) const {
    if (query < -1 || query >= static_cast<int>(queryVoltages.size())) {
        throw std::invalid_argument("NAND query symbol outside query_voltages");
    }
    if (query == -1) return 0;
    return (passVoltage - queryVoltages.at(query))
            + (passVoltage - queryVoltages.at(queryVoltages.size() - 1 - query));
}

int NandResistanceTable::WorstEnergyQuery() const {
    if (queryVoltages.empty()) throw std::invalid_argument("NAND query_voltages must not be empty");
    int worst = 0;
    for (std::size_t query = 1; query < queryVoltages.size(); ++query) {
        if (queryVoltages[query] + queryVoltages[queryVoltages.size() - 1 - query]
                < queryVoltages[worst] + queryVoltages[queryVoltages.size() - 1 - worst]) {
            worst = static_cast<int>(query);
        }
    }
    return worst;
}

std::pair<double, double> NandTableTimeBounds(const NandResistanceTable &table,
        double passResistance, const std::vector<double> &weights, int firstKey,
        int symbols, double baseTimeConstant, double invalidTimeIncrement) {
    if (firstKey < 0 || symbols <= 0 || static_cast<std::size_t>(firstKey) > weights.size()
            || static_cast<std::size_t>(symbols) > (weights.size() - firstKey) / 2) {
        throw std::invalid_argument("NAND table bounds require weights for every complementary pair");
    }
    double slowMatch = baseTimeConstant, fastMatch = baseTimeConstant;
    double fastMismatch = std::numeric_limits<double>::infinity();
    double fastAny = baseTimeConstant;
    const int states = static_cast<int>(table.resistance.size());
    for (int symbol = 0; symbol < symbols; ++symbol) {
        double minMatch = std::numeric_limits<double>::infinity();
        double maxMatch = 0, minMismatch = std::numeric_limits<double>::infinity();
        for (int stored = -1; stored < states; ++stored) {
            for (int query = -1; query < states; ++query) {
                const auto pair = table.Pair(stored, query, passResistance);
                const double delta = (pair.first - passResistance) * weights[firstKey + 2 * symbol]
                        + (pair.second - passResistance) * weights[firstKey + 2 * symbol + 1];
                if (stored == -1 || query == -1 || stored == query) {
                    minMatch = std::min(minMatch, delta);
                    maxMatch = std::max(maxMatch, delta);
                } else {
                    minMismatch = std::min(minMismatch, delta);
                }
            }
        }
        const double minAny = std::min(minMatch, minMismatch);
        fastMismatch = std::min(fastMismatch + minAny, fastMatch + minMismatch);
        fastMatch += minMatch;
        fastAny += minAny;
        slowMatch += maxMatch;
    }
    fastMismatch = std::min(fastMismatch, fastAny + invalidTimeIncrement);
    if (!std::isfinite(slowMatch) || !std::isfinite(fastMismatch)
            || slowMatch <= 0 || fastMismatch <= 0) {
        throw std::invalid_argument("NAND table time constants must be finite and positive");
    }
    return {slowMatch, fastMismatch};
}
