#ifndef EVACAM_CONFIG_TECHNOLOGYLOADER_H_
#define EVACAM_CONFIG_TECHNOLOGYLOADER_H_

#include "config/InputConfig.h"
#include "config/PeripheralConfig.h"
#include "config/TechnologyContext.h"
#include "config/VariationConfig.h"

class TechnologyLoader {
    public:
        static TechnologyContext Load(
                const InputConfig &input,
                const PeripheralConfig &peripherals,
                VariationConfig *variation = nullptr);
};

#endif  // EVACAM_CONFIG_TECHNOLOGYLOADER_H_
