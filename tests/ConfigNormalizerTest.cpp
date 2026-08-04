#include "config/ConfigNormalizer.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "EvaCamConfig.h"
#include "config/IntValueDomain.h"
#include "TestSupport.h"

namespace {

bool DomainsEqual(const IntValueDomain &lhs, const IntValueDomain &rhs) {
    return lhs.Kind() == rhs.Kind() && lhs.Values() == rhs.Values();
}

void AssertDomain(const IntValueDomain &actual, const IntValueDomain &expected) {
    assert(DomainsEqual(actual, expected));
}

void AssertSameGeometry(const GeometryExplorationSpec &actual,
        const GeometryExplorationSpec &expected) {
    AssertDomain(actual.numRowMat, expected.numRowMat);
    AssertDomain(actual.numColumnMat, expected.numColumnMat);
    AssertDomain(actual.numActiveMatPerRow, expected.numActiveMatPerRow);
    AssertDomain(actual.numActiveMatPerColumn, expected.numActiveMatPerColumn);
    AssertDomain(actual.numRowSubarray, expected.numRowSubarray);
    AssertDomain(actual.numColumnSubarray, expected.numColumnSubarray);
    AssertDomain(actual.numActiveSubarrayPerRow, expected.numActiveSubarrayPerRow);
    AssertDomain(actual.numActiveSubarrayPerColumn, expected.numActiveSubarrayPerColumn);
    AssertDomain(actual.numRow, expected.numRow);
    AssertDomain(actual.numColumn, expected.numColumn);
    AssertDomain(actual.muxSenseAmp, expected.muxSenseAmp);
    AssertDomain(actual.muxOutputLev1, expected.muxOutputLev1);
    AssertDomain(actual.muxOutputLev2, expected.muxOutputLev2);
}

void TestOrdinaryDefaultsRestrictDefaultGeometry() {
    EvaCamConfig config;

    ConfigNormalizer::Normalize(config);

    assert(!config.exploration.deepExploration);
    assert(!config.exploration.useCactiAssumption);
    AssertDomain(config.exploration.geometry.numRowMat, IntValueDomain::PowersOfTwo(1, 4));
    AssertDomain(config.exploration.geometry.numColumnMat, IntValueDomain::PowersOfTwo(1, 4));
    AssertDomain(config.exploration.geometry.numRowSubarray, IntValueDomain::PowersOfTwo(1, 8));
    AssertDomain(config.exploration.geometry.numColumnSubarray, IntValueDomain::PowersOfTwo(1, 8));
    AssertDomain(config.exploration.geometry.muxSenseAmp, IntValueDomain::PowersOfTwo(1, 32));
}

void TestDeepExplorationExpandsOnlyDefaultDomains() {
    EvaCamConfig config;
    config.requestDeepExploration = true;

    ConfigNormalizer::Normalize(config);

    assert(config.exploration.deepExploration);
    AssertDomain(config.exploration.geometry.numRowMat, IntValueDomain::PowersOfTwo(1, 64));
    AssertDomain(config.exploration.geometry.numColumnMat, IntValueDomain::PowersOfTwo(1, 64));
    AssertDomain(config.exploration.geometry.muxSenseAmp, IntValueDomain::PowersOfTwo(1, 64));
    AssertDomain(config.exploration.geometry.muxOutputLev1, IntValueDomain::PowersOfTwo(1, 64));
    AssertDomain(config.exploration.geometry.muxOutputLev2, IntValueDomain::PowersOfTwo(1, 64));
    AssertDomain(config.exploration.geometry.numRowSubarray, IntValueDomain::PowersOfTwo(1, 16));
    AssertDomain(config.exploration.geometry.numColumnSubarray, IntValueDomain::PowersOfTwo(1, 16));
}

void TestExplicitGeometryAndFixedDomainArePreserved() {
    EvaCamConfig config;
    config.exploration.geometry.numRowMat = IntValueDomain::FixedSet({1, 2, 4, 8, 16});
    config.exploration.geometry.numColumnMat = IntValueDomain::FixedSet({2, 8});
    config.exploration.geometry.numRowSubarray = IntValueDomain::PowersOfTwo(2, 4);
    config.exploration.geometry.numColumnSubarray = IntValueDomain::PowersOfTwo(4, 8);
    config.exploration.geometry.muxSenseAmp = IntValueDomain::PowersOfTwo(2, 8);
    config.exploration.geometry.muxOutputLev1 = IntValueDomain::FixedSet({2});
    config.exploration.geometry.muxOutputLev2 = IntValueDomain::FixedSet({4, 8});
    const GeometryExplorationSpec expected = config.exploration.geometry;

    ConfigNormalizer::Normalize(config);

    AssertSameGeometry(config.exploration.geometry, expected);
}

void TestCactiAssumptionOverridesActiveGeometry() {
    EvaCamConfig config;
    config.useCactiAssumption = true;
    config.exploration.geometry.numColumnMat = IntValueDomain::PowersOfTwo(2, 8);
    config.exploration.geometry.numActiveMatPerRow = IntValueDomain::PowersOfTwo(1, 4);
    config.exploration.geometry.numActiveMatPerColumn = IntValueDomain::PowersOfTwo(2, 4);
    config.exploration.geometry.numRowSubarray = IntValueDomain::PowersOfTwo(4, 8);
    config.exploration.geometry.numColumnSubarray = IntValueDomain::PowersOfTwo(4, 8);
    config.exploration.geometry.numActiveSubarrayPerRow = IntValueDomain::PowersOfTwo(1, 8);
    config.exploration.geometry.numActiveSubarrayPerColumn = IntValueDomain::PowersOfTwo(1, 8);

    ConfigNormalizer::Normalize(config);

    assert(config.exploration.useCactiAssumption);
    AssertDomain(config.exploration.geometry.numActiveMatPerRow, IntValueDomain::PowersOfTwo(8, 8));
    AssertDomain(config.exploration.geometry.numActiveMatPerColumn, IntValueDomain::PowersOfTwo(1, 1));
    AssertDomain(config.exploration.geometry.numRowSubarray, IntValueDomain::PowersOfTwo(2, 2));
    AssertDomain(config.exploration.geometry.numColumnSubarray, IntValueDomain::PowersOfTwo(2, 2));
    AssertDomain(config.exploration.geometry.numActiveSubarrayPerRow, IntValueDomain::PowersOfTwo(2, 2));
    AssertDomain(config.exploration.geometry.numActiveSubarrayPerColumn, IntValueDomain::PowersOfTwo(2, 2));
}

void TestNormalizeIsIdempotent() {
    EvaCamConfig config;
    config.requestDeepExploration = true;
    config.useCactiAssumption = true;
    config.exploration.geometry.numColumnMat = IntValueDomain::PowersOfTwo(2, 8);

    ConfigNormalizer::Normalize(config);
    const ExplorationSpec expected = config.exploration;
    ConfigNormalizer::Normalize(config);

    assert(config.exploration.deepExploration == expected.deepExploration);
    assert(config.exploration.useCactiAssumption == expected.useCactiAssumption);
    AssertSameGeometry(config.exploration.geometry, expected.geometry);
}

void TestConflictingDomainsArePreservedWithoutCactiAssumption() {
    EvaCamConfig config;
    config.exploration.geometry.numRowMat = IntValueDomain::FixedSet({1});
    config.exploration.geometry.numColumnMat = IntValueDomain::FixedSet({2});
    config.exploration.geometry.numActiveMatPerRow = IntValueDomain::FixedSet({8});
    config.exploration.geometry.numActiveMatPerColumn = IntValueDomain::FixedSet({4});
    config.exploration.geometry.numRowSubarray = IntValueDomain::FixedSet({1});
    config.exploration.geometry.numColumnSubarray = IntValueDomain::FixedSet({1});
    const GeometryExplorationSpec expected = config.exploration.geometry;

    ConfigNormalizer::Normalize(config);

    AssertSameGeometry(config.exploration.geometry, expected);
}

void TestCactiAssumptionRejectsNonPositiveColumnDomain() {
    EvaCamConfig config;
    config.useCactiAssumption = true;
    config.exploration.geometry.numColumnMat = IntValueDomain::FixedSet({0});

    TestSupport::AssertThrows<std::invalid_argument>(
            [&config]() { ConfigNormalizer::Normalize(config); },
            "PowersOfTwo domain requires positive bounds.");
}

}  // namespace

int main() {
    TestOrdinaryDefaultsRestrictDefaultGeometry();
    TestDeepExplorationExpandsOnlyDefaultDomains();
    TestExplicitGeometryAndFixedDomainArePreserved();
    TestCactiAssumptionOverridesActiveGeometry();
    TestNormalizeIsIdempotent();
    TestConflictingDomainsArePreservedWithoutCactiAssumption();
    TestCactiAssumptionRejectsNonPositiveColumnDomain();

    std::cout << "Config normalizer tests passed" << std::endl;
    return 0;
}
