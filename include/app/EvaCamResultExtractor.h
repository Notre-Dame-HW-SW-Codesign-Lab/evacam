#ifndef EVACAM_RESULT_EXTRACTOR_H_
#define EVACAM_RESULT_EXTRACTOR_H_

#include <memory>
#include <vector>

#include "EvaCamRunResult.h"

class CAM_Result;

EvaCamRunResultDto ExtractEvaCamRunResult(
        long long numSolutions,
        const std::vector<std::shared_ptr<CAM_Result>> &bestResults,
        const std::string &explorationCsvPath,
        const std::string &outputYamlPath);

#endif /* EVACAM_RESULT_EXTRACTOR_H_ */
