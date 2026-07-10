#include "config/TechnologyLoader.h"

#include <memory>
#include <stdexcept>
#include <vector>

#include "MemCell.h"
#include "Technology.h"
#include "config/VariationConfigBuilder.h"
#include "input/TechnologyYamlLoader.h"

namespace {

const TechnologySpec *FindYamlSpec(
        const std::vector<TechnologySpec> &specs,
        int featureSizeInNano,
        DeviceRoadmap roadmap) {
    for (const TechnologySpec &spec : specs) {
        if (spec.featureSizeInNano == featureSizeInNano && spec.roadmap == roadmap) {
            return &spec;
        }
    }
    return nullptr;
}

int LegacyBucketNode(int featureSizeInNano) {
    if (featureSizeInNano >= 200) return 200;
    if (featureSizeInNano >= 120) return 120;
    if (featureSizeInNano >= 90) return 90;
    if (featureSizeInNano >= 65) return 65;
    if (featureSizeInNano >= 45) return 45;
    if (featureSizeInNano >= 32) return 32;
    if (featureSizeInNano >= 22) return 22;
    return -1;
}

const TechnologySpec *FindYamlBaseSpec(
        const std::vector<TechnologySpec> &specs,
        int featureSizeInNano,
        DeviceRoadmap roadmap) {
    if (const TechnologySpec *spec = FindYamlSpec(specs, featureSizeInNano, roadmap)) {
        return spec;
    }

    const int bucketNode = LegacyBucketNode(featureSizeInNano);
    if (bucketNode == -1) {
        return nullptr;
    }
    return FindYamlSpec(specs, bucketNode, roadmap);
}

std::shared_ptr<Technology> BuildTechFromSpec(const TechnologySpec &spec) {
    auto tech = std::make_shared<Technology>();
    tech->InitializeFromSpec(spec);
    return tech;
}

int HighInterpolationNode(int processNode) {
    if (processNode > 200) {
        throw std::runtime_error("[Error] Technology node above 200nm not supported.");
    }
    if (processNode > 120) return 200;
    if (processNode > 90) return 120;
    if (processNode > 65) return 90;
    if (processNode > 45) return 65;
    if (processNode > 32) return 45;
    if (processNode > 22) return 32;
    if (processNode > 14) return 22;
    if (processNode > 10) return 14;
    if (processNode > 7) return 10;
    if (processNode == 7) return 7;
    throw std::runtime_error("[Error]: Technology node below 7nm is not supported.");
}

double InterpolationAlpha(int processNode) {
    if (processNode > 120) return (processNode - 120.0) / 60;
    if (processNode > 90) return (processNode - 90.0) / 30;
    if (processNode > 65) return (processNode - 65.0) / 25;
    if (processNode > 45) return (processNode - 45.0) / 20;
    if (processNode > 32) return (processNode - 32.0) / 13;
    if (processNode > 22) return (processNode - 22.0) / 10;
    if (processNode > 14) return (processNode - 14.0) / 8;
    if (processNode > 10) return (processNode - 10.0) / 4;
    if (processNode > 7) return (processNode - 10.0) / 3;
    return 1;
}

std::shared_ptr<Technology> LoadTechFromYaml(
        const InputConfig &input,
        const std::vector<TechnologySpec> &specs) {
    const TechnologySpec *baseSpec = FindYamlBaseSpec(
            specs, input.processNode, input.deviceRoadmap);
    if (!baseSpec) {
        throw std::runtime_error("[Technology] Technology file does not provide requested roadmap/process_node.");
    }

    auto tech = BuildTechFromSpec(*baseSpec);
    const int highNode = HighInterpolationNode(input.processNode);
    const TechnologySpec *highSpec = FindYamlSpec(specs, highNode, input.deviceRoadmap);
    if (!highSpec) {
        throw std::runtime_error("[Technology] Technology file does not provide interpolation bound.");
    }
    Technology techHigh;
    techHigh.InitializeFromSpec(*highSpec);
    tech->InterpolateWith(techHigh, InterpolationAlpha(input.processNode));
    return tech;
}

std::shared_ptr<MemCell> LoadCell(const InputConfig &input, const std::shared_ptr<Technology> &tech) {
    auto cell = std::make_shared<MemCell>();
    cell->ReadCellFromFile(input.fileMemCell, input.designTarget, tech->vdd());
    if (input.hasCamWidthMatchTran) {
        cell->camWidthMatchTran = input.camWidthMatchTran;
    }
    cell->CalculateWriteEnergy();
    return cell;
}

std::shared_ptr<Technology> LoadFefetTech(
        const InputConfig &input,
        const std::vector<TechnologySpec> &yamlSpecs) {
    if (const TechnologySpec *spec = FindYamlBaseSpec(yamlSpecs, input.processNode, FEFET)) {
        return BuildTechFromSpec(*spec);
    }
    if (const TechnologySpec *spec = FindYamlBaseSpec(
                yamlSpecs, input.processNode, input.deviceRoadmap)) {
        return BuildTechFromSpec(*spec);
    }
    throw std::runtime_error("[Technology] Technology file does not provide FeFET fallback.");
}

}  // namespace

TechnologyContext TechnologyLoader::Load(
        const InputConfig &input,
        const PeripheralConfig &,
        VariationConfig *variation) {
    TechnologyContext technology;
    if (input.fileTechnology.empty()) {
        throw std::runtime_error("[Technology] v2 configs require a technology file.");
    }
    const std::vector<TechnologySpec> yamlSpecs =
            YamlHelpers::ReadTechnologySpecsFromYaml(input.fileTechnology);
    technology.tech = LoadTechFromYaml(input, yamlSpecs);
    technology.cell = LoadCell(input, technology.tech);
    technology.fefetTech = LoadFefetTech(input, yamlSpecs);
    if (variation) {
        *variation = VariationConfigBuilder::FromCell(*technology.cell);
    }
    return technology;
}
