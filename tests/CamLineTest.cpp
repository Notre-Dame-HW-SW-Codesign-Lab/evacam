#include <cassert>
#include <iostream>
#include <memory>

#include "CAM_Line.h"

#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertFinitePositive;
using TestSupport::AssertNear;
using TestSupport::Require;

std::shared_ptr<EvaCamConfig> MakeLineConfig() {
    auto config = TestModelBuilders::MakeEvaCamConfig();
    config->technology.fefetTech = config->technology.tech;
    return config;
}

CAMPort MakePort(CAM_PortType type, CAM_CmosRegion region, double wireWidth = 1) {
    CAMPort port{};
    port.Type = type;
    port.ConnectedRegion = region;
    port.numCmos = 1;
    port.isNMOS = true;
    port.widthCmos = 1;
    port.widthWire = wireWidth;
    return port;
}

void TestConstructorAndCopySemantics() {
    CAM_Line line;
    assert(!line.initialized);
    assert(!line.invalid);

    auto config = MakeLineConfig();
    config->technology.cell->camPort[0][0] = MakePort(Wordline, gate);
    const Wire wire = TestModelBuilders::MakeWire(config);
    line.Initialize(true, 0, 10e-6, 32, config, wire);

    CAM_Line copy(line);
    assert(copy.initialized);
    assert(!copy.invalid);
    assert(copy.config == config);
    AssertNear(copy.len, line.len);
    AssertNear(copy.cap, line.cap);
    AssertNear(copy.res, line.res);
    AssertNear(copy.numCell, line.numCell);

    line.len = 20e-6;
    AssertNear(copy.len, 10e-6);
}

void TestRowWordlineUsesGateLoadingAndNoMuxCurrent() {
    auto config = MakeLineConfig();
    config->technology.cell->camPort[0][0] = MakePort(Wordline, gate);
    const Wire wire = TestModelBuilders::MakeWire(config);

    CAM_Line line;
    line.Initialize(true, 0, 12e-6, 64, config, wire);
    assert(line.initialized);
    assert(!line.invalid);
    assert(line.isRow);
    assert(line.index == 0);
    assert(line.CellPort.Type == Wordline);
    assert(line.CellPort.ConnectedRegion == gate);
    assert(line.config == config);
    AssertNear(line.len, 12e-6);
    AssertNear(line.numCell, 64);
    AssertFinitePositive(line.cap, "wordline capacitance");
    AssertFinitePositive(line.res, "wordline resistance");
    AssertNear(line.maxCurrent, 0);
    AssertNear(line.minMuxWidth, 0);
}

void TestColumnBitlineUsesWideWireAndCurrentSizing() {
    auto config = MakeLineConfig();
    config->technology.cell->camPort[1][0] = MakePort(Bitline, drain, 2);
    const Wire wire = TestModelBuilders::MakeWire(config);

    CAM_Line line;
    line.Initialize(false, 0, 15e-6, 128, config, wire);
    assert(line.initialized);
    assert(!line.invalid);
    assert(!line.isRow);
    AssertFinitePositive(line.cap, "bitline capacitance");
    AssertFinitePositive(line.res, "bitline resistance");
    AssertFinitePositive(line.maxCurrent, "bitline maximum current");
    AssertFinitePositive(line.minMuxWidth, "bitline mux width");
    AssertNear(line.minMuxWidth,
            line.maxCurrent / config->technology.tech->currentOnNmos()[0]);
}

void TestMatchlineBitlineIncludesWriteCurrent() {
    auto config = MakeLineConfig();
    config->technology.cell->accessType = CMOS_access;
    config->technology.cell->camPort[1][0] = MakePort(Matchline_Bitline, drain);
    const Wire wire = TestModelBuilders::MakeWire(config);

    CAM_Line line;
    line.Initialize(false, 0, 8e-6, 16, config, wire);
    assert(line.initialized);
    assert(!line.invalid);
    AssertFinitePositive(line.maxCurrent, "matchline-bitline maximum current");
    AssertFinitePositive(line.minMuxWidth, "matchline-bitline mux width");
}

void TestInvalidPortRegionLeavesLineUninitialized() {
    auto config = MakeLineConfig();
    const Wire wire = TestModelBuilders::MakeWire(config);

    config->technology.cell->camPort[0][0] = MakePort(Wordline, drain);
    CAM_Line wordline;
    wordline.Initialize(true, 0, 10e-6, 32, config, wire);
    assert(wordline.invalid);
    assert(!wordline.initialized);

    config->technology.cell->camPort[1][0] = MakePort(Bitline, gate);
    CAM_Line bitline;
    bitline.Initialize(false, 0, 10e-6, 32, config, wire);
    assert(bitline.invalid);
    assert(!bitline.initialized);
}

void TestMuxSignalOverloadAndReinitialization() {
    auto config = MakeLineConfig();
    const Wire wire = TestModelBuilders::MakeWire(config);

    CAM_Line line;
    line.Initialize(9e-6, 24, 3, config, wire);
    assert(line.initialized);
    assert(!line.invalid);
    assert(line.isRow);
    assert(line.config == config);
    AssertNear(line.len, 9e-6);
    AssertNear(line.numCell, 24);
    AssertFinitePositive(line.cap, "mux line capacitance");
    AssertFinitePositive(line.res, "mux line resistance");
    AssertNear(line.maxCurrent, 1e11);
    AssertNear(line.minMuxWidth, 0);

    const double originalCap = line.cap;
    line.Initialize(40e-6, 80, 8, config, wire);
    AssertNear(line.len, 9e-6);
    AssertNear(line.numCell, 24);
    AssertNear(line.cap, originalCap);
}

}  // namespace

int main() {
    TestConstructorAndCopySemantics();
    TestRowWordlineUsesGateLoadingAndNoMuxCurrent();
    TestColumnBitlineUsesWideWireAndCurrentSizing();
    TestMatchlineBitlineIncludesWriteCurrent();
    TestInvalidPortRegionLeavesLineUninitialized();
    TestMuxSignalOverloadAndReinitialization();
    std::cout << "CAM line tests passed\n";
    return 0;
}
