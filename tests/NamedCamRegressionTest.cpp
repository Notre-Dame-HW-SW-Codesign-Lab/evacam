#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include "CAM_SubArray.h"
#include "EvaCamConfig.h"
#include "EvaCamExplorer.h"
#include "config/ExplorationSpaceResolver.h"
#include "Mat.h"
#include "Result.h"
#include "TestSupport.h"
#include "input/MemoryDeviceYamlLoader.h"
#include "input/PhysicalDomainValidators.h"
#include "formula.h"
#include "constant.h"
#include "output/ResultsYaml.h"

namespace {

using TestSupport::AssertNear;
using TestSupport::Require;
using TestSupport::AssertThrows;

// A complete v2 reference chain in a temporary directory, including the
// independently resolved sensing and amplifier paths.
struct Fixture {
    TestSupport::TemporaryDirectory directory{"paper-model"};
    YAML::Node top, architecture, cell, device, sensing;

    explicit Fixture(const std::string &name) {
        const auto source = std::filesystem::absolute("config/" + name);
        top = YAML::LoadFile((source / (name + ".config.yaml")).string());
        architecture = YAML::LoadFile((source / top["architecture"].as<std::string>()).string());
        cell = YAML::LoadFile((source / top["cell"].as<std::string>()).string());
        device = YAML::LoadFile((source / cell["memory_device"].as<std::string>()).string());
        sensing = YAML::LoadFile((source / architecture["sensing"].as<std::string>()).string());
        sensing["sense_amplifier"] = (source / sensing["sense_amplifier"].as<std::string>()).string();
        top["technology"] = (source / top["technology"].as<std::string>()).string();
        top["architecture"] = "architecture.yaml";
        top["cell"] = "cell.yaml";
        architecture["sensing"] = "sensing.yaml";
        cell["memory_device"] = "device.yaml";
    }

    std::shared_ptr<EvaCamConfig> Load() {
        directory.WriteFile("architecture.yaml", YAML::Dump(architecture));
        directory.WriteFile("cell.yaml", YAML::Dump(cell));
        directory.WriteFile("device.yaml", YAML::Dump(device));
        directory.WriteFile("sensing.yaml", YAML::Dump(sensing));
        auto config = std::make_shared<EvaCamConfig>();
        config->ReadConfigFromFile(directory.WriteFile("run.yaml", YAML::Dump(top)).string());
        config->logger.SetOutputEnabled(false);
        return config;
    }
};

std::shared_ptr<EvaCamConfig> Load(const std::string &name) {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/" + name + "/" + name + ".config.yaml");
    config->logger.SetOutputEnabled(false);
    return config;
}

std::shared_ptr<Result> Run(const std::shared_ptr<EvaCamConfig> &config) {
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);
    const auto result = EvaCamExplorer(config, 1).Run();
    Require(result.numSolution > 0, "named CAM regression must have a valid candidate");
    return result.bestResults.at(read_latency_optimized);
}

void TestRestoredCurrentSensing() {
    for (const char *name : {"ReRAM-4T2R-VLSIC14", "ReRAM-3T1R-ISSCC15",
            "ReRAM-2.5T1R-ISSCC16"}) {
        auto currentConfig = Load(name);
        assert(currentConfig->peripherals.typeSenseAmp == nvsim_current_sense);
        const auto current = Run(currentConfig);
        auto voltageConfig = Load(name);
        voltageConfig->peripherals.typeSenseAmp = nvsim_voltage_sense;
        const auto voltage = Run(voltageConfig);
        Require(current->bank->mat->subarray->senseAmp->readLatency
                    > voltage->bank->mat->subarray->senseAmp->readLatency,
                "current sensing must include converter latency");
        Require(current->bank->mat->subarray->senseAmp->readDynamicEnergy
                    > voltage->bank->mat->subarray->senseAmp->readDynamicEnergy,
                "current sensing must include converter energy");
    }
}

void TestRectangularImplicitAndExplicitGeometry() {
    auto implicit = Load("ReRAM-2.5T1R-ISSCC16");
    auto explicitConfig = Load("ReRAM-2.5T1R-ISSCC16");
    explicitConfig->runtimeSizing.hasFixedSubarrayDimensions = true;
    explicitConfig->runtimeSizing.fixedSubarrayRows = 64;
    explicitConfig->runtimeSizing.fixedSubarrayColumns = 256;
    const auto first = Run(implicit);
    const auto second = Run(explicitConfig);
    const auto &subarray = *first->bank->mat->subarray;
    assert(subarray.ConfiguredRows() == 64);
    assert(subarray.ConfiguredColumns() == 256);
    assert(subarray.Col[subarray.indexMatchline].numCell == 256);
    AssertNear(first->bank->area, second->bank->area);
    AssertNear(first->bank->searchLatency, second->bank->searchLatency);
    AssertNear(first->bank->searchDynamicEnergy, second->bank->searchDynamicEnergy);
}

void TestPhysicalCapacityAndSerialSchedule() {
    auto config = Load("MRAM-2T2R-ASPDAC12");
    assert(config->wordGeometry.entryCount == 128);
    assert(config->wordGeometry.allocatedCapacityBits == 128 * 72);
    const auto result = Run(config);
    const auto &bank = *result->bank;
    assert(bank.CAM_opt.ComparisonColumns == 8);
    assert(bank.mat->subarray->ConfiguredRows() == 128);
    assert(bank.mat->subarray->ConfiguredColumns() == 72);
    AssertNear(bank.searchLatency, bank.mat->subarray->searchLatency * 9);

    auto larger = Load("MRAM-2T2R-ASPDAC12");
    larger->runtimeSizing.realCapacity *= 2;
    larger->ResolveWordGeometry(1);
    const auto doubled = Run(larger);
    assert(doubled->bank->mat->subarray->ConfiguredRows() == 256);
    assert(doubled->bank->mat->subarray->ConfiguredColumns() == 72);
}

void TestMigratedPartitionSettings() {
    for (const char *name : {"PCM-2T2R-JSSC11", "SRAM-16T-ESSCIRC15"}) {
        const auto config = Load(name);
        const auto result = Run(config);
        const auto &bank = *result->bank;
        const auto &subarray = *bank.mat->subarray;
        const long long cells = subarray.ConfiguredRows() * subarray.ConfiguredColumns()
            * bank.numRowMat * bank.numColumnMat * bank.numRowSubarray * bank.numColumnSubarray;
        assert(cells == config->wordGeometry.allocatedCapacityBits);
        assert(subarray.CAM_opt.ComparisonColumns == subarray.ConfiguredColumns());
    }
    const auto partitioned = Run(Load("ReRAM-3T1R-ISSCC15"));
    const auto &subarray = *partitioned->bank->mat->subarray;
    assert(subarray.CAM_opt.ComparisonColumns == subarray.ConfiguredColumns());

    auto serial = Load("ReRAM-3T1R-ISSCC15");
    serial->runtimeSizing.hasExplicitComparisonColumns = true;
    serial->exploration.cam.bitSerialWidth = IntValueDomain::FixedSet({16});
    serial->peripherals.withOutputAcc = true;
    const auto serialResult = Run(serial);
    assert(serialResult->bank->CAM_opt.ComparisonColumns == 16);
    assert(serialResult->bank->mat->subarray->CAM_opt.ComparisonColumns == 8);
}

void TestPaperReferenceFixtures() {
    const auto manifest = YAML::LoadFile("docs/validation/named-cam.reference.yaml");
    assert(manifest["cases"].size() == 7);
    for (const auto &fixture : manifest["cases"]) {
        const auto expected = fixture["expected"];
        auto config = std::make_shared<EvaCamConfig>();
        config->ReadConfigFromFile(fixture["config"].as<std::string>());
        config->logger.SetOutputEnabled(false);
        assert(config->input.optimizationTarget == search_latency_optimized);
        const int node = expected["node_nm"].as<int>();
        assert(config->input.processNode == node);
        assert(config->technology.cell->processNode == node);
        AssertNear(config->technology.tech->featureSize() * 1e9, node);
        assert(config->wordGeometry.entryCount == expected["entries"].as<int>());
        assert(config->wordGeometry.physicalColumnsPerWord == expected["word_bits"].as<int>());
        const auto sensing = expected["sensing_mode"].as<std::string>();
        assert(config->peripherals.typeSenseAmp == (sensing == "nvsim_cur"
                ? nvsim_current_sense : nvsim_voltage_sense));
        const std::string timingModel = expected["matchline_timing_model"]
                ? expected["matchline_timing_model"].as<std::string>() : "horowitz_50_percent";
        assert(config->peripherals.decisionMode == (timingModel == "analytical_differential"
                ? CamDecisionMode::Differential : timingModel == "analytical_voltage_threshold"
                ? CamDecisionMode::FixedThreshold : CamDecisionMode::LegacyHorowitz));
        assert(config->peripherals.explicitSearchTiming == static_cast<bool>(expected["search_timing"]));
        if (expected["cell_area_um2"]) {
            const double featureUm = config->technology.tech->featureSize() * 1e6;
            AssertNear(config->technology.cell->area * featureUm * featureUm,
                    expected["cell_area_um2"].as<double>(), 0, 1e-10);
        }
        if (fixture["reference"]["supply_v"]) {
            AssertNear(config->technology.tech->vdd(),
                    fixture["reference"]["supply_v"].as<double>());
        }
        const auto result = Run(config);
        const auto &bank = *result->bank;
        const auto &subarray = *bank.mat->subarray;
        assert(subarray.ConfiguredRows() == expected["subarray"][0].as<int>());
        assert(subarray.ConfiguredColumns() == expected["subarray"][1].as<int>());
        const int subarrayCount = bank.numRowMat * bank.numColumnMat
                * bank.numRowSubarray * bank.numColumnSubarray;
        assert(subarrayCount == expected["subarray_count"].as<int>());
        assert(subarray.ConfiguredRows() * subarray.ConfiguredColumns() * subarrayCount
                == config->wordGeometry.allocatedCapacityBits);
        assert(subarray.CAM_opt.ComparisonColumns == subarray.ConfiguredColumns());
        Require(std::isfinite(bank.searchLatency) && bank.searchLatency > 0,
                "paper reference must produce a finite positive search latency");
        // Geometry and physical contracts are regression requirements. A paper
        // headline is not a golden output for an incomplete circuit reconstruction.
        assert(fixture["status"].as<std::string>() == "partial");
        assert(fixture["assumptions"].size() > 0);
    }
}

std::shared_ptr<EvaCamConfig> DischargeVariant(TestSupport::TemporaryDirectory &directory,
        bool deviceFlag, bool portFlag, int resistanceScale) {
    const std::string name = "MRAM-4T2R-VLSIC12";
    const auto source = std::filesystem::absolute("config/" + name);
    YAML::Node device = YAML::LoadFile((source / (name + ".memory_device.yaml")).string());
    device["match"]["is_nvm_discharge"] = deviceFlag;
    device["resistance"]["on"] = std::to_string(1000 * resistanceScale) + "ohm";
    device["resistance"]["off"] = std::to_string(100000 * resistanceScale) + "ohm";
    const auto devicePath = directory.WriteFile("device.yaml", YAML::Dump(device));
    MemCell direct = *Load(name)->technology.cell;
    YamlHelpers::ReadMemoryDeviceFromYaml(direct, devicePath.string());
    assert(direct.isNVMdischarge == deviceFlag);
    YAML::Node cell = YAML::LoadFile((source / (name + ".cell.yaml")).string());
    cell["memory_device"] = devicePath.string();
    cell["ports"]["column"][0]["is_nvm_discharge"] = portFlag;
    const auto cellPath = directory.WriteFile("cell.yaml", YAML::Dump(cell));
    YAML::Node top = YAML::LoadFile((source / (name + ".config.yaml")).string());
    top["architecture"] = (source / (name + ".architecture.yaml")).string();
    top["cell"] = cellPath.string();
    top["technology"] = std::filesystem::absolute("config/lib/technology/cmos.legacy.yaml").string();
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile(directory.WriteFile("config.yaml", YAML::Dump(top)).string());
    assert(config->technology.cell->isNVMdischarge == deviceFlag);
    config->logger.SetOutputEnabled(false);
    config->input.capacity = 256;
    config->input.wordWidth = 32;
    config->exploration.cam.bitSerialWidth = IntValueDomain::FixedSet({32});
    config->technology.cell->minSenseVoltage = 1e-6;
    config->ResolveWordGeometry(1);
    return config;
}

void TestDischargeFlagsReachMatchlineResistance() {
    TestSupport::TemporaryDirectory directory("named-cam-discharge");
    const auto base = Run(DischargeVariant(directory, false, false, 1));
    const auto isolated = Run(DischargeVariant(directory, false, false, 100));
    AssertNear(base->bank->mat->subarray->resMemCellOn,
            isolated->bank->mat->subarray->resMemCellOn);
    const auto device = Run(DischargeVariant(directory, true, false, 1));
    const auto port = Run(DischargeVariant(directory, false, true, 1));
    const auto high = Run(DischargeVariant(directory, true, false, 100));
    AssertNear(device->bank->mat->subarray->resMemCellOn,
            port->bank->mat->subarray->resMemCellOn);
    Require(device->bank->mat->subarray->resMemCellOn > base->bank->mat->subarray->resMemCellOn,
            "NVM participation must add memory resistance");
    Require(high->bank->mat->subarray->matchlineDelay > device->bank->mat->subarray->matchlineDelay,
            "NVM resistance must affect discharge delay when enabled");
}

void TestScalarSensingThroughV2() {
    Fixture fixture("ReRAM-3T1R-ISSCC15");
    auto scalar = YAML::Load("schema: sense_amp\nname: measured-amplifier\nmodel: scalar\n"
            "geometry: {area: 2um^2}\ntiming: {latency: 40ps}\n"
            "power: {read_dynamic_energy: 7fJ, leakage: 0W}\n"
            "load: {capacitance: 1fF}\n");
    fixture.sensing["sense_amplifier"] = "amplifiers/characterized.yaml";
    fixture.sensing.remove("sensing_mode");
    fixture.directory.WriteFile("amplifiers/characterized.yaml", YAML::Dump(scalar));
    const auto config = fixture.Load();
    assert(config->peripherals.customSenseAmp);
    assert(config->peripherals.fileSenseAmp.empty());
    assert(config->peripherals.typeSenseAmp == nvsim_voltage_sense);
    assert(std::filesystem::exists(config->peripherals.fileCustomSA));
    const auto result = Run(config);
    const auto &sub = *result->bank->mat->subarray;
    const auto &amp = *sub.senseAmp;
    AssertNear(amp.readLatency, 40e-12);
    AssertNear(amp.readDynamicEnergy, 7e-15 * sub.numSenseAmp);
    AssertNear(amp.leakage, 0);
    AssertNear(amp.area, 2e-12 * sub.numSenseAmp);
    AssertNear(amp.capLoad, 1e-15);
    std::ostringstream serialized;
    WriteResultsYaml(serialized, *result);
    const auto output = YAML::Load(serialized.str());
    assert(output["assumptions"]["modeling_options"]["sense_amplifier_model"].as<std::string>() == "scalar");

    scalar["timing"]["latency"] = "140ps";
    fixture.directory.WriteFile("amplifiers/characterized.yaml", YAML::Dump(scalar));
    const auto slower = Run(fixture.Load());
    AssertNear(slower->bank->mat->subarray->matchlineDelay, sub.matchlineDelay);
    AssertNear(slower->bank->searchLatency - result->bank->searchLatency, 100e-12);

    scalar["load"]["capacitance"] = "10fF";
    fixture.directory.WriteFile("amplifiers/characterized.yaml", YAML::Dump(scalar));
    const auto loaded = Run(fixture.Load());
    Require(loaded->bank->mat->subarray->matchlineDelay > sub.matchlineDelay,
            "characterized amplifier capacitance must load the matchline");

    scalar["timing"].remove("latency");
    fixture.directory.WriteFile("amplifiers/characterized.yaml", YAML::Dump(scalar));
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "latency");
    scalar["model"] = "unknown";
    fixture.directory.WriteFile("amplifiers/characterized.yaml", YAML::Dump(scalar));
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "sense");
}

void TestGateTopologyAndTiming() {
    Fixture fixture("FeFET-2Fe1T-DATE-2021");
    const auto config = fixture.Load();
    assert(config->technology.cell->fefetGate);
    const auto result = Run(config);
    const auto &sub = *result->bank->mat->subarray;
    const auto &tech = *config->technology.tech;
    AssertNear(sub.resMemCellOn, CalculateOnResistance(
            config->technology.cell->camPort[1][sub.indexMatchline].widthCmos * tech.featureSize(),
            NMOS, config->input.temperature, tech));
    AssertNear(sub.resMatchTran, 0);
    Require(sub.gateNodeDelay > 0 && sub.gateNodeCapacitance > 0, "separate gate node required");
    Require(sub.gateNodeMatchVoltage < .5 && sub.gateNodeMismatchVoltage > .5,
            "control voltage must distinguish a match");
    std::ostringstream serialized;
    WriteResultsYaml(serialized, *result);
    const auto output = YAML::Load(serialized.str());
    assert(output["assumptions"]["cell_topology"].as<std::string>() == "fefet_gate");
    assert(output["assumptions"]["fefet_gate"]["calibration_status"].as<std::string>() == "uncharacterized");
    assert(output["assumptions"]["modeling_options"]["matchline_timing_model"].as<std::string>() == "horowitz_50_percent");
    AssertNear(sub.cellReadEnergy,
            (sub.gateNodeChargingEnergy + sub.gateNodeStaticPower
                * (sub.gateNodeDelay + sub.matchlineDelay + sub.senseAmpLatency))
            * sub.CAM_opt.BitSerialWidth * sub.numColumn / sub.muxSenseAmp);

    fixture.device["resistance"]["on"] = "100000ohm";
    const auto slower = Run(fixture.Load());
    const auto &slowerSub = *slower->bank->mat->subarray;
    AssertNear(sub.resMemCellOn, slowerSub.resMemCellOn);
    Require(slowerSub.gateNodeDelay > sub.gateNodeDelay, "FeFET resistance must affect control delay");
    Require(slower->bank->searchLatency > result->bank->searchLatency, "control delay must reach bank timing");

    fixture.cell["gate_node"]["additional_capacitance"] = "2fF";
    const auto loaded = Run(fixture.Load());
    Require(loaded->bank->mat->subarray->gateNodeDelay > slowerSub.gateNodeDelay,
            "extra control-node capacitance must affect delay");

    // Both bank implementations' historical single-sense scope includes gate delay.
    for (auto routing : {h_tree, non_h_tree}) {
        auto scoped = fixture.Load();
        scoped->input.routingMode = routing;
        scoped->peripherals.noPrechargeInc = true;
        const auto scopedResult = Run(scoped);
        const auto &scopedSub = *scopedResult->bank->mat->subarray;
        const double evaluation = scopedSub.gateNodeDelay + scopedSub.matchlineDelay
                + scopedSub.ColMux[scopedSub.indexMatchline]->readLatency
                + scopedSub.senseAmpLatency + scopedSub.outputAcc->readLatency;
        Require(scopedResult->bank->searchLatency >= evaluation, "bank must include control delay");
    }

    // Plain two-device cells retain direct matchline discharge.
    auto direct = Load("2FeFET_TCAM");
    const auto directResult = Run(direct);
    assert(!direct->technology.cell->fefetGate);
    AssertNear(directResult->bank->mat->subarray->gateNodeDelay, 0);
}

void TestGateTopologyRejectsUnsupportedInputs() {
    Fixture fixture("FeFET-2Fe1T-DATE-2021");
    fixture.cell["gate_node"]["additional_capacitance"] = "-1fF";
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "additional_capacitance");
    fixture.cell["gate_node"].remove("additional_capacitance");
    fixture.device["read"]["power"] = "1uW";
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "control-node energy");
    fixture.device["read"]["power"] = "0W";
    fixture.cell["ports"]["column"][0]["is_nvm_discharge"] = true;
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "without NVM discharge");
    fixture.cell["ports"]["column"][0]["is_nvm_discharge"] = false;
    fixture.cell["ports"]["row"][0]["is_nmos"] = false;
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "searchline ports");
    fixture.cell["ports"]["row"][0]["is_nmos"] = true;
    auto config = fixture.Load();
    config->technology.cell->withVariation = true;
    AssertThrows<std::runtime_error>([&] {
        PhysicalDomainValidators::ValidateMemCell(*config->technology.cell);
    }, "variation");
    fixture.cell.remove("topology");
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "requires topology");
    fixture.cell["topology"] = "fefet_gate";
    fixture.cell.remove("gate_node");
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "gate_node");
}

void TestGateWordlengthAndTemperatureSweep() {
    for (int temperature : {300, 350, 400}) {
        double lastDelay = 0;
        for (int bits : {16, 32, 64}) {
            Fixture fixture("FeFET-2Fe1T-DATE-2021");
            fixture.architecture["design"]["temperature"] = std::to_string(temperature) + "K";
            fixture.architecture["memory"]["word_width"] = std::to_string(bits) + "bits";
            fixture.architecture["memory"]["capacity"] = std::to_string(64 * bits / 8) + "B";
            const auto result = Run(fixture.Load());
            const auto &sub = *result->bank->mat->subarray;
            assert(sub.ConfiguredRows() == 64 && sub.ConfiguredColumns() == bits);
            Require(sub.matchlineDelay > lastDelay, "longer matchline must take longer at each temperature");
            lastDelay = sub.matchlineDelay;
            Require(sub.gateNodeDelay > 0 && std::isfinite(result->bank->searchDynamicEnergy),
                    "sweep must retain a finite control-node and bank solution");
        }
    }
}

void TestAnalyticalDecisionsAndMatcher() {
    for (const char *name : {"FeFET-2Fe1T-DATE-2021", "ReRAM-3T1R-ISSCC15"}) {
        Fixture fixture(name);
        const auto legacy = Run(fixture.Load());
        fixture.sensing["decision"]["model"] = "differential";
        const auto config = fixture.Load();
        assert(config->peripherals.decisionMode == CamDecisionMode::Differential);
        const auto result = Run(config);
        const auto &sub = *result->bank->mat->subarray;
        Require(sub.matchlineDelay < legacy->bank->mat->subarray->matchlineDelay,
                "small-swing decision must precede the legacy half-swing event in this fixture");
        AssertNear(sub.senseMargin, sub.senseVoltage);
        AssertNear(sub.TcamSensedVoltage(0) - sub.TcamSensedVoltage(1), sub.senseVoltage, 1e-15);
        assert(sub.EvaluateBinaryMatchByMismatches(0).hit);
        assert(!sub.EvaluateBinaryMatchByMismatches(1).hit);
        assert(!sub.EvaluateBinaryMatchByMismatches(sub.CAM_opt.BitSerialWidth).hit);
        AssertNear(sub.EvaluateBinaryMatchByMismatches(0).searchLatency,
                sub.EvaluateBinaryMatchByMismatches(sub.CAM_opt.BitSerialWidth).searchLatency);
        // Raising the actual required separation must delay the decision.
        fixture.device["read"]["min_sense_voltage"] = "100mV";
        const auto largerMargin = Run(fixture.Load());
        Require(largerMargin->bank->mat->subarray->matchlineDelay > sub.matchlineDelay,
                "analytical decision must respond to sensing margin");
        fixture.sensing["decision"]["model"] = "voltage_threshold";
        fixture.sensing["decision"]["threshold"] = "0.5V";
        const auto threshold = Run(fixture.Load());
        AssertNear(threshold->bank->mat->subarray->TcamSensedVoltage(1), .5);
        std::ostringstream yaml;
        WriteResultsYaml(yaml, *threshold);
        const auto diagnostics = YAML::Load(yaml.str());
        assert(diagnostics["summary"]["timing"]["decision"]["time"]);
        assert(diagnostics["summary"]["timing"]["decision"]["activation_time"]);
        assert(diagnostics["summary"]["timing"]["decision"]["match_voltage"]);
        assert(diagnostics["summary"]["timing"]["decision"]["one_miss_voltage"]);
        assert(diagnostics["assumptions"]["modeling_options"]["matchline_timing_model"]
                .as<std::string>() == "analytical_voltage_threshold");
    }
}

void TestExplicitSearchSchedule() {
    for (auto routing : {h_tree, non_h_tree}) {
        Fixture fixture("FeFET-2Fe1T-DATE-2021");
        fixture.sensing["decision"]["model"] = "voltage_threshold";
        fixture.sensing["decision"]["threshold"] = "0.5V";
        fixture.architecture["search_timing"] = YAML::Load(
                "{control: broadcast, precharge: overlap_input, driver_load: physical_line, recovery: 0ps}");
        fixture.architecture["organization"]["comparison_columns_per_step"] = 16;
        fixture.architecture["organization"]["mux"]["sense_amp"] = 2;
        fixture.architecture["peripherals"]["output"]["accumulator"] = true;
        auto config = fixture.Load();
        config->input.routingMode = routing;
        assert(config->peripherals.searchBroadcast && config->peripherals.usePhysicalDriverLoad);
        const auto result = Run(config);
        const auto &sub = *result->bank->mat->subarray;
        assert(sub.CAM_opt.ComparisonColumns == 16 && sub.muxSenseAmp == 2);
        Require(result->bank->searchLatency >= sub.ScheduledSearchLatency(2, 4),
                "bank must contain all comparison operations and any routing delay");
        if (routing == h_tree) AssertNear(result->bank->searchLatency, sub.ScheduledSearchLatency(2, 4));
        AssertThrows<std::invalid_argument>([&] { sub.ScheduledSearchLatency(0, 1); }, "positive");
        AssertNear(sub.searchLatency, sub.searchPhases.resultReady);
        Require(sub.searchPhases.evaluationStart >= sub.precharger->readLatency,
                "evaluation must wait for precharge");
        fixture.architecture["search_timing"]["recovery"] = "50ps";
        auto recoveryConfig = fixture.Load();
        recoveryConfig->input.routingMode = routing;
        const auto recovered = Run(recoveryConfig);
        const auto &recoveredSub = *recovered->bank->mat->subarray;
        AssertNear(recovered->bank->searchLatency - result->bank->searchLatency, 7 * 50e-12);
        AssertNear(recoveredSub.searchPhases.cycleTime - recoveredSub.searchLatency, 50e-12);
        std::ostringstream output;
        WriteResultsYaml(output, *recovered);
        const auto yaml = YAML::Load(output.str());
        assert(yaml["summary"]["timing"]["search_cycle_time"]);
        assert(yaml["assumptions"]["search_timing"]["control"].as<std::string>() == "broadcast");
        fixture.architecture["search_timing"]["precharge"] = "serial";
        const auto serial = Run(fixture.Load());
        Require(serial->bank->mat->subarray->searchLatency > recoveredSub.searchLatency,
                "serial precharge must not overlap query preparation");
        fixture.architecture["search_timing"]["control"] = "decoded";
        const auto decoded = Run(fixture.Load());
        Require(decoded->bank->mat->subarray->searchPhases.queryReady
                > serial->bank->mat->subarray->searchPhases.queryReady,
                "decoded control must include address decoding");
        // Reusing a latched query cannot remove hidden input time from a
        // precharge-dominated critical path (two 300 ps operations).
        auto &scheduled = *result->bank->mat->subarray;
        scheduled.inputBuf->readLatency = 40e-12;
        scheduled.precharger->readLatency = 100e-12;
        scheduled.searchPhases.queryReady = 50e-12;
        scheduled.searchPhases.evaluationStart = 100e-12;
        scheduled.searchLatency = 300e-12;
        AssertNear(scheduled.ScheduledSearchLatency(2, 1), 600e-12);
    }
}

void TestAnalyticalConfigValidation() {
    Fixture fixture("FeFET-2Fe1T-DATE-2021");
    fixture.sensing["decision"] = YAML::Load("{model: voltage_threshold, threshold: 0V}");
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "threshold");
    fixture.sensing["decision"] = YAML::Load("{model: differential, threshold: 0.5V}");
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "requires voltage_threshold");
    fixture.sensing["decision"] = YAML::Load("{model: invalid}");
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "decision.model");
    fixture.sensing["decision"] = YAML::Load("{model: voltage_threshold, threshold: 2V}");
    AssertThrows<std::runtime_error>([&] { Run(fixture.Load()); }, "precharge");
    fixture.sensing["decision"] = YAML::Load("{model: differential}");
    fixture.top["modeling"]["exclude_precharge_latency"] = true;
    AssertThrows<std::runtime_error>([&] { Run(fixture.Load()); }, "exclude_precharge_latency");
    fixture.top.remove("modeling");
    auto variation = fixture.Load();
    variation->variation.enabled = true;
    AssertThrows<std::runtime_error>([&] { Run(variation); }, "nominal exact");
    for (const char *field : {"control", "precharge", "driver_load"}) {
        fixture.architecture["search_timing"] = YAML::Load(
                "{control: broadcast, precharge: serial, driver_load: physical_line}");
        fixture.architecture["search_timing"][field] = "invalid";
        AssertThrows<std::runtime_error>([&] { fixture.Load(); }, std::string("search_timing.") + field);
    }
    fixture.architecture["search_timing"] = YAML::Load(
            "{control: decoded, precharge: serial, driver_load: physical_line, recovery: -1ps}");
    AssertThrows<std::runtime_error>([&] { fixture.Load(); }, "recovery");
}

}  // namespace

int main() {
    TestRestoredCurrentSensing();
    TestRectangularImplicitAndExplicitGeometry();
    TestPhysicalCapacityAndSerialSchedule();
    TestMigratedPartitionSettings();
    TestPaperReferenceFixtures();
    TestDischargeFlagsReachMatchlineResistance();
    TestScalarSensingThroughV2();
    TestGateTopologyAndTiming();
    TestGateTopologyRejectsUnsupportedInputs();
    TestGateWordlengthAndTemperatureSweep();
    TestAnalyticalDecisionsAndMatcher();
    TestExplicitSearchSchedule();
    TestAnalyticalConfigValidation();
    std::cout << "Named CAM regression tests passed\n";
}
