#include "config/ExplorationSpaceResolver.h"
#include "config/ExplorationSpec.h"
#include "config/IntValueDomain.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void AssertEqual(const std::vector<int> &actual, const std::vector<int> &expected) {
    assert(actual == expected);
}

void TestIntValueDomain() {
    AssertEqual(IntValueDomain::PowersOfTwo(1, 16).Values(), {1, 2, 4, 8, 16});
    AssertEqual(IntValueDomain::PowersOfTwo(3, 12).Values(), {3, 6, 12});
    AssertEqual(IntValueDomain::Sequential(1, 3).Values(), {1, 2, 3});
    AssertEqual(IntValueDomain::FixedSet({4, 1, 4, 2}).Values(), {1, 2, 4});

    try {
        (void)IntValueDomain::PowersOfTwo(0, 16);
        assert(false && "Expected invalid powers-of-two bounds to throw.");
    } catch (const std::invalid_argument &) {
    }
}

void TestExplorationSpecDefaults() {
    const ExplorationSpec spec = ExplorationSpec::Default();
    const ResolvedExplorationSpace resolved = ExplorationSpaceResolver::Resolve(spec);

    AssertEqual(resolved.geometry.numRowMatValues, {1, 2, 4, 8, 16});
    AssertEqual(resolved.geometry.numColumnMatValues, {1, 2, 4, 8, 16});
    AssertEqual(resolved.geometry.numRowSubarrayValues, {1, 2, 4, 8, 16});
    AssertEqual(resolved.geometry.muxSenseAmpValues, {1, 2, 4, 8, 16, 32});
    AssertEqual(resolved.cam.bitSerialWidthValues, {8, 16, 32, 64, 128, 256, 512, 1024});
}

void TestDeepExplorationDefaults() {
    ExplorationSpec spec = ExplorationSpec::Default();
    spec.ApplyDeepExplorationDefaults(4);
    const ResolvedExplorationSpace resolved = ExplorationSpaceResolver::Resolve(spec);

    AssertEqual(resolved.geometry.numRowMatValues, {1, 2, 4, 8, 16, 32, 64});
    AssertEqual(resolved.geometry.muxSenseAmpValues, {1, 2, 4, 8, 16, 32, 64});
    AssertEqual(resolved.wires.localWireRepeaterTypeValues, {repeated_none, repeated_opt});
}

void TestDependentDomains() {
    ExplorationSpec spec = ExplorationSpec::Default();
    AssertEqual(spec.ActiveMatPerRowValues(8), {1, 2, 4, 8});
    AssertEqual(spec.ActiveSubarrayPerColumnValues(4), {1, 2, 4});

    spec.geometry.numActiveMatPerRow = IntValueDomain::PowersOfTwo(2, 8);
    AssertEqual(spec.ActiveMatPerRowValues(8), {2, 4, 8});
}

}  // namespace

int main() {
    TestIntValueDomain();
    TestExplorationSpecDefaults();
    TestDeepExplorationDefaults();
    TestDependentDomains();

         
    std::cout << "Exploration domain tests passed" << std::endl;
    return 0;
}
