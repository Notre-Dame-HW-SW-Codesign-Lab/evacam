#include "config/TechnologyLoader.h"
#include "config/VariationConfigBuilder.h"
#include "input/TechnologyYamlLoader.h"
#include "technology/MemCell.h"
#include "technology/Technology.h"

#include "TestSupport.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::TemporaryDirectory;

std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input(path);
    Require(input.is_open(), "could not open fixture: " + path.string());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::filesystem::path RepositoryPath(const std::string &relativePath) {
    return std::filesystem::current_path() / relativePath;
}

InputConfig MakeInput(const std::filesystem::path &technologyFile,
        const std::filesystem::path &cellFile, int processNode = 45) {
    InputConfig input;
    input.fileTechnology = technologyFile.string();
    input.fileMemCell = cellFile.string();
    input.processNode = processNode;
    input.deviceRoadmap = HP;
    input.designTarget = CAM_chip;
    return input;
}

PeripheralConfig EmptyPeripherals() {
    return PeripheralConfig();
}

const TechnologySpec &FindSpec(const std::vector<TechnologySpec> &specs,
        int node, DeviceRoadmap roadmap) {
    for (const TechnologySpec &spec : specs) {
        if (spec.featureSizeInNano == node && spec.roadmap == roadmap) {
            return spec;
        }
    }
    throw std::runtime_error("missing expected technology specification");
}

std::filesystem::path WriteCellFixture(TemporaryDirectory &directory) {
    const std::filesystem::path sourceCell =
            RepositoryPath("config/SRAM-16T-ESSCIRC15/SRAM-16T-ESSCIRC15.cell.yaml");
    const std::filesystem::path sourceDevice =
            RepositoryPath("config/SRAM-16T-ESSCIRC15/SRAM-16T-ESSCIRC15.memory_device.yaml");
    directory.WriteFile("fixtures/SRAM-16T-ESSCIRC15.cell.yaml", ReadFile(sourceCell));
    directory.WriteFile("fixtures/SRAM-16T-ESSCIRC15.memory_device.yaml", ReadFile(sourceDevice));
    return directory.Path() / "fixtures/SRAM-16T-ESSCIRC15.cell.yaml";
}

void TestTechnologyLoaderLoadsExactUpdatedAndLegacyNodes() {
    TemporaryDirectory directory("evacam-technology-loader-exact");
    const std::filesystem::path cellFile = WriteCellFixture(directory);
    const PeripheralConfig peripherals = EmptyPeripherals();

    TechnologyContext updated = TechnologyLoader::Load(
            MakeInput(RepositoryPath("config/lib/technology/cmos.updated.yaml"), cellFile),
            peripherals);
    assert(updated.tech != nullptr);
    assert(updated.fefetTech != nullptr);
    assert(updated.tech->featureSizeInNano() == 45);
    assert(updated.tech->deviceRoadmap() == HP);
    assert(updated.tech->useUpdatedLib());
    AssertNear(updated.tech->vdd(), 1.0);

    TechnologyContext legacy = TechnologyLoader::Load(
            MakeInput(RepositoryPath("config/lib/technology/cmos.legacy.yaml"), cellFile),
            peripherals);
    assert(legacy.tech != nullptr);
    assert(legacy.fefetTech != nullptr);
    assert(legacy.tech->featureSizeInNano() == 45);
    assert(!legacy.tech->useUpdatedLib());
    assert(!legacy.fefetTech->useUpdatedLib());
}

void TestTechnologyLoaderInterpolatesBetweenUpdatedNodes() {
    TemporaryDirectory directory("evacam-technology-loader-interpolation");
    const std::filesystem::path technologyFile =
            RepositoryPath("config/lib/technology/cmos.updated.yaml");
    const std::vector<TechnologySpec> specs = YamlHelpers::ReadTechnologySpecsFromYaml(
            technologyFile.string());
    const TechnologySpec &low = FindSpec(specs, 22, HP);
    const TechnologySpec &high = FindSpec(specs, 32, HP);

    TechnologyContext loaded = TechnologyLoader::Load(
            MakeInput(technologyFile, WriteCellFixture(directory), 28), EmptyPeripherals());
    assert(loaded.tech->featureSizeInNano() == 22);
    const double alpha = 0.6;
    AssertNear(loaded.tech->vdd(), (1.0 - alpha) * low.vdd + alpha * high.vdd);
    AssertNear(loaded.tech->vth(), (1.0 - alpha) * low.vth + alpha * high.vth);
    AssertNear(loaded.tech->currentOnNmos()[0],
            (1.0 - alpha) * low.currentOnNmos[0] + alpha * high.currentOnNmos[0]);
}

void TestTechnologyLoaderUsesLegacyBucketsAndLoadsCellRelativeToCellFile() {
    TemporaryDirectory directory("evacam-technology-loader-cell");
    const std::filesystem::path cellFile = WriteCellFixture(directory);
    InputConfig input = MakeInput(
            RepositoryPath("config/lib/technology/cmos.legacy.yaml"), cellFile, 40);
    input.hasCamWidthMatchTran = true;
    input.camWidthMatchTran = 2.5;

    TechnologyContext loaded = TechnologyLoader::Load(input, EmptyPeripherals());
    assert(loaded.tech->featureSizeInNano() == 32);
    assert(loaded.cell != nullptr);
    assert(loaded.cell->processNode == 28);
    assert(loaded.cell->camType == TCAM);
    AssertNear(loaded.cell->camWidthMatchTran, 2.5);
    TestSupport::AssertFiniteNonNegative(loaded.cell->readPower, "loaded cell read power");
}

void TestTechnologyLoaderRejectsUnavailableNodesAndRoadmaps() {
    TemporaryDirectory directory("evacam-technology-loader-errors");
    const std::filesystem::path cellFile = WriteCellFixture(directory);
    const std::filesystem::path technologyFile =
            RepositoryPath("config/lib/technology/cmos.updated.yaml");

    AssertThrows<std::runtime_error>([&] {
        (void)TechnologyLoader::Load(MakeInput(technologyFile, cellFile, 6), EmptyPeripherals());
    }, "requested roadmap/process_node");

    AssertThrows<std::runtime_error>([&] {
        (void)TechnologyLoader::Load(MakeInput(
                RepositoryPath("config/lib/technology/cmos.legacy.yaml"), cellFile, 201),
                EmptyPeripherals());
    }, "above 200nm");

    InputConfig missingRoadmap = MakeInput(technologyFile, cellFile);
    missingRoadmap.deviceRoadmap = LSTP;
    AssertThrows<std::runtime_error>([&] {
        (void)TechnologyLoader::Load(missingRoadmap, EmptyPeripherals());
    }, "requested roadmap/process_node");
}

MemCell BaseVariationCell() {
    MemCell cell;
    cell.withVariation = true;
    cell.variationMode = "single_point";
    cell.monteCarloGranularity = "cell";
    cell.variationSamples = 19;
    cell.variationLutFile = "variation.csv";
    cell.resistanceOnVariation = 0.05;
    cell.resistanceOffVariation = 0.07;
    return cell;
}

void TestVariationConfigBuilderBuildsNominalAndSinglePointConfigurations() {
    MemCell disabled = BaseVariationCell();
    disabled.withVariation = false;
    disabled.variationMode = "monte_carlo";
    VariationConfig nominal = VariationConfigBuilder::FromCell(disabled);
    assert(!nominal.enabled);
    assert(nominal.mode == "nominal");
    assert(nominal.samples == 1);

    VariationConfig singlePoint = VariationConfigBuilder::FromCell(BaseVariationCell());
    assert(singlePoint.enabled);
    assert(singlePoint.mode == "single_point");
    assert(singlePoint.samples == 1);
    AssertNear(singlePoint.memoryDeviceResOnStdev, 0.05);
    AssertNear(singlePoint.memoryDeviceResOffStdev, 0.07);
}

void TestVariationConfigBuilderBuildsMonteCarloCellAndEffectiveGranularity() {
    MemCell cell = BaseVariationCell();
    cell.variationMode = "monte_carlo";
    cell.variationSamples = 9;
    cell.monteCarloGranularity = "cell";
    VariationConfig perCell = VariationConfigBuilder::FromCell(cell);
    assert(perCell.mode == "monte_carlo");
    assert(perCell.samples == 9);
    assert(perCell.monteCarloGranularity == "cell");

    cell.monteCarloGranularity = "effective";
    VariationConfig effective = VariationConfigBuilder::FromCell(cell);
    assert(effective.monteCarloGranularity == "effective");
    assert(effective.samples == 9);
}

void TestVariationConfigBuilderDerivesCornerCountsAndSeeds() {
    MemCell oneDimension = BaseVariationCell();
    oneDimension.variationMode = "corner";
    oneDimension.resistanceOnMaxVariation = 0.1;
    VariationConfig oneCorner = VariationConfigBuilder::FromCell(oneDimension);
    assert(oneCorner.samples == 2);

    oneDimension.resistanceOffMaxVariation = 0.2;
    oneDimension.hasVariationSeed = true;
    oneDimension.variationSeed = 123456u;
    VariationConfig twoCorners = VariationConfigBuilder::FromCell(oneDimension);
    assert(twoCorners.samples == 4);
    assert(twoCorners.hasUserSeed);
    assert(twoCorners.seed == 123456u);

    oneDimension.hasVariationSeed = false;
    VariationConfig defaultSeed = VariationConfigBuilder::FromCell(oneDimension);
    assert(!defaultSeed.hasUserSeed);
    assert(defaultSeed.seed != 0u);
}

void TestVariationConfigBuilderRejectsInvalidValues() {
    MemCell cell = BaseVariationCell();
    cell.variationMode = "nominal";
    AssertThrows<std::runtime_error>([&] { (void)VariationConfigBuilder::FromCell(cell); },
            "cannot be 'nominal'");

    cell.variationMode = "monte_carlo";
    cell.variationSamples = 1;
    AssertThrows<std::runtime_error>([&] { (void)VariationConfigBuilder::FromCell(cell); },
            "samples must be > 1");

    cell.variationSamples = 3;
    cell.monteCarloGranularity = "array";
    AssertThrows<std::runtime_error>([&] { (void)VariationConfigBuilder::FromCell(cell); },
            "must be 'cell' or 'effective'");

    cell.variationMode = "unknown";
    cell.monteCarloGranularity = "cell";
    AssertThrows<std::runtime_error>([&] { (void)VariationConfigBuilder::FromCell(cell); },
            "must be 'single_point', 'monte_carlo', or 'corner'");

    cell.variationMode = "corner";
    AssertThrows<std::runtime_error>([&] { (void)VariationConfigBuilder::FromCell(cell); },
            "requires at least one positive");

    cell.resistanceOnMaxVariation = -0.01;
    AssertThrows<std::runtime_error>([&] { (void)VariationConfigBuilder::FromCell(cell); },
            "must be non-negative");

    cell.resistanceOnMaxVariation = 1.0;
    AssertThrows<std::runtime_error>([&] { (void)VariationConfigBuilder::FromCell(cell); },
            "keep low-corner resistances positive");
}

}  // namespace

int main() {
    TestTechnologyLoaderLoadsExactUpdatedAndLegacyNodes();
    TestTechnologyLoaderInterpolatesBetweenUpdatedNodes();
    TestTechnologyLoaderUsesLegacyBucketsAndLoadsCellRelativeToCellFile();
    TestTechnologyLoaderRejectsUnavailableNodesAndRoadmaps();
    TestVariationConfigBuilderBuildsNominalAndSinglePointConfigurations();
    TestVariationConfigBuilderBuildsMonteCarloCellAndEffectiveGranularity();
    TestVariationConfigBuilderDerivesCornerCountsAndSeeds();
    TestVariationConfigBuilderRejectsInvalidValues();
    return 0;
}
