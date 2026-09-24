#include <memory>
#include <stdexcept>

#include "factories/NandCamFactory.h"
#include "model/Nand3dCamModel.h"

std::unique_ptr<NandCamBackend> CreateNandCamBackend(MemCellType type) {
    switch (type) {
        case SLCNAND: return std::make_unique<NandCamModel>();
        case NAND3D: return std::make_unique<Nand3dCamModel>();
        default: throw std::invalid_argument("NAND backend requires SLCNAND or NAND3D memory device");
    }
}
