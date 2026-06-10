#include "factories/WireFactory.h"

#include "EvaCamConfig.h"
#include "Wire.h"

Wire WireFactory::CreateDefaultLocalWire(
        const std::shared_ptr<EvaCamConfig> &config) {
    Wire wire;

    const WireType wireType = config->exploration.wires.localWireType.IsFixed()
        ? static_cast<WireType>(config->exploration.wires.localWireType.Min())
        : local_aggressive;

    const WireRepeaterType repeaterType = config->exploration.wires.localWireRepeaterType.IsFixed()
        ? static_cast<WireRepeaterType>(config->exploration.wires.localWireRepeaterType.Min())
        : repeated_none;

    const bool isLowSwing = config->exploration.wires.isLocalWireLowSwing.IsFixed()
        ? static_cast<bool>(config->exploration.wires.isLocalWireLowSwing.Min())
        : false;

    wire.Initialize(config->input.processNode, wireType, repeaterType,
            config->input.temperature, isLowSwing, config);

    return wire;
}

Wire WireFactory::CreateDefaultGlobalWire(
        const std::shared_ptr<EvaCamConfig> &config) {
    
    Wire wire;

    const WireType wireType = config->exploration.wires.globalWireType.IsFixed()
        ? static_cast<WireType>(config->exploration.wires.globalWireType.Min())
        : global_aggressive;

    const WireRepeaterType repeaterType = config->exploration.wires.globalWireRepeaterType.IsFixed()
        ? static_cast<WireRepeaterType>(config->exploration.wires.globalWireRepeaterType.Min())
        : repeated_none;

    const bool isLowSwing = config->exploration.wires.isGlobalWireLowSwing.IsFixed()
        ? static_cast<bool>(config->exploration.wires.isGlobalWireLowSwing.Min())
        : false;

    wire.Initialize(config->input.processNode, wireType, repeaterType,
            config->input.temperature, isLowSwing, config);

    return wire;
}
