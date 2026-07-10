#include "input/TechnologyYamlLoader.h"

#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

#include "input/YamlNodeHelpers.h"
#include "input/YamlUnitParsers.h"

namespace {

using YamlHelpers::child_required;
using YamlHelpers::child_required_index;
using YamlHelpers::LengthUnits;
using YamlHelpers::read_enum_required;
using YamlHelpers::read_quantity_required;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kBuildInPotential = 0.9;
constexpr double kCapJunctionBottom = 1e-3;
constexpr double kCapJunctionSidewall = 2.5e-10;
constexpr double kCapDrainToChannelEdge = 0.5e-10;
constexpr double kCapJunctionGrade = 0.5;
constexpr double kCapSidewallGrade = 0.33;
constexpr double kCapDrainToChannelGrade = 0.33;
constexpr double kSiliconSaturationVelocity = 1e5;
constexpr std::size_t kCurrentTemperatureGridSize = 11;

const std::vector<YamlHelpers::UnitSpec>& CapacitancePerLengthUnits() {
    static const std::vector<YamlHelpers::UnitSpec> k = {
        {"F/m", 1.0},
        {"fF/m", 1e-15},
        {"pF/m", 1e-12},
    };
    return k;
}

double ComputeJunctionCap(double vdd) {
    return kCapJunctionBottom / std::pow(1 + vdd / kBuildInPotential, kCapJunctionGrade);
}

double ComputeSidewallCap(double vdd) {
    return kCapJunctionSidewall / std::pow(1 + vdd / kBuildInPotential, kCapSidewallGrade);
}

double ComputeDrainToChannelCap(double vdd) {
    return kCapDrainToChannelEdge / std::pow(1 + vdd / kBuildInPotential, kCapDrainToChannelGrade);
}

double ComputeVdsat(double phyGateLength, double mobility) {
    if (mobility == 0.0) {
        return kInf;
    }
    return phyGateLength * kSiliconSaturationVelocity / mobility;
}

std::array<double, 11> ReadTable(const YAML::Node& node, const char* what,
        std::size_t expectedSize) {
    if (!node.IsSequence() || node.size() != expectedSize) {
        throw std::runtime_error(std::string("Invalid table length for ") + what);
    }
    std::array<double, 11> values{};
    for (std::size_t i = 0; i < expectedSize; ++i) {
        values[i] = YamlHelpers::read_scalar_required<double>(
                child_required_index(node, i, what), what);
    }
    return values;
}

bool ReadUseUpdatedLib(const YAML::Node& root) {
    const std::string libraryModel = YamlHelpers::read_required<std::string>(
            root, "library_model");
    if (libraryModel == "updated") {
        return true;
    }
    if (libraryModel == "legacy") {
        return false;
    }
    throw std::runtime_error("unsupported technology.library_model: " + libraryModel);
}

DeviceRoadmap ReadRoadmapKey(const YAML::Node& node) {
    const std::string raw = YamlHelpers::read_scalar_required<std::string>(
            node, "technology.roadmaps key");
    for (const auto& entry : YamlHelpers::EnumTraits<DeviceRoadmap>::mapping()) {
        if (raw == entry.first) {
            return entry.second;
        }
    }
    throw std::runtime_error("Invalid technology.roadmaps key: " + raw);
}

std::size_t ReadTemperatureGridSize(const YAML::Node& root) {
    const YAML::Node temperatureGrid = child_required(root, "temperature_grid");
    if (!temperatureGrid.IsSequence()) {
        throw std::runtime_error("technology.temperature_grid must be a sequence");
    }
    if (temperatureGrid.size() != kCurrentTemperatureGridSize) {
        throw std::runtime_error(
                "technology.temperature_grid must match current 300K-400K table size");
    }
    return temperatureGrid.size();
}

TechnologySpec BuildSpec(const YAML::Node& node, bool useUpdatedLib,
        DeviceRoadmap expectedRoadmap, std::size_t temperatureGridSize) {
    TechnologySpec spec{};
    spec.featureSizeInNano = static_cast<int>(std::lround(
            read_quantity_required(node, "process_node", LengthUnits(), 1.0,
                    "technology.process_node") / 1e-9));
    spec.roadmap = read_enum_required<DeviceRoadmap>(node, "roadmap");
    if (spec.roadmap != expectedRoadmap) {
        throw std::runtime_error("technology node roadmap does not match containing roadmap");
    }
    spec.useUpdatedLib = useUpdatedLib;
    spec.featureSize = spec.featureSizeInNano * 1e-9;

    const YAML::Node operatingPoint = child_required(node, "operating_point");
    spec.vdd = read_quantity_required(
            operatingPoint, "vdd", YamlHelpers::VoltageUnits(), 1.0, "technology.vdd");
    spec.vth = read_quantity_required(
            operatingPoint, "vth", YamlHelpers::VoltageUnits(), 1.0, "technology.vth");
    spec.phyGateLength = read_quantity_required(
            operatingPoint, "physical_gate_length", LengthUnits(), 1.0,
            "technology.physical_gate_length");

    const YAML::Node capacitance = child_required(node, "capacitance");
    spec.capIdealGate = read_quantity_required(
            capacitance, "ideal_gate", CapacitancePerLengthUnits(), 1.0,
            "technology.capacitance.ideal_gate");
    spec.capFringe = read_quantity_required(
            capacitance, "fringe", CapacitancePerLengthUnits(), 1.0,
            "technology.capacitance.fringe");
    spec.capOx = read_quantity_required(
            capacitance, "oxide", CapacitancePerLengthUnits(), 1.0,
            "technology.capacitance.oxide");
    spec.capJunction = ComputeJunctionCap(spec.vdd);
    spec.capOverlap = spec.capIdealGate * 0.2;
    spec.capSidewall = ComputeSidewallCap(spec.vdd);
    spec.capDrainToChannel = ComputeDrainToChannelCap(spec.vdd);
    spec.buildInPotential = kBuildInPotential;

    const YAML::Node mobility = child_required(node, "mobility");
    spec.effectiveElectronMobility = YamlHelpers::read_required<double>(mobility, "electron");
    spec.effectiveHoleMobility = YamlHelpers::read_required<double>(mobility, "hole");
    spec.vdsatNmos = ComputeVdsat(spec.phyGateLength, spec.effectiveElectronMobility);
    spec.vdsatPmos = ComputeVdsat(spec.phyGateLength, spec.effectiveHoleMobility);

    const YAML::Node sizing = child_required(node, "sizing");
    spec.pnSizeRatio = YamlHelpers::read_required<double>(sizing, "pn_size_ratio");
    spec.effectiveResistanceMultiplier =
            YamlHelpers::read_required<double>(sizing, "effective_resistance_multiplier");

    const YAML::Node gm = child_required(node, "gm");
    spec.currentGmNmos = YamlHelpers::read_required<double>(gm, "nmos");
    spec.currentGmPmos = YamlHelpers::read_required<double>(gm, "pmos");

    const YAML::Node fin = child_required(node, "fin");
    spec.heightFin = read_quantity_required(
            fin, "height", LengthUnits(), 1.0, "technology.fin.height");
    spec.widthFin = read_quantity_required(
            fin, "width", LengthUnits(), 1.0, "technology.fin.width");
    spec.PitchFin = read_quantity_required(
            fin, "pitch", LengthUnits(), 1.0, "technology.fin.pitch");
    spec.capPolywire = read_quantity_required(
            fin, "polywire_cap", CapacitancePerLengthUnits(), 1.0,
            "technology.fin.polywire_cap");

    const YAML::Node currents = child_required(node, "currents");
    spec.currentOnNmos = ReadTable(
            child_required(currents, "on_nmos"), "technology.currents.on_nmos",
            temperatureGridSize);
    spec.currentOnPmos = ReadTable(
            child_required(currents, "on_pmos"), "technology.currents.on_pmos",
            temperatureGridSize);
    spec.currentOffNmos = ReadTable(
            child_required(currents, "off_nmos"), "technology.currents.off_nmos",
            temperatureGridSize);
    spec.currentOffPmos = ReadTable(
            child_required(currents, "off_pmos"), "technology.currents.off_pmos",
            temperatureGridSize);

    return spec;
}

}  // namespace

namespace YamlHelpers {

std::vector<TechnologySpec> ReadTechnologySpecsFromYaml(const std::string& inputFile) {
    const YAML::Node root = YAML::LoadFile(inputFile);
    require_schema(root, "technology", "technology config");
    const std::size_t temperatureGridSize = ReadTemperatureGridSize(root);
    const bool useUpdatedLib = ReadUseUpdatedLib(root);
    const YAML::Node roadmaps = child_required(root, "roadmaps");
    if (!roadmaps.IsMap()) {
        throw std::runtime_error("technology.roadmaps must be a mapping");
    }

    std::vector<TechnologySpec> specs;
    for (const auto& entry : roadmaps) {
        const DeviceRoadmap roadmap = ReadRoadmapKey(entry.first);
        const YAML::Node nodes = child_required(entry.second, "nodes");
        if (!nodes.IsSequence()) {
            throw std::runtime_error("technology roadmap nodes must be a sequence");
        }
        std::set<int> seenNodes;
        for (const auto& node : nodes) {
            TechnologySpec spec = BuildSpec(
                    node, useUpdatedLib, roadmap, temperatureGridSize);
            if (!seenNodes.insert(spec.featureSizeInNano).second) {
                throw std::runtime_error("duplicate technology process_node entry");
            }
            specs.push_back(spec);
        }
    }
    return specs;
}

}  // namespace YamlHelpers
