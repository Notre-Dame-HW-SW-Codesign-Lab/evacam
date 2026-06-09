#include "SenseAmp.h"
#include "input/CustomSenseAmpYamlLoader.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char *kCustomSenseAmpPath = "tests/tmp_custom_sense_amp_loader.yaml";
const char *kMissingFieldPath = "tests/tmp_custom_sense_amp_loader_missing.yaml";

bool AlmostEqual(double lhs, double rhs, double tolerance) {
    return std::fabs(lhs - rhs) <= tolerance;
}

void TestNestedYamlLoadsQuantities() {
    std::ofstream out(kCustomSenseAmpPath);
    out <<
        "custom_sense_amp:\n"
        "  height: 10F\n"
        "  width: 4F\n"
        "  latency: 20ps\n"
        "  energy: 4pJ\n"
        "  leakage: 10pW\n"
        "  cap_load: 2fF\n";
    out.close();

    SenseAmp senseAmp;
    YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, kCustomSenseAmpPath, 45e-9);

    assert(AlmostEqual(senseAmp.height, 450e-9, 1e-18));
    assert(AlmostEqual(senseAmp.width, 180e-9, 1e-18));
    assert(AlmostEqual(senseAmp.area, 450e-9 * 180e-9, 1e-24));
    assert(AlmostEqual(senseAmp.readLatency, 20e-12, 1e-24));
    assert(AlmostEqual(senseAmp.readDynamicEnergy, 4e-12, 1e-24));
    assert(AlmostEqual(senseAmp.leakage, 10e-12, 1e-24));
    assert(AlmostEqual(senseAmp.capLoad, 2e-15, 1e-27));
}

void TestTopLevelYamlLoadsArea() {
    std::ofstream out(kCustomSenseAmpPath);
    out <<
        "area: 0.001um^2\n"
        "latency: 12ps\n"
        "energy: 3pJ\n"
        "cap_load: 7fF\n";
    out.close();

    SenseAmp senseAmp;
    YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, kCustomSenseAmpPath, 45e-9);

    assert(AlmostEqual(senseAmp.area, 0.001e-12, 1e-27));
    assert(senseAmp.height == 0);
    assert(senseAmp.width == 0);
    assert(AlmostEqual(senseAmp.readLatency, 12e-12, 1e-24));
    assert(AlmostEqual(senseAmp.readDynamicEnergy, 3e-12, 1e-24));
    assert(AlmostEqual(senseAmp.capLoad, 7e-15, 1e-27));
}

void TestMissingRequiredFieldThrows() {
    std::ofstream out(kMissingFieldPath);
    out <<
        "custom_sense_amp:\n"
        "  height: 10F\n"
        "  width: 4F\n"
        "  latency: 20ps\n";
    out.close();

    try {
        SenseAmp senseAmp;
        YamlHelpers::ReadCustomSenseAmpFromYaml(senseAmp, kMissingFieldPath, 45e-9);
    } catch (const std::runtime_error &e) {
        assert(std::string(e.what()).find("missing required fields") != std::string::npos);
        return;
    }
    assert(false && "expected missing required fields error");
}

}  // namespace

int main() {
    TestNestedYamlLoadsQuantities();
    TestTopLevelYamlLoadsArea();
    TestMissingRequiredFieldThrows();
    std::cout << "CustomSenseAmpYamlLoader tests passed" << std::endl;
    return 0;
}
