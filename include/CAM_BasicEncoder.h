/*
 *  CAM_BasicEncoder.h
 *  binary encoder: 8-to-3 only for now
 *  The encoder is z-output with one carry-in signal indicating whether 0 or not
 *  the carry-in signal is using dynamic logic
 */

#ifndef CAM_BASICENCODER_H_
#define CAM_BASICENCODER_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "InputParameter.h"

class CAM_BasicEncoder: public FunctionUnit {
public:
	CAM_BasicEncoder();
	CAM_BasicEncoder(const CAM_BasicEncoder&) {}
	virtual ~CAM_BasicEncoder() {}

	/* Functions */
	void PrintProperty();
	void Initialize(int _numInputBit, double _capLoad, double _resLoad, 
                std::shared_ptr<InputParameter> _inputParameter);
	void CalculateArea();
	void CalculateRC();
	void CalculateLatency(double _rampInput);
	void CalculatePower();
        std::unique_ptr<FunctionUnit> clone() const override {
                return std::make_unique<CAM_BasicEncoder>(*this);
        }

	/* Properties */
	bool initialized;	/* Initialization flag */
	OutputDriver outputDriver;
	double capLoad;		/* Load capacitance, Unit: F */
	double resLoad;		/* Load resistance, Unit: ohm */
	int numInputBit;
	int numNorInput;	/* Type of Nor */
	int numNorGate;     /* Number of Nor Gates */
        int temperature;

	double widthNorN, widthNorP;
	double widthNandN, widthNandP;
	double capNorInput, capNorOutput;
	double capNandInput, capNandOutput;
	double capInvInput, capInvOutput;
	double rampInput, rampOutput;
	double widthN, widthP;
	double capDyn;

	/* TODO: Basic decoder so far does not take OptPriority input because the output driver is already quite fixed in this module */

};


#endif /* CAM_BASICENCODER_H_ */
