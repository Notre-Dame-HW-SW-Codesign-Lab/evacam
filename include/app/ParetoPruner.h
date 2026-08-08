#ifndef EVACAM_APP_PARETOPRUNER_H_
#define EVACAM_APP_PARETOPRUNER_H_

#include <array>
#include <cstddef>
#include <vector>

#include "CandidateSpec.h"

struct ParetoPruningResult {
    std::vector<EvaluatedCandidate> retainedCandidates;
    std::size_t rejectedCandidates = 0;
};

class ParetoPruner {
    public:
        static constexpr std::size_t kMetricCount = 8;
        using MetricValues = std::array<double, kMetricCount>;

        static MetricValues Metrics(const CandidateMetrics &metrics);
        static bool HasEqualMetrics(const CandidateMetrics &left,
                const CandidateMetrics &right);
        static bool Dominates(const CandidateMetrics &left,
                const CandidateMetrics &right);
        static ParetoPruningResult Prune(
                std::vector<EvaluatedCandidate> candidates);
};

#endif  // EVACAM_APP_PARETOPRUNER_H_
