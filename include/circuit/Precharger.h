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
        Precharger(const Precharger&) {}
        virtual ~Precharger() {}

        /* Functions */
        void PrintProperty();
        void Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline,
                std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire);
        void CalculateArea();
        void CalculateRC();
        void CalculateLatency(double _rampInput);
        void CalculatePower();
        Precharger & operator=(const Precharger &);

        /* Properties */
        bool initialized;	/* Initialization flag */
        std::shared_ptr<OutputDriver> outputDriver;
        double voltagePrecharge;  /* Precharge Voltage */
        double capBitline, resBitline;
        double capLoadInv;
        double capOutputBitlinePrecharger;
        double capWireLoadPerColumn, resWireLoadPerColumn;
        double enableLatency;
        int numColumn;			/* Number of columns */
        double widthPMOSBitlinePrecharger, widthPMOSBitlineEqual;
        double widthInvNmos, widthInvPmos;
        double capLoadPerColumn;
        double rampInput, rampOutput;

        std::shared_ptr<Wire> localWire;
};

#endif /* PRECHARGER_H_ */
