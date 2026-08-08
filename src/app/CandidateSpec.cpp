#include "CandidateSpec.h"

#include <sstream>

#include "Bank.h"

namespace {

auto CandidateTuple(const CandidateSpec &candidate) {
    return std::tie(candidate.numRowMat,
            candidate.numColumnMat,
            candidate.numActiveMatPerRow,
            candidate.numActiveMatPerColumn,
            candidate.numRowSubarray,
            candidate.numColumnSubarray,
            candidate.numActiveSubarrayPerRow,
            candidate.numActiveSubarrayPerColumn,
            candidate.muxSenseAmp,
            candidate.muxOutputLev1,
            candidate.muxOutputLev2,
            candidate.areaOptimizationLevel,
            candidate.rowDriverOptimizationLevel,
            candidate.priorityOptimizationLevel,
            candidate.bitSerialWidth,
            candidate.localWire.type,
            candidate.localWire.repeaterType,
            candidate.localWire.isLowSwing,
            candidate.globalWire.type,
            candidate.globalWire.repeaterType,
            candidate.globalWire.isLowSwing);
}

void HashCombine(std::size_t &seed, int value) {
    seed ^= std::hash<int>{}(value) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
}

}  // namespace

CandidateSpec CandidateSpec::FromBank(const Bank &bank) {
    CandidateSpec candidate;
    candidate.numRowMat = bank.numRowMat;
    candidate.numColumnMat = bank.numColumnMat;
    candidate.numActiveMatPerRow = bank.numActiveMatPerRow;
    candidate.numActiveMatPerColumn = bank.numActiveMatPerColumn;
    candidate.numRowSubarray = bank.numRowSubarray;
    candidate.numColumnSubarray = bank.numColumnSubarray;
    candidate.numActiveSubarrayPerRow = bank.numActiveSubarrayPerRow;
    candidate.numActiveSubarrayPerColumn = bank.numActiveSubarrayPerColumn;
    candidate.muxSenseAmp = bank.muxSenseAmp;
    candidate.muxOutputLev1 = bank.muxOutputLev1;
    candidate.muxOutputLev2 = bank.muxOutputLev2;
    candidate.areaOptimizationLevel = bank.areaOptimizationLevel;
    candidate.rowDriverOptimizationLevel = bank.CAM_opt.RowDriver;
    candidate.priorityOptimizationLevel = bank.CAM_opt.Proirity;
    candidate.bitSerialWidth = bank.CAM_opt.BitSerialWidth;
    candidate.localWire = {bank.localWire.wireType,
        bank.localWire.wireRepeaterType, bank.localWire.isLowSwing};
    candidate.globalWire = {bank.globalWire.wireType,
        bank.globalWire.wireRepeaterType, bank.globalWire.isLowSwing};
    return candidate;
}

std::string CandidateSpec::StableId() const {
    std::ostringstream output;
    output << "v1";
    std::apply([&output](const auto &... values) { ((output << '.' << values), ...); },
            CandidateTuple(*this));
    return output.str();
}

bool CandidateSpec::operator==(const CandidateSpec &other) const {
    return CandidateTuple(*this) == CandidateTuple(other);
}

bool CandidateSpec::operator<(const CandidateSpec &other) const {
    return CandidateTuple(*this) < CandidateTuple(other);
}

std::size_t CandidateSpecHash::operator()(const CandidateSpec &candidate) const {
    std::size_t seed = 0;
    std::apply([&seed](const auto &... values) { (HashCombine(seed, values), ...); },
            CandidateTuple(candidate));
    return seed;
}

CandidateAccounting &CandidateAccounting::operator+=(const CandidateAccounting &other) {
    rawCandidates += other.rawCandidates;
    structurallyRejected += other.structurallyRejected;
    duplicateCandidates += other.duplicateCandidates;
    modeledCandidates += other.modeledCandidates;
    invalidCandidates += other.invalidCandidates;
    validCandidates += other.validCandidates;
    constraintRejected += other.constraintRejected;
    constraintPassingCandidates += other.constraintPassingCandidates;
    pruningRejectedCandidates += other.pruningRejectedCandidates;
    retainedCandidates += other.retainedCandidates;
    reconstructionEvaluations += other.reconstructionEvaluations;
    return *this;
}

bool CandidateAccounting::HasConsistentEnumerationCounts() const {
    return rawCandidates == structurallyRejected + duplicateCandidates + modeledCandidates;
}

bool CandidateAccounting::HasConsistentModelCounts() const {
    return modeledCandidates == invalidCandidates + validCandidates;
}

bool CandidateAccounting::HasConsistentFilteringCounts() const {
    return validCandidates == constraintRejected + constraintPassingCandidates
        && constraintPassingCandidates
            == pruningRejectedCandidates + retainedCandidates;
}

CandidateMetrics CandidateMetrics::FromBank(const Bank &bank) {
    CandidateMetrics metrics;
    metrics.objectiveValues[read_latency_optimized] = bank.readLatency;
    metrics.objectiveValues[write_latency_optimized] = bank.writeLatency;
    metrics.objectiveValues[read_energy_optimized] = bank.readDynamicEnergy;
    metrics.objectiveValues[write_energy_optimized] = bank.writeDynamicEnergy;
    metrics.objectiveValues[read_edp_optimized] = bank.readLatency * bank.readDynamicEnergy;
    metrics.objectiveValues[write_edp_optimized] = bank.writeLatency * bank.writeDynamicEnergy;
    metrics.objectiveValues[area_optimized] = bank.area;
    metrics.objectiveValues[leakage_optimized] = bank.leakage;
    metrics.objectiveValues[search_latency_optimized] = bank.searchLatency;
    metrics.objectiveValues[search_energy_optimized] = bank.searchDynamicEnergy;
    metrics.objectiveValues[search_edp_optimized] = bank.searchLatency * bank.searchDynamicEnergy;
    metrics.readLatency = bank.readLatency;
    metrics.writeLatency = bank.writeLatency;
    metrics.readDynamicEnergy = bank.readDynamicEnergy;
    metrics.writeDynamicEnergy = bank.writeDynamicEnergy;
    metrics.readEdp = bank.readLatency * bank.readDynamicEnergy;
    metrics.writeEdp = bank.writeLatency * bank.writeDynamicEnergy;
    metrics.area = bank.area;
    metrics.leakage = bank.leakage;
    return metrics;
}
