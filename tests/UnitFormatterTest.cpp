#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

#include "TestSupport.h"
#include "UnitFormatter.h"

namespace {

void CheckUnit(const FormattedUnit &actual, double expectedValue, const char *expectedSuffix) {
    TestSupport::AssertNear(actual.value, expectedValue);
    assert(std::string(actual.suffix) == expectedSuffix);
}

double Below(double threshold) {
    return std::nextafter(threshold, 0.0);
}

void TestToSecondThresholdsAndExtremes() {
    CheckUnit(ToSecond(0), 0, "ps");
    CheckUnit(ToSecond(-2e-9), -2000, "ps");
    CheckUnit(ToSecond(Below(1e-9)), Below(1e-9) * 1e12, "ps");
    CheckUnit(ToSecond(1e-9), 1, "ns");
    CheckUnit(ToSecond(Below(1e-6)), Below(1e-6) * 1e9, "ns");
    CheckUnit(ToSecond(1e-6), 1, "us");
    CheckUnit(ToSecond(Below(1e-3)), Below(1e-3) * 1e6, "us");
    CheckUnit(ToSecond(1e-3), 1, "ms");
    CheckUnit(ToSecond(Below(1)), Below(1) * 1e3, "ms");
    CheckUnit(ToSecond(1), 1, "s");
    CheckUnit(ToSecond(1e15), 1e15, "s");
}

void TestToBpsThresholdsAndExtremes() {
    CheckUnit(ToBps(0), 0, "B/s");
    CheckUnit(ToBps(-1e6), -1e6, "B/s");
    CheckUnit(ToBps(Below(1e3)), Below(1e3), "B/s");
    CheckUnit(ToBps(1e3), 1, "KB/s");
    CheckUnit(ToBps(Below(1e6)), Below(1e6) / 1e3, "KB/s");
    CheckUnit(ToBps(1e6), 1, "MB/s");
    CheckUnit(ToBps(Below(1e9)), Below(1e9) / 1e6, "MB/s");
    CheckUnit(ToBps(1e9), 1, "GB/s");
    CheckUnit(ToBps(Below(1e12)), Below(1e12) / 1e9, "GB/s");
    CheckUnit(ToBps(1e12), 1, "TB/s");
    CheckUnit(ToBps(1e18), 1e6, "TB/s");
}

void TestToJouleThresholdsAndExtremes() {
    CheckUnit(ToJoule(0), 0, "pJ");
    CheckUnit(ToJoule(-2e-9), -2000, "pJ");
    CheckUnit(ToJoule(Below(1e-9)), Below(1e-9) * 1e12, "pJ");
    CheckUnit(ToJoule(1e-9), 1, "nJ");
    CheckUnit(ToJoule(Below(1e-6)), Below(1e-6) * 1e9, "nJ");
    CheckUnit(ToJoule(1e-6), 1, "uJ");
    CheckUnit(ToJoule(Below(1e-3)), Below(1e-3) * 1e6, "uJ");
    CheckUnit(ToJoule(1e-3), 1, "mJ");
    CheckUnit(ToJoule(Below(1)), Below(1) * 1e3, "mJ");
    CheckUnit(ToJoule(1), 1, "J");
    CheckUnit(ToJoule(1e15), 1e15, "J");
}

void TestToWattThresholdsAndExtremes() {
    CheckUnit(ToWatt(0), 0, "pW");
    CheckUnit(ToWatt(-2e-9), -2000, "pW");
    CheckUnit(ToWatt(Below(1e-9)), Below(1e-9) * 1e12, "pW");
    CheckUnit(ToWatt(1e-9), 1, "nW");
    CheckUnit(ToWatt(Below(1e-6)), Below(1e-6) * 1e9, "nW");
    CheckUnit(ToWatt(1e-6), 1, "uW");
    CheckUnit(ToWatt(Below(1e-3)), Below(1e-3) * 1e6, "uW");
    CheckUnit(ToWatt(1e-3), 1, "mW");
    CheckUnit(ToWatt(Below(1)), Below(1) * 1e3, "mW");
    CheckUnit(ToWatt(1), 1, "W");
    CheckUnit(ToWatt(1e15), 1e15, "W");
}

void TestToMeterThresholdsAndExtremes() {
    CheckUnit(ToMeter(0), 0, "pm");
    CheckUnit(ToMeter(-2e-9), -2000, "pm");
    CheckUnit(ToMeter(Below(1e-9)), Below(1e-9) * 1e12, "pm");
    CheckUnit(ToMeter(1e-9), 1, "nm");
    CheckUnit(ToMeter(Below(1e-6)), Below(1e-6) * 1e9, "nm");
    CheckUnit(ToMeter(1e-6), 1, "um");
    CheckUnit(ToMeter(Below(1e-3)), Below(1e-3) * 1e6, "um");
    CheckUnit(ToMeter(1e-3), 1, "mm");
    CheckUnit(ToMeter(Below(1)), Below(1) * 1e3, "mm");
    CheckUnit(ToMeter(1), 1, "m");
    CheckUnit(ToMeter(1e15), 1e15, "m");
}

void TestToSquareMeterThresholdsAndExtremes() {
    CheckUnit(ToSquareMeter(0), 0, "nm^2");
    CheckUnit(ToSquareMeter(-2e-12), -2e6, "nm^2");
    CheckUnit(ToSquareMeter(Below(1e-12)), Below(1e-12) * 1e18, "nm^2");
    CheckUnit(ToSquareMeter(1e-12), 1, "um^2");
    CheckUnit(ToSquareMeter(Below(1e-6)), Below(1e-6) * 1e12, "um^2");
    CheckUnit(ToSquareMeter(1e-6), 1, "mm^2");
    CheckUnit(ToSquareMeter(Below(1)), Below(1) * 1e6, "mm^2");
    CheckUnit(ToSquareMeter(1), 1, "m^2");
    CheckUnit(ToSquareMeter(1e15), 1e15, "m^2");
}

void TestFormattedUnitStreamInsertion() {
    std::ostringstream output;
    output << FormattedUnit{1.25, "ns"};
    assert(output.str() == "1.25ns");
}

}  // namespace

int main() {
    TestToSecondThresholdsAndExtremes();
    TestToBpsThresholdsAndExtremes();
    TestToJouleThresholdsAndExtremes();
    TestToWattThresholdsAndExtremes();
    TestToMeterThresholdsAndExtremes();
    TestToSquareMeterThresholdsAndExtremes();
    TestFormattedUnitStreamInsertion();
    std::cout << "Unit formatter tests passed\n";
    return 0;
}
