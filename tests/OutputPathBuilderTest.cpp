#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "config/OutputPathBuilder.h"
#include "MemCell.h"

namespace {

TechnologyContext MakeTechnology(bool readMode) {
    TechnologyContext technology;
    technology.cell = std::make_shared<MemCell>();
    technology.cell->readMode = readMode;
    return technology;
}

void TestDefaultResultsYamlPath() {
    assert(OutputPathBuilder::DefaultResultsYamlPath(
            "config/2FeFET_TCAM/2FeFET_TCAM_system_config.yaml")
            == "results/2FeFET_TCAM_results.yaml");
    assert(OutputPathBuilder::DefaultResultsYamlPath("case-config.yaml")
            == "results/case_results.yaml");
}

void TestVariationPaths() {
    assert(OutputPathBuilder::VariationSamplesCsvPath("results/case_results.yaml")
            == "results/case_variation_samples.csv");
    assert(OutputPathBuilder::VariationSamplesCsvPath("results/case_results.yaml", "area")
            == "results/case_area_variation_samples.csv");
    assert(OutputPathBuilder::VariationHistogramsSvgPath("results/case_variation_samples.csv")
            == "results/case_variation_histograms.svg");
}

void TestExplorationCsvPathOmitsAssociativity() {
    InputConfig input;
    input.outputFilePrefix = "results/output";
    input.capacity = 2LL * 1024 * 1024;
    input.wordWidth = 512;
    input.internalSensing = true;

    assert(OutputPathBuilder::ExplorationCsvPath(input, MakeTechnology(true))
            == "results/output_2048K_512_IN_VOL.csv");

    input.internalSensing = false;
    assert(OutputPathBuilder::ExplorationCsvPath(input, MakeTechnology(false))
            == "results/output_2048K_512_EX_CUR.csv");
}

}  // namespace

int main() {
    TestDefaultResultsYamlPath();
    TestVariationPaths();
    TestExplorationCsvPathOmitsAssociativity();

    std::cout << "OutputPathBuilder tests passed" << std::endl;
    return 0;
}
