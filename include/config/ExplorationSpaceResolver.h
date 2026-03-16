#ifndef EVACAM_CONFIG_EXPLORATIONSPACERESOLVER_H_
#define EVACAM_CONFIG_EXPLORATIONSPACERESOLVER_H_

#include "ExplorationSpec.h"
#include "ResolvedExplorationSpace.h"

class ExplorationSpaceResolver {
    public:
        static ResolvedExplorationSpace Resolve(const ExplorationSpec &spec);
};

#endif  // EVACAM_CONFIG_EXPLORATIONSPACERESOLVER_H_
