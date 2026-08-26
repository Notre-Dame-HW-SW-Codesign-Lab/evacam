#include "config/EvaCamConfigValidator.h"
#include "config/InputRuleValidator.h"

#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "EvaCamConfig.h"
#include "TestModelBuilders.h"
#include "TestSupport.h"

namespace {

using TestSupport::AssertThrows;
using TestSupport::Require;

std::string MakeCellYaml(const std::string &camType = "TCAM",
        const std::string &rowPorts =
                "    0:\n      type: searchline\n      cmos_region: gate\n"
                "    1:\n      type: searchline\n      cmos_region: gate\n",
        const std::string &columnPorts =
                "    0:\n      type: matchline\n      cmos_region: drain\n"
                "    1:\n      type: matchline\n      cmos_region: drain\n") {
    return "schema: cell\n"
           "name: validator-cell\n"
           "cam_type: " + camType + "\n"
           "memory_device: device.yaml\n"
           "access_device: {type: none}\n"
           "ports:\n  row:\n" + rowPorts + "  column:\n" + columnPorts;
}

std::string MakeDeviceYaml(const std::string &type, const std::string &extra = "") {
    return "schema: memory_device\nname: validator-device\ntype: " + type + "\n" + extra;
}

struct ValidationFixture {
    TestSupport::TemporaryDirectory directory{"config-validators"};
    std::shared_ptr<EvaCamConfig> config = TestModelBuilders::MakeEvaCamConfig();

    ValidationFixture(const std::string &deviceType = "SRAM",
            const std::string &camType = "TCAM", const std::string &mcam = "",
            const std::string &rowPorts =
                    "    0:\n      type: searchline\n      cmos_region: gate\n"
                    "    1:\n      type: searchline\n      cmos_region: gate\n",
            const std::string &columnPorts =
                    "    0:\n      type: matchline\n      cmos_region: drain\n"
                    "    1:\n      type: matchline\n      cmos_region: drain\n") {
        directory.WriteFile("device.yaml", MakeDeviceYaml(deviceType, mcam));
        config->input.fileMemCell = directory.WriteFile("cell.yaml",
                MakeCellYaml(camType, rowPorts, columnPorts)).string();
        config->runtimeSizing.hasExplicitCapacity = true;
        config->input.capacity = 1024;
        config->input.wordWidth = 64;
        config->input.temperature = 300;
        config->input.processNode = 7;
    }

    void ValidateInput() {
        InputRuleValidator::Validate(*config);
    }
};

void TestEvaCamConfigValidatorAcceptsSupportedBoundary() {
    auto config = TestModelBuilders::MakeEvaCamConfig();
    config->technology.cell->memCellType = SRAM;
    config->input.routingMode = h_tree;
    config->input.internalSensing = true;
    EvaCamConfigValidator::Validate(*config);
}

void TestEvaCamConfigValidatorRejectsUnsupportedMemoryTypes() {
    const MemCellType unsupported[] = {DRAM, eDRAM, MLCNAND};
    for (MemCellType type : unsupported) {
        auto config = TestModelBuilders::MakeEvaCamConfig();
        config->technology.cell->memCellType = type;
        AssertThrows<std::runtime_error>([&] { EvaCamConfigValidator::Validate(*config); },
                "under development");
    }
}

void TestEvaCamConfigValidatorRejectsExternalSensing() {
    for (RoutingMode mode : {h_tree, non_h_tree}) {
        auto config = TestModelBuilders::MakeEvaCamConfig();
        config->input.routingMode = mode;
        config->input.internalSensing = false;
        AssertThrows<std::runtime_error>([&] { EvaCamConfigValidator::Validate(*config); },
                "requires internal sensing");
    }
}

void TestInputRuleValidatorAcceptsScalarAndTopologyBoundaries() {
    for (int temperature : {300, 400}) {
        ValidationFixture fixture;
        fixture.config->input.temperature = temperature;
        fixture.config->input.processNode = temperature == 300 ? 7 : 200;
        fixture.config->input.maxNmosSize = 1;
        fixture.config->peripherals.matchlineSenseMargin = 1e-15;
        fixture.config->peripherals.addCapOnML = 0;
        fixture.config->peripherals.scaledVoltage = 0;
        fixture.ValidateInput();
    }
}

void TestInputRuleValidatorRejectsScalarDomainRules() {
    struct Case { const char *name; void (*change)(EvaCamConfig &); const char *message; };
    const Case cases[] = {
        {"temperature", [](EvaCamConfig &c) { c.input.temperature = 299; }, "design.temperature"},
        {"process node", [](EvaCamConfig &c) { c.input.processNode = 201; }, "design.system_process_node"},
        {"word width", [](EvaCamConfig &c) { c.input.wordWidth = 0; }, "memory.word_width"},
        {"nmos size", [](EvaCamConfig &c) { c.input.maxNmosSize = 0; }, "advanced.max_nmos_size"},
        {"driver current", [](EvaCamConfig &c) { c.input.maxDriverCurrent = -1; }, "extra.max_driver_current"},
        {"matchline capacitance", [](EvaCamConfig &c) { c.peripherals.addCapOnML = -1; }, "matchline.additional_cap"},
        {"sense margin", [](EvaCamConfig &c) { c.peripherals.matchlineSenseMargin = 0; }, "sensing.worst_case_sense_margin"},
        {"scaled voltage", [](EvaCamConfig &c) { c.peripherals.scaledVoltage = -1; }, "modeling.scaled_voltage"},
    };
    for (const Case &test : cases) {
        ValidationFixture fixture;
        test.change(*fixture.config);
        AssertThrows<std::runtime_error>([&] { fixture.ValidateInput(); }, test.message);
    }

    ValidationFixture flash;
    flash.config->input.pageSize = 3;
    flash.config->input.flashBlockSize = 10;
    AssertThrows<std::runtime_error>([&] { flash.ValidateInput(); }, "integer multiple");
}

void TestInputRuleValidatorRejectsDerivedCapacityAndConstraintRules() {
    ValidationFixture missingCapacity;
    missingCapacity.config->runtimeSizing.hasExplicitCapacity = false;
    AssertThrows<std::runtime_error>([&] { missingCapacity.ValidateInput(); }, "memory.capacity is required");

    ValidationFixture nonPowerOfTwo;
    nonPowerOfTwo.config->input.wordWidth = 96;
    AssertThrows<std::runtime_error>([&] { nonPowerOfTwo.ValidateInput(); }, "non-power-of-two");

    ValidationFixture badConstraint;
    badConstraint.config->constraints.enabled = true;
    badConstraint.config->constraints.area = 0;
    AssertThrows<std::runtime_error>([&] { badConstraint.ValidateInput(); }, "design_constraints.area");

    ValidationFixture pruning;
    pruning.config->exploration.pruningEnabled = true;
    AssertThrows<std::runtime_error>([&] { pruning.ValidateInput(); },
            "requires optimization.target: Exploration");

    ValidationFixture explorationPruning;
    explorationPruning.config->input.optimizationTarget = full_exploration;
    explorationPruning.config->exploration.pruningEnabled = true;
    explorationPruning.ValidateInput();
}

void TestInputRuleValidatorResolvesFixedDimensionsAndRejectsMismatch() {
    ValidationFixture fixture;
    fixture.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    fixture.config->runtimeSizing.hasExplicitCapacity = false;
    fixture.config->runtimeSizing.fixedSubarrayRows = 64;
    fixture.config->runtimeSizing.fixedSubarrayColumns = 64;
    fixture.config->input.wordWidth = 64;
    fixture.ValidateInput();
    Require(fixture.config->input.capacity == 512, "fixed dimensions must derive byte capacity");

    ValidationFixture asymmetric;
    asymmetric.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    asymmetric.config->runtimeSizing.hasExplicitCapacity = false;
    asymmetric.config->runtimeSizing.fixedSubarrayRows = 16;
    asymmetric.config->runtimeSizing.fixedSubarrayColumns = 64;
    asymmetric.config->input.wordWidth = 64;
    asymmetric.ValidateInput();
    Require(asymmetric.config->input.capacity == 128,
            "fixed dimensions must treat columns as word width");

    ValidationFixture mismatch;
    mismatch.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    mismatch.config->runtimeSizing.fixedSubarrayRows = 64;
    mismatch.config->runtimeSizing.fixedSubarrayColumns = 64;
    AssertThrows<std::runtime_error>([&] { mismatch.ValidateInput(); }, "does not match organization.subarray.dimensions");
}

void TestInputRuleValidatorResolvesMcamLogicalAndPhysicalGeometry() {
    for (const auto &sample : std::vector<std::pair<int, int>>{
            {1, 64}, {2, 32}, {3, 22}, {4, 16}}) {
        auto config = TestModelBuilders::MakeEvaCamConfig();
        config->input.capacity = 512;
        config->input.wordWidth = 64;
        config->ResolveWordGeometry(sample.first);
        Require(config->wordGeometry.physicalColumnsPerWord == sample.second,
                "MCAM columns must be ceil(logical bits / bits per cell)");
    }

    for (const auto &sample : std::vector<std::tuple<int, int, int>>{
            {2, 1, 64}, {4, 2, 32}, {8, 3, 22}, {16, 4, 16}}) {
        const int numStates = std::get<0>(sample);
        const int expectedBitsPerCell = std::get<1>(sample);
        const int expectedColumns = std::get<2>(sample);
        std::ostringstream mcam;
        mcam << "mcam:\n  num_resistance_state: " << numStates
             << "\n  resistance_state: [";
        for (int state = 1; state <= numStates; state++) {
            if (state > 1) mcam << ", ";
            mcam << state << "kohm";
        }
        mcam << "]\n  searchline_voltage: [";
        for (int state = 1; state <= numStates; state++) {
            if (state > 1) mcam << ", ";
            mcam << state * 100 << "mV";
        }
        mcam << "]\n";

        ValidationFixture stateFixture("FEFETRAM", "MCAM", mcam.str());
        stateFixture.config->runtimeSizing.hasFixedSubarrayDimensions = true;
        stateFixture.config->runtimeSizing.hasExplicitCapacity = false;
        stateFixture.config->runtimeSizing.fixedSubarrayRows = 64;
        stateFixture.config->runtimeSizing.fixedSubarrayColumns = expectedColumns;
        stateFixture.config->input.wordWidth = 64;
        stateFixture.ValidateInput();
        Require(stateFixture.config->wordGeometry.bitsPerCell == expectedBitsPerCell,
                "MCAM bits per cell must be log2(resistance states)");
        Require(stateFixture.config->wordGeometry.physicalColumnsPerWord == expectedColumns,
                "MCAM physical columns must follow the configured state count");
    }

    const std::string eightStateMcam =
            "mcam:\n  num_resistance_state: 8\n"
            "  resistance_state: [1kohm, 2kohm, 3kohm, 4kohm, 5kohm, 6kohm, 7kohm, 8kohm]\n"
            "  searchline_voltage: [0.1V, 0.2V, 0.3V, 0.4V, 0.5V, 0.6V, 0.7V, 0.8V]\n";
    ValidationFixture fixture("FEFETRAM", "MCAM", eightStateMcam);
    fixture.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    fixture.config->runtimeSizing.hasExplicitCapacity = false;
    fixture.config->runtimeSizing.fixedSubarrayRows = 64;
    fixture.config->runtimeSizing.fixedSubarrayColumns = 22;
    fixture.config->input.wordWidth = 64;
    fixture.ValidateInput();

    const ResolvedWordGeometry &geometry = fixture.config->wordGeometry;
    Require(geometry.logicalCapacityBits == 4096, "MCAM logical capacity must use logical word bits");
    Require(geometry.logicalWordBits == 64, "MCAM logical word width must remain 64 bits");
    Require(geometry.entryCount == 64, "MCAM fixed rows must resolve to 64 entries");
    Require(geometry.bitsPerCell == 3, "eight-state MCAM must encode three bits per cell");
    Require(geometry.physicalColumnsPerWord == 22, "64 MCAM bits must occupy 22 cells");
    Require(geometry.paddingBits == 2, "64 bits in 22 three-bit cells must have two padding bits");

    ValidationFixture oversized("FEFETRAM", "MCAM", eightStateMcam);
    oversized.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    oversized.config->runtimeSizing.hasExplicitCapacity = false;
    oversized.config->runtimeSizing.fixedSubarrayRows = 64;
    oversized.config->runtimeSizing.fixedSubarrayColumns = 32;
    oversized.config->input.wordWidth = 64;
    oversized.ValidateInput();
    Require(oversized.config->wordGeometry.physicalColumnsPerWord == 32,
            "user-supplied MCAM columns above the minimum must be retained");
    Require(oversized.config->wordGeometry.paddingBits == 32,
            "surplus MCAM cell capacity must be reported as padding bits");

    ValidationFixture mismatch("FEFETRAM", "MCAM", eightStateMcam);
    mismatch.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    mismatch.config->runtimeSizing.hasExplicitCapacity = false;
    mismatch.config->runtimeSizing.fixedSubarrayRows = 64;
    mismatch.config->runtimeSizing.fixedSubarrayColumns = 21;
    mismatch.config->input.wordWidth = 64;
    AssertThrows<std::runtime_error>([&] { mismatch.ValidateInput(); },
            "provides 21 columns");
}

void TestInputRuleValidatorRejectsPeripheralRules() {
    ValidationFixture encoder;
    encoder.config->peripherals.customInputEnc = true;
    AssertThrows<std::runtime_error>([&] { encoder.ValidateInput(); }, "custom input encoder");

    ValidationFixture senseAmp;
    senseAmp.config->peripherals.typeSenseAmp = static_cast<TypeOfSenseAmp>(-1);
    AssertThrows<std::runtime_error>([&] { senseAmp.ValidateInput(); }, "sensing.sensing_mode");

    ValidationFixture customSenseAmp;
    customSenseAmp.config->peripherals.customSenseAmp = true;
    AssertThrows<std::runtime_error>([&] { customSenseAmp.ValidateInput(); }, "custom_sa_input_file");

    ValidationFixture lowSwing;
    lowSwing.config->exploration.wires.isLocalWireLowSwing = IntValueDomain::FixedSet({1});
    lowSwing.config->exploration.wires.localWireRepeaterType = IntValueDomain::FixedSet({repeated_20});
    AssertThrows<std::runtime_error>([&] { lowSwing.ValidateInput(); }, "wires.local.low_swing");
}

void TestInputRuleValidatorRejectsCamModelAndPortRules() {
    ValidationFixture acam("SRAM", "ACAM");
    AssertThrows<std::runtime_error>([&] { acam.ValidateInput(); }, "ACAM is not supported");

    ValidationFixture mcam("SRAM", "MCAM");
    AssertThrows<std::runtime_error>([&] { mcam.ValidateInput(); }, "only 2FeFET MCAM");

    ValidationFixture noMatchline("SRAM", "TCAM", "",
            "    0:\n      type: searchline\n      cmos_region: gate\n",
            "    0:\n      type: bitline\n      cmos_region: drain\n");
    AssertThrows<std::runtime_error>([&] { noMatchline.ValidateInput(); }, "at least one CAM matchline");

    ValidationFixture gateMatchline("SRAM", "TCAM", "", "    0:\n      type: searchline\n      cmos_region: gate\n",
            "    0:\n      type: matchline\n      cmos_region: gate\n");
    AssertThrows<std::runtime_error>([&] { gateMatchline.ValidateInput(); }, "cannot use cmos_region gate");
}

void TestInputRuleValidatorValidatesMcamStateBoundaries() {
    const std::string validMcam =
            "mcam:\n  num_resistance_state: 2\n  resistance_state: [1kohm, 2kohm]\n"
            "  searchline_voltage: [0.2V, 0.8V]\n";
    ValidationFixture accepted("FEFETRAM", "MCAM", validMcam);
    accepted.ValidateInput();

    ValidationFixture badCount("FEFETRAM", "MCAM",
            "mcam:\n  num_resistance_state: 1\n  resistance_state: [1kohm]\n");
    AssertThrows<std::runtime_error>([&] { badCount.ValidateInput(); }, "between 2 and 64");

    ValidationFixture negativeResistance("FEFETRAM", "MCAM",
            "mcam:\n  num_resistance_state: 2\n  resistance_state: [1kohm, 0ohm]\n");
    AssertThrows<std::runtime_error>([&] { negativeResistance.ValidateInput(); }, "must be positive");

    ValidationFixture nonPowerOfTwo("FEFETRAM", "MCAM",
            "mcam:\n  num_resistance_state: 3\n"
            "  resistance_state: [1kohm, 2kohm, 3kohm]\n"
            "  searchline_voltage: [0.2V, 0.5V, 0.8V]\n");
    AssertThrows<std::runtime_error>([&] { nonPowerOfTwo.ValidateInput(); },
            "power of two");
}

void TestInputRuleValidatorEnforcesSupportedMcamTopology() {
    const std::string mcam =
            "mcam:\n  num_resistance_state: 2\n  resistance_state: [1kohm, 2kohm]\n"
            "  searchline_voltage: [0.2V, 0.8V]\n";
    const std::string twoSearchlines =
            "    0: {type: searchline, cmos_region: gate}\n"
            "    1: {type: searchline, cmos_region: gate}\n";
    const std::string twoMatchlines =
            "    0: {type: matchline, cmos_region: drain}\n"
            "    1: {type: matchline, cmos_region: drain}\n";

    ValidationFixture oneSearchline("FEFETRAM", "MCAM", mcam,
            "    0: {type: searchline, cmos_region: gate}\n", twoMatchlines);
    AssertThrows<std::runtime_error>([&] { oneSearchline.ValidateInput(); },
            "exactly two row ports indexed 0 and 1");

    ValidationFixture wrongSearchline("FEFETRAM", "MCAM", mcam,
            "    0: {type: searchline, cmos_region: gate}\n"
            "    1: {type: dataline, cmos_region: gate}\n", twoMatchlines);
    AssertThrows<std::runtime_error>([&] { wrongSearchline.ValidateInput(); },
            "both row ports to be gate-connected searchlines");

    ValidationFixture oneMatchline("FEFETRAM", "MCAM", mcam, twoSearchlines,
            "    0: {type: matchline, cmos_region: drain}\n");
    AssertThrows<std::runtime_error>([&] { oneMatchline.ValidateInput(); },
            "exactly two column ports indexed 0 and 1");

    ValidationFixture wrongMatchline("FEFETRAM", "MCAM", mcam, twoSearchlines,
            "    0: {type: matchline, cmos_region: drain}\n"
            "    1: {type: matchline, cmos_region: source}\n");
    AssertThrows<std::runtime_error>([&] { wrongMatchline.ValidateInput(); },
            "both column ports to be drain-connected matchlines");

    ValidationFixture accessDevice("FEFETRAM", "MCAM", mcam);
    accessDevice.config->input.fileMemCell = accessDevice.directory.WriteFile("access-cell.yaml",
            MakeCellYaml("MCAM").replace(
                    MakeCellYaml("MCAM").find("access_device: {type: none}"),
                    std::string("access_device: {type: none}").size(),
                    "access_device: {type: CMOS}")).string();
    AssertThrows<std::runtime_error>([&] { accessDevice.ValidateInput(); },
            "requires access_device.type: none");
}

// Mapping: ResolveReference is exercised through relative and absolute v2
// references; the obsolete legacy cell shape is rejected at validation.
void TestInputRuleValidatorAcceptsV2ReferencePathsAndRejectsLegacyCells() {
    ValidationFixture relative("MRAM");
    relative.ValidateInput();

    ValidationFixture absolute("PCRAM");
    const std::string devicePath = absolute.directory.WriteFile("absolute-device.yaml",
            MakeDeviceYaml("PCRAM")).string();
    absolute.config->input.fileMemCell = absolute.directory.WriteFile("absolute-cell.yaml",
            "schema: cell\nname: absolute-tcam\ncam_type: TCAM\nmemory_device: "
            + devicePath + "\nports:\n  row:\n    0: {type: searchline, cmos_region: gate}\n"
            "  column:\n    0: {type: matchline, cmos_region: drain}\n").string();
    absolute.ValidateInput();

    ValidationFixture legacy;
    legacy.config->input.fileMemCell = legacy.directory.WriteFile("legacy-cell.yaml",
            "cell:\n  name: legacy-tcam\n  type: memristor\nports:\n  row:\n"
            "    0: {type: searchline, cmos_region: gate}\n  column:\n"
            "    0: {type: matchline, cmos_region: source}\n").string();
    AssertThrows<std::runtime_error>([&] { legacy.ValidateInput(); },
            "cell config schema must be cell");
}

// Mapping: absent cam_type drives InferCamTypeToken for TCAM/ACAM/MCAM names.
void TestInputRuleValidatorInfersCamTypeFromCellName() {
    ValidationFixture inferredTcam;
    inferredTcam.config->input.fileMemCell = inferredTcam.directory.WriteFile("plain-cell.yaml",
            MakeCellYaml().replace(MakeCellYaml().find("cam_type: TCAM\n"), 15, "")).string();
    inferredTcam.ValidateInput();

    ValidationFixture inferredAcam;
    inferredAcam.config->input.fileMemCell = inferredAcam.directory.WriteFile("acam-cell.yaml",
            MakeCellYaml().replace(MakeCellYaml().find("cam_type: TCAM\n"), 15, "")).string();
    AssertThrows<std::runtime_error>([&] { inferredAcam.ValidateInput(); }, "ACAM is not supported");

    ValidationFixture inferredMcam("FEFETRAM", "TCAM",
            "mcam:\n  resistance_state: [1kohm, 2kohm]\n"
            "  searchline_voltage: [0.2V, 0.8V]\n");
    inferredMcam.config->input.fileMemCell = inferredMcam.directory.WriteFile("mcam-cell.yaml",
            MakeCellYaml("TCAM").replace(MakeCellYaml("TCAM").find("cam_type: TCAM\n"), 15, "")).string();
    inferredMcam.ValidateInput();
}

// Mapping: all IsCamModelMemCellTypeSupported branches and v2 dram rejection.
void TestInputRuleValidatorAcceptsAndRejectsEveryCamMemoryType() {
    for (const char *type : {"SRAM", "MRAM", "PCRAM", "memristor", "FEFETRAM"}) {
        ValidationFixture fixture(type);
        fixture.ValidateInput();
    }
    for (const char *type : {"DRAM", "eDRAM", "FBRAM", "SLCNAND", "MLCNAND"}) {
        ValidationFixture fixture(type);
        AssertThrows<std::runtime_error>([&] { fixture.ValidateInput(); }, "not supported");
    }
    ValidationFixture dramField("SRAM", "TCAM", "dram: {}\n");
    AssertThrows<std::runtime_error>([&] { dramField.ValidateInput(); }, "dram is not supported");
}

// Mapping: CAM row/column presence, connection.kind decoding, and every accepted
// matchline region are covered via the public validation entry point.
void TestInputRuleValidatorValidatesCamPortPresenceAndConnections() {
    ValidationFixture missingRows("SRAM", "TCAM", "", "", "    0: {type: matchline, cmos_region: drain}\n");
    AssertThrows<std::runtime_error>([&] { missingRows.ValidateInput(); }, "row must define");
    ValidationFixture missingColumns("SRAM", "TCAM", "", "    0: {type: searchline, cmos_region: gate}\n", "");
    AssertThrows<std::runtime_error>([&] { missingColumns.ValidateInput(); }, "column must define");
    for (const char *region : {"drain", "source", "diode", "none"}) {
        ValidationFixture accepted("SRAM", "TCAM", "", "    0: {type: searchline, cmos_region: gate}\n",
                std::string("    0: {type: matchline, cmos_region: ") + region + "}\n");
        accepted.ValidateInput();
    }
    ValidationFixture terminal("SRAM", "TCAM", "", "    0: {type: searchline, cmos_region: gate}\n",
            "    0:\n      type: matchline\n      connection: {kind: memory_terminal, terminal: drain}\n");
    terminal.ValidateInput();
    ValidationFixture unsupportedConnection("SRAM", "TCAM", "", "    0: {type: searchline, cmos_region: gate}\n",
            "    0:\n      type: matchline\n      connection: {kind: wire, terminal: drain}\n");
    AssertThrows<std::runtime_error>([&] { unsupportedConnection.ValidateInput(); }, "unsupported connection.kind");
}

// Mapping: MCAM sequence/map states, optional state voltages, paper-style
// analog-inverse pairing, and topology all execute ValidateMcamResistanceStates.
void TestInputRuleValidatorValidatesMcamMapsAndVoltages() {
    const std::string ports = "    0: {type: searchline, cmos_region: gate}\n    1: {type: searchline, cmos_region: gate}\n";
    ValidationFixture mapStates("FEFETRAM", "MCAM",
            "mcam:\n  num_resistance_state: 2\n  resistance_state: {0: 1kohm, 1: 2kohm}\n"
            "  ml_precharge_voltage: {0: 0V, 1: 1V}\n  searchline_voltage: [0.2V, 0.8V]\n",
            ports);
    mapStates.ValidateInput();
    ValidationFixture missingState("FEFETRAM", "MCAM",
            "mcam:\n  num_resistance_state: 2\n  resistance_state: {0: 1kohm}\n");
    AssertThrows<std::runtime_error>([&] { missingState.ValidateInput(); },
            "must contain exactly");
    ValidationFixture derivedCenter("FEFETRAM", "MCAM",
            "mcam:\n  resistance_state: [1kohm, 2kohm]\n  searchline_voltage: [1V, 1V]\n");
    AssertThrows<std::runtime_error>([&] { derivedCenter.ValidateInput(); },
            "must be distinct");
    ValidationFixture missingVariationState("FEFETRAM", "MCAM",
            "mcam:\n  resistance_state: [1kohm, 2kohm]\n"
            "  state_variation: [5%]\n"
            "  searchline_voltage: [0.2V, 0.8V]\n");
    AssertThrows<std::runtime_error>([&] { missingVariationState.ValidateInput(); },
            "mcam.state_variation must define every configured resistance state");
    ValidationFixture wrongRows("FEFETRAM", "MCAM",
            "mcam:\n  resistance_state: [1kohm, 2kohm]\n  searchline_voltage: [0.2V, 0.8V]\n");
    wrongRows.config->input.fileMemCell = wrongRows.directory.WriteFile("one-row-cell.yaml",
            MakeCellYaml("MCAM",
                    "    0: {type: searchline, cmos_region: gate}\n")).string();
    AssertThrows<std::runtime_error>([&] { wrongRows.ValidateInput(); }, "exactly two row ports");
    ValidationFixture invalidAnalogInverse("FEFETRAM", "MCAM",
            "mcam:\n  num_resistance_state: 4\n"
            "  resistance_state: [1kohm, 2kohm, 3kohm, 4kohm]\n"
            "  searchline_voltage: [0.1V, 0.4V, 0.8V, 1V]\n", ports);
    AssertThrows<std::runtime_error>(
            [&] { invalidAnalogInverse.ValidateInput(); }, "analog-inverse");
}

// Mapping: supported sense modes plus custom/default sense-amp file validation,
// input encoding, and global low-swing/repeater conflict.
void TestInputRuleValidatorValidatesPeripheralVariants() {
    for (TypeOfSenseAmp type : {nvsim_voltage_sense, nvsim_current_sense, discharge}) {
        ValidationFixture fixture;
        fixture.config->peripherals.typeSenseAmp = type;
        fixture.ValidateInput();
    }
    ValidationFixture missingDefault;
    missingDefault.config->peripherals.fileSenseAmp = "does-not-exist.yaml";
    AssertThrows<std::runtime_error>([&] { missingDefault.ValidateInput(); }, "sense amp file cannot be found");
    ValidationFixture validDefault;
    validDefault.config->peripherals.fileSenseAmp = "config/lib/sense_amp/nvsim_vol.sense_amp.yaml";
    validDefault.ValidateInput();
    ValidationFixture missingCustom;
    missingCustom.config->peripherals.customSenseAmp = true;
    missingCustom.config->peripherals.fileCustomSA = "does-not-exist.yaml";
    AssertThrows<std::runtime_error>([&] { missingCustom.ValidateInput(); }, "custom sense amp file cannot be found");
    ValidationFixture invalidEncoder;
    invalidEncoder.config->peripherals.typeInputEnc = static_cast<TypeOfInputEncoder>(1);
    AssertThrows<std::runtime_error>([&] { invalidEncoder.ValidateInput(); }, "input encoder type");
    ValidationFixture globalLowSwing;
    globalLowSwing.config->exploration.wires.isGlobalWireLowSwing = IntValueDomain::FixedSet({1});
    globalLowSwing.config->exploration.wires.globalWireRepeaterType = IntValueDomain::FixedSet({repeated_5});
    AssertThrows<std::runtime_error>([&] { globalLowSwing.ValidateInput(); }, "wires.global.low_swing");
}

// Mapping: individual conditional scalar/constraint/flash checks and derived
// capacity, real-capacity geometry, overflow, and fixed-dimension branches.
void TestInputRuleValidatorCoversRemainingSizingAndScalarRules() {
    ValidationFixture matchWidth;
    matchWidth.config->input.hasCamWidthMatchTran = true;
    AssertThrows<std::runtime_error>([&] { matchWidth.ValidateInput(); }, "match_transistor");
    for (const char *field : {"readLatency", "writeLatency", "readDynamicEnergy", "writeDynamicEnergy",
             "readEdp", "writeEdp", "area", "leakage"}) {
        ValidationFixture constraint;
        constraint.config->constraints.enabled = true;
        if (std::string(field) == "readLatency") constraint.config->constraints.readLatency = 0;
        if (std::string(field) == "writeLatency") constraint.config->constraints.writeLatency = 0;
        if (std::string(field) == "readDynamicEnergy") constraint.config->constraints.readDynamicEnergy = 0;
        if (std::string(field) == "writeDynamicEnergy") constraint.config->constraints.writeDynamicEnergy = 0;
        if (std::string(field) == "readEdp") constraint.config->constraints.readEdp = 0;
        if (std::string(field) == "writeEdp") constraint.config->constraints.writeEdp = 0;
        if (std::string(field) == "area") constraint.config->constraints.area = 0;
        if (std::string(field) == "leakage") constraint.config->constraints.leakage = 0;
        AssertThrows<std::runtime_error>([&] { constraint.ValidateInput(); }, "design_constraints");
    }
    ValidationFixture realCapacity;
    realCapacity.config->runtimeSizing.realCapacity = 512;
    AssertThrows<std::runtime_error>([&] { realCapacity.ValidateInput(); }, "must be >=");
    ValidationFixture incompatibleRealCapacity;
    incompatibleRealCapacity.config->runtimeSizing.realCapacity = 1025;
    AssertThrows<std::runtime_error>([&] { incompatibleRealCapacity.ValidateInput(); }, "word_width");
    ValidationFixture negativePage;
    negativePage.config->input.pageSize = -1;
    AssertThrows<std::runtime_error>([&] { negativePage.ValidateInput(); }, "flash.page_size");
    ValidationFixture negativeBlock;
    negativeBlock.config->input.flashBlockSize = -1;
    AssertThrows<std::runtime_error>([&] { negativeBlock.ValidateInput(); }, "flash.block_size");
    ValidationFixture nonDividingPartition;
    nonDividingPartition.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    nonDividingPartition.config->runtimeSizing.fixedSubarrayRows = 64;
    nonDividingPartition.config->runtimeSizing.fixedSubarrayColumns = 64;
    nonDividingPartition.config->exploration.geometry.numRowMat = IntValueDomain::FixedSet({2});
    nonDividingPartition.config->exploration.geometry.numActiveMatPerColumn = IntValueDomain::FixedSet({4});
    AssertThrows<std::runtime_error>([&] { nonDividingPartition.ValidateInput(); }, "partitioning must divide");
    ValidationFixture disallowedDseDimensions;
    disallowedDseDimensions.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    disallowedDseDimensions.config->runtimeSizing.fixedSubarrayRows = 64;
    disallowedDseDimensions.config->runtimeSizing.fixedSubarrayColumns = 64;
    disallowedDseDimensions.config->input.optimizationTarget = full_exploration;
    AssertThrows<std::runtime_error>([&] { disallowedDseDimensions.ValidateInput(); }, "only supported for fixed");
    ValidationFixture overflow;
    overflow.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    overflow.config->runtimeSizing.fixedSubarrayRows = 64;
    overflow.config->runtimeSizing.fixedSubarrayColumns = 64;
    overflow.config->exploration.geometry.numRowMat = IntValueDomain::FixedSet({std::numeric_limits<int>::max()});
    overflow.config->exploration.geometry.numColumnMat = IntValueDomain::FixedSet({std::numeric_limits<int>::max()});
    overflow.config->exploration.geometry.numActiveMatPerRow = IntValueDomain::FixedSet({std::numeric_limits<int>::max()});
    overflow.config->exploration.geometry.numActiveMatPerColumn = IntValueDomain::FixedSet({std::numeric_limits<int>::max()});
    overflow.config->exploration.geometry.numRowSubarray = IntValueDomain::FixedSet({std::numeric_limits<int>::max()});
    overflow.config->exploration.geometry.numColumnSubarray = IntValueDomain::FixedSet({std::numeric_limits<int>::max()});
    overflow.config->exploration.geometry.numActiveSubarrayPerRow = IntValueDomain::FixedSet({std::numeric_limits<int>::max()});
    overflow.config->exploration.geometry.numActiveSubarrayPerColumn = IntValueDomain::FixedSet({std::numeric_limits<int>::max()});
    AssertThrows<std::runtime_error>([&] { overflow.ValidateInput(); }, "exceeds int64_t");
    ValidationFixture invalidDimensions;
    invalidDimensions.config->runtimeSizing.hasFixedSubarrayDimensions = true;
    invalidDimensions.config->runtimeSizing.fixedSubarrayRows = 7;
    invalidDimensions.config->runtimeSizing.fixedSubarrayColumns = 64;
    AssertThrows<std::runtime_error>([&] { invalidDimensions.ValidateInput(); }, "between 8 and 512");
}

}  // namespace

int main() {
    TestEvaCamConfigValidatorAcceptsSupportedBoundary();
    TestEvaCamConfigValidatorRejectsUnsupportedMemoryTypes();
    TestEvaCamConfigValidatorRejectsExternalSensing();
    TestInputRuleValidatorAcceptsScalarAndTopologyBoundaries();
    TestInputRuleValidatorRejectsScalarDomainRules();
    TestInputRuleValidatorRejectsDerivedCapacityAndConstraintRules();
    TestInputRuleValidatorResolvesFixedDimensionsAndRejectsMismatch();
    TestInputRuleValidatorResolvesMcamLogicalAndPhysicalGeometry();
    TestInputRuleValidatorRejectsPeripheralRules();
    TestInputRuleValidatorRejectsCamModelAndPortRules();
    TestInputRuleValidatorValidatesMcamStateBoundaries();
    TestInputRuleValidatorEnforcesSupportedMcamTopology();
    TestInputRuleValidatorAcceptsV2ReferencePathsAndRejectsLegacyCells();
    TestInputRuleValidatorInfersCamTypeFromCellName();
    TestInputRuleValidatorAcceptsAndRejectsEveryCamMemoryType();
    TestInputRuleValidatorValidatesCamPortPresenceAndConnections();
    TestInputRuleValidatorValidatesMcamMapsAndVoltages();
    TestInputRuleValidatorValidatesPeripheralVariants();
    TestInputRuleValidatorCoversRemainingSizingAndScalarRules();
    return 0;
}
