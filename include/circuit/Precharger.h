#ifndef PRECHARGER_H_
#define PRECHARGER_H_

#include "FunctionUnit.h"
#include "OutputDriver.h"
#include "EvaCamConfig.h"
#include "Wire.h"

class Precharger: public FunctionUnit {
    public:
        Precharger() {
            initialized = false;
            enableLatency = 0;
        }
        Precharger(const Precharger&) = delete;
        Precharger& operator=(const Precharger&) = delete;
        Precharger(Precharger&&) noexcept = default;
        Precharger& operator=(Precharger&&) noexcept = default;
        virtual ~Precharger() = default;

        /* Functions */
        void PrintProperty();
        void Initialize(
                double _voltagePrecharge, 
                int _numColumn, 
                double _capBitline, 
                double _resBitline,
                std::shared_ptr<EvaCamConfig> _config, 
                const Wire &_localWire);

        void CalculateArea();
        void CalculateRC();
        void CalculateLatency(double _rampInput);
        void CalculatePower();

        /* Properties */
        bool initialized;	    /* Initialization flag */
        OutputDriver outputDriver;
        double voltagePrecharge;    /* Precharge Voltage */
        double capBitline;
        double resBitline;
        double capLoadInv;
        double capOutputBitlinePrecharger;
        double capWireLoadPerColumn;
        double resWireLoadPerColumn;
        double enableLatency;

        int numColumn;		    /* Number of columns */
        double widthPMOSBitlinePrecharger;
        double widthPMOSBitlineEqual;
        double widthInvNmos;
        double widthInvPmos;
        double capLoadPerColumn;
        double rampInput;
        double rampOutput;

        Wire localWire;
};

#endif /* PRECHARGER_H_ */
