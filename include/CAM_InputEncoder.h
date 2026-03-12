/*
 * optional encoding input mem_data to reduce leakage during searching
 * refer to JSSC-11-Li
 */
#ifndef CAM_INPUTENCODER_H_
#define CAM_INPUTENCODER_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "typedef.h"
#include "EvaCamConfig.h"

class CAM_InputEncoder: public FunctionUnit {
    public:
        CAM_InputEncoder();
        CAM_InputEncoder(const CAM_InputEncoder&) {}
        virtual ~CAM_InputEncoder() {}

        /* Functions */
        void PrintProperty();
        void Initialize(TypeOfInputEncoder _typeEncoder, bool _isCustom, 
                double _capLoad, double _resLoad, std::shared_ptr<EvaCamConfig> _config);
        void ReadCustomDesign(char* _fileName);
        void CalculateArea();
        void CalculateRC();
        void CalculateLatency(double _rampInput);
        void CalculatePower();
        /* Note that this is a single encoder, not yet multiplied by number of inputs */
        CAM_InputEncoder & operator=(const CAM_InputEncoder &);
        std::unique_ptr<FunctionUnit> clone() const override {
            return std::make_unique<CAM_InputEncoder>(*this);
        }

        /* Properties */
        bool initialized;	/* Initialization flag */
        TypeOfInputEncoder typeEncoder;	/* Encoder type look up table index */
        bool isCustom;    	/* Enable custom parameter or not */
        int numInputBits;   /* Number of input bits */
        double capNandIn, capNandOut;
        double widthNandN, widthNandP;
        double capLoad;		/* Load capacitance, Unit: F */
        double resLoad;		/* Load resistance, Unit: ohm */
        std::shared_ptr<OutputDriver> outputDriver;
        double rampInput, rampOutput;
};

#endif /* CAM_INPUTENCODER_H_ */
