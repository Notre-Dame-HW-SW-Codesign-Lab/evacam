#ifndef EVACAM_MATCH_H_
#define EVACAM_MATCH_H_

#include <memory>
#include <string>
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
        EvaCAMMatchResult evaluate(const std::vector<int> &stored, const std::vector<int> &query) const;
        std::vector<EvaCAMMatchResult> evaluate_rows(
                const std::vector<std::vector<int>> &storedRows,
                const std::vector<int> &query) const;
        size_t word_width() const;

    private:
        void InitializeConfiguredBank();
        void BuildMismatchLut();
        int CountMismatches(const std::vector<int> &stored, const std::vector<int> &query) const;
        void ValidateBinaryVector(const std::vector<int> &value, const char *name) const;
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
