#ifndef INPUT_CUSTOMSENSEAMPYAMLLOADER_H_
#define INPUT_CUSTOMSENSEAMPYAMLLOADER_H_

#include <string>

class SenseAmp;

namespace YamlHelpers {

void ReadCustomSenseAmpFromYaml(
        SenseAmp& senseAmp,
        const std::string& inputFile,
        double featureSize);

}  // namespace YamlHelpers

#endif  // INPUT_CUSTOMSENSEAMPYAMLLOADER_H_
