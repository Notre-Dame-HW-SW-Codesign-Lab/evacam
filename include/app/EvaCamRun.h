#ifndef EVACAM_RUN_H_
#define EVACAM_RUN_H_

#include <string>

#include "EvaCamRunResult.h"

struct EvaCamRunOptions {
    std::string configPath;
    int threads = 1;
    std::string outputYamlPath;
    int subarrayRows = 0;
    int subarrayColumns = 0;
    bool writeYaml = false;
    bool stdoutOutput = false;
    bool verbose = false;
    bool variationPlots = false;
};

EvaCamRunResultDto RunEvaCam(const EvaCamRunOptions &options);

#endif /* EVACAM_RUN_H_ */
