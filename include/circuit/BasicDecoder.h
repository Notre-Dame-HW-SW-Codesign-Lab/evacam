#ifndef BASICDECODER_H_
#define BASICDECODER_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "EvaCamConfig.h"

class BasicDecoder: public FunctionUnit {
    public:
        BasicDecoder() {
            initialized = false;
        }
        BasicDecoder(const BasicDecoder&) = delete;
        BasicDecoder& operator=(const BasicDecoder&) = delete;
        BasicDecoder(BasicDecoder&&) noexcept = default;
        BasicDecoder& operator=(BasicDecoder&&) noexcept = default;
        virtual ~BasicDecoder() = default;

        /* Functions */
        void PrintProperty();
        void Initialize(int _numAddressBit, 
                double _capLoad, 
                double _resLoad, 
                std::shared_ptr<EvaCamConfig> _config);

        void CalculateArea();
        void CalculateRC();
        void CalculateLatency(double _rampInput);
        void CalculatePower();

        /* Properties */
        bool initialized;	/* Initialization flag */
        OutputDriver outputDriver;
        double capLoad;		/* Load capacitance, Unit: F */
        double resLoad;		/* Load resistance, Unit: ohm */
        int numNandInput;	/* Type of NAND, NAND2 or NAND3 */
        int numNandGate;        /* Number of NAND Gates */

        double widthNandN, widthNandP;
        double capNandInput, capNandOutput;
        double rampInput, rampOutput;
        /* TODO: Basic decoder so far does not take OptPriority input because the output driver is already quite fixed in this module */

};

#endif /* BASICDECODER_H_ */
