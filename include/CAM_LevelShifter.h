/*
 * Latches for input/output mem_data
 * refer to ASP-DAC-12-Ohino
 */
#ifndef CAM_LEVELSHIFTER_H_
#define CAM_LEVELSHIFTER_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "InputParameter.h"

class CAM_LevelShifter: public FunctionUnit {
public:
	CAM_LevelShifter();
	CAM_LevelShifter(const CAM_LevelShifter&) {}
	virtual ~CAM_LevelShifter() {}

	/* Functions */
	void PrintProperty();
	void Initialize(int _numInputBit, double _capLoad, double _resLoad, std::shared_ptr<InputParameter> inputParameter);
	void CalculateArea();
	void CalculateRC();
	void CalculateLatency(double _rampInput);
	void CalculatePower();
	/* Note that this is a single latch, not yet multiplied by number of inputs */
	CAM_LevelShifter & operator=(const CAM_LevelShifter &);
        std::unique_ptr<FunctionUnit> clone() const override {
                return std::make_unique<CAM_LevelShifter>(*this);
        }

	/* Properties */
	bool initialized;	/* Initialization flag */
	double capLoad;		/* Load capacitance, Unit: F */
	double resLoad;		/* Load resistance, Unit: ohm */
	int numInputBit;	/* Number of input bits */
	double capNandIn, capNandOut;
	double widthNandN, widthNandP;
	// OutputDriver outputDriver;
	double rampInput, rampOutput;
};


#endif /* CAM_LEVELSHIFTER_H_ */
