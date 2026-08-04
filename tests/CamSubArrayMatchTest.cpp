#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "CAM_SubArray.h"

#include "TestModelBuilders.h"
#include "TestSupport.h"

struct CamSubArrayTestAccessor {
    static int CountMismatches(const CAM_SubArray &subarray, const std::vector<int> &stored,
            const std::vector<int> &query) {
        return subarray.CountMismatches(stored, query);
    }
    static double EffectiveMatchlineCellResistance(const CAM_SubArray &subarray, int mismatches,
            double onResistance, double offResistance) {
        return subarray.EffectiveMatchlineCellResistance(mismatches, onResistance, offResistance);
    }
    static double EffectiveMcamStateResistance(const CAM_SubArray &subarray, double resistance,
            double baseResistance) {
        return subarray.EffectiveMcamStateResistance(resistance, baseResistance);
    }
    static std::vector<double> EffectiveMcamStateResistances(const CAM_SubArray &subarray) {
        return subarray.EffectiveMcamStateResistances();
    }
    static double MeanSquaredSearchVoltage(const CAM_SubArray &subarray, int rowPortIndex) {
        return subarray.MeanSquaredSearchVoltage(rowPortIndex);
    }
    static double CalculateSearchlineDriveEnergy(const CAM_SubArray &subarray) {
        return subarray.CalculateSearchlineDriveEnergy();
    }
    static std::vector<double> McamStateTaus(const CAM_SubArray &subarray,
            const std::vector<double> &resistances) {
        return subarray.McamStateTaus(resistances);
    }
    static double McamStateDelay(const CAM_SubArray &subarray, double tau, double *ramp) {
        return subarray.McamStateDelay(tau, ramp);
    }
    static double MatchlineDischargeTau(const CAM_SubArray &subarray, double resistance,
            double wireResistance) {
        return subarray.MatchlineDischargeTau(resistance, wireResistance);
    }
    static double MatchlineEffectiveResistance(const CAM_SubArray &subarray,
            const CAMResistanceSample &sample, int mismatches) {
        return subarray.MatchlineEffectiveResistance(sample, mismatches);
    }
    static double MatchlineAllMatchTau(const CAM_SubArray &subarray,
            const CAMResistanceSample &sample) {
        return subarray.MatchlineAllMatchTau(sample);
    }
    static double MatchlineAllMatchTau(const CAM_SubArray &subarray, double resistance,
            double wireResistance) {
        return subarray.MatchlineAllMatchTau(resistance, wireResistance);
    }
    static double MatchlineSenseMargin(const CAM_SubArray &subarray, double allMatchTau,
            double oneMissTau, double senseTime) {
        return subarray.MatchlineSenseMargin(allMatchTau, oneMissTau, senseTime);
    }
    static double MatchlineBeta(const CAM_SubArray &subarray, double resistance, int paths) {
        return subarray.MatchlineBeta(resistance, paths);
    }
    static double MatchlineHorowitzDelay(const CAM_SubArray &subarray, double tau,
            double resistance, double *ramp, int paths) {
        return subarray.MatchlineHorowitzDelay(tau, resistance, ramp, paths);
    }
};

namespace {

using TestSupport::AssertFinitePositive;
using TestSupport::AssertFiniteNonNegative;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;

struct SubArrayFixture {
    std::shared_ptr<EvaCamConfig> config = TestModelBuilders::MakeEvaCamConfig();
    CAM_SubArray subarray;

    SubArrayFixture() {
        auto &cell = *config->technology.cell;
        config->technology.fefetTech = config->technology.tech;
        cell.camType = TCAM;
        cell.camNumRow = 2;
        cell.camNumCol = 1;
        cell.minSenseVoltage = 0;
        cell.camPort[0][0].Type = Searchline;
        cell.camPort[0][0].volSearch0 = 0.2;
        cell.camPort[0][0].volSearch1 = 0.8;
        cell.camPort[1][0].Type = Matchline;
        cell.camPort[1][0].widthCmos = 2;

        subarray.config = config;
        subarray.initialized = true;
        subarray.invalid = false;
        subarray.CAM_opt = {0, 0, 4};
        subarray.indexMatchline = 0;
        subarray.capCellAccess = 1e-15;
        subarray.matchlineWireRes = 40;
        subarray.resMemCellOn = 100;
        subarray.resMemCellOff = 10000;
        subarray.voltagePrecharge = 1.2;
        subarray.senseVoltage = 0;
        subarray.matchlineDelay = 10e-12;
        subarray.searchLatency = 100e-12;
        subarray.searchDynamicEnergy = 2e-15;
        subarray.decoderLatency = 3e-12;

        subarray.Col.resize(1);
        subarray.Col[0].cap = 2e-15;
        subarray.Col[0].CellPort.widthCmos = 2;
        subarray.ColMux.resize(1);
        subarray.ColMux[0] = std::make_unique<Mux>();
        subarray.ColMux[0]->capForPreviousDelayCalculation = 3e-15;
        subarray.precharger = std::make_unique<Precharger>();
        subarray.precharger->capOutputBitlinePrecharger = 4e-15;
        subarray.senseAmp = std::make_unique<CAM_SenseAmp>();
        subarray.senseAmp->capLoad = 5e-15;

        subarray.RowDriver.resize(2);
        for (auto &driver : subarray.RowDriver) {
            driver = std::make_unique<RowDecoder>();
            driver->readLatency = 2e-12;
            driver->readDynamicEnergy = 8e-15;
            driver->rampOutput = 1e-12;
        }
    }
};

void TestBinaryMatchAndValidation() {
    SubArrayFixture fixture;
    const std::vector<int> stored{0, 1, 0, 1};
    const EvaCAMMatchResult exact = fixture.subarray.EvaluateBinaryMatch(stored, stored);
    assert(exact.hit);
    AssertFinitePositive(exact.matchlineDelay, "exact-match delay");
    AssertFiniteNonNegative(exact.senseMargin, "exact-match sense margin");

    const EvaCAMMatchResult oneMiss = fixture.subarray.EvaluateBinaryMatch(
            stored, {1, 1, 0, 1});
    const EvaCAMMatchResult twoMiss = fixture.subarray.EvaluateBinaryMatchByMismatches(2);
    assert(!oneMiss.hit);
    assert(!twoMiss.hit);
    Require(oneMiss.searchDynamicEnergy > exact.searchDynamicEnergy,
            "one mismatch must include additional discharge energy");
    Require(twoMiss.searchDynamicEnergy > oneMiss.searchDynamicEnergy,
            "additional mismatches must increase discharge energy");
    Require(twoMiss.matchlineDelay < oneMiss.matchlineDelay,
            "parallel mismatch paths must discharge the matchline faster");

    AssertThrows<std::invalid_argument>([&] {
        fixture.subarray.EvaluateBinaryMatch({0, 1}, {0, 1});
    }, "BitSerialWidth");
    AssertThrows<std::invalid_argument>([&] {
        fixture.subarray.EvaluateBinaryMatchByMismatches(-1);
    }, "out of range");
    AssertThrows<std::invalid_argument>([&] {
        fixture.subarray.EvaluateBinaryMatchByMismatches(5);
    }, "out of range");
    fixture.subarray.initialized = false;
    AssertThrows<std::runtime_error>([&] {
        fixture.subarray.EvaluateBinaryMatchByMismatches(0);
    }, "initialization");
}

void TestResistanceAndMatchlineMath() {
    SubArrayFixture fixture;
    CAM_SubArray &subarray = fixture.subarray;
    AssertNear(CamSubArrayTestAccessor::CountMismatches(subarray, {0, 1, 1}, {0, 0, 1}), 1);
    AssertNear(CamSubArrayTestAccessor::EffectiveMatchlineCellResistance(
            subarray, 0, 100, 10000), 2500);
    AssertNear(CamSubArrayTestAccessor::EffectiveMatchlineCellResistance(
            subarray, 1, 100, 10000),
            (100.0 * 10000.0) / (3 * 100.0 + 10000.0));
    Require(CamSubArrayTestAccessor::EffectiveMatchlineCellResistance(subarray, 4, 100, 10000)
                    < CamSubArrayTestAccessor::EffectiveMatchlineCellResistance(
                            subarray, 1, 100, 10000),
            "more mismatch paths must reduce effective resistance");

    CAMResistanceSample nominal{};
    nominal.mlWireRes = 40;
    nominal.cellResOn = 100;
    nominal.cellResOff = 10000;
    AssertNear(CamSubArrayTestAccessor::MatchlineEffectiveResistance(subarray, nominal, 1),
            CamSubArrayTestAccessor::EffectiveMatchlineCellResistance(
                    subarray, 1, 100, 10000));
    AssertNear(CamSubArrayTestAccessor::MatchlineAllMatchTau(subarray, nominal),
            CamSubArrayTestAccessor::MatchlineAllMatchTau(subarray, 10000, 40));
    nominal.hasAggregateMatchlineRes = true;
    nominal.oneMissEffectiveCellRes = 55;
    nominal.allMatchEffectiveCellRes = 2500;
    AssertNear(CamSubArrayTestAccessor::MatchlineEffectiveResistance(subarray, nominal, 1), 55);
    AssertNear(CamSubArrayTestAccessor::MatchlineEffectiveResistance(subarray, nominal, 0), 2500);

    const double tauOne = CamSubArrayTestAccessor::MatchlineDischargeTau(subarray, 100, 40);
    const double tauHigherResistance = CamSubArrayTestAccessor::MatchlineDischargeTau(
            subarray, 200, 40);
    AssertFinitePositive(tauOne, "matchline discharge tau");
    Require(tauHigherResistance > tauOne, "tau must increase with cell resistance");
    AssertFinitePositive(CamSubArrayTestAccessor::MatchlineAllMatchTau(subarray, 10000, 40),
            "all-match tau");
    const double margin = CamSubArrayTestAccessor::MatchlineSenseMargin(
            subarray, 20e-12, 5e-12, 4e-12);
    AssertFinitePositive(margin, "all-match versus miss sense margin");
    Require(CamSubArrayTestAccessor::MatchlineBeta(subarray, 100, 2)
                    < CamSubArrayTestAccessor::MatchlineBeta(subarray, 100, 1),
            "parallel paths must lower beta");
    double ramp = 0;
    const double delay = CamSubArrayTestAccessor::MatchlineHorowitzDelay(
            subarray, tauOne, 100, &ramp, 1);
    AssertFinitePositive(delay, "Horowitz matchline delay");
    AssertFinitePositive(ramp, "Horowitz output ramp");
}

void TestMcamAndSearchlineHelpers() {
    SubArrayFixture fixture;
    CAM_SubArray &subarray = fixture.subarray;
    auto &cell = *fixture.config->technology.cell;
    cell.camType = MCAM;
    cell.numResistanceState = 2;
    cell.ResistanceState[0] = 1000;
    cell.ResistanceState[1] = 2000;
    cell.hasMcamSearchlineVoltages = true;
    cell.searchlineVoltage[0] = 0.2;
    cell.searchlineVoltage[1] = 0.8;
    cell.camPort[0][1].Type = Searchline;
    cell.camPort[0][1].volSearch0 = 0.4;
    cell.camPort[0][1].volSearch1 = 0.6;

    AssertNear(CamSubArrayTestAccessor::MeanSquaredSearchVoltage(subarray, 0),
            (0.04 + 0.64) / 2);
    AssertNear(CamSubArrayTestAccessor::MeanSquaredSearchVoltage(subarray, 1),
            (0.64 + 0.04) / 2);
    AssertNear(CamSubArrayTestAccessor::CalculateSearchlineDriveEnergy(subarray),
            2 * 8e-15 * ((0.04 + 0.64) / 2) / (1.2 * 1.2));
    AssertThrows<std::runtime_error>([&] {
        CamSubArrayTestAccessor::MeanSquaredSearchVoltage(subarray, 2);
    }, "out of range");
    AssertThrows<std::runtime_error>([&] {
        CamSubArrayTestAccessor::EffectiveMcamStateResistance(subarray, -1, 1000);
    }, "positive");
    const std::vector<double> effective =
            CamSubArrayTestAccessor::EffectiveMcamStateResistances(subarray);
    assert(effective.size() == 2);
    Require(effective[1] > effective[0], "higher MCAM state resistance must remain higher");
    const std::vector<double> taus = CamSubArrayTestAccessor::McamStateTaus(subarray, effective);
    assert(taus.size() == effective.size());
    Require(taus[1] > taus[0], "MCAM tau must increase with effective resistance");
    double ramp = 0;
    AssertFinitePositive(CamSubArrayTestAccessor::McamStateDelay(subarray, taus[0], &ramp),
            "MCAM state delay");
    AssertFinitePositive(ramp, "MCAM state ramp");
}

}  // namespace

int main() {
    TestBinaryMatchAndValidation();
    TestResistanceAndMatchlineMath();
    TestMcamAndSearchlineHelpers();
    std::cout << "CAM subarray match tests passed\n";
    return 0;
}
