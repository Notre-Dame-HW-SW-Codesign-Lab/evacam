#ifndef FACTORIES_WIREFACTORY_H_
#define FACTORIES_WIREFACTORY_H_

#include <memory>

class EvaCamConfig;
class Wire;

class WireFactory {
    public:
        static std::shared_ptr<Wire> CreateDefaultLocalWire(
                const std::shared_ptr<EvaCamConfig> &config);
        static std::shared_ptr<Wire> CreateDefaultGlobalWire(
                const std::shared_ptr<EvaCamConfig> &config);
};

#endif /* FACTORIES_WIREFACTORY_H_ */
