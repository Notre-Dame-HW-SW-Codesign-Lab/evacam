#ifndef EVACAM_CONFIG_VALIDATOR_H_
#define EVACAM_CONFIG_VALIDATOR_H_

class EvaCamConfig;

class EvaCamConfigValidator {
    public:
        static void Validate(EvaCamConfig &config);
};

#endif  // EVACAM_CONFIG_VALIDATOR_H_
