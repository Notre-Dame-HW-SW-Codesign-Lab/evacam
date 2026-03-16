#include "config/EvaCamConfigValidator.h"

#include <stdexcept>

#include "EvaCamConfig.h"
#include "formula.h"

void EvaCamConfigValidator::Validate(EvaCamConfig &config) {
    if (config.technology.cell->memCellType == DRAM)
        throw std::runtime_error("[ERROR] DRAM model is still under development");

    if (config.technology.cell->memCellType == eDRAM)
        throw std::runtime_error("[ERROR] Embedded DRAM model is still under development");

    if (config.technology.cell->memCellType == MLCNAND)
        throw std::runtime_error("[ERROR] MLC NAND flash model is still under development");

    if (config.input.designTarget != cache && config.input.associativity > 1) {
        config.logger.Verbose() << "[WARNING] Associativity setting is ignored for non-cache designs.";
        config.input.associativity = 1;
    }

    if (!isPow2(config.input.associativity))
        throw std::runtime_error("[ERROR] The associativity value has to be a power of 2 in this version.");

    if (config.input.routingMode == h_tree && config.input.internalSensing == false)
        throw std::runtime_error("[ERROR] H-tree does not support external sensing scheme in this version.");
}
