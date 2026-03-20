/*
 * Inherit Precharger.cpp in NVsim_origin
 * Modification: get rid of equalization circuit
 */
#ifndef CAM_PRECHARGER_H_
#define CAM_PRECHARGER_H_

#include "Precharger.h"

class CAM_Precharger: public Precharger {
    public:
        CAM_Precharger() {
            initialized = false;
            enableLatency = 0;
        }
        //CAM_Precharger(const CAM_Precharger&) {}
        ~CAM_Precharger() {}

        /* Functions */
        void Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline, 
                std::shared_ptr<EvaCamConfig> _config, std::shared_ptr<Wire> _localWire);
        void CalculateArea();
        CAM_Precharger & operator=(const CAM_Precharger &);
};


#endif /* CAM_PRECHARGER_H_ */
