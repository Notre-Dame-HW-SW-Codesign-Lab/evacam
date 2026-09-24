#include <cmath>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "EvaCAM_Match.h"
#include "EvaCamConfig.h"
#include "EvaCamExplorer.h"
#include "EvaCamRun.h"
#include "Nand3dCamModel.h"
#include "NandCamBank.h"
#include "NandCamFactory.h"
#include "Result.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

const std::string example = "config/NAND_3D_TCAM/NAND_3D_TCAM.config.yaml";

std::string Fixture(TestSupport::TemporaryDirectory &directory, const std::string &name,
        bool tree, bool twoBlocks, bool parallel, int mux = 1) {
    auto architecture = YAML::LoadFile("config/NAND_3D_TCAM/NAND_3D_TCAM.architecture.yaml");
    architecture["routing"]["type"] = tree ? "H-tree" : "NonH-tree";
    architecture["organization"]["mux"]["sense_amp"] = mux;
    if (twoBlocks) {
        architecture["organization"]["banks"]["total"] = YAML::Load("[2, 1]");
        architecture["organization"]["banks"]["active"] = YAML::Load(parallel ? "[2, 1]" : "[1, 1]");
        architecture["memory"]["capacity"] = "512B";
    }
    YAML::Emitter archOutput;
    archOutput << architecture;
    const auto archPath = directory.WriteFile(name + ".architecture.yaml", archOutput.c_str());
    auto config = YAML::LoadFile(example);
    config["architecture"] = archPath.string();
    config["cell"] = std::filesystem::absolute("config/NAND_3D_TCAM/NAND_3D_TCAM.cell.yaml").string();
    config["technology"] = std::filesystem::absolute("config/lib/technology/cmos.legacy.yaml").string();
    YAML::Emitter output;
    output << config;
    return directory.WriteFile(name + ".config.yaml", output.c_str()).string();
}

std::shared_ptr<Result> Explore(const std::string &path) {
    auto config = std::make_shared<EvaCamConfig>();
    config->logger.SetOutputEnabled(false);
    config->ReadConfigFromFile(path);
    EvaCamExplorer explorer(config, 1);
    const auto run = explorer.Run();
    Require(run.numSolution > 0, "NAND3D example has feasible sensing");
    for (const auto &result : run.bestResults) {
        if (result && result->bank && result->bank->initialized) return result;
    }
    throw std::runtime_error("missing NAND3D design");
}

void TestDistinctBackendFactory() {
    const auto planar = CreateNandCamBackend(SLCNAND);
    const auto vertical = CreateNandCamBackend(NAND3D);
    Require(dynamic_cast<NandCamModel*>(planar.get()), "SLCNAND retains analytical backend");
    Require(dynamic_cast<Nand3dCamModel*>(vertical.get()), "NAND3D selects distinct physical backend");
    AssertThrows<std::invalid_argument>([] { CreateNandCamBackend(SRAM); }, "SLCNAND or NAND3D");
    AssertThrows<std::runtime_error>([&] { vertical->Metrics(); }, "initializ");
}

void TestPhysicalPagesGroupsAndBankScheduling() {
    TestSupport::TemporaryDirectory directory("nand3d-banks");
    for (bool tree : {false, true}) {
        const auto single = Explore(Fixture(directory, "single", tree, false, true));
        const auto parallel = Explore(Fixture(directory, "parallel", tree, true, true));
        const auto serial = Explore(Fixture(directory, "serial", tree, true, false));
        const auto multiplexed = Explore(Fixture(directory, "mux", tree, false, true, 2));
        const auto &base = *single->bank;
        const auto &all = *parallel->bank;
        const auto &one = *serial->bank;
        const auto &metrics = base.mat->subarray->nandModel->Metrics();
        const auto &device = single->config->technology.cell->nand3d;
        Require(dynamic_cast<NandCamBank*>(single->bank.get()), "3D uses shared NAND bank scheduler");
        Require(dynamic_cast<Nand3dCamModel*>(base.mat->subarray->nandModel.get()), "backend dispatch preserved through Mat");
        Require(metrics.entries == device.stringRows * device.stringColumns, "all physical strings hold entries");
        Require(metrics.physicalPageBits == device.stringColumns, "one selected string group supplies a physical page");
        Require(metrics.physicalBlockBits == metrics.entries * device.storageLayers, "storage cells exclude dummy/select devices");
        Require(metrics.searchRounds == device.stringRows, "groups sharing bitlines require separate search rounds");
        const auto &mux = multiplexed->bank->mat->subarray->nandModel->Metrics();
        Require(mux.senseAmplifiers * 2 == metrics.senseAmplifiers, "sense mux halves the physical sense channels");
        Require(mux.searchRounds == 2 * metrics.searchRounds, "sense mux multiplies group rounds");
        Require(mux.searchLatency > metrics.searchLatency, "serial sensing adds latency");
        AssertNear(one.searchLatency, 2 * all.searchLatency, 1e-18, 1e-10);
        Require(all.searchDynamicEnergy > 2 * base.searchDynamicEnergy, "all blocks and routes consume query energy");
        Require(one.searchDynamicEnergy > all.searchDynamicEnergy, "serial block scheduling repeats query routing");
        AssertNear(one.writeDynamicEnergy, all.writeDynamicEnergy, 1e-25, 1e-10);
        AssertNear(one.resetDynamicEnergy, all.resetDynamicEnergy, 1e-25, 1e-10);
        Require(!base.mat->subarray->rowDecoder && !base.mat->subarray->inputBuf,
                "NAND3D bypasses generic parallel CAM circuits");
    }
}

void TestPublicSearchAndConcurrentEvaluation() {
    EvaCAM_Match matcher(example);
    const auto width = matcher.word_width();
    std::vector<int> zeros(width, 0), ones(width, 1), masks(width, -1);
    const auto exact = matcher.evaluate_nand(zeros, zeros);
    Require(exact.hit && exact.senseMarginPass, "matching key is sensed");
    Require(matcher.evaluate_nand(zeros, masks).hit, "query wildcard conducts");
    Require(matcher.evaluate_nand(masks, ones).hit, "stored wildcard conducts");
    const auto invalid = matcher.evaluate_nand(zeros, masks, false);
    Require(!invalid.hit && invalid.senseMarginPass, "programmed invalid entry cannot match masked query");
    const auto mismatch = matcher.evaluate_nand(zeros, ones);
    Require(!mismatch.hit && mismatch.matchlineVoltage > exact.matchlineVoltage,
            "mismatched string blocks discharge");
    std::vector<std::future<EvaCAMMatchResult>> calls;
    for (int index = 0; index < 4; ++index) {
        calls.push_back(std::async(std::launch::async, [&] { return matcher.evaluate_nand(zeros, zeros); }));
    }
    for (auto &call : calls) {
        const auto actual = call.get();
        AssertNear(actual.matchlineVoltage, exact.matchlineVoltage, 1e-14, 1e-12);
        AssertNear(actual.searchDynamicEnergy, exact.searchDynamicEnergy, 1e-25, 1e-12);
    }
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_nand({0}, zeros); }, "keyWidth");
    zeros[0] = 2;
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_nand(zeros, ones); }, "-1");
    AssertThrows<std::invalid_argument>([&] { matcher.evaluate_threshold(0, 1); }, "Threshold");
}

}  // namespace

int main() {
    TestDistinctBackendFactory();
    TestPhysicalPagesGroupsAndBankScheduling();
    TestPublicSearchAndConcurrentEvaluation();
    std::cout << "NAND3D integration tests passed\n";
}
