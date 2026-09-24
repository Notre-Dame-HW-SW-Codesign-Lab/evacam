#ifndef FACTORIES_NANDCAMFACTORY_H_
#define FACTORIES_NANDCAMFACTORY_H_

#include <memory>

#include "circuit/typedef.h"
#include "model/NandCamModel.h"

std::unique_ptr<NandCamBackend> CreateNandCamBackend(MemCellType type);

#endif  // FACTORIES_NANDCAMFACTORY_H_
