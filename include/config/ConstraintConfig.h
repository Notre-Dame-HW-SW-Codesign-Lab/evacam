#ifndef EVACAM_CONFIG_CONSTRAINTCONFIG_H_
#define EVACAM_CONFIG_CONSTRAINTCONFIG_H_

struct ConstraintConfig {
    double readLatency = 1e41;
    double writeLatency = 1e41;
    double readDynamicEnergy = 1e41;
    double writeDynamicEnergy = 1e41;
    double readEdp = 1e41;
    double writeEdp = 1e41;
    double area = 1e41;
    double leakage = 1e41;

    bool enabled = false;
    bool pruningEnabled = false;
};

#endif  // EVACAM_CONFIG_CONSTRAINTCONFIG_H_
