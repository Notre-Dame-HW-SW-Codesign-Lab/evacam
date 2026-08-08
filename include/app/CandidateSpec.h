#ifndef EVACAM_APP_CANDIDATESPEC_H_
#define EVACAM_APP_CANDIDATESPEC_H_

#include <array>
#include <cstddef>
#include <string>
#include <tuple>

#include "Wire.h"
#include "typedef.h"

class Bank;

struct WireSpec {
    int type = 0;
    int repeaterType = 0;
    int isLowSwing = 0;
};

struct CandidateSpec {
    int numRowMat = 0;
    int numColumnMat = 0;
    int numActiveMatPerRow = 0;
    int numActiveMatPerColumn = 0;
    int numRowSubarray = 0;
    int numColumnSubarray = 0;
    int numActiveSubarrayPerRow = 0;
    int numActiveSubarrayPerColumn = 0;
    int muxSenseAmp = 0;
    int muxOutputLev1 = 0;
    int muxOutputLev2 = 0;
    int areaOptimizationLevel = 0;
    int rowDriverOptimizationLevel = 0;
    int priorityOptimizationLevel = 0;
    int bitSerialWidth = 0;
    WireSpec localWire;
    WireSpec globalWire;

    static CandidateSpec FromBank(const Bank &bank);
    std::string StableId() const;
    bool operator==(const CandidateSpec &other) const;
    bool operator<(const CandidateSpec &other) const;
};

struct CandidateSpecHash {
    std::size_t operator()(const CandidateSpec &candidate) const;
};

struct CandidateAccounting {
    long long rawCandidates = 0;
    long long structurallyRejected = 0;
    long long duplicateCandidates = 0;
    long long modeledCandidates = 0;
    long long invalidCandidates = 0;
    long long validCandidates = 0;
    long long constraintRejected = 0;
    long long retainedCandidates = 0;
    long long reconstructionEvaluations = 0;

    CandidateAccounting &operator+=(const CandidateAccounting &other);
    bool HasConsistentEnumerationCounts() const;
    bool HasConsistentModelCounts() const;
};

struct CandidateMetrics {
    std::array<double, static_cast<std::size_t>(full_exploration)> objectiveValues{};
    double readLatency = 0;
    double writeLatency = 0;
    double readDynamicEnergy = 0;
    double writeDynamicEnergy = 0;
    double readEdp = 0;
    double writeEdp = 0;
    double area = 0;
    double leakage = 0;

    static CandidateMetrics FromBank(const Bank &bank);
};

struct EvaluatedCandidate {
    CandidateSpec spec;
    CandidateMetrics metrics;
};

#endif  // EVACAM_APP_CANDIDATESPEC_H_
