#ifndef FUNCTIONUNIT_H_
#define FUNCTIONUNIT_H_

#include <memory>
#include <stdexcept>
#include <string>

#include "EvaCamConfig.h"

/* Unused
   struct SearchPerf{
   double hit;
   double oneMiss;
   double allMiss;
   double* Miss;
   };
 */

class FunctionUnit {
    public:
        FunctionUnit();
        FunctionUnit(const FunctionUnit&) = default;
        virtual ~FunctionUnit() = default;

        /* Functions */
        virtual void PrintProperty();

        /* Properties */
        double height;		/* Unit: m */
        double width;		/* Unit: m */
        double area;		/* Unit: m^2 */
        double readLatency, writeLatency;		/* Unit: s */
        double readDynamicEnergy, writeDynamicEnergy;	/* Unit: J */
        double leakage;		/* Unit: W */

        /* Optional properties (not valid for all the memory cells */
        double setLatency, resetLatency;				/* Unit: s */
        double setDynamicEnergy, resetDynamicEnergy;	/* Unit: J */
        double cellReadEnergy, cellSetEnergy, cellResetEnergy;			/* Unit: J */

        std::shared_ptr<EvaCamConfig> config;

    protected:
        [[noreturn]] static void ThrowInitializationError(const char *component) {
            throw std::runtime_error(std::string(component) + " Error: Require initialization first!");
        }
};

#endif /* FUNCTIONUNIT_H_ */
