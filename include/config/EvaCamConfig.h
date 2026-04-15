#ifndef EVACAMCONFIG_H_
#define EVACAMCONFIG_H_

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <stdint.h>
#include <vector>

#include "Logger.h"
#include "config/ConstraintConfig.h"
#include "config/ExplorationSpec.h"
#include "config/ExplorationSpaceResolver.h"
#include "config/InputConfig.h"
#include "config/PeripheralConfig.h"
#include "config/ResolvedExplorationSpace.h"
#include "config/RuntimeSizingConfig.h"
#include "config/TechnologyContext.h"
#include "config/VariationConfig.h"
#include "typedef.h"
#include "Technology.h"
#include "MemCell.h"

class Wire;
class Result;
class Bank;
struct CAM_Opt;

struct ResultLimits {
    double readLatency;
    double writeLatency;
    double readDynamicEnergy;
    double writeDynamicEnergy;
    double readEdp;
    double writeEdp;
    double area;
    double leakage;
};

class EvaCamConfig {
    public:
        EvaCamConfig()
            : exploration(ExplorationSpec::Default()),
              resolvedExploration(ExplorationSpaceResolver::Resolve(exploration)) {}
        EvaCamConfig(const EvaCamConfig&) = delete;
        virtual ~EvaCamConfig() {}

        /* Functions */
        void ReadConfigFromFile(const std::string & inputFile);
        void SetDeepExploration(bool enabled);
        ResultLimits BuildResultLimits(const std::vector<std::shared_ptr<Result>> &bestResults) const;
        void ApplyResultLimits(const ResultLimits &limits, const std::vector<std::shared_ptr<Result>> &results) const;

        /* Properties */
        Logger logger;
        InputConfig input;
        PeripheralConfig peripherals;
        VariationConfig variation;
        TechnologyContext technology;
        ConstraintConfig constraints;
        bool useCactiAssumption = false;		/* Use the CACTI assumptions on the array organization */
        bool requestDeepExploration = false;

        RuntimeSizingConfig runtimeSizing;

        ExplorationSpec exploration;
        ResolvedExplorationSpace resolvedExploration;
};

#endif /* EVACAMCONFIG_H_ */
