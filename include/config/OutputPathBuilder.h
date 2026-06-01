#ifndef OUTPUTPATHBUILDER_H_
#define OUTPUTPATHBUILDER_H_

#include <string>

#include "config/InputConfig.h"
#include "config/TechnologyContext.h"

class OutputPathBuilder {
    public:
        static std::string DefaultResultsYamlPath(const std::string &inputFile);
        static std::string VariationSamplesCsvPath(const std::string &outputYamlFile);
        static std::string VariationSamplesCsvPath(const std::string &outputYamlFile,
                const std::string &label);
        static std::string VariationHistogramsSvgPath(const std::string &samplesCsvFile);
        static std::string ExplorationCsvPath(const InputConfig &input, const TechnologyContext &technology);
};

#endif /* OUTPUTPATHBUILDER_H_ */
