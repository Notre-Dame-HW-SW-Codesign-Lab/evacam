#ifndef EVACAM_TESTS_MCAMTESTCONFIG_H_
#define EVACAM_TESTS_MCAMTESTCONFIG_H_

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "TestSupport.h"

namespace McamTestConfig {

inline std::string Read(const std::filesystem::path &path) {
    std::ifstream input(path);
    TestSupport::Require(input.is_open(), "could not open MCAM fixture " + path.string());
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

inline void ReplaceAll(std::string &text, const std::string &from, const std::string &to) {
    for (size_t position = 0; (position = text.find(from, position)) != std::string::npos;
            position += to.size()) {
        text.replace(position, from.size(), to);
    }
}

inline std::filesystem::path WriteZeroSenseVariant(
        TestSupport::TemporaryDirectory &temporary,
        const std::filesystem::path &sourceConfig) {
    const auto directory = sourceConfig.parent_path();
    std::string tool = Read(sourceConfig);
    const auto architectureName = sourceConfig.stem().string().substr(0,
            sourceConfig.stem().string().find(".config")) + ".architecture.yaml";
    std::string architecture = Read(directory / architectureName);
    ReplaceAll(architecture, "sensing: ./2FeFET_MCAM.sensing.yaml",
            "sensing: " + std::filesystem::absolute(directory / "2FeFET_MCAM.sensing.yaml").string());
    const auto architecturePath = temporary.WriteFile("mcam.architecture.yaml", architecture);

    std::string device = Read(directory / "2FeFET_MCAM.memory_device.yaml");
    ReplaceAll(device, "min_sense_voltage: 70mV", "min_sense_voltage: 0V");
    const auto devicePath = temporary.WriteFile("mcam.memory_device.yaml", device);
    std::string cell = Read(directory / "2FeFET_MCAM.cell.yaml");
    ReplaceAll(cell, "memory_device: ./2FeFET_MCAM.memory_device.yaml",
            "memory_device: " + devicePath.string());
    const auto cellPath = temporary.WriteFile("mcam.cell.yaml", cell);

    ReplaceAll(tool, "architecture: " + architectureName,
            "architecture: " + architecturePath.string());
    ReplaceAll(tool, "cell: 2FeFET_MCAM.cell.yaml", "cell: " + cellPath.string());
    ReplaceAll(tool, "technology: ../lib/technology/cmos.legacy.yaml",
            "technology: " + std::filesystem::absolute(
                    directory / "../lib/technology/cmos.legacy.yaml").string());
    return temporary.WriteFile("mcam.config.yaml", tool);
}

}  // namespace McamTestConfig

#endif
