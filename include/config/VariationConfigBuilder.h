#ifndef CONFIG_VARIATIONCONFIGBUILDER_H_
#define CONFIG_VARIATIONCONFIGBUILDER_H_

#include "config/VariationConfig.h"

class MemCell;

class VariationConfigBuilder {
    public:
        static VariationConfig FromCell(const MemCell &cell);
};

#endif  // CONFIG_VARIATIONCONFIGBUILDER_H_
