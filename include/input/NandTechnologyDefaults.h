#ifndef INPUT_NANDTECHNOLOGYDEFAULTS_H_
#define INPUT_NANDTECHNOLOGYDEFAULTS_H_

#include <memory>

class EvaCamConfig;
class MemCell;

// Resolve only omitted, supported NAND inputs using the configured technology.
// Explicit inputs are never replaced. These are CMOS estimates, not NAND data.
void ApplyNandTechnologyDefaults(MemCell &cell,
        const std::shared_ptr<EvaCamConfig> &context);

#endif
