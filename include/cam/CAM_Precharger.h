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
        CAM_Precharger(const CAM_Precharger&) = delete;
        CAM_Precharger& operator=(const CAM_Precharger&) = delete;
        CAM_Precharger(CAM_Precharger&&) noexcept = default;
        CAM_Precharger& operator=(CAM_Precharger&&) noexcept = default;
        ~CAM_Precharger() override = default;

        /* Functions */
        void Initialize(double _voltagePrecharge, int _numColumn, double _capBitline, double _resBitline, 
                std::shared_ptr<EvaCamConfig> _config, const Wire &_localWire);
        void CalculateArea();
};


#endif /* CAM_PRECHARGER_H_ */
