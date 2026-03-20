#ifndef EVACAM_MATCH_H_
#define EVACAM_MATCH_H_

#include <string>
#include <memory>
#include <vector>

#include "EvaCAMMatchResult.h"

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
        size_t word_width() const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
};

#endif /* EVACAM_MATCH_H_ */
