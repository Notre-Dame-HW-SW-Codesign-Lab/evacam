#ifndef EVACAM_CONTEXT_BUILDER_H_
#define EVACAM_CONTEXT_BUILDER_H_

#include <memory>
#include <string>

#include "input/CliOptions.h"

class EvaCamConfig;

struct EvaCamContext {
    std::shared_ptr<EvaCamConfig> config;
    std::string inputFileName;
    std::string outputYamlFileName;
};

class EvaCamContextBuilder {
    public:
        static EvaCamContext Build(const CliOptions &options);
};

#endif /* EVACAM_CONTEXT_BUILDER_H_ */
