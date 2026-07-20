#include "config/EvaCamConfig.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool HasPathPart(const std::filesystem::path& path, const std::string& part) {
    for (const auto& item : path) {
        if (item == part) {
            return true;
        }
    }
    return false;
}

std::vector<std::filesystem::path> GeneratedConfigFiles() {
    std::vector<std::filesystem::path> configs;
    for (const auto& entry : std::filesystem::recursive_directory_iterator("config")) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (HasPathPart(path, "examples") || HasPathPart(path, "v1schema")) {
            continue;
        }
        if (path.filename().string().size() < std::string(".config.yaml").size()
                || path.filename().string().find(".config.yaml") == std::string::npos) {
            continue;
        }
        configs.push_back(path);
    }
    std::sort(configs.begin(), configs.end());
    return configs;
}

void AssertRepresentativeConfigValues(const std::filesystem::path& configFile,
        const EvaCamConfig& config) {
    if (configFile == "config/2FeFET_TCAM_DSE/2FeFET_TCAM_DSE.config.yaml"
            || configFile == "config/2FeFET_TCAM_var/2FeFET_TCAM.config.yaml") {
        assert(config.input.processNode == 45);
        assert(config.technology.cell->camType == TCAM);
    } else if (configFile == "config/2FeFET_MCAM/2FeFET_MCAM.config.yaml") {
        assert(config.input.processNode == 22);
        assert(config.technology.cell->camType == MCAM);
    }
}

}  // namespace

int main() {
    const auto configs = GeneratedConfigFiles();
    assert(!configs.empty());

    for (const auto& configFile : configs) {
        try {
            EvaCamConfig config;
            config.ReadConfigFromFile(configFile.string());
            assert(config.technology.cell != nullptr);
            assert(!config.input.fileMemCell.empty());
            AssertRepresentativeConfigValues(configFile, config);
        } catch (const std::exception& e) {
            std::cerr << "Failed generated v2 config: " << configFile
                      << "\n" << e.what() << std::endl;
            assert(false);
        }
    }

    return 0;
}
