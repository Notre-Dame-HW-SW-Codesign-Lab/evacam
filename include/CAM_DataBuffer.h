/*
 * Latches for input/output mem_data
 * refer to ASP-DAC-12-Ohino
 */
#ifndef CAM_DATABUFFER_H_
#define CAM_DATABUFFER_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "InputParameter.h"

class CAM_DataBuffer: public FunctionUnit {
public:
	CAM_DataBuffer();
	CAM_DataBuffer(const CAM_DataBuffer&) {}
	virtual ~CAM_DataBuffer() {}

	/* Functions */
	void PrintProperty();
	void Initialize(bool _differential, double _capLoad, double _resLoad, std::shared_ptr<InputParameter> _inputParameter);
	void CalculateArea();
	void CalculateRC();
	void CalculateLatency(double _rampInput);
	void CalculatePower();
	/* Note that this is a single latch, not yet multiplied by number of inputs */
	CAM_DataBuffer & operator=(const CAM_DataBuffer &);
        std::unique_ptr<FunctionUnit> clone() const override {
                return std::make_unique<CAM_DataBuffer>(*this);
        }

	/* Properties */
	bool initialized;	/* Initialization flag */
	bool differential;	/* The output could be differential since SL may require this */
	double capLoad;		/* Load capacitance, Unit: F */
	double resLoad;		/* Load resistance, Unit: ohm */
	double capNandIn, capNandOut;
	double widthNandN, widthNandP;
	std::shared_ptr<OutputDriver> outputDriver;
	double rampInput, rampOutput;
};


#endif /* CAM_DATABUFFER_H_ */
