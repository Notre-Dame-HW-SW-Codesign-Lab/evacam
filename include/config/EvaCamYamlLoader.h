#ifndef CONFIG_EVACAMYAMLLOADER_H_
#define CONFIG_EVACAMYAMLLOADER_H_

#include <string>

class EvaCamConfig;

class EvaCamYamlLoader {
    public:
        static void Load(const std::string &inputFile, EvaCamConfig &config);
};

#endif /* CONFIG_EVACAMYAMLLOADER_H_ */
