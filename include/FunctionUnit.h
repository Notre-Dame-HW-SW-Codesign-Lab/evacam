#ifndef FUNCTIONUNIT_H_
#define FUNCTIONUNIT_H_

#include <memory>
#include <iostream>

#include "InputParameter.h"

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
        virtual std::unique_ptr<FunctionUnit> clone() const = 0;


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

        std::shared_ptr<InputParameter> inputParameter;
};

#endif /* FUNCTIONUNIT_H_ */
