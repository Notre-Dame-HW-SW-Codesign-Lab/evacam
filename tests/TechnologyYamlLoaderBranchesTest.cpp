#include "TestSupport.h"
#include "input/TechnologyYamlLoader.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using TestSupport::AssertNear;
using TestSupport::AssertThrows;
using TestSupport::Require;
using TestSupport::TemporaryDirectory;

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    const std::size_t position = text.find(from);
    Require(position != std::string::npos, "fixture replacement target was not found");
    text.replace(position, from.size(), to);
    return text;
}

std::string Table(const std::string& values = "1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11") {
    return "[" + values + "]";
}

std::string Node(const std::string& roadmap, int processNode,
        const std::string& fin = "height: 0nm, width: 0nm, pitch: 0nm, polywire_cap: 0pF/m",
        const std::string& currents = "") {
    const std::string tables = currents.empty()
            ? "on_nmos: " + Table() + "\n"
              "          on_pmos: " + Table() + "\n"
              "          off_nmos: " + Table() + "\n"
              "          off_pmos: " + Table()
            : currents;
    return "      - process_node: " + std::to_string(processNode) + "nm\n"
           "        roadmap: " + roadmap + "\n"
           "        operating_point: {vdd: 900mV, vth: 200mV, physical_gate_length: 20nm}\n"
           "        capacitance: {ideal_gate: 2pF/m, fringe: 3fF/m, oxide: 4F/m}\n"
           "        mobility: {electron: 0.02, hole: 0}\n"
           "        sizing: {pn_size_ratio: 2, effective_resistance_multiplier: 1.5}\n"
           "        gm: {nmos: 0, pmos: 1}\n"
           "        fin: {" + fin + "}\n"
           "        currents:\n"
           "          " + tables + "\n";
}

std::string Document(const std::string& libraryModel = "updated",
        const std::string& roadmap = "HP", int processNode = 20,
        const std::string& nodeSuffix = "", const std::string& grid =
                "[300K, 310K, 320K, 330K, 340K, 350K, 360K, 370K, 380K, 390K, 400K]") {
    return "schema: technology\n"
           "name: branch-test\n"
           "library_model: " + libraryModel + "\n"
           "temperature_grid: " + grid + "\n"
           "interpolation: {temperature: linear, process_node: linear, off_current: geometric}\n"
           "roadmaps:\n"
           "  " + roadmap + ":\n"
           "    nodes:\n" + Node(roadmap, processNode) + nodeSuffix;
}

std::vector<TechnologySpec> Load(const TemporaryDirectory& directory,
        const std::string& contents, const std::string& filename = "technology.yaml") {
    return YamlHelpers::ReadTechnologySpecsFromYaml(
            directory.WriteFile(filename, contents).string());
}

void ExpectFailure(const TemporaryDirectory& directory, const std::string& contents,
        const std::string& message) {
    const std::string path = directory.WriteFile("bad.yaml", contents).string();
    AssertThrows<std::runtime_error>([&] {
        (void)YamlHelpers::ReadTechnologySpecsFromYaml(path);
    }, message);
}

void TestUpdatedAndLegacyModelsAndDerivedValues() {
    TemporaryDirectory directory("evacam-technology-branches");
    const TechnologySpec updated = Load(directory, Document()).front();
    Require(updated.useUpdatedLib, "updated library model was not retained");
    Require(updated.featureSizeInNano == 20 && updated.roadmap == HP,
            "updated identity fields were not parsed");
    AssertNear(updated.featureSize, 20e-9);
    AssertNear(updated.vdd, 0.9);
    AssertNear(updated.phyGateLength, 20e-9);
    AssertNear(updated.capIdealGate, 2e-12);
    AssertNear(updated.capFringe, 3e-15);
    AssertNear(updated.capOx, 4.0);
    AssertNear(updated.capOverlap, 0.4e-12);
    AssertNear(updated.capJunction, 1e-3 / std::pow(2.0, 0.5));
    AssertNear(updated.capSidewall, 2.5e-10 / std::pow(2.0, 0.33));
    AssertNear(updated.capDrainToChannel, 0.5e-10 / std::pow(2.0, 0.33));
    AssertNear(updated.buildInPotential, 0.9);
    AssertNear(updated.vdsatNmos, 0.1);
    Require(std::isinf(updated.vdsatPmos), "zero mobility must produce infinite Vdsat");

    const TechnologySpec legacy = Load(directory, Document("legacy", "LOP", 90), "legacy.yaml").front();
    Require(!legacy.useUpdatedLib && legacy.roadmap == LOP && legacy.featureSizeInNano == 90,
            "legacy model or roadmap was not parsed");

    ExpectFailure(directory, Document("new"), "unsupported technology.library_model");
    ExpectFailure(directory, Document("true"), "unsupported technology.library_model");
    ExpectFailure(directory, Replace(Document(), "library_model: updated\n", ""),
            "Missing key: library_model");
}

void TestRoadmapsMultipleSpecsAndFinGeometry() {
    TemporaryDirectory directory("evacam-technology-roadmaps");
    std::string contents = Document("legacy", "HP", 20,
            Node("HP", 10, "height: 30nm, width: 8nm, pitch: 40nm, polywire_cap: 5pF/m"));
    contents += "  FEFET:\n    nodes:\n" + Node("FEFET", 22);
    const std::vector<TechnologySpec> specs = Load(directory, contents);
    Require(specs.size() == 3, "multiple roadmap nodes were not loaded");
    Require(specs[1].heightFin > 0 && specs[1].widthFin > 0 && specs[1].PitchFin > 0,
            "complete FinFET geometry was not loaded");
    AssertNear(specs[1].capPolywire, 5e-12);
    Require(specs[2].roadmap == FEFET, "FEFET roadmap key was not accepted");

    ExpectFailure(directory, Replace(Document(), "  HP:\n", "  BAD:\n"),
            "Invalid technology.roadmaps key");
    ExpectFailure(directory, Replace(Document(), "roadmap: HP", "roadmap: LP"),
            "does not match containing roadmap");
    ExpectFailure(directory, Replace(Document(), "height: 0nm, width: 0nm, pitch: 0nm",
            "height: 10nm, width: 0nm, pitch: 0nm"), "must either all be positive or all be zero");
    ExpectFailure(directory, Document("updated", "HP", 20, Node("HP", 20)),
            "duplicate technology process_node entry");
}

void TestGridTablesAndNumericDomains() {
    TemporaryDirectory directory("evacam-technology-tables");
    ExpectFailure(directory, Document("updated", "HP", 20, "", "[300K]"),
            "must match current 300K-400K table size");
    ExpectFailure(directory, Document("updated", "HP", 20, "", "300K"),
            "temperature_grid must be a sequence");
    ExpectFailure(directory, Document("updated", "HP", 20, "",
            "[300K, 310K, 320K, 330K, 340K, 351K, 360K, 370K, 380K, 390K, 400K]"),
            "must contain 300K through 400K");
    ExpectFailure(directory, Replace(Document(), "on_nmos: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]",
            "on_nmos: [1, 2]"), "Invalid table length");
    ExpectFailure(directory, Replace(Document(), "on_pmos: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]",
            "on_pmos: 1"), "Invalid table length");
    ExpectFailure(directory, Replace(Document(), "off_nmos: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]",
            "off_nmos: [1, 2, 3, 4, nan, 6, 7, 8, 9, 10, 11]"), "Bad conversion");
    ExpectFailure(directory, Replace(Document(), "off_pmos: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]",
            "off_pmos: [1, 2, 3, 4, 0, 6, 7, 8, 9, 10, 11]"), "must be positive");

    const TechnologySpec nonMonotonic = Load(directory, Replace(Document(),
            "on_nmos: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]",
            "on_nmos: [1, 11, 2, 10, 3, 9, 4, 8, 5, 7, 6]"), "nonmonotonic.yaml").front();
    Require(nonMonotonic.currentOnNmos[1] > nonMonotonic.currentOnNmos[2],
            "fixture did not demonstrate current-table ordering behavior");
}

void TestSchemaStructureUnknownAndMissingFields() {
    TemporaryDirectory directory("evacam-technology-structure");
    ExpectFailure(directory, Replace(Document(), "schema: technology", "schema: cell"),
            "technology config");
    ExpectFailure(directory, Replace(Document(), "name: branch-test\n", "name: branch-test\nextra: 1\n"),
            "unknown key 'technology.extra'");
    ExpectFailure(directory, Replace(Document(), "interpolation: {temperature: linear, process_node: linear, off_current: geometric}",
            "interpolation: {mode: linear}"), "unknown key 'technology.interpolation.mode'");
    ExpectFailure(directory,
            "schema: technology\n"
            "library_model: updated\n"
            "temperature_grid: [300K, 310K, 320K, 330K, 340K, 350K, 360K, 370K, 380K, 390K, 400K]\n"
            "roadmaps: []\n", "technology.roadmaps must be a mapping");
    ExpectFailure(directory,
            "schema: technology\n"
            "library_model: updated\n"
            "temperature_grid: [300K, 310K, 320K, 330K, 340K, 350K, 360K, 370K, 380K, 390K, 400K]\n"
            "roadmaps:\n"
            "  HP:\n"
            "    nodes: {}\n", "technology roadmap nodes must be a sequence");
    ExpectFailure(directory, Replace(Document(), "        gm: {nmos: 0, pmos: 1}\n", ""),
            "Missing key: gm");
    ExpectFailure(directory, Replace(Document(), "vdd: 900mV", "vdd: 0V"), "must be positive");
    ExpectFailure(directory, Replace(Document(), "vth: 200mV", "vth: 700mV"),
            "must be less than 0.7 * vdd");
    ExpectFailure(directory, Replace(Document(), "process_node: 20nm", "process_node: 20.5nm"),
            "must resolve to a whole number");
}

}  // namespace

int main() {
    TestUpdatedAndLegacyModelsAndDerivedValues();
    TestRoadmapsMultipleSpecsAndFinGeometry();
    TestGridTablesAndNumericDomains();
    TestSchemaStructureUnknownAndMissingFields();
    return 0;
}
