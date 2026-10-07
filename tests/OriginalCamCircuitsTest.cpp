#include <cassert>
#include <memory>
#include <string>

#include "EvaCamConfig.h"
#include "EvaCamExplorer.h"
#include "Result.h"
#include "config/EvaCamConfigValidator.h"
#include "config/ExplorationSpaceResolver.h"
#include "TestSupport.h"

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

std::shared_ptr<EvaCamConfig> LoadCircuit(const std::string &name) {
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile("config/original_validation/" + name + "/" + name + ".config.yaml");
    config->logger.SetOutputEnabled(false);
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);
    return config;
}

std::shared_ptr<Result> RunCircuit(const std::shared_ptr<EvaCamConfig> &config) {
    const auto run = EvaCamExplorer(config,1).Run();
    Require(run.numSolution > 0,"circuit must be feasible");
    return run.bestResults.at(read_latency_optimized);
}

void TestDirectFeFetCircuit() {
    const auto config = LoadCircuit("FeFET-2Fe-TCAS19");
    const auto result = RunCircuit(config);
    const auto &sub = *result->bank->mat->subarray;
    AssertNear(sub.resMemCellOn, config->technology.cell->resistanceOn);
    AssertNear(sub.resMemCellOff, config->technology.cell->resistanceOff);
    assert(sub.cellReadEnergy == 0); // no fictitious second supply through floating ML
    const auto &ml = sub.Col[sub.indexMatchline];
    AssertNear(ml.deviceCap,sub.capCellAccess*sub.numRow);
    AssertNear(ml.cap,ml.wireCap+ml.deviceCap);
    AssertNear(sub.precharger->capBitline,ml.cap);
    assert(config->peripherals.inverterTripDecision);
    assert(config->peripherals.decisionThreshold > config->technology.tech->vth());
    assert(sub.senseAmp->normalSenseAmp->model.model=="analytical_inverter");
    assert(sub.TcamSensedVoltage(0)>sub.TcamSensedVoltage(1));
    config->peripherals.prechargeVoltage=2;
    AssertThrows<std::runtime_error>([&] { EvaCamConfigValidator::Validate(*config); },"precharge");
    config->peripherals.prechargeVoltage=1;
    config->peripherals.inverterTripDecision=false;
    config->peripherals.matchlineCircuit="legacy";
    config->peripherals.decisionMode=CamDecisionMode::Differential;
    AssertThrows<std::runtime_error>([&] { EvaCamConfigValidator::Validate(*config); },"analytical_inverter requires");
}

void TestKeeperCircuit() {
    const auto config = LoadCircuit("MRAM-4T2R-VLSIC12");
    const auto result = RunCircuit(config);
    const auto &sub = *result->bank->mat->subarray;
    AssertNear(sub.TcamSensedVoltage(1),config->peripherals.decisionThreshold);
    assert(sub.TcamSensedVoltage(0)>=config->peripherals.keeperHighClamp);
    assert(sub.TcamSensedVoltage(32)>=config->peripherals.keeperLowClamp);
    assert(sub.TcamSensedVoltage(32)<sub.TcamSensedVoltage(1));
    config->peripherals.keeperMidpointDecision=false;
    config->peripherals.matchlineCircuit="clamped_keeper";
    config->peripherals.decisionThreshold=.515;
    const auto linear = RunCircuit(config);
    AssertNear(linear->bank->mat->subarray->TcamSensedVoltage(1),.515);
    config->peripherals.bitsPerDischargePath=2;
    AssertThrows<std::runtime_error>([&] { EvaCamConfigValidator::Validate(*config); },"one bit");
}

void TestEncodedPcmAccessPaths() {
    const auto config = LoadCircuit("PCM-2T2R-JSSC14");
    auto rejected = EvaCamExplorer(config,1).Run();
    assert(rejected.numSolution==0); // inherited electrical inputs fail the sourced geometry
    // Exercise electrical accounting with a deliberately relaxed test-only margin.
    // The publication fixture and its expected infeasibility are preserved.
    config->technology.cell->minSenseVoltage=.001;
    const auto result = RunCircuit(config);
    const auto &sub = *result->bank->mat->subarray;
    assert(sub.CAM_opt.BitSerialWidth==64 && sub.numRow==64);
    AssertNear(sub.resMemCellOff-config->technology.cell->resistanceOff,
            sub.resMemCellOn-config->technology.cell->resistanceOn);
    assert(sub.resMemCellOn>config->technology.cell->resistanceOn);
    assert(sub.TcamSensedVoltage(0)>sub.TcamSensedVoltage(1));
    AssertThrows<std::invalid_argument>([&] { sub.TcamSensedVoltage(2); },"pair locations");
    assert(sub.cellReadEnergy==0);
    config->peripherals.withInputEnc=false;
    AssertThrows<std::runtime_error>([&] { EvaCamConfigValidator::Validate(*config); },"two-bit encoder");
}

int main() {
    TestDirectFeFetCircuit();
    TestKeeperCircuit();
    TestEncodedPcmAccessPaths();
}
