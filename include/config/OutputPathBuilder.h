#ifndef OUTPUTPATHBUILDER_H_
#define OUTPUTPATHBUILDER_H_

#include <string>

#include "config/InputConfig.h"
#include "config/TechnologyContext.h"

class OutputPathBuilder {
    public:
        static std::string DefaultResultsYamlPath(const std::string &inputFile);
        static std::string ExplorationCsvPath(const InputConfig &input, const TechnologyContext &technology);
};

#endif /* OUTPUTPATHBUILDER_H_ */
