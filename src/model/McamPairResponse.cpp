#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "McamPairResponse.h"

void ValidateMcamPairResistance(const std::vector<std::vector<double>> &resistance) {
    const size_t states = resistance.size();
    if (states < 2 || states > 64 || (states & (states - 1)) != 0) {
        throw std::invalid_argument("MCAM pair_resistance must have a power-of-two size from 2 to 64.");
    }
    for (const auto &row : resistance) {
        if (row.size() != states) {
            throw std::invalid_argument("MCAM pair_resistance must be square.");
        }
        for (double value : row) {
            if (!std::isfinite(value) || value <= 0 || !std::isfinite(1.0 / value)) {
                throw std::invalid_argument("MCAM pair_resistance entries must be finite positive ohms.");
            }
        }
    }
}

std::vector<EvaCAMDistanceVoltageBounds> McamPairConductanceBounds(
        const std::vector<std::vector<double>> &resistance,
        int dimensions, const std::vector<int> &query) {
    ValidateMcamPairResistance(resistance);
    const int states = static_cast<int>(resistance.size());
    if (dimensions <= 0 || (!query.empty() && query.size() != static_cast<size_t>(dimensions))) {
        throw std::invalid_argument("MCAM pair bounds require positive dimensions and a matching query length.");
    }
    for (int symbol : query) {
        if (symbol < 0 || symbol >= states) {
            throw std::invalid_argument("MCAM pair query symbol is out of range.");
        }
    }
    const long long maxDistanceLong = 1LL * dimensions * (states - 1) * (states - 1);
    // Two backpointers per DP entry, plus output witnesses. Refuse pathological
    // requests rather than overflowing an index or exhausting arbitrary memory.
    if (maxDistanceLong > 1000000 || (dimensions + 1LL) * (maxDistanceLong + 1) > 4000000) {
        throw std::length_error("MCAM pair bounds exceed the supported DP workspace (4 million entries).");
    }
    const int maxDistance = static_cast<int>(maxDistanceLong);
    const double infinity = std::numeric_limits<double>::infinity();
    struct Choice {
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -std::numeric_limits<double>::infinity();
        int minStored = -1, minQuery = -1, maxStored = -1, maxQuery = -1;
    };
    struct Previous { int distance = -1, stored = -1, query = -1; };
    std::vector<std::vector<Previous>> minPrevious(dimensions + 1), maxPrevious(dimensions + 1);
    std::vector<double> minimum(maxDistance + 1, infinity), maximum(maxDistance + 1, -infinity);
    minimum[0] = maximum[0] = 0;
    const int maxCellDistance = (states - 1) * (states - 1);
    for (int coordinate = 0; coordinate < dimensions; ++coordinate) {
        std::vector<Choice> choices(states);
        for (int stored = 0; stored < states; ++stored) {
            const int firstQuery = query.empty() ? 0 : query[coordinate];
            const int lastQuery = query.empty() ? states - 1 : query[coordinate];
            for (int searched = firstQuery; searched <= lastQuery; ++searched) {
                auto &choice = choices[std::abs(stored - searched)];
                const double conductance = 1.0 / resistance[stored][searched];
                if (conductance < choice.minimum) {
                    choice.minimum = conductance;
                    choice.minStored = stored;
                    choice.minQuery = searched;
                }
                if (conductance > choice.maximum) {
                    choice.maximum = conductance;
                    choice.maxStored = stored;
                    choice.maxQuery = searched;
                }
            }
        }
        const int nextLimit = (coordinate + 1) * maxCellDistance;
        minPrevious[coordinate + 1].resize(nextLimit + 1);
        maxPrevious[coordinate + 1].resize(nextLimit + 1);
        std::vector<double> nextMinimum(maxDistance + 1, infinity), nextMaximum(maxDistance + 1, -infinity);
        for (int distance = 0; distance <= coordinate * maxCellDistance; ++distance) {
            if (!std::isfinite(minimum[distance])) {
                continue;
            }
            for (int delta = 0; delta < states; ++delta) {
                const auto &choice = choices[delta];
                if (choice.minStored < 0) {
                    continue;
                }
                const int nextDistance = distance + delta * delta;
                const double low = minimum[distance] + choice.minimum;
                const double high = maximum[distance] + choice.maximum;
                if (low < nextMinimum[nextDistance]) {
                    nextMinimum[nextDistance] = low;
                    minPrevious[coordinate + 1][nextDistance] = {distance, choice.minStored, choice.minQuery};
                }
                if (high > nextMaximum[nextDistance]) {
                    nextMaximum[nextDistance] = high;
                    maxPrevious[coordinate + 1][nextDistance] = {distance, choice.maxStored, choice.maxQuery};
                }
            }
        }
        minimum.swap(nextMinimum);
        maximum.swap(nextMaximum);
    }
    std::vector<EvaCAMDistanceVoltageBounds> result;
    for (int distance = 0; distance <= maxDistance; ++distance) {
        if (!std::isfinite(minimum[distance])) {
            continue;
        }
        EvaCAMDistanceVoltageBounds bound{};
        bound.squaredEuclideanDistance = distance;
        bound.minimumConductance = minimum[distance];
        bound.maximumConductance = maximum[distance];
        bound.minimumConductanceStored.resize(dimensions);
        bound.minimumConductanceQuery.resize(dimensions);
        bound.maximumConductanceStored.resize(dimensions);
        bound.maximumConductanceQuery.resize(dimensions);
        bound.minimumConductanceDeltaCounts.assign(states, 0);
        bound.maximumConductanceDeltaCounts.assign(states, 0);
        int minDistance = distance, maxDistanceRemaining = distance;
        for (int coordinate = dimensions; coordinate > 0; --coordinate) {
            const auto &low = minPrevious[coordinate][minDistance];
            const auto &high = maxPrevious[coordinate][maxDistanceRemaining];
            bound.minimumConductanceStored[coordinate - 1] = low.stored;
            bound.minimumConductanceQuery[coordinate - 1] = low.query;
            bound.maximumConductanceStored[coordinate - 1] = high.stored;
            bound.maximumConductanceQuery[coordinate - 1] = high.query;
            ++bound.minimumConductanceDeltaCounts[std::abs(low.stored - low.query)];
            ++bound.maximumConductanceDeltaCounts[std::abs(high.stored - high.query)];
            minDistance = low.distance;
            maxDistanceRemaining = high.distance;
        }
        result.push_back(std::move(bound));
    }
    return result;
}
