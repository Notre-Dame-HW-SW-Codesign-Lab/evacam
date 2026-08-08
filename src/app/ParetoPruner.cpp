#include "ParetoPruner.h"

#include <algorithm>
#include <utility>

ParetoPruner::MetricValues ParetoPruner::Metrics(const CandidateMetrics &metrics) {
    return {
        metrics.objectiveValues[read_latency_optimized],
        metrics.objectiveValues[write_latency_optimized],
        metrics.objectiveValues[search_latency_optimized],
        metrics.objectiveValues[read_energy_optimized],
        metrics.objectiveValues[write_energy_optimized],
        metrics.objectiveValues[search_energy_optimized],
        metrics.objectiveValues[area_optimized],
        metrics.objectiveValues[leakage_optimized],
    };
}

bool ParetoPruner::HasEqualMetrics(const CandidateMetrics &left,
        const CandidateMetrics &right) {
    return Metrics(left) == Metrics(right);
}

bool ParetoPruner::Dominates(const CandidateMetrics &left,
        const CandidateMetrics &right) {
    const MetricValues leftValues = Metrics(left);
    const MetricValues rightValues = Metrics(right);
    bool strictlyBetter = false;
    for (std::size_t index = 0; index < leftValues.size(); ++index) {
        if (leftValues[index] > rightValues[index]) {
            return false;
        }
        strictlyBetter = strictlyBetter || leftValues[index] < rightValues[index];
    }
    return strictlyBetter;
}

ParetoPruningResult ParetoPruner::Prune(
        std::vector<EvaluatedCandidate> candidates) {
    std::sort(candidates.begin(), candidates.end(),
            [](const EvaluatedCandidate &left, const EvaluatedCandidate &right) {
                return left.spec < right.spec;
            });

    std::vector<EvaluatedCandidate> frontier;
    for (EvaluatedCandidate &candidate : candidates) {
        const bool rejected = std::any_of(frontier.begin(), frontier.end(),
                [&candidate](const EvaluatedCandidate &retained) {
                    return HasEqualMetrics(retained.metrics, candidate.metrics)
                        || Dominates(retained.metrics, candidate.metrics);
                });
        if (rejected) {
            continue;
        }

        frontier.erase(std::remove_if(frontier.begin(), frontier.end(),
                [&candidate](const EvaluatedCandidate &retained) {
                    return Dominates(candidate.metrics, retained.metrics);
                }), frontier.end());
        frontier.push_back(std::move(candidate));
    }

    std::sort(frontier.begin(), frontier.end(),
            [](const EvaluatedCandidate &left, const EvaluatedCandidate &right) {
                return left.spec < right.spec;
            });

    ParetoPruningResult result;
    result.rejectedCandidates = candidates.size() - frontier.size();
    result.retainedCandidates = std::move(frontier);
    return result;
}
