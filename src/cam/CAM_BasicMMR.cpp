#include "CAM_BasicMMR.h"
#include "formula.h"

#include <cstring>

CAM_BasicMMR::CAM_BasicMMR() {
    initialized = false;
    capLoad = 0;
    resLoad = 0;
    numInputBits = 8;
    capLAintra = capLAout = 0;
    memset(capD, 0, sizeof(double)*4);
    capInvIn = capInvOut = 0;
    widthN = widthP = 0;
    capLaLoad = resLaLoad = 0;
    rampInput = 0;
    rampOutput = 0;
    LookAheadLatency = 0;
    rampLAout = 0;
    capIn = 0;
}

void CAM_BasicMMR::Initialize(int _numInputBits, double _capLoad, double _resLoad, 
        double _capLaLoad, double _resLaLoad, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[CAM_MMR] Warning: Already initialized!";
    capLoad = _capLoad;
    resLoad = _resLoad;
    capLaLoad = _capLaLoad;
    resLaLoad = _resLaLoad;
    numInputBits = _numInputBits;
    config= _config;
    /* gate sizing: used as trans-gates */
    widthN = MIN_NMOS_SIZE * config->technology.tech->featureSize();
    widthP = config->technology.tech->pnSizeRatio() * MIN_NMOS_SIZE * config->technology.tech->featureSize();
    if(numInputBits == 8) {
        double tmp1, tmp2, tmp3, tmp;
        // footed dynamic logic, Nmos sized as 2 in NOR, n+1 in NAND
        CalculateGateCapacitance(NOR, 8, widthN*2, 0, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp1, &capLAintra);
        CalculateGateCapacitance(NOR, 4, widthN*2, 0, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp2, &capLAout);
        CalculateGateCapacitance(INV, 1, widthN*3, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, capD);
        CalculateGateCapacitance(INV, 1, widthN*4, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, capD+1);
        CalculateGateCapacitance(INV, 1, widthN*5, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, capD+2);
        CalculateGateCapacitance(INV, 1, widthN*6, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp3, capD+3);
        CalculateGateCapacitance(INV, 1, widthN, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &capInvIn, &capInvOut);
        double logicEffort = 2 / (1+config->technology.tech->pnSizeRatio());
        LookAheadDriver.Initialize(logicEffort, capLAout, capLaLoad, resLaLoad, true, latency_first, 0, config);
        capIn = tmp1 + tmp2 + tmp3 + capInvIn;
    } else {
        // TODO 4-input and 2-input MMR
        throw std::runtime_error("[CAM_BasicMMR] Error: only 8-input MMR blocks are supported.");
    }
    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}

void CAM_BasicMMR::CalculateArea(){
    if (!initialized) {
        ThrowInitializationError("[CAM_BasicMMR]");
    } else {
        area = 0;
        if(numInputBits == 8) {
            LookAheadDriver.CalculateArea();
            area += LookAheadDriver.area;
            double hLAintra, wLAintra;
            double hLAout, wLAout;
            double hD[4], wD[4];
            double hInv, wInv;
            // both inverter and footed clock in the dynamic circuits
            CalculateGateArea(INV, 1, widthN, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &hInv, &wInv, config->peripherals.useUpdatedLib);
            // the LA for output, dynamic logic for 8-input NOR

            CalculateGateArea(NOR, 8, widthN*2, 0, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &hLAout, &wLAout, config->peripherals.useUpdatedLib);
            hLAout += hInv;
            wLAout =MAX(wLAout, wInv);
            // the LA for internal, dynamic logic for 4-input NOR
            CalculateGateArea(NOR, 4, widthN*2, 0, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &hLAintra, &wLAintra, config->peripherals.useUpdatedLib);
            hLAintra += hInv;
            wLAintra =MAX(wLAintra, wInv);
            // the mem_data line: 2/3/4/5-input NAND dynamic logic
            for(int i=0;i<4;i++){
                CalculateGateArea(NAND, i+2, widthN*(i+3), 0, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, hD+i, wD+i, config->peripherals.useUpdatedLib);
                hD[i] += hInv;
                wD[i] =MAX(wD[i], wInv);
                area += (hD[i]*wD[i]);
            }
            // TODO: a better layout
            double wDSum = wD[0] + wD[1] + wD[2] + wD[3];
            height = MAX(hLAout, wDSum);
            // inverter used as differential input signal, input LA and internal LA
            area += ( hLAout*wLAout + hLAintra*wLAintra + hInv*wInv * (8+1+1) );
            width = area / height;
        } else {
            // TODO 4-input and 2-input MMR
            throw std::runtime_error("[CAM_BasicMMR] Error: only 8-input MMR blocks are supported.");
        }
    }
}

void CAM_BasicMMR::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[CAM_BasicMMR]");
    } else {
        if(numInputBits == 8) {
            LookAheadDriver.CalculateRC();
            double tmp;
            // footed dynamic logic, Nmos sized as 2 in NOR, n+1 in NAND
            CalculateGateCapacitance(NOR, 8, widthN*2, 0, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, &capLAintra);
            CalculateGateCapacitance(NOR, 4, widthN*2, 0, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, &capLAout);
            CalculateGateCapacitance(INV, 1, widthN*3, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, capD);
            CalculateGateCapacitance(INV, 1, widthN*4, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, capD+1);
            CalculateGateCapacitance(INV, 1, widthN*5, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, capD+2);
            CalculateGateCapacitance(INV, 1, widthN*6, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &tmp, capD+3);
            CalculateGateCapacitance(INV, 1, widthN, widthP, config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, config->technology.tech, &capInvIn, &capInvOut);
        } else {
            // TODO 4-input and 2-input MMR
            throw std::runtime_error("[CAM_BasicMMR] Error: only 8-input MMR blocks are supported.");
        }
    }
}

void CAM_BasicMMR::CalculateLatency(double _rampInput) {
    if (!initialized) {
        ThrowInitializationError("[CAM_BasicMMR]");
    } else {
        LookAheadLatency = 0;
        if(numInputBits == 8) {
            rampInput = _rampInput;
            // double lLAintra; // TODO: why is this unused?
            double resPullDown;
            double cap;
            double tr;		/* time constant */
            double gm;		/* transconductance */
            double beta;	/* for horowitz calculation */
            double rampInternal;
            // Output look ahead latency (worst case, only one input is turned on)
            resPullDown = CalculateOnResistance(widthN*2, NMOS, config->input.temperature, config->technology.tech);
            gm = CalculateTransconductance(widthN*2, NMOS, config->technology.tech);
            beta = 1 / (resPullDown * gm);
            cap = capLAout + LookAheadDriver.capInput[0];
            tr = resPullDown * cap;
            LookAheadLatency = horowitz(tr, beta, rampInput, &rampInternal);
            LookAheadDriver.CalculateLatency(rampInternal);
            rampLAout = LookAheadDriver.rampOutput;

            // Internal look ahead latency (worst case, only one input is turned on)
            resPullDown = CalculateOnResistance(widthN*2, NMOS, config->input.temperature, config->technology.tech);
            gm = CalculateTransconductance(widthN*2, NMOS, config->technology.tech);
            beta = 1 / (resPullDown * gm);
            // internal look ahead controls other 4 mem_data path, each dynamic circuit equals to inverter
            cap = capLAintra + capInvIn*4;
            tr = resPullDown * cap;
            //lLAintra = horowitz(tr, beta, rampInput, &rampInternal); // TODO: why is this set but unused?

            // D4/8 the longest path (include the inverter and the dynamic NAND)
            // inverter part
            resPullDown = CalculateOnResistance(widthN, NMOS, config->input.temperature, config->technology.tech);
            gm = CalculateTransconductance(widthN, NMOS, config->technology.tech);
            beta = 1 / (resPullDown * gm);
            // worst case inverter drives 3 other mem_data lines
            cap = capInvIn*3;
            tr = resPullDown * cap;
            readLatency = horowitz(tr, beta, rampInput, &rampInternal);
            // dynamic logic part
            resPullDown = CalculateOnResistance(widthN*(4+1), NMOS, config->input.temperature, config->technology.tech);
            gm = CalculateTransconductance(widthN*(4+1), NMOS, config->technology.tech);
            beta = 1 / (resPullDown * gm);
            cap = capD[3] + capLoad;
            tr = resPullDown * cap;
            readLatency += horowitz(tr, beta, rampInternal, &rampInternal);
            rampOutput = rampInternal;
            writeLatency = readLatency;
        } else {
            // TODO 4-input and 2-input MMR
            throw std::runtime_error("[CAM_BasicMMR] Error: only 8-input MMR blocks are supported.");
        }
    }
}

void CAM_BasicMMR::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[CAM_BasicMMR]");
    } else {
        readDynamicEnergy = 0;
        leakage = 0;
        if(numInputBits == 8) {
            LookAheadDriver.CalculatePower();
            // leakage calculation
            leakage += LookAheadDriver.leakage;
            // dynamic logic leakage calculation, only Pmos matters
            int tempIndex = (int)config->input.temperature - 300;
            if ((tempIndex > 100) || (tempIndex < 0)) {
                throw std::runtime_error("Error: Temperature is out of range");
            }
            const double *leakP = config->technology.tech->currentOffPmos().data();
            double leakageP = widthP * leakP[tempIndex];
            double leakageInv = CalculateGateLeakage(INV, 1, widthN, widthP, config->input.temperature, config->technology.tech);
            // TODO: double check
            leakage += (leakageP*(1+1+8) + leakageInv*(6+2)) * config->technology.tech->vdd();

            double cap;
            // dynamic calculation (worst case)
            // for the LA out
            cap = LookAheadDriver.capInput[0] + capLAout;
            readDynamicEnergy += (cap * config->technology.tech->vdd() * config->technology.tech->vdd());
            // for the LA internal
            cap = capLAintra + capInvIn*4;
            readDynamicEnergy += (cap * config->technology.tech->vdd() * config->technology.tech->vdd());
            // for the inverter
            cap = capInvIn*3;
            readDynamicEnergy += (cap * config->technology.tech->vdd() * config->technology.tech->vdd());
            // for the mem_data path
            cap = capD[3] + capLoad;
            readDynamicEnergy += (cap * config->technology.tech->vdd() * config->technology.tech->vdd() * 8);
            writeDynamicEnergy = readDynamicEnergy;
        } else {
            // TODO 4-input and 2-input MMR
            throw std::runtime_error("[CAM_BasicMMR] Error: only 8-input MMR blocks are supported.");
        }
    }
}

void CAM_BasicMMR::PrintProperty() {
    std::cout << "CAM_MMR Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

CAM_BasicMMR & CAM_BasicMMR::operator=(const CAM_BasicMMR &rhs) {
    height = rhs.height;
    width = rhs.width;
    area = rhs.area;
    readLatency = rhs.readLatency;
    writeLatency = rhs.writeLatency;
    readDynamicEnergy = rhs.readDynamicEnergy;
    writeDynamicEnergy = rhs.writeDynamicEnergy;
    leakage = rhs.leakage;
    initialized = rhs.initialized;
    capLoad = rhs.capLoad;
    resLoad = rhs.resLoad;
    numInputBits = rhs.numInputBits;
    capLAintra = rhs.capLAintra;
    capLAout = rhs.capLAout;
    capD[0] = rhs.capD[0];
    capD[1] = rhs.capD[1];
    capD[2] = rhs.capD[2];
    capD[3] = rhs.capD[3];
    capInvIn = rhs.capInvIn;
    capInvOut = rhs.capInvOut;
    widthN = rhs.widthN;
    widthP = rhs.widthP;
    rampInput = rhs.rampInput;
    rampOutput = rhs.rampOutput;
    LookAheadLatency = rhs.LookAheadLatency;
    rampLAout = rhs.rampLAout;
    capIn = rhs.capIn;
    return *this;
}
