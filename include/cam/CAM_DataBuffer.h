/*
 * Latches for input/output mem_data
 * refer to ASP-DAC-12-Ohino
 */
#ifndef CAM_DATABUFFER_H_
#define CAM_DATABUFFER_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "EvaCamConfig.h"

class CAM_DataBuffer: public FunctionUnit {
    public:
        CAM_DataBuffer();
        CAM_DataBuffer(const CAM_DataBuffer&) = delete;
        CAM_DataBuffer& operator=(const CAM_DataBuffer&) = delete;
        CAM_DataBuffer(CAM_DataBuffer&&) noexcept = default;
        CAM_DataBuffer& operator=(CAM_DataBuffer&&) noexcept = default;
        virtual ~CAM_DataBuffer() = default;

        /* Functions */
        void PrintProperty();
        void Initialize(bool _differential, double _capLoad, double _resLoad, std::shared_ptr<EvaCamConfig> _config);
        void CalculateArea();
        void CalculateRC();
        void CalculateLatency(double _rampInput);
        void CalculatePower();
        /* Note that this is a single latch, not yet multiplied by number of inputs */

        /* Properties */
        bool initialized;	/* Initialization flag */
        bool differential;	/* The output could be differential since SL may require this */
        double capLoad;		/* Load capacitance, Unit: F */
        double resLoad;		/* Load resistance, Unit: ohm */
        double capNandIn, capNandOut;
        double widthNandN, widthNandP;
        std::unique_ptr<OutputDriver> outputDriver;
        double rampInput, rampOutput;
};


#endif /* CAM_DATABUFFER_H_ */
