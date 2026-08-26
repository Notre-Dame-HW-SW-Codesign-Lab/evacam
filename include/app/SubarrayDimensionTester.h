#ifndef SUBARRAY_DIMENSION_TESTER_H_
#define SUBARRAY_DIMENSION_TESTER_H_

#include <filesystem>
#include <string>
#include <vector>

struct SubarrayDimensionTestConfig {
    std::string name;
    std::filesystem::path configPath;
    std::string configPattern;
    std::filesystem::path baseConfigPath;
    std::vector<int> rows;
    std::vector<int> columns;
    int threadsPerRun = 1;
    std::filesystem::path outputDirectory;
    std::filesystem::path summaryCsvPath;
};

struct SubarrayDimensionRunSpec {
    int rows = 0;
    int columns = 0;
    std::filesystem::path configPath;
    std::filesystem::path resultPath;
    std::string optimizationTarget;
    bool overrideDimensions = false;

    std::string Label() const;
};

struct SubarrayDimensionTesterOptions {
    std::string configPath;
    int jobs = 1;
    bool stdoutOutput = true;
    bool verbose = false;
    bool variationPlots = true;
};

struct SubarrayDimensionTestSummary {
    int totalRuns = 0;
    int completedRuns = 0;
    int failedRuns = 0;
    std::filesystem::path summaryCsvPath;
};

class SubarrayDimensionTester {
    public:
        static SubarrayDimensionTestConfig LoadConfig(
                const std::filesystem::path &configPath);
        static std::vector<SubarrayDimensionRunSpec> BuildRunSpecs(
                const SubarrayDimensionTestConfig &config);
        static SubarrayDimensionTestSummary Run(
                const SubarrayDimensionTesterOptions &options);
};

#endif  // SUBARRAY_DIMENSION_TESTER_H_
