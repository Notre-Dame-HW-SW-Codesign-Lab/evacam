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
    const IntValueDomain powers = IntValueDomain::PowersOfTwo(1, 16);
    AssertEqual(powers.Values(), {1, 2, 4, 8, 16});
    assert(powers.Kind() == ValueDomainKind::PowersOfTwo);
    assert(powers.Min() == 1);
    assert(powers.Max() == 16);
    assert(powers.Contains(8));
    assert(!powers.Contains(3));
    assert(!powers.IsFixed());

    AssertEqual(IntValueDomain::PowersOfTwo(3, 12).Values(), {3, 6, 12});
    const IntValueDomain sequential = IntValueDomain::Sequential(1, 3);
    AssertEqual(sequential.Values(), {1, 2, 3});
    assert(sequential.Kind() == ValueDomainKind::Sequential);
    const IntValueDomain fixed = IntValueDomain::FixedSet({4, 1, 4, 2});
    AssertEqual(fixed.Values(), {1, 2, 4});
    assert(fixed.Kind() == ValueDomainKind::FixedSet);
    assert(fixed.Min() == 1);
    assert(fixed.Max() == 4);
    assert(fixed.Contains(2));
    assert(!fixed.Contains(3));
    assert(!fixed.IsFixed());
    assert(IntValueDomain::FixedSet({7, 7}).IsFixed());

    try {
        (void)IntValueDomain::PowersOfTwo(0, 16);
        assert(false && "Expected invalid powers-of-two bounds to throw.");
    } catch (const std::invalid_argument &) {
    }
    try {
        (void)IntValueDomain::Sequential(2, 1);
        assert(false && "Expected invalid sequential bounds to throw.");
    } catch (const std::invalid_argument &) {
    }
    try {
        (void)IntValueDomain::FixedSet({});
        assert(false && "Expected empty fixed set to throw.");
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
    spec.ApplyDeepExplorationDefaults();
    const ResolvedExplorationSpace resolved = ExplorationSpaceResolver::Resolve(spec);

    AssertEqual(resolved.geometry.numRowMatValues, {1, 2, 4, 8, 16, 32, 64});
    AssertEqual(resolved.geometry.muxSenseAmpValues, {1, 2, 4, 8, 16, 32, 64});
    AssertEqual(resolved.wires.localWireRepeaterTypeValues, {repeated_none, repeated_opt});
}

void TestDependentDomains() {
    ExplorationSpec spec = ExplorationSpec::Default();
    AssertEqual(spec.ActiveMatPerRowValues(8), {1, 2, 4, 8});
    AssertEqual(spec.ActiveMatPerColumnValues(4), {1, 2, 4});
    AssertEqual(spec.ActiveSubarrayPerRowValues(2), {1, 2});
    AssertEqual(spec.ActiveSubarrayPerColumnValues(4), {1, 2, 4});

    spec.geometry.numActiveMatPerRow = IntValueDomain::PowersOfTwo(2, 8);
    AssertEqual(spec.ActiveMatPerRowValues(8), {2, 4, 8});
    AssertEqual(spec.ActiveMatPerRowValues(1), {1});

    assert(spec.IsValidPartitioning(64, 2, 2, 2, 2));
    assert(!spec.IsValidPartitioning(15, 2, 2, 2, 2));
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
