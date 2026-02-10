#ifndef COMPARATOR_H_
#define COMPARATOR_H_

#include "FunctionUnit.h"
#include "constant.h"
#include "InputParameter.h"

class Comparator: public FunctionUnit {
public:
	Comparator() {
                initialized = false;
                capLoad = 0;
                rampOutput = 1e40;
        }
	Comparator(const Comparator&) {}
	virtual ~Comparator() {}

	/* Functions */
	void PrintProperty() override;
	void Initialize(int _numTagBits, double _capLoad, std::shared_ptr<InputParameter> _inputParameter);
	void CalculateArea();
	void CalculateRC();
	void CalculateLatency(double _rampInput);
	void CalculatePower();
	Comparator & operator=(const Comparator &);
        std::unique_ptr<FunctionUnit> clone() const override {
                return std::make_unique<Comparator>(*this);
        }

	/* Properties */
	bool initialized;	/* Initialization flag */
	int numTagBits;     /* Number of tag bits */
	double capLoad;     /* Load Capacitance */
	double widthNMOSInv[COMPARATOR_INV_CHAIN_LEN];
	double widthPMOSInv[COMPARATOR_INV_CHAIN_LEN];
	double widthNMOSComp;
	double widthPMOSComp;
	double capInput[COMPARATOR_INV_CHAIN_LEN];
	double capOutput[COMPARATOR_INV_CHAIN_LEN];
	double capBottom, capTop, resBottom, resTop;
	double rampInput, rampOutput;
};

#endif /* COMPARATOR_H_ */
