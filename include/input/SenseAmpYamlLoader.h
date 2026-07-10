#ifndef INPUT_SENSEAMPYAMLLOADER_H_
#define INPUT_SENSEAMPYAMLLOADER_H_

#include <string>
#include <vector>

struct SenseAmpModel {
    bool loaded = false;
    std::string model;
    double minPitch = 2.0;
    double sameTypeDiffGap = 1.5;
    double pToNDiffGap = 2.0;
    double pSenseWidth = 30.0;
    double nSenseWidth = 15.0;
    double isolationWidth = 15.0;
    double enableWidth = 5.0;
    double muxWidth = 9.0;
    double ivConverterArea = 5000.0;

    struct NodeValue {
        double minFeatureSize = 0;
        double value = 0;
    };

    std::vector<NodeValue> currentSenseLatency = {
        {119e-9, 0.49e-9},
        {89e-9, 0.53e-9},
        {64e-9, 0.62e-9},
        {44e-9, 0.80e-9},
        {31e-9, 1.07e-9},
        {0, 1.45e-9},
    };
    std::vector<NodeValue> currentSenseEnergy = {
        {119e-9, 8.52e-14},
        {89e-9, 8.72e-14},
        {64e-9, 9.00e-14},
        {44e-9, 10.26e-14},
        {31e-9, 12.56e-14},
        {0, 15e-14},
    };
    std::vector<NodeValue> currentSenseLeakage = {
        {119e-9, 1.40e-8},
        {89e-9, 1.87e-8},
        {64e-9, 2.57e-8},
        {44e-9, 4.41e-9},
        {31e-9, 12.54e-8},
        {0, 15e-8},
    };
};

namespace YamlHelpers {

SenseAmpModel ReadSenseAmpModelFromYaml(const std::string& inputFile);

}  // namespace YamlHelpers

#endif  // INPUT_SENSEAMPYAMLLOADER_H_
