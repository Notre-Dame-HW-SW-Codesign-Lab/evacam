#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

#include "ParetoPruner.h"

namespace {

EvaluatedCandidate MakeCandidate(int identity,
        const ParetoPruner::MetricValues &values) {
    EvaluatedCandidate candidate;
    candidate.spec.numRowMat = identity;
    candidate.metrics.objectiveValues[read_latency_optimized] = values[0];
    candidate.metrics.objectiveValues[write_latency_optimized] = values[1];
    candidate.metrics.objectiveValues[search_latency_optimized] = values[2];
    candidate.metrics.objectiveValues[read_energy_optimized] = values[3];
    candidate.metrics.objectiveValues[write_energy_optimized] = values[4];
    candidate.metrics.objectiveValues[search_energy_optimized] = values[5];
    candidate.metrics.objectiveValues[area_optimized] = values[6];
    candidate.metrics.objectiveValues[leakage_optimized] = values[7];
    return candidate;
}

std::vector<int> Identities(const ParetoPruningResult &result) {
    std::vector<int> identities;
    for (const EvaluatedCandidate &candidate : result.retainedCandidates) {
        identities.push_back(candidate.spec.numRowMat);
    }
    return identities;
}

void TestMetricSelection() {
    CandidateMetrics metrics;
    metrics.objectiveValues[read_latency_optimized] = 1;
    metrics.objectiveValues[write_latency_optimized] = 2;
    metrics.objectiveValues[search_latency_optimized] = 3;
    metrics.objectiveValues[read_energy_optimized] = 4;
    metrics.objectiveValues[write_energy_optimized] = 5;
    metrics.objectiveValues[search_energy_optimized] = 6;
    metrics.objectiveValues[area_optimized] = 7;
    metrics.objectiveValues[leakage_optimized] = 8;
    metrics.objectiveValues[read_edp_optimized] = 99;
    assert((ParetoPruner::Metrics(metrics)
            == ParetoPruner::MetricValues{1, 2, 3, 4, 5, 6, 7, 8}));
}

void TestDominanceAndEquality() {
    const auto best = MakeCandidate(1, {1, 1, 1, 1, 1, 1, 1, 1});
    const auto dominated = MakeCandidate(2, {2, 2, 2, 2, 2, 2, 2, 2});
    const auto tradeoff = MakeCandidate(3, {0.5, 3, 3, 3, 3, 3, 3, 3});
    const auto equalButHigherIdentity = MakeCandidate(4, {1, 1, 1, 1, 1, 1, 1, 1});

    assert(ParetoPruner::Dominates(best.metrics, dominated.metrics));
    assert(!ParetoPruner::Dominates(dominated.metrics, best.metrics));
    assert(!ParetoPruner::Dominates(best.metrics, tradeoff.metrics));
    assert(!ParetoPruner::Dominates(tradeoff.metrics, best.metrics));
    assert(ParetoPruner::HasEqualMetrics(best.metrics, equalButHigherIdentity.metrics));

    const ParetoPruningResult result = ParetoPruner::Prune(
            {dominated, equalButHigherIdentity, tradeoff, best});
    assert((Identities(result) == std::vector<int>{1, 3}));
    assert(result.rejectedCandidates == 2);
}

void TestInputOrderDoesNotAffectFrontier() {
    std::vector<EvaluatedCandidate> candidates = {
        MakeCandidate(7, {7, 1, 7, 1, 7, 1, 7, 1}),
        MakeCandidate(5, {1, 7, 1, 7, 1, 7, 1, 7}),
        MakeCandidate(9, {9, 9, 9, 9, 9, 9, 9, 9}),
    };
    const ParetoPruningResult forward = ParetoPruner::Prune(candidates);
    std::reverse(candidates.begin(), candidates.end());
    const ParetoPruningResult reverse = ParetoPruner::Prune(candidates);
    assert((Identities(forward) == std::vector<int>{5, 7}));
    assert(Identities(reverse) == Identities(forward));
    assert(reverse.rejectedCandidates == forward.rejectedCandidates);
}

void TestDerivedEdpDoesNotParticipate() {
    auto lowerIdentity = MakeCandidate(1, {1, 1, 1, 1, 1, 1, 1, 1});
    auto higherIdentity = MakeCandidate(2, {1, 1, 1, 1, 1, 1, 1, 1});
    lowerIdentity.metrics.objectiveValues[read_edp_optimized] = 100;
    higherIdentity.metrics.objectiveValues[read_edp_optimized] = 1;
    const ParetoPruningResult result = ParetoPruner::Prune(
            {higherIdentity, lowerIdentity});
    assert((Identities(result) == std::vector<int>{1}));
    assert(result.rejectedCandidates == 1);
}

void TestEmptyAndSingleCandidateInputs() {
    const ParetoPruningResult empty = ParetoPruner::Prune({});
    assert(empty.retainedCandidates.empty());
    assert(empty.rejectedCandidates == 0);

    const ParetoPruningResult single = ParetoPruner::Prune(
            {MakeCandidate(4, {1, 2, 3, 4, 5, 6, 7, 8})});
    assert((Identities(single) == std::vector<int>{4}));
    assert(single.rejectedCandidates == 0);
}

}  // namespace

int main() {
    TestMetricSelection();
    TestDominanceAndEquality();
    TestInputOrderDoesNotAffectFrontier();
    TestDerivedEdpDoesNotParticipate();
    TestEmptyAndSingleCandidateInputs();
    std::cout << "Pareto pruner tests passed" << std::endl;
    return 0;
}
