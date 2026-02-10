#ifndef BASICDECODER_H_
#define BASICDECODER_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "InputParameter.h"

class BasicDecoder: public FunctionUnit {
public:
	BasicDecoder() {
            initialized = false;
            outputDriver = std::make_unique<OutputDriver>();
        }
	BasicDecoder(const BasicDecoder&) {}
	virtual ~BasicDecoder() {}

	/* Functions */
	void PrintProperty();
	void Initialize(int _numAddressBit, double _capLoad, double _resLoad, 
                std::shared_ptr<InputParameter> _inputParameter);

	void CalculateArea();
	void CalculateRC();
	void CalculateLatency(double _rampInput);
	void CalculatePower();
        std::unique_ptr<FunctionUnit> clone() const override {
                return std::make_unique<BasicDecoder>(*this);
        }
	
        /* Properties */
	bool initialized;	/* Initialization flag */
        std::unique_ptr<OutputDriver> outputDriver;
	double capLoad;		/* Load capacitance, Unit: F */
	double resLoad;		/* Load resistance, Unit: ohm */
	int numNandInput;	/* Type of NAND, NAND2 or NAND3 */
	int numNandGate;    /* Number of NAND Gates */

	double widthNandN, widthNandP;
	double capNandInput, capNandOutput;
	double rampInput, rampOutput;
	/* TODO: Basic decoder so far does not take OptPriority input because the output driver is already quite fixed in this module */

};

#endif /* BASICDECODER_H_ */
