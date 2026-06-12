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
        EvaCAMMatchResult evaluate_threshold(int mismatches, int maxMismatches) const;
        EvaCAMMatchResult evaluate_vector(
                const std::vector<std::pair<double, double>> &stored,
                const std::vector<double> &query) const;
        std::vector<EvaCAMMatchResult> evaluate_array(const std::vector<int> &mismatchCounts) const;
        std::vector<EvaCAMMatchResult> evaluate_array(
                const std::vector<std::vector<int>> &storedRows,
                const std::vector<int> &query) const;
        std::vector<EvaCAMMatchResult> evaluate_array(
                const std::vector<std::vector<std::pair<double, double>>> &storedRows,
                const std::vector<double> &query) const;
        size_t word_width() const;

    private:
        void InitializeConfiguredBank();
        void BuildMismatchLut();
        void EnsureInitialized() const;
        EvaCAMMatchResult EvaluateExactVector(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult EvaluateBestVector(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult EvaluateThresholdVector(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult EvaluateExactTcamVector(const std::vector<int> &stored, const std::vector<int> &query) const;
        EvaCAMMatchResult EvaluateExactAcamVector(
                const std::vector<std::pair<double, double>> &stored,
                const std::vector<double> &query) const;
        EvaCAMMatchResult EvaluateBestAcamVector(
                const std::vector<std::pair<double, double>> &stored,
                const std::vector<double> &query) const;
        EvaCAMMatchResult EvaluateThresholdAcamVector(
                const std::vector<std::pair<double, double>> &stored,
                const std::vector<double> &query) const;
        std::vector<EvaCAMMatchResult> EvaluateBestTcamArray(const std::vector<int> &mismatchCounts) const;
        EvaCAMMatchResult LookupMismatchResult(int mismatches) const;
        int CountTcamMismatches(const std::vector<int> &stored, const std::vector<int> &query) const;
        int MaxDetectableMismatches() const;
        void ValidateTcamMismatchCounts(const std::vector<int> &mismatchCounts) const;
        void ValidateBestMatchSenseMargin(const std::vector<int> &mismatchCounts, int bestMismatches) const;
        void ValidateTcamMismatchCount(int mismatches) const;
        void ValidateMismatchCount(int mismatches) const;
        void ValidateMaxMismatches(int maxMismatches) const;
        void ValidateThresholdSenseMargin(int maxMismatches) const;
        void ValidateVectorLength(size_t size, const char *name) const;
        void ValidateBinaryVector(const std::vector<int> &value, const char *name) const;
        void ValidateTcamStoredVector(const std::vector<int> &value, const char *name) const;
        void ValidateAnalogVector(const std::vector<double> &value, const char *name) const;
        void ValidateAcamRangeVector(
                const std::vector<std::pair<double, double>> &value,
                const char *name) const;
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
