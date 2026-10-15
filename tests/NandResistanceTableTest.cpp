#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "EvaCAM_Match.h"
#include "EvaCamConfig.h"
#include "EvaCamExplorer.h"
#include "EvaCamResultExtractor.h"
#include "Result.h"
#include "ResultsYaml.h"
#include "factories/NandCamFactory.h"
#include "model/NandResistanceTable.h"
#include "TestSupport.h"

namespace {
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

std::string Example(bool vertical) {
    const std::string name = vertical ? "NAND_3D_MLC_TCAM" : "NAND_MLC_TCAM";
    return "config/" + name + "/" + name + ".config.yaml";
}

std::shared_ptr<EvaCamConfig> Load(bool vertical) {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile(Example(vertical));
    return config;
}

NandDeviceSpec &Electrical(EvaCamConfig &config) {
    auto &cell = *config.technology.cell;
    return cell.memCellType == NAND3D ? cell.nand3d.electrical : cell.nand;
}

void TestTableContract() {
    const auto config = Load(false);
    const auto table = Electrical(*config).table;
    table.Validate(5, 5000);
    Require(table.BitsPerCell() == 2 && NandResistanceTable{}.BitsPerCell() == 1, "state count to bit count");
    AssertNear(table.Pair(1, 2, 5000).first, 8000);
    AssertNear(table.Pair(1, 2, 5000).second, 1e9);
    AssertNear(table.Pair(-1, 2, 5000).first, 6000);
    AssertNear(table.Pair(-1, 2, 5000).second, 8000);
    AssertNear(table.Pair(3, -1, 5000).first, 5000);
    AssertNear(table.RechargeVoltageSum(-1, 5), 0);
    AssertNear(table.RechargeVoltageSum(0, 5), 7);
    AssertNear(table.RechargeVoltageSum(1, 5), 7.2);
    Require(table.WorstEnergyQuery() == 1, "worst gate energy need not be query zero");
    for (const auto &change : std::vector<std::function<void(NandResistanceTable&)>>{
            [](auto &t) { t.resistance.pop_back(); },
            [](auto &t) { t.resistance[1].pop_back(); },
            [](auto &t) { t.queryVoltages.pop_back(); },
            [](auto &t) { t.queryVoltages[1] = t.queryVoltages[0]; },
            [](auto &t) { t.queryVoltages[0] = -1; },
            [](auto &t) { t.queryVoltages[3] = 5; },
            [](auto &t) { t.resistance[0][0] = 4999; },
            [](auto &t) { t.resistance[0][0] = std::numeric_limits<double>::infinity(); },
            [](auto &t) { t.queryVoltages[0] = std::numeric_limits<double>::quiet_NaN(); }}) {
        auto broken = table;
        change(broken);
        AssertThrows<std::invalid_argument>([&] { broken.Validate(5, 5000); }, "");
    }
    AssertThrows<std::invalid_argument>([&] { table.Validate(0, 5000); }, "");
    AssertThrows<std::invalid_argument>([&] { table.Pair(4, -1, 5000); }, "");
    AssertThrows<std::invalid_argument>([&] { table.Pair(0, -2, 5000); }, "");
    AssertThrows<std::invalid_argument>([&] { table.RechargeVoltageSum(4, 5); }, "");
    AssertThrows<std::invalid_argument>([] { NandResistanceTable{}.WorstEnergyQuery(); }, "");
    auto malformed = table;
    malformed.resistance.resize(3);
    AssertThrows<std::invalid_argument>([&] { malformed.BitsPerCell(); }, "");
    AssertThrows<std::invalid_argument>([&] { NandTableTimeBounds(table, 5000, {}, 0, 1, 1, 1); }, "");
    AssertThrows<std::invalid_argument>([&] {
        NandTableTimeBounds(table, 5000, {1, 1}, 0, 1, std::numeric_limits<double>::infinity(), 1);
    }, "");
}

void TestExhaustiveBoundsAndIndependentRc() {
    for (bool vertical : {false, true}) {
        for (bool arbitrary : {false, true}) {
            auto config = Load(vertical);
            auto &cell = *config->technology.cell;
            auto &device = Electrical(*config);
            // Two symbols keep exhaustive enumeration small. Every physical
            // capacitor and individual series resistor is included independently.
            if (arbitrary) {
                for (int s = 0; s < 4; ++s) {
                    for (int q = 0; q < 4; ++q) {
                        device.table.resistance[s][q] = 5000 + ((s * 11 + q * 7) % 13) * 10000;
                    }
                }
            }
            const int layers = 8, dummy = vertical ? 2 : 0;
            config->input.flashBlockSize = 64 * layers * 2;
            if (vertical) cell.nand3d.storageLayers = layers;
            Wire wire;
            wire.initialized = true;
            wire.capWirePerUnit = wire.resWirePerUnit = 0;
            auto model = CreateNandCamBackend(cell.memCellType);
            model->Initialize(config, 64, 2, 2, wire);
            const auto &metrics = model->Metrics();
            double maxMatchVoltage = 0, minNonmatchVoltage = std::numeric_limits<double>::infinity();
            double maxEnergy = 0;
            for (int s0 = -1; s0 < 4; ++s0) for (int s1 = -1; s1 < 4; ++s1) {
                for (int q0 = -1; q0 < 4; ++q0) for (int q1 = -1; q1 < 4; ++q1) {
                    for (bool valid : {false, true}) {
                        const std::vector<int> stored{s0, s1}, query{q0, q1};
                        const auto result = model->Evaluate(stored, query, valid);
                        bool hit = valid;
                        std::vector<double> resistance(layers + dummy, device.resistancePass);
                        resistance[dummy / 2] = valid ? device.resistanceReadOn : device.resistanceOff;
                        for (int symbol = 0; symbol < 2; ++symbol) {
                            const int s = stored[symbol], q = query[symbol];
                            if (s != -1 && q != -1 && s != q) hit = false;
                            if (q != -1) {
                                resistance[dummy / 2 + 2 + 2 * symbol] = device.table.resistance[s == -1 ? 0 : s][q];
                                resistance[dummy / 2 + 3 + 2 * symbol] = device.table.resistance[s == -1 ? 0 : 3 - s][3 - q];
                            }
                        }
                        double totalResistance = 2 * device.resistanceSelect;
                        double tau = device.capacitanceSource * device.resistanceSelect;
                        for (std::size_t i = 0; i < resistance.size(); ++i) {
                            totalResistance += resistance[i];
                            // Each resistor sees every downstream internal C and the BL.
                            tau += resistance[i] * (device.capacitanceBitline
                                    + (resistance.size() - i) * device.capacitanceInternal);
                        }
                        tau += device.resistanceSelect * (2 * device.capacitanceBitline
                                + resistance.size() * device.capacitanceInternal);
                        const double expectedVoltage = device.voltagePrecharge * std::exp(-device.decisionTime / tau);
                        Require(result.hit == hit, "logical multilevel exact/wildcard matching");
                        AssertNear(result.matchlineConductance, 1 / totalResistance);
                        AssertNear(result.matchlineVoltage, expectedVoltage, 1e-15);
                        if (hit) maxMatchVoltage = std::max(maxMatchVoltage, expectedVoltage);
                        else minNonmatchVoltage = std::min(minNonmatchVoltage, expectedVoltage);
                        Require(result.senseMargin + 1e-12 >= metrics.senseMargin, "global bounds cover every pattern and invalid entry");
                        maxEnergy = std::max(maxEnergy, result.searchDynamicEnergy);
                    }
                }
            }
            AssertNear(metrics.matchVoltage, maxMatchVoltage, 1e-14);
            AssertNear(metrics.mismatchVoltage, minNonmatchVoltage, 1e-14);
            AssertNear(metrics.searchEnergy, maxEnergy);
            AssertThrows<std::invalid_argument>([&] { model->Evaluate({0}, {0}); }, "keyWidth");
            AssertThrows<std::invalid_argument>([&] { model->Evaluate({0, 4}, {0, -1}); }, "state index");
            // Independent supply-charge and voltage-update ledger, including a mask.
            const auto mask = model->Evaluate({0, 0}, {-1, -1});
            const auto active = model->Evaluate({1, 0}, {1, -1});
            const int repeats = vertical ? metrics.searchRounds : metrics.searchRounds - 1;
            const double gateCap = 64 * device.capacitanceGate;
            const double charging = gateCap * repeats * device.voltagePass * 7.2 / device.supplyEfficiency;
            const double driver = 2 * (vertical ? 2 * metrics.searchRounds : 2 * metrics.searchRounds - 1)
                    * device.wordlineDriver.energy;
            AssertNear(active.searchDynamicEnergy - mask.searchDynamicEnergy, charging + driver, 1e-25);
        }
    }
}

struct YamlFixture {
    TestSupport::TemporaryDirectory directory{"nand-table"};
    YAML::Node root, architecture, cell, device;
    bool vertical;

    explicit YamlFixture(bool value) : vertical(value) {
        const auto path = std::filesystem::path(Example(value));
        root = YAML::LoadFile(path.string());
        architecture = YAML::LoadFile((path.parent_path() / root["architecture"].as<std::string>()).string());
        cell = YAML::LoadFile((path.parent_path() / root["cell"].as<std::string>()).string());
        device = YAML::LoadFile((path.parent_path() / cell["memory_device"].as<std::string>()).string());
        root["technology"] = std::filesystem::absolute("config/lib/technology/cmos.legacy.yaml").string();
        root["architecture"] = "architecture.yaml";
        root["cell"] = "cell.yaml";
        cell["memory_device"] = "device.yaml";
    }

    std::string Write() {
        for (const auto &item : std::vector<std::pair<std::string, YAML::Node>>{
                {"config.yaml", root}, {"architecture.yaml", architecture}, {"cell.yaml", cell}, {"device.yaml", device}}) {
            YAML::Emitter output;
            output << item.second;
            directory.WriteFile(item.first, output.c_str());
        }
        return (directory.Path() / "config.yaml").string();
    }
};

void TestYamlAndGeometryRejections() {
    for (bool vertical : {false, true}) {
        const std::string section = vertical ? "nand3d" : "nand";
        for (const auto &change : std::vector<std::function<void(YamlFixture&)>>{
                [&](auto &f) { f.device[section].remove("query_voltages"); },
                [&](auto &f) { f.device[section].remove("resistance_table"); },
                [&](auto &f) { f.device[section]["resistance_table"] = YAML::Load("[]"); },
                [&](auto &f) { f.device[section]["resistance_table"][0] = "bad"; },
                [&](auto &f) { f.device[section]["resistance_table"][0][0] = "1V"; },
                [&](auto &f) { f.device[section]["resistance_table"][0][0] = "0ohm"; },
                [&](auto &f) { f.device[section]["query_voltages"][0] = "-1V"; },
                [&](auto &f) { f.device[section]["resistance_table_typo"] = "bad"; },
                [&](auto &f) { f.architecture["memory"]["word_width"] = "31bits"; },
                [&](auto &f) { f.architecture["memory"]["capacity"] = "128B"; },
                [&](auto &f) { f.architecture["organization"]["subarray"]["dimensions"][1] = 32; },
                [&](auto &f) { f.architecture["flash"]["block_size"] = "584B"; },
                [&](auto &f) {
                    if (vertical) f.device[section]["storage_mode"] = "SLC";
                    else f.device["type"] = "SLCNAND";
                }}) {
            YamlFixture fixture(vertical);
            change(fixture);
            const auto path = fixture.Write();
            AssertThrows<std::exception>([&] { EvaCamConfig config; config.ReadConfigFromFile(path); }, "");
        }
    }
}

void TestEndToEndAndOutput() {
    for (bool vertical : {false, true}) for (bool tree : {false, true}) {
        YamlFixture fixture(vertical);
        fixture.architecture["routing"]["type"] = tree ? "H-tree" : "NonH-tree";
        // More than one block exercises symbol-width routing and logical capacity.
        fixture.architecture["organization"]["banks"]["total"] = YAML::Load("[1, 2]");
        fixture.architecture["memory"]["capacity"] = "512B";
        const auto path = fixture.Write();
        EvaCAM_Match matcher(path);
        Require(matcher.word_width() == 32 && matcher.storage_width_bits() == 32
                && matcher.bits_per_symbol() == 2 && matcher.symbol_width() == 16,
                "API separates logical bit width from vector symbol count");
        std::vector<int> stored(16, 3), query(16, 3);
        Require(matcher.evaluate_vector(stored, query).hit, "public vector API accepts multilevel symbols");
        const std::vector<int> ones(32, 1);
        Require(matcher.evaluate_bits(ones, ones).hit, "binary input packs multilevel symbols MSB-first");
        auto differentBits = ones;
        differentBits.back() = 0;
        Require(!matcher.evaluate_bits(ones, differentBits).hit, "packed multilevel mismatch");
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_bits({1}, {1}); }, "storage width");
        query.back() = 2;
        Require(!matcher.evaluate_nand(stored, query).hit, "multilevel inequality is a mismatch");
        query.back() = -1;
        Require(matcher.evaluate_nand(stored, query).hit, "query mask");
        Require(!matcher.evaluate_nand(stored, query, false).hit, "binary validity marker");
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_distance(stored, query); }, "");
        auto config = std::make_shared<EvaCamConfig>();
        config->ReadConfigFromFile(path);
        EvaCamExplorer explorer(config, 1);
        const auto run = explorer.Run();
        Require(run.numSolution > 0, "table example produces a feasible design");
        bool checked = false;
        for (const auto &result : run.bestResults) {
            if (!result || !result->bank || !result->bank->initialized) continue;
            const auto dto = ExtractEvaCamDesignResult(*result);
            AssertNear(dto.geometry.at("logical_capacity_bits"), 4096);
            AssertNear(dto.geometry.at("allocated_capacity_bits"), 4096);
            AssertNear(dto.geometry.at("logical_word_width_bits"), 32);
            AssertNear(dto.geometry.at("symbols_per_key"), 16);
            AssertNear(dto.geometry.at("physical_cell_count"), 2 * 64 * 36);
            AssertNear(dto.geometry.at("physical_block_bits"), 64 * 36 * 2);
            Require(dto.metadata.at("cell_response") == "stored_state_by_query_voltage_resistance_table", "response provenance");
            std::ostringstream output;
            WriteResultsYaml(output, *result);
            const auto yaml = YAML::Load(output.str());
            Require(yaml["geometry"]["bits_per_cell"].as<int>() == 2, "YAML multilevel geometry");
            Require(yaml["assumptions"]["nand"]["bits_per_cell"].as<int>() == 2, "YAML assumptions");
            checked = true;
            break;
        }
        Require(checked, "checked a complete structured result");
    }
}

void TestDistanceIntegration() {
    for (bool vertical : {false, true}) for (bool tree : {false, true}) {
        YamlFixture fixture(vertical);
        fixture.architecture["design"]["search_function"] = "TH";
        fixture.architecture["routing"]["type"] = tree ? "H-tree" : "NonH-tree";
        fixture.architecture["organization"]["banks"]["total"] = YAML::Load("[1, 2]");
        fixture.architecture["memory"]["capacity"] = "512B";
        EvaCAM_Match matcher(fixture.Write());
        std::vector<int> query(16, 0), stored(query);
        stored[0] = 1;
        const auto value = matcher.evaluate_distance(stored, query);
        AssertNear(value.squaredEuclideanDistance, 1);
        const auto exact = matcher.evaluate_nand(stored, query);
        AssertNear(value.searchDynamicEnergy, exact.searchDynamicEnergy);
        AssertNear(value.searchLatency, exact.searchLatency);
        auto config = std::make_shared<EvaCamConfig>();
        config->ReadConfigFromFile(fixture.Write());
        const auto run = EvaCamExplorer(config, 1).Run();
        bool checked = false;
        for (const auto &design : run.bestResults) {
            if (!design || !design->bank || !design->bank->initialized) continue;
            const auto dto = ExtractEvaCamDesignResult(*design);
            Require(dto.metadata.at("sense_margin_scope") == "exact_search_diagnostic_only", "TH aggregate margin scope");
            Require(dto.metadata.at("threshold_feasibility") == "requires_explicit_query_and_threshold_evaluation", "TH aggregate is not a threshold certification");
            AssertNear(dto.summary.at("timing.sense_margin_enforced"), 0);
            std::ostringstream output;
            WriteResultsYaml(output, *design);
            const auto yaml = YAML::Load(output.str());
            Require(yaml["assumptions"]["nand"]["threshold_feasibility"].as<std::string>()
                    == "requires_explicit_query_and_threshold_evaluation", "TH YAML scope");
            checked = true;
            break;
        }
        Require(checked, "TH aggregate result available");
        const auto bounds = matcher.distance_voltage_bounds(query, false);
        Require(bounds.size() > 16, "distance classes include multilevel deltas");
        for (const auto &bound : bounds) {
            const auto low = matcher.evaluate_distance(bound.minimumVoltageStored, bound.minimumVoltageQuery);
            const auto high = matcher.evaluate_distance(bound.maximumVoltageStored, bound.maximumVoltageQuery);
            AssertNear(low.matchlineVoltage, bound.minimumVoltage, 1e-12);
            AssertNear(high.matchlineVoltage, bound.maximumVoltage, 1e-12);
            Require(low.matchlineConductance <= bound.maximumConductance * (1+1e-12)
                    && low.matchlineConductance >= bound.minimumConductance * (1-1e-12), "voltage witness within DC bounds");
            AssertNear(low.searchLatency, bound.minimumSearchLatency);
        }
        Require(!matcher.evaluate_distance_threshold(stored, query, 0).hit, "threshold zero excludes mismatch");
        const auto threshold = matcher.evaluate_distance_threshold(stored, query, 1);
        Require(threshold.hit && threshold.senseMarginApplicable, "logical threshold and electrical diagnostic");
        const auto invalid = matcher.evaluate_nand(query, query, false);
        const auto all = matcher.evaluate_distance_threshold(stored, query, 144);
        AssertNear(all.rejectedMinimumVoltage, invalid.matchlineVoltage, 1e-12);
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_distance_threshold(stored, query, -1); }, "threshold");
        // TH must retain tables with overlapping classes for study.
        const std::string section = vertical ? "nand3d" : "nand";
        fixture.device[section]["resistance_table"] = YAML::Load(
                "[[10000,10000,10000,10000],[10000,10000,10000,10000],[10000,10000,10000,10000],[10000,10000,10000,10000]]");
        EvaCAM_Match overlapping(fixture.Write());
        const auto overlap = overlapping.evaluate_distance_threshold(stored, query, 1);
        Require(!overlap.senseMarginPass && overlap.senseMargin < 0, "negative margins retained");
        fixture.architecture["sensing"]["strict_sense_margin"] = true;
        EvaCAM_Match strict(fixture.Write());
        AssertThrows<std::runtime_error>([&] { strict.evaluate_distance_threshold(stored, query, 1); }, "sense margin");
    }
}

void TestOtherStateCountsAndInfeasibility() {
    for (bool vertical : {false, true}) for (int bits : {1, 3, 4}) {
        YamlFixture fixture(vertical);
        const int states = 1 << bits;
        const std::string section = vertical ? "nand3d" : "nand";
        if (vertical) fixture.device[section]["storage_mode"] = bits == 1 ? "SLC" : bits == 3 ? "TLC" : "QLC";
        else fixture.device["type"] = bits == 1 ? "SLCNAND" : "MLCNAND";
        YAML::Node table(YAML::NodeType::Sequence), voltages(YAML::NodeType::Sequence);
        for (int s = 0; s < states; ++s) {
            YAML::Node row(YAML::NodeType::Sequence);
            for (int q = 0; q < states; ++q) row.push_back(s > q ? "1Gohm" : "10kohm");
            table.push_back(row);
            voltages.push_back(0.1 + 4.0 * s / states);
        }
        fixture.device[section]["resistance_table"] = table;
        fixture.device[section]["query_voltages"] = voltages;
        fixture.architecture["memory"]["word_width"] = std::to_string(16 * bits) + "bits";
        fixture.architecture["memory"]["capacity"] = std::to_string(128 * bits) + "B";
        fixture.architecture["flash"]["block_size"] = std::to_string(288 * bits) + "B";
        EvaCAM_Match matcher(fixture.Write());
        Require(matcher.bits_per_symbol() == static_cast<size_t>(bits), "2/8/16-state table integration");
        std::vector<int> query(16, states - 1);
        Require(matcher.evaluate_vector(query, query).hit, "highest symbol fits without binary truncation");
        auto stored = query;
        stored[0] = 0;
        AssertNear(matcher.evaluate_distance(stored, query).squaredEuclideanDistance,
                (states-1)*(states-1));
        const auto bounds = matcher.distance_voltage_bounds(query, false);
        AssertNear(bounds.back().squaredEuclideanDistance, 16*(states-1)*(states-1));
    }
    // A positive table need not separate equality electrically; reject the
    // resulting design instead of fabricating a logical/electrical agreement.
    YamlFixture fixture(true);
    fixture.device["nand3d"]["resistance_table"] = YAML::Load(
            "[[10000,10000,10000,10000],[10000,10000,10000,10000],[10000,10000,10000,10000],[10000,10000,10000,10000]]");
    AssertThrows<std::runtime_error>([&] { EvaCAM_Match matcher(fixture.Write()); }, "invalid for matching");
}
}  // namespace

int main() {
    TestTableContract();
    TestExhaustiveBoundsAndIndependentRc();
    TestYamlAndGeometryRejections();
    TestEndToEndAndOutput();
    TestDistanceIntegration();
    TestOtherStateCountsAndInfeasibility();
}
