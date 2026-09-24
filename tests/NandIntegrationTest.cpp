#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "EvaCAM_Match.h"
#include "EvaCamConfig.h"
#include "EvaCamExplorer.h"
#include "EvaCamRun.h"
#include "NandCamBank.h"
#include "Result.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertThrows;
using TestSupport::Require;

void Close(double actual, double expected) {
    Require(std::abs(actual - expected) <= std::max(std::abs(expected), 1e-30) * 1e-10,
            "NAND integration metrics agree");
}

std::string Fixture(TestSupport::TemporaryDirectory &directory, const std::string &name,
        bool tree, bool multiple, bool parallel, int mux = 1) {
    auto architecture = YAML::LoadFile("config/NAND_TCAM/NAND_TCAM.architecture.yaml");
    architecture["routing"]["type"] = tree ? "H-tree" : "NonH-tree";
    architecture["organization"]["mux"]["sense_amp"] = mux;
    if (multiple) {
        architecture["organization"]["banks"]["total"] = YAML::Load("[2, 2]");
        architecture["organization"]["mats"]["total"] = YAML::Load("[2, 1]");
        architecture["organization"]["banks"]["active"] = YAML::Load(parallel ? "[2, 2]" : "[1, 1]");
        architecture["organization"]["mats"]["active"] = YAML::Load(parallel ? "[2, 1]" : "[1, 1]");
        architecture["memory"]["capacity"] = "2048B";
    }
    YAML::Emitter archOutput;
    archOutput << architecture;
    const auto archPath = directory.WriteFile(name + ".architecture.yaml", archOutput.c_str());
    auto config = YAML::LoadFile("config/NAND_TCAM/NAND_TCAM.config.yaml");
    config["architecture"] = archPath.string();
    config["cell"] = std::filesystem::absolute("config/NAND_TCAM/NAND_TCAM.cell.yaml").string();
    config["technology"] = std::filesystem::absolute("config/lib/technology/cmos.legacy.yaml").string();
    YAML::Emitter output;
    output << config;
    return directory.WriteFile(name + ".config.yaml", output.c_str()).string();
}

std::shared_ptr<Result> Explore(const std::string &path, int threads = 1) {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile(path);
    EvaCamExplorer explorer(config, threads);
    const auto run = explorer.Run();
    Require(run.numSolution > 0, "NAND integration finds a design");
    for (const auto &result : run.bestResults) {
        if (result && result->bank && result->bank->initialized) return result;
    }
    throw std::runtime_error("missing initialized NAND result");
}

void TestWholeBankSchedulingAndAddressedOperations() {
    TestSupport::TemporaryDirectory directory("nand-banks");
    for (bool tree : {false, true}) {
        const auto single = Explore(Fixture(directory, "single", tree, false, true));
        const auto parallel = Explore(Fixture(directory, "parallel", tree, true, true));
        const auto serial = Explore(Fixture(directory, "serial", tree, true, false));
        const auto &base = *single->bank;
        const auto &all = *parallel->bank;
        const auto &one = *serial->bank;
        Require(dynamic_cast<NandCamBank*>(parallel->bank.get()), "factory selects NAND bank for both routes");
        const auto &metrics = all.mat->subarray->nandModel->Metrics();
        Close(one.searchLatency, 8 * all.searchLatency);
        Require(all.searchDynamicEnergy > 8 * base.searchDynamicEnergy,
                "multi-block search includes every block and positive global routing");
        Require(one.searchDynamicEnergy > all.searchDynamicEnergy,
                "serial rounds repeat query routing but do not remove stored blocks");
        Require(all.area >= 8 * base.area, "all physical blocks counted in area");
        Close(all.mat->leakage, 2 * metrics.leakage);
        Close(one.writeDynamicEnergy, all.writeDynamicEnergy);
        Close(one.resetDynamicEnergy, all.resetDynamicEnergy);
        Require(all.writeDynamicEnergy - metrics.programPageEnergy
                > all.resetDynamicEnergy - metrics.eraseBlockEnergy,
                "program transfers page payload while erase only addresses a block");
        Close(all.mat->writeDynamicEnergy, metrics.programPageEnergy);
        Close(all.mat->resetDynamicEnergy, metrics.eraseBlockEnergy);
        Require(!all.mat->subarray->inputBuf && !all.mat->subarray->rowDecoder,
                "NAND integration never constructs parallel CAM circuits");
        const auto threaded = Explore(Fixture(directory, "threaded", tree, true, true), 2);
        threaded->bank->CalculateRC();
        threaded->bank->CalculateArea();
        threaded->bank->CalculateLatencyAndPower();
        Close(threaded->bank->searchDynamicEnergy, all.searchDynamicEnergy);
        Close(threaded->bank->searchLatency, all.searchLatency);

        // Explicit legacy bank construction must not reach generic CAM circuitry.
        std::vector<std::shared_ptr<Bank>> legacyBanks = {
            std::make_shared<BankWithHtree>(), std::make_shared<BankWithoutHtree>()};
        for (const auto &legacy : legacyBanks) {
            AssertThrows<std::invalid_argument>([&] {
                legacy->Initialize(1, 1, 2048, 32, 1, 1, 1, true, 1, 1, 1, 1, 1, 1,
                        latency_first, TCAM, EX, single->config, base.localWire,
                        base.globalWire, base.CAM_opt);
            }, "BankFactory");
        }

        // Public C++ entry points must reject malformed geometry before arithmetic.
        auto mutableConfig = parallel->config;
        mutableConfig->input.pageSize = std::numeric_limits<long>::max();
        NandCamBank invalid;
        AssertThrows<std::invalid_argument>([&] {
            invalid.Initialize(1, 1, 2048, 32, 1, 1, 1, true, 1, 1, 1, 1, 1, 1,
                    latency_first, TCAM, EX, mutableConfig, all.localWire, all.globalWire, all.CAM_opt);
        }, "Physical page");
    }
}

void TestMatchingAndUnsupportedApis() {
    TestSupport::TemporaryDirectory directory("nand-match");
    for (bool tree : {false, true}) {
        const auto path = Fixture(directory, "match", tree, true, false, 2);
        EvaCAM_Match matcher(path);
        Require(matcher.word_width() == 32 && matcher.storage_width_bits() == 32,
                "public widths remain logical key widths");
        std::vector<int> stored(32, 0), query(32, 0), mask(32, -1);
        auto match = matcher.evaluate_vector(stored, query);
        Require(match.hit && match.senseMarginPass && match.senseMarginApplicable,
                "exact match is electrically distinguishable");
        Require(matcher.match(stored, mask), "query wildcards apply in standard vector API");
        Require(matcher.match(mask, query), "stored ternary wildcards conduct");
        auto invalid = matcher.evaluate_nand(stored, mask, false);
        Require(!invalid.hit && invalid.senseMarginPass, "programmed invalid marker overrides all-X query");
        query[0] = 1;
        auto mismatch = matcher.evaluate_nand(stored, query);
        Require(!mismatch.hit && mismatch.matchlineVoltage > match.matchlineVoltage,
                "a mismatch blocks the string discharge");
        const auto rows = matcher.evaluate_array(std::vector<std::vector<int>>{stored, mask}, query);
        Require(rows.size() == 2 && !rows[0].hit && rows[1].hit, "array API delegates exact NAND semantics");
        Close(rows[0].searchLatency, rows[1].searchLatency);
        const auto design = Explore(path);
        Close(match.searchLatency, design->bank->searchLatency);
        Close(match.searchDynamicEnergy, design->bank->searchDynamicEnergy);
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_mismatches(1); }, "vectors");
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_array(std::vector<int>{0, 1}); }, "vectors");
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_threshold(0, 1); }, "Threshold");
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_threshold(stored, query, 1); }, "Threshold");
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_nand(std::vector<int>{0}, query); }, "keyWidth");
        query[0] = 2;
        AssertThrows<std::invalid_argument>([&] { matcher.evaluate_nand(stored, query); }, "-1");
    }
    EvaCAM_Match conventional("config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml");
    AssertThrows<std::invalid_argument>([&] { conventional.evaluate_nand({0}, {0}); }, "NAND");
}

void TestRunContractAndOverrides() {
    EvaCamRunOptions options;
    options.configPath = "config/NAND_TCAM/NAND_TCAM.config.yaml";
    const auto run = RunEvaCam(options);
    Require(run.numSolutions > 0 && !run.bestResults.empty(), "full application path supports NAND");
    Require(run.bestResults.count("ReadLatency") == 0 && run.bestResults.count("ReadDynamicEnergy") == 0,
            "unavailable read metrics never create best-result entries");
    for (const auto &entry : run.bestResults) {
        Require(entry.second.metadata.at("topology") == "nand_string", "run DTO records topology");
        Require(entry.second.summary.count("timing.read_latency_s") == 0, "read metrics unavailable");
    }
    options.subarrayRows = 64;
    options.subarrayColumns = 32;
    AssertThrows<std::invalid_argument>([&] { RunEvaCam(options); }, "cannot be overridden");
}

} // namespace

int main() {
    TestWholeBankSchedulingAndAddressedOperations();
    TestMatchingAndUnsupportedApis();
    TestRunContractAndOverrides();
    std::cout << "NAND integration tests passed\n";
}
