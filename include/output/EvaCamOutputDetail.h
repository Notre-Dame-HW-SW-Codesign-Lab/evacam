#ifndef OUTPUT_EVACAM_OUTPUT_DETAIL_H_
#define OUTPUT_EVACAM_OUTPUT_DETAIL_H_

#include <functional>
#include <memory>
#include <string>

class Result;

namespace EvaCamOutputDetail {

using CommandRunner = std::function<int(const std::string &)>;

bool HasVariationSamples(const std::shared_ptr<Result> &result);
bool HasMonteCarloSamples(const std::shared_ptr<Result> &result);
std::string ShellQuote(const std::string &value);
std::string BuildVariationHistogramCommand(
        const std::string &samplesPath, const std::string &plotPath);
bool WriteVariationHistogramFile(
        const std::string &samplesPath,
        const std::string &plotPath,
        const CommandRunner &runner = {});

}  // namespace EvaCamOutputDetail

#endif /* OUTPUT_EVACAM_OUTPUT_DETAIL_H_ */
