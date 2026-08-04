#ifndef DERIVEDVALUEHELPERS_H_
#define DERIVEDVALUEHELPERS_H_

#include "config/InputConfig.h"
#include "config/ResolvedExplorationSpace.h"

class DerivedValueHelpers {
    public:
        static long long EffectiveCapacityBits(const InputConfig &input);
        static long EffectiveBlockSizeBits(const InputConfig &input);
        static bool IsFullExploration(const InputConfig &input);
        static bool ShouldWriteExplorationCsv(const InputConfig &input);
        static bool HasFixedOuterGeometry(const ResolvedExplorationSpace &resolvedExploration);
};

#endif /* DERIVEDVALUEHELPERS_H_ */
