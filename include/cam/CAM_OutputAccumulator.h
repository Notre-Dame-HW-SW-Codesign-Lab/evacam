#ifndef CAM_OUTPUTACCUMULATOR_H_
#define CAM_OUTPUTACCUMULATOR_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "EvaCamConfig.h"

class CAM_OutputAccumulator: public FunctionUnit {
    public:
        CAM_OutputAccumulator();
        CAM_OutputAccumulator(const CAM_OutputAccumulator&) = delete;
        CAM_OutputAccumulator& operator=(const CAM_OutputAccumulator&) = delete;
        virtual ~CAM_OutputAccumulator() {}

        /* Functions */
        void PrintProperty();
        void Initialize(double _capLoad, double _resLoad, std::shared_ptr<EvaCamConfig> config);
        void CalculateArea();
        void CalculateRC();
        void CalculateLatency(double _rampInput);
        void CalculatePower();
        /* Properties */
        bool initialized;	/* Initialization flag */
        double capLoad;		/* Load capacitance, Unit: F */
        double resLoad;		/* Load resistance, Unit: ohm */

        double capNandIn, capNandOut;
        double widthNandN, widthNandP;
        OutputDriver outputDriver;
        double rampInput, rampOutput;
};

#endif /* CAM_OUTPUTACCUMULATOR_H_ */
