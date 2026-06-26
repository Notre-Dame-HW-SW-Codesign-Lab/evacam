#include "config/EvaCamConfigValidator.h"

#include <stdexcept>

#include "EvaCamConfig.h"

void EvaCamConfigValidator::Validate(EvaCamConfig &config) {
    if (config.technology.cell->memCellType == DRAM)
        throw std::runtime_error("[ERROR] DRAM model is still under development");

    if (config.technology.cell->memCellType == eDRAM)
        throw std::runtime_error("[ERROR] Embedded DRAM model is still under development");

    if (config.technology.cell->memCellType == MLCNAND)
        throw std::runtime_error("[ERROR] MLC NAND flash model is still under development");

    if (config.input.routingMode == h_tree && config.input.internalSensing == false)
        throw std::runtime_error("[ERROR] H-tree does not support external sensing scheme in this version.");
}
