#ifndef FUNCTIONUNIT_H_
#define FUNCTIONUNIT_H_

#include <memory>
#include <stdexcept>
#include <string>

#include "EvaCamConfig.h"

class FunctionUnit {
    public:
        FunctionUnit();
        FunctionUnit(const FunctionUnit&) = default;
        virtual ~FunctionUnit() = default;

        /* Functions */
        virtual void PrintProperty();

        /* Properties */
        double height;		    /* Unit: m */
        double width;		    /* Unit: m */
        double area;		    /* Unit: m^2 */
        double readLatency;         /* Unit: s */
        double writeLatency;	    /* Unit: s */
        double readDynamicEnergy;   /* Unit: J */
        double writeDynamicEnergy;  /* Unit: J */
        double leakage;		    /* Unit: W */

        /* Optional properties (not valid for all the memory cells */
        double setLatency;          /* Unit: s */
        double resetLatency;	    /* Unit: s */
        double setDynamicEnergy;    /* Unit: J */ 
        double resetDynamicEnergy;  /* Unit: J */
        double cellReadEnergy;      /* Unit: J */
        double cellSetEnergy;       /* Unit: J */
        double cellResetEnergy;	    /* Unit: J */

        std::shared_ptr<EvaCamConfig> config;

    protected:
        [[noreturn]] static void ThrowInitializationError(const char *component) {
            throw std::runtime_error(std::string(component) + " Error: Require initialization first!");
        }
};

#endif /* FUNCTIONUNIT_H_ */
