#ifndef FACTORIES_WIREFACTORY_H_
#define FACTORIES_WIREFACTORY_H_

#include <memory>

#include "Wire.h"

class EvaCamConfig;

class WireFactory {
    public:
        static Wire CreateDefaultLocalWire(
                const std::shared_ptr<EvaCamConfig> &config);
        static Wire CreateDefaultGlobalWire(
                const std::shared_ptr<EvaCamConfig> &config);
};

#endif /* FACTORIES_WIREFACTORY_H_ */
