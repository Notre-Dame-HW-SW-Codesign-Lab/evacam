#include "CAM_RowNand.h"
#include "formula.h"

void CAM_RowNand::Initialize(int _numRow, double _capLoad, double _resLoad,
        bool _multipleRowPerSet, bool _inv, BufferDesignTarget _areaOptimizationLevel, 
        double _minDriverCurrent, std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[Row Decoder] Warning: Already initialized!";

    numRow = _numRow;
    capLoad = _capLoad;
    resLoad = _resLoad;
    multipleRowPerSet = _multipleRowPerSet;
    areaOptimizationLevel = _areaOptimizationLevel;
    minDriverCurrent = _minDriverCurrent;
    driverInv = _inv;
    config = _config;

    if (numRow <= 8) {	/* The predecoder output is used directly */
        if (multipleRowPerSet)
            numNandInput = 2;	/* NAND way-select with predecoder output */
        else
            numNandInput = 0;	/* no circuit needed */
    } else {
        if (multipleRowPerSet)
            numNandInput = 3;	/* NAND way-select with two predecoder outputs */
        else
            numNandInput = 2;	/* just NAND two predecoder outputs */
    }

    outputDriver = std::make_unique<OutputDriver>();

    if (numNandInput > 0) {
        double logicEffortNand;
        double capNand;
        if (numNandInput == 2) {	/* NAND2 */
            widthNandN = 2 * MIN_NMOS_SIZE * config->technology.tech->featureSize();
            logicEffortNand = (2+config->technology.tech->pnSizeRatio()) / (1+config->technology.tech->pnSizeRatio());
        } else {					/* NAND3 */
            widthNandN = 3 * MIN_NMOS_SIZE * config->technology.tech->featureSize();
            logicEffortNand = (3+config->technology.tech->pnSizeRatio()) / (1+config->technology.tech->pnSizeRatio());
        }
        widthNandP = config->technology.tech->pnSizeRatio() * MIN_NMOS_SIZE * config->technology.tech->featureSize();
        capNand = CalculateGateCap(widthNandN, config->technology.tech) + CalculateGateCap(widthNandP, config->technology.tech);
        // begin_change
        //outputDriver->Initialize(logicEffortNand, capNand, capLoad, resLoad, true, areaOptimizationLevel, minDriverCurrent);
        outputDriver->Initialize(logicEffortNand, capNand, capLoad, resLoad, driverInv, areaOptimizationLevel, minDriverCurrent, config);
        // end_change
    } else {
        /* we only need an 1-level output buffer to driver the wordline */
        double capInv;
        widthNandN = MIN_NMOS_SIZE * config->technology.tech->featureSize();
        widthNandP = config->technology.tech->pnSizeRatio() * MIN_NMOS_SIZE * config->technology.tech->featureSize();
        capInv = CalculateGateCap(widthNandN, config->technology.tech) + CalculateGateCap(widthNandP, config->technology.tech);
        // begin_change
        //outputDriver->Initialize(1, capInv, capLoad, resLoad, true, areaOptimizationLevel, minDriverCurrent);
        outputDriver->Initialize(1, capInv, capLoad, resLoad, driverInv, areaOptimizationLevel, minDriverCurrent, config);
        // end_change
    }

    if (outputDriver->invalid) {
        invalid = true;
        std::cout << "invalid output driver" << std::endl;
        return;
    }

    initialized = true;
    CalculateArea();
    CalculateRC();
    CalculatePower();
}
