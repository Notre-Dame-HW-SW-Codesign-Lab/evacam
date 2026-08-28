#ifndef EVACAM_MATCH_H_
#define EVACAM_MATCH_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "EvaCAMMatchResult.h"
#include "Wire.h"

class Bank;
class EvaCamConfig;

class EvaCAM_Match {
    public:
        explicit EvaCAM_Match(const std::string &configPath);
        ~EvaCAM_Match();

        EvaCAM_Match(const EvaCAM_Match&) = delete;
        EvaCAM_Match& operator=(const EvaCAM_Match&) = delete;
        EvaCAM_Match(EvaCAM_Match&&) noexcept;
        EvaCAM_Match& operator=(EvaCAM_Match&&) noexcept;

        bool match(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult evaluate_vector(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult evaluate_mismatches(int mismatches) const;
        EvaCAMMatchResult evaluate_threshold(
                const std::vector<int> &stored,
                const std::vector<int> &query,
                int maxMismatches) const;
        EvaCAMMatchResult evaluate_distance_threshold(
                const std::vector<int> &stored,
                const std::vector<int> &query,
                double maxSquaredDistance) const;
        EvaCAMMatchResult evaluate_threshold(int mismatches, int maxMismatches) const;
        EvaCAMMatchResult evaluate_vector(
                const std::vector<std::pair<double, double>> &stored,
                const std::vector<double> &query) const;
        std::vector<EvaCAMMatchResult> evaluate_array(const std::vector<int> &mismatchCounts) const;
        std::vector<EvaCAMMatchResult> evaluate_array(
                const std::vector<std::vector<int>> &storedRows,
                const std::vector<int> &query) const;
        std::vector<EvaCAMMatchResult> evaluate_knn(
                const std::vector<std::vector<int>> &storedRows,
                const std::vector<int> &query,
                size_t k) const;
        std::vector<EvaCAMMatchResult> evaluate_array(
                const std::vector<std::vector<std::pair<double, double>>> &storedRows,
                const std::vector<double> &query) const;
        // word_width() is defined only for single-bit CAMs. MCAM callers use
        // vector_dimensions(); storage_width_bits() is a derived capacity.
        size_t word_width() const;
        // Compatibility alias for non-MCAM callers. MCAM has vector
        // dimensions and an encoded storage width, not a logical word width.
        size_t logical_word_width_bits() const;
        size_t storage_width_bits() const;
        size_t vector_dimensions() const;
        size_t bits_per_symbol() const;
        size_t symbol_width() const;
        EvaCAMMatchResult evaluate_distance(const std::vector<int> &stored,
                const std::vector<int> &query) const;
        EvaCAMMatchResult evaluate_symbols(const std::vector<int> &stored,
                const std::vector<int> &query) const;
        EvaCAMMatchResult evaluate_bits(const std::vector<int> &storedBits,
                const std::vector<int> &queryBits) const;

    private:
        void InitializeConfiguredBank();
        void BuildMismatchLut();
        void EnsureInitialized() const;
        EvaCAMMatchResult EvaluateExactVector(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult EvaluateBestVector(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult EvaluateThresholdVector(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult EvaluateExactTcamVector(const std::vector<int> &stored, const std::vector<int> &query) const;
        std::vector<EvaCAMMatchResult> EvaluateBestTcamArray(const std::vector<int> &mismatchCounts) const;
        std::vector<EvaCAMMatchResult> EvaluateBestMcamArray(
                const std::vector<std::vector<int>> &storedRows,
                const std::vector<int> &query) const;
        EvaCAMMatchResult LookupMismatchResult(int mismatches) const;
        int CountTcamMismatches(const std::vector<int> &stored, const std::vector<int> &query) const;
        int MaxDetectableMismatches() const;
        void ValidateTcamMismatchCounts(const std::vector<int> &mismatchCounts) const;
        void ValidateBestMatchSenseMargin(const std::vector<int> &mismatchCounts, int bestMismatches) const;
        void ValidateTcamMismatchCount(int mismatches) const;
        void ValidateMismatchCount(int mismatches) const;
        void ValidateMaxMismatches(int maxMismatches) const;
        void ValidateThresholdSenseMargin(int maxMismatches) const;
        void ValidateMcamThreshold(double maxSquaredDistance) const;
        double McamThresholdVoltageMargin(
                const std::vector<int> &query,
                double maxSquaredDistance) const;
        double McamMaximumSquaredDistance(const std::vector<int> &query) const;
        void SetMcamSenseDiagnostics(
                EvaCAMMatchResult &result,
                double margin,
                bool applicable = true) const;
        void EnforceMcamSenseMargin(
                const EvaCAMMatchResult &result,
                const char *message) const;
        void ValidateVectorLength(size_t size, const char *name) const;
        void ValidateBinaryVector(const std::vector<int> &value, const char *name) const;
        void ValidateMcamVector(const std::vector<int> &value, const char *name) const;
        std::vector<int> PackMcamBits(const std::vector<int> &bits, const char *name) const;
        void ValidateTcamStoredVector(const std::vector<int> &value, const char *name) const;
        Wire CreateLocalWire() const;
        Wire CreateGlobalWire() const;

        std::shared_ptr<EvaCamConfig> config;
        std::shared_ptr<Bank> bank;
        Wire localWire;
        Wire globalWire;
        CAM_Opt camOpt{};
        std::vector<EvaCAMMatchResult> mismatchResults;
};

#endif /* EVACAM_MATCH_H_ */
