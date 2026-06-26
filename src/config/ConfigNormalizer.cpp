#include "config/ConfigNormalizer.h"

#include "EvaCamConfig.h"

namespace {

bool SameDomain(const IntValueDomain &lhs, const IntValueDomain &rhs) {
    return lhs.Kind() == rhs.Kind() && lhs.Values() == rhs.Values();
}

void ApplyExplorationDefaults(EvaCamConfig &config) {
    const ExplorationSpec base = ExplorationSpec::Default();
    if (config.requestDeepExploration) {
        config.exploration.deepExploration = true;
        if (SameDomain(config.exploration.geometry.numRowMat, base.geometry.numRowMat)) {
            config.exploration.geometry.numRowMat = IntValueDomain::PowersOfTwo(1, 64);
        }
        if (SameDomain(config.exploration.geometry.numColumnMat, base.geometry.numColumnMat)) {
            config.exploration.geometry.numColumnMat = IntValueDomain::PowersOfTwo(1, 64);
        }
        if (SameDomain(config.exploration.geometry.muxSenseAmp, base.geometry.muxSenseAmp)) {
            config.exploration.geometry.muxSenseAmp = IntValueDomain::PowersOfTwo(1, 64);
        }
        if (SameDomain(config.exploration.geometry.muxOutputLev1, base.geometry.muxOutputLev1)) {
            config.exploration.geometry.muxOutputLev1 = IntValueDomain::PowersOfTwo(1, 64);
        }
        if (SameDomain(config.exploration.geometry.muxOutputLev2, base.geometry.muxOutputLev2)) {
            config.exploration.geometry.muxOutputLev2 = IntValueDomain::PowersOfTwo(1, 64);
        }
        return;
    }

    config.exploration.deepExploration = false;
    if (SameDomain(config.exploration.geometry.numRowMat, base.geometry.numRowMat)) {
        config.exploration.geometry.numRowMat = IntValueDomain::PowersOfTwo(1, 4);
    }
    if (SameDomain(config.exploration.geometry.numColumnMat, base.geometry.numColumnMat)) {
        config.exploration.geometry.numColumnMat = IntValueDomain::PowersOfTwo(1, 4);
    }
    if (SameDomain(config.exploration.geometry.numRowSubarray, base.geometry.numRowSubarray)) {
        config.exploration.geometry.numRowSubarray = IntValueDomain::PowersOfTwo(1, 8);
    }
    if (SameDomain(config.exploration.geometry.numColumnSubarray, base.geometry.numColumnSubarray)) {
        config.exploration.geometry.numColumnSubarray = IntValueDomain::PowersOfTwo(1, 8);
    }
}

void ApplyCactiAssumption(EvaCamConfig &config) {
    config.exploration.useCactiAssumption = config.useCactiAssumption;
    if (!config.useCactiAssumption) {
        return;
    }

    const int numColumnMat = config.exploration.geometry.numColumnMat.Max();
    config.exploration.geometry.numActiveMatPerRow = IntValueDomain::PowersOfTwo(numColumnMat, numColumnMat);
    config.exploration.geometry.numActiveMatPerColumn = IntValueDomain::PowersOfTwo(1, 1);
    config.exploration.geometry.numRowSubarray = IntValueDomain::PowersOfTwo(2, 2);
    config.exploration.geometry.numColumnSubarray = IntValueDomain::PowersOfTwo(2, 2);
    config.exploration.geometry.numActiveSubarrayPerRow = IntValueDomain::PowersOfTwo(2, 2);
    config.exploration.geometry.numActiveSubarrayPerColumn = IntValueDomain::PowersOfTwo(2, 2);
}

}  // namespace

void ConfigNormalizer::Normalize(EvaCamConfig &config) {
    ApplyExplorationDefaults(config);
    ApplyCactiAssumption(config);
}
