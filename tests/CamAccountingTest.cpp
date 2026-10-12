#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>

#include <yaml-cpp/yaml.h>

#include "CAM_SubArray.h"
#include "BankWithoutHtree.h"
#include "EvaCamConfig.h"
#include "EvaCamExplorer.h"
#include "EvaCamResultExtractor.h"
#include "Result.h"
#include "TestSupport.h"
#include "config/ExplorationSpaceResolver.h"
#include "output/ResultsYaml.h"

using TestSupport::AssertNear;
using TestSupport::Require;

struct CamSubArrayTestAccessor {
    static int SearchRowDriverIndex(const CAM_SubArray &sub) { return sub.SearchRowDriverIndex(); }
    static double SearchRamp(const CAM_SubArray &sub) { return sub.MatchlineInputRamp(); }
};

namespace {
std::shared_ptr<EvaCamConfig> Load(const std::string &name) {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/" + name + "/" + name.substr(name.find_last_of('/') + 1) + ".config.yaml");
    config->logger.SetOutputEnabled(false);
    return config;
}
std::shared_ptr<Result> Run(const std::shared_ptr<EvaCamConfig> &config) {
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);
    const auto results = EvaCamExplorer(config, 1).Run();
    Require(results.numSolution > 0, "accounting fixture must be feasible");
    return results.bestResults.at(config->input.optimizationTarget);
}
double Sum(const std::map<std::string, double> &values) {
    double total = 0;
    for (const auto &component : values) {
        Require(std::isfinite(component.second) && component.second >= 0,
                "component budget must be finite and nonnegative: " + component.first);
        total += component.second;
    }
    return total;
}

void TestAdditiveBudgetsAndSerialization() {
    for (const char *name : {"paper_reference/FeFET-2Fe1T-DATE21", "paper_reference/MRAM-6T2R-VLSIC11",
            "original_validation/MRAM-4T2R-VLSIC12", "original_validation/FeFET-2Fe-TCAS19",
            "paper_reference/ReRAM-3T1R-ISSCC15", "2FeFET_TCAM", "8T-BCAM_65nm"}) {
        const auto result = Run(Load(name));
        auto &sub = *result->bank->mat->subarray;
        AssertNear(Sum(sub.areaBreakdown), sub.area);
        AssertNear(Sum(sub.searchEnergyBreakdown), sub.searchDynamicEnergy);
        AssertNear(Sum(sub.leakageBreakdown), sub.leakage);
        AssertNear(Sum(sub.searchLatencyBreakdown), sub.searchLatency);
        AssertNear(sub.width * sub.height, sub.area);
        Require(sub.width >= sub.lenRow && sub.height >= sub.lenCol,
                "rectangular layout must contain the physical array");
        std::ostringstream stream;
        WriteResultsYaml(stream, *result);
        const auto yaml = YAML::Load(stream.str());
        const auto dto = ExtractEvaCamDesignResult(*result);
        for (const auto &entry : sub.searchEnergyBreakdown)
            AssertNear(dto.breakdown.at("search_dynamic_energy." + entry.first + "_j"), entry.second);
        for (const auto &entry : sub.searchLatencyBreakdown)
            AssertNear(dto.breakdown.at("search_critical_path." + entry.first + "_s"), entry.second);
        // Every additive component must appear in the public result, including
        // zero-cost disabled blocks; no second hand-written inventory can drift.
        for (const auto &entry : sub.areaBreakdown)
            assert(yaml["breakdown"]["subarray_area"][entry.first]);
        for (const auto &entry : sub.searchEnergyBreakdown)
            assert(yaml["breakdown"]["search_dynamic_energy"][entry.first]);
        for (const auto &entry : sub.searchLatencyBreakdown)
            assert(yaml["breakdown"]["search_critical_path"][entry.first]);
        const double area = sub.area, energy = sub.searchDynamicEnergy, leakage = sub.leakage;
        sub.CalculateArea();
        sub.CalculatePower();
        AssertNear(area, sub.area);
        AssertNear(energy, sub.searchDynamicEnergy);
        AssertNear(leakage, sub.leakage);
    }
}

void TestReplicationAndIndependentAreaPerturbations() {
    const auto result = Run(Load("original_validation/MRAM-4T2R-VLSIC12"));
    auto &sub = *result->bank->mat->subarray;
    assert(sub.numRow != sub.numColumn);
    const double delta = 1e-12;
    double before = sub.area;
    sub.RowDriver[0]->area += delta;
    sub.CalculateArea();
    AssertNear(sub.area - before, delta); // RowDecoder already owns its copies.
    before = sub.area;
    sub.inputLS->area += delta;
    sub.CalculateArea();
    AssertNear(sub.area - before, delta * sub.numRow);
    before = sub.area;
    sub.outputLS->area += delta;
    sub.CalculateArea();
    AssertNear(sub.area - before, delta * sub.numColumn / sub.muxSenseAmp);
    before = sub.area;
    for (auto &driver : sub.WriteDriver) {
        if (driver && driver->initialized) {
            driver->area += delta;
            driver->outputDriver.area += 100 * delta; // Included inside aggregate already.
            break;
        }
    }
    sub.CalculateArea();
    AssertNear(sub.area - before, delta);
    // All write-driver instances, including consecutive bitlines, count once.
    double driverTotal = 0;
    for (const auto &driver : sub.WriteDriver)
        if (driver && driver->initialized) driverTotal += driver->area;
    AssertNear(sub.WriteDriverArea, driverTotal);
}

void TestParallelSearchEnergyAndOptionalPeripherals() {
    auto config = Load("paper_reference/FeFET-2Fe1T-DATE21");
    config->peripherals.withOutputBuffer = true;
    config->peripherals.withOutputAcc = true;
    const auto result = Run(config);
    auto &sub = *result->bank->mat->subarray;
    // Complementary, equiprobable search inputs charge one line per bit.
    const double expectedDriverEnergy = (sub.RowDriver[0]->readDynamicEnergy
            + sub.RowDriver[1]->readDynamicEnergy) * .5 * sub.CAM_opt.BitSerialWidth;
    AssertNear(sub.searchlineDriveDynamicEnergy, expectedDriverEnergy);
    const double outputs = sub.numColumn / sub.muxSenseAmp;
    AssertNear(sub.searchEnergyBreakdown.at("output_buffer"), sub.outputBuf->readDynamicEnergy * outputs);
    AssertNear(sub.searchEnergyBreakdown.at("output_accumulator"), sub.outputAcc->readDynamicEnergy * outputs);
    AssertNear(sub.leakageBreakdown.at("input_buffer"), sub.inputBuf->leakage * sub.numRow);
    AssertNear(sub.searchInputDynamicEnergy, sub.inputBuf->readDynamicEnergy * sub.CAM_opt.BitSerialWidth);
    const double fullEnergy = sub.searchlineDriveDynamicEnergy;
    sub.CAM_opt.BitSerialWidth /= 2;
    sub.CalculatePower();
    AssertNear(sub.searchlineDriveDynamicEnergy, fullEnergy / 2);
    AssertNear(Sum(sub.searchEnergyBreakdown), sub.searchDynamicEnergy);
}

void TestFeFetCapacitanceEnergyOwnership() {
    for (const char *name : {"original_validation/FeFET-2Fe-TCAS19",
            "paper_reference/FeFET-2Fe1T-DATE21"}) {
        const auto config = Load(name);
        const auto result = Run(config);
        auto &sub = *result->bank->mat->subarray;
        Require(sub.senseAmp->normalSenseAmp->model.model == "analytical_inverter",
                "FeFET reference must use its explicit inverter");
        const double vdd = config->technology.tech->vdd();
        const double outputs = sub.numColumn / sub.muxSenseAmp;
        const double mlCap = sub.Col[sub.indexMatchline].wireCap
                + sub.capCellAccess * sub.numRow
                + sub.ColMux[sub.indexMatchline]->capForPreviousDelayCalculation
                + config->peripherals.addCapOnML + sub.precharger->capOutputBitlinePrecharger
                + sub.senseAmp->capLoad;
        AssertNear(sub.searchEnergyBreakdown.at("matchline"), mlCap * vdd * sub.voltagePrecharge * outputs);
        // Perturb capacitor ownership independently: an inverter input is paid
        // once by ML recharge, while a driver output is paid once by the driver.
        const double delta = 1e-15;
        const double beforeMl = sub.searchEnergyBreakdown.at("matchline");
        const double beforeSa = sub.searchEnergyBreakdown.at("sense_amplifier");
        sub.senseAmp->capLoad += delta;
        sub.CalculatePower();
        AssertNear(sub.searchEnergyBreakdown.at("matchline") - beforeMl,
                delta * vdd * sub.voltagePrecharge * outputs);
        AssertNear(sub.searchEnergyBreakdown.at("sense_amplifier"), beforeSa);
        const double beforeDriver = sub.searchEnergyBreakdown.at("row_driver");
        const double beforeTotal = sub.searchDynamicEnergy;
        sub.RowDriver[0]->outputDriver.outputCap += delta;
        sub.CalculatePower();
        const double driverIncrease = .5 * delta * vdd * vdd * sub.CAM_opt.BitSerialWidth;
        AssertNear(sub.searchEnergyBreakdown.at("row_driver") - beforeDriver, driverIncrease);
        AssertNear(sub.searchDynamicEnergy - beforeTotal, driverIncrease);
        std::ostringstream stream;
        WriteResultsYaml(stream, *result);
        const auto assumptions = YAML::Load(stream.str())["assumptions"];
        assert(assumptions["search_energy"]["query_history"].as<std::string>() == "not_modeled");
        assert(assumptions["search_energy"]["workload_validation"].as<std::string>() == "unmatched");
        if (config->technology.cell->fefetGate) {
            AssertNear(assumptions["fefet_gate"]["pull_down_width_f"].as<double>(), 1);
            assert(assumptions["modeling_options"]["decision_reference"].as<std::string>() == "analytical_inverter_trip");
        }
    }
}

void TestCachedInputsAcrossSenseGroups() {
    for (auto route : {h_tree, non_h_tree}) {
        auto config = Load("paper_reference/FeFET-2Fe1T-DATE21");
        config->input.routingMode = route;
        config->exploration.geometry.muxSenseAmp = IntValueDomain::FixedSet({2});
        const auto result = Run(config);
        const auto &bank = *result->bank;
        const auto &sub = *bank.mat->subarray;
        assert(sub.muxSenseAmp == 2);
        assert(bank.numRowMat * bank.numColumnMat == 1);
        assert(bank.numRowSubarray * bank.numColumnSubarray == 1);
        // Two sense groups share one complete query-buffer/encoder update.
        // Non-H-tree includes the single mat's incoming wire; H-tree has zero levels.
        double routeEnergy = 0;
        if (route == non_h_tree) {
            Wire wire = bank.globalWire;
            double latency = 0, leakage = 0;
            wire.CalculateLatencyAndPower(bank.mat->height, &latency, &routeEnergy, &leakage);
            const auto &routed = dynamic_cast<const BankWithoutHtree &>(bank);
            routeEnergy *= routed.numAddressBitRouteToMat + routed.numDataBitRouteToMat;
        }
        AssertNear(bank.searchDynamicEnergy - routeEnergy,
                2 * sub.searchDynamicEnergy - sub.searchInputDynamicEnergy);
        const auto dto = ExtractEvaCamDesignResult(*result);
        AssertNear(dto.breakdown.at("search_dynamic_energy.mat_j"),
                2 * sub.searchDynamicEnergy - sub.searchInputDynamicEnergy);
        AssertNear(dto.breakdown.at("search_latency.mat_s"), sub.ScheduledSearchLatency(2, 1));
        Require(sub.searchInputDynamicEnergy > sub.inputBuf->readDynamicEnergy,
                "cached input energy must include every compared bit");
        Require(sub.searchEnergyBreakdown.at("column_mux") > 0,
                "enabled column muxes must calculate their switching energy");
    }
}

void TestIdleWriteLinesCannotDelaySearch() {
    const auto result = Run(Load("paper_reference/MRAM-6T2R-VLSIC11"));
    auto &sub = *result->bank->mat->subarray;
    assert(CamSubArrayTestAccessor::SearchRowDriverIndex(sub) < 2);
    const double ramp = CamSubArrayTestAccessor::SearchRamp(sub);
    sub.RowDriver[2]->readLatency = 1;
    sub.RowDriver[2]->rampOutput = 1;
    assert(CamSubArrayTestAccessor::SearchRowDriverIndex(sub) < 2);
    AssertNear(CamSubArrayTestAccessor::SearchRamp(sub), ramp);
    sub.config->technology.cell->camPort[0][2].volSearch1 = 1.2;
    assert(CamSubArrayTestAccessor::SearchRowDriverIndex(sub) == 2);
    AssertNear(CamSubArrayTestAccessor::SearchRamp(sub), 1);
    for (int i = 0; i < 4; ++i) sub.config->technology.cell->camPort[0][i].volSearch0 = sub.config->technology.cell->camPort[0][i].volSearch1 = 0;
    assert(CamSubArrayTestAccessor::SearchRowDriverIndex(sub) == -1);
    AssertNear(CamSubArrayTestAccessor::SearchRamp(sub), 1e20);
}
}
int main() {
    TestAdditiveBudgetsAndSerialization();
    TestReplicationAndIndependentAreaPerturbations();
    TestParallelSearchEnergyAndOptionalPeripherals();
    TestFeFetCapacitanceEnergyOwnership();
    TestIdleWriteLinesCannotDelaySearch();
    TestCachedInputsAcrossSenseGroups();
    std::cout << "CAM accounting tests passed\n";
}
