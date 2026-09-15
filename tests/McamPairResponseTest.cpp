#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

#include "EvaCAM_Match.h"
#include "McamPairResponse.h"
#include "TestSupport.h"

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

namespace {

double Conductance(const std::vector<std::vector<double>> &resistance,
        const std::vector<int> &stored, const std::vector<int> &query) {
    double sum = 0;
    for (size_t i = 0; i < stored.size(); ++i) {
        sum += 1.0 / resistance[stored[i]][query[i]];
    }
    return sum;
}

int Distance(const std::vector<int> &stored, const std::vector<int> &query) {
    int distance = 0;
    for (size_t i = 0; i < stored.size(); ++i) {
        const int delta = stored[i] - query[i];
        distance += delta * delta;
    }
    return distance;
}

void CheckBounds(const std::vector<std::vector<double>> &resistance,
        const std::vector<int> &fixedQuery) {
    std::map<int, std::pair<double, double>> exhaustive;
    for (int storedCode = 0; storedCode < 64; ++storedCode) {
        const std::vector<int> stored{storedCode / 8, storedCode % 8};
        for (int queryCode = 0; queryCode < (fixedQuery.empty() ? 64 : 1); ++queryCode) {
            const std::vector<int> query = fixedQuery.empty()
                ? std::vector<int>{queryCode / 8, queryCode % 8} : fixedQuery;
            const int distance = Distance(stored, query);
            const double conductance = Conductance(resistance, stored, query);
            const auto found = exhaustive.find(distance);
            if (found == exhaustive.end()) {
                exhaustive.emplace(distance, std::make_pair(conductance, conductance));
            } else {
                found->second.first = std::min(found->second.first, conductance);
                found->second.second = std::max(found->second.second, conductance);
            }
        }
    }
    const auto bounds = McamPairConductanceBounds(resistance, 2, fixedQuery);
    Require(bounds.size() == exhaustive.size(), "Only reachable distances should be returned.");
    for (const auto &bound : bounds) {
        const int distance = static_cast<int>(bound.squaredEuclideanDistance);
        const auto expected = exhaustive.at(distance);
        AssertNear(bound.minimumConductance, expected.first);
        AssertNear(bound.maximumConductance, expected.second);
        Require(Distance(bound.minimumConductanceStored, bound.minimumConductanceQuery) == distance,
                "Minimum witness must have the requested distance.");
        Require(Distance(bound.maximumConductanceStored, bound.maximumConductanceQuery) == distance,
                "Maximum witness must have the requested distance.");
        AssertNear(Conductance(resistance, bound.minimumConductanceStored,
                    bound.minimumConductanceQuery), bound.minimumConductance);
        AssertNear(Conductance(resistance, bound.maximumConductanceStored,
                    bound.maximumConductanceQuery), bound.maximumConductance);
        if (!fixedQuery.empty()) {
            Require(bound.minimumConductanceQuery == fixedQuery
                    && bound.maximumConductanceQuery == fixedQuery,
                    "Fixed-query witnesses must preserve the query.");
        }
    }
}

void CheckNativeModel() {
    EvaCAM_Match matcher("config/2FeFET_MCAM_pair/2FeFET_MCAM_8x8.config.yaml");
    const size_t dimensions = matcher.vector_dimensions();
    Require(dimensions == 8, "Pair example should use eight vector dimensions.");
    double slowestExactDelay = 0;
    double lowestExactVoltage = std::numeric_limits<double>::infinity();
    double highestExactVoltage = 0;
    for (int state = 0; state < 8; ++state) {
        const std::vector<int> row(dimensions, state);
        const auto result = matcher.evaluate_distance(row, row);
        Require(result.hit, "Pair mode retains the ideal equality decision.");
        slowestExactDelay = std::max(slowestExactDelay, result.matchlineDelay);
        lowestExactVoltage = std::min(lowestExactVoltage, result.matchlineVoltage);
        highestExactVoltage = std::max(highestExactVoltage, result.matchlineVoltage);
    }
    const auto bounds = matcher.distance_voltage_bounds({}, false);
    Require(!bounds.empty() && bounds.front().squaredEuclideanDistance == 0,
            "All-pair bounds must include exact matches.");
    AssertNear(bounds.front().sensingTime, slowestExactDelay);
    AssertNear(bounds.front().minimumVoltage, lowestExactVoltage);
    AssertNear(bounds.front().maximumVoltage, highestExactVoltage);
    Require(highestExactVoltage > lowestExactVoltage,
            "The table's unequal diagonal responses must not collapse into one exact match.");
    double highestMismatchVoltage = 0;
    for (const auto &bound : bounds) {
        if (bound.squaredEuclideanDistance > 0) {
            highestMismatchVoltage = std::max(highestMismatchVoltage, bound.maximumVoltage);
        }
        const auto lowG = matcher.evaluate_distance(
                bound.minimumConductanceStored, bound.minimumConductanceQuery);
        const auto highG = matcher.evaluate_distance(
                bound.maximumConductanceStored, bound.maximumConductanceQuery);
        AssertNear(lowG.matchlineConductance, bound.minimumConductance);
        AssertNear(highG.matchlineConductance, bound.maximumConductance);
        AssertNear(lowG.matchlineVoltage, bound.maximumVoltage);
        AssertNear(highG.matchlineVoltage, bound.minimumVoltage);
        AssertNear(lowG.searchLatency, bound.maximumSearchLatency);
        AssertNear(highG.searchLatency, bound.minimumSearchLatency);
        AssertNear(bound.sensingTime, slowestExactDelay);
    }
    std::vector<int> stored(dimensions, 3), query(dimensions, 3);
    stored[0] = 1;
    query[0] = 2;
    const auto first = matcher.evaluate_distance(stored, query);
    stored[0] = 6;
    query[0] = 7;
    const auto second = matcher.evaluate_distance(stored, query);
    AssertNear(first.squaredEuclideanDistance, second.squaredEuclideanDistance);
    Require(std::abs(first.matchlineConductance - second.matchlineConductance) > 1e-10,
            "Equal one-step differences must retain different pair conductances.");
    std::swap(stored[0], query[0]);
    const auto reversed = matcher.evaluate_distance(stored, query);
    Require(std::abs(second.matchlineConductance - reversed.matchlineConductance) > 1e-10,
            "Reversing a pair must not silently symmetrize its conductance.");
    const auto fixed = matcher.distance_voltage_bounds(query, false);
    for (const auto &bound : fixed) {
        Require(bound.minimumConductanceQuery == query && bound.maximumConductanceQuery == query,
                "Native fixed-query bounds must retain their query.");
    }
    const std::vector<int> zero(dimensions, 0);
    const auto exactResult = matcher.evaluate_distance(zero, zero);
    AssertNear(exactResult.senseMargin, lowestExactVoltage - highestMismatchVoltage);
    Require(exactResult.senseMargin < 0 && !exactResult.senseMarginPass,
            "The provisional table's overlapping responses must retain a failed signed margin.");
    Require(std::isfinite(exactResult.searchLatency) && exactResult.searchLatency > 0,
            "Non-strict overlap analysis should retain a finite nominal timing budget.");
    auto counts = std::vector<int>(8, 0);
    counts[0] = static_cast<int>(dimensions);
    AssertNear(matcher.evaluate_zero_query_composition(counts).matchlineVoltage,
            matcher.evaluate_distance(zero, zero).matchlineVoltage);
    AssertThrows<std::invalid_argument>([&] {
        matcher.evaluate_zero_query_composition(counts, 1);
    }, "zero sigma offset");
}

} // namespace

int main() {
    std::vector<std::vector<double>> resistance(8, std::vector<double>(8));
    for (int stored = 0; stored < 8; ++stored) {
        for (int query = 0; query < 8; ++query) {
            // Deliberately asymmetric, unequal diagonals, and nonmonotonic.
            resistance[stored][query] = 1e5 + 1e4 * ((stored * 11 + query * 7) % 19);
        }
    }
    ValidateMcamPairResistance(resistance);
    CheckBounds(resistance, {});
    CheckBounds(resistance, {2, 6});
    AssertThrows<std::invalid_argument>([] { ValidateMcamPairResistance({}); }, "power-of-two");
    AssertThrows<std::invalid_argument>([] { ValidateMcamPairResistance({{1, 2}, {3}}); }, "square");
    AssertThrows<std::invalid_argument>([] { ValidateMcamPairResistance({{1, 0}, {3, 4}}); }, "positive");
    AssertThrows<std::invalid_argument>([&] { McamPairConductanceBounds(resistance, 0); }, "dimensions");
    AssertThrows<std::invalid_argument>([&] { McamPairConductanceBounds(resistance, 2, {0}); }, "query length");
    AssertThrows<std::invalid_argument>([&] { McamPairConductanceBounds(resistance, 2, {0, 8}); }, "out of range");
    AssertThrows<std::length_error>([&] { McamPairConductanceBounds(resistance, 1000000); }, "workspace");
    CheckNativeModel();
    std::cout << "MCAM pair-response tests passed.\n";
}
