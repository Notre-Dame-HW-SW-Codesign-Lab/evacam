#ifndef EVACAM_RUN_RESULT_H_
#define EVACAM_RUN_RESULT_H_

#include <string>
#include <unordered_map>
#include <vector>

struct EvaCamMetricStatsDto {
    bool available = false;
    double nominal = 0;
    double sample = 0;
    double mean = 0;
    double stddev = 0;
    double min = 0;
    double max = 0;
    double p95 = 0;
};

struct EvaCamVariationSampleDto {
    int sample = 0;
    std::string cornerLabel;
    std::string memoryDeviceResOnCorner = "nominal";
    std::string memoryDeviceResOffCorner = "nominal";
    double matchlineDelay = 0;
    double searchLatency = 0;
    double searchDynamicEnergy = 0;
    double senseMargin = 0;
    double referenceDelay = 0;
};

struct EvaCamVariationDto {
    bool enabled = false;
    std::string mode = "nominal";
    int samples = 0;
    EvaCamMetricStatsDto matchlineDelay;
    EvaCamMetricStatsDto searchLatency;
    EvaCamMetricStatsDto searchDynamicEnergy;
    EvaCamMetricStatsDto senseMargin;
    EvaCamMetricStatsDto referenceDelay;
    std::vector<EvaCamVariationSampleDto> sampleData;
};

struct EvaCamDesignResultDto {
    std::string optimizationTarget;
    std::unordered_map<std::string, double> summary;
    std::unordered_map<std::string, double> breakdown;
    std::unordered_map<std::string, double> geometry;
    EvaCamVariationDto variation;
};

struct EvaCamRunResultDto {
    long long numSolutions = 0;
    std::string explorationCsvPath;
    std::string outputYamlPath;
    std::unordered_map<std::string, EvaCamDesignResultDto> bestResults;
};

#endif /* EVACAM_RUN_RESULT_H_ */
