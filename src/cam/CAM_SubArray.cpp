/*
 * CAM_SubArray.cpp
 *
 */
#include "CAM_SubArray.h"
#include "formula.h"
#include "constant.h"
#include "CAM_Line.h"
#include "MemCell.h"
#include "macros.h"

#include <algorithm>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <random>
#include <stdexcept>

namespace {

bool UsesSramStyleCamModel(MemCellType type) {
    return type == SRAM || type == FEFETRAM;
}

bool UsesResistiveCamModel(MemCellType type) {
    return type == MRAM || type == PCRAM || type == memristor;
}

struct MatchlineElectricalParams {
    double nominalResCellAccessOff = 0;
    double nominalResMatchTranOff = 0;
    double resMemCellOff = 0;
    double resCellAccess = 0;
    double resMatchTran = 0;
    double resMemCellOn = 0;
    double capCellAccess = 0;
};

int MatchlineMosType(const CAMPort &cellPort) {
    return cellPort.isNMOS ? NMOS : PMOS;
}

double MatchlineOffCurrent(const CAMPort &cellPort, const Technology &tech, int temperature) {
    const auto &offCurrents = cellPort.isNMOS ? tech.currentOffNmos() : tech.currentOffPmos();
    return offCurrents[temperature - 300];
}

MatchlineElectricalParams BuildDrainSourceMatchlineParams(
        const CAMPort &cellPort,
        const MemCell &cell,
        const Technology &tech,
        int temperature) {

    MatchlineElectricalParams params;
    const int mosType = MatchlineMosType(cellPort);
    const double featureSize = tech.featureSize();
    const double cmosWidth = cellPort.widthCmos * featureSize;
    const double cellWidth = cell.widthInFeatureSize * featureSize;
    const double offCurrent = MatchlineOffCurrent(cellPort, tech, temperature);

    params.nominalResCellAccessOff = tech.vdd() / offCurrent / featureSize / cellPort.numCmos;
    if (!cellPort.isNMOS && cell.isNVMdischarge) {
        params.nominalResCellAccessOff /= 2;
    }

    params.nominalResMatchTranOff = cell.isNVMdischarge ? cell.resistanceOff / 2 : 0;
    params.resMatchTran = cell.isNVMdischarge ? cell.resistanceOn : 0;

    const double accessMultiplier = (cellPort.isNMOS == cell.isNVMdischarge) ? cellPort.numCmos : 1;
    params.resCellAccess = CalculateOnResistance(cmosWidth, mosType, temperature, tech) * accessMultiplier;

    if (cellPort.isNMOS && cell.isNVMdischarge) {
        params.capCellAccess = (CalculateDrainCap(cmosWidth * 3, NMOS, cellWidth * 3, tech)
                + cell.capacitanceOff) * cellPort.numCmos;
    } else {
        params.capCellAccess = CalculateDrainCap(cmosWidth, mosType, cellWidth, tech) * cellPort.numCmos;
    }

    params.resMemCellOff = params.nominalResCellAccessOff + params.nominalResMatchTranOff;
    params.resMemCellOn = params.resCellAccess + params.resMatchTran;
    return params;
}

MatchlineElectricalParams BuildDiodeMatchlineParams(
        const CAMPort &cellPort,
        const MemCell &cell,
        const Technology &tech,
        int temperature) {
    MatchlineElectricalParams params;
    const int mosType = MatchlineMosType(cellPort);
    const double featureSize = tech.featureSize();
    const double cmosWidth = cellPort.widthCmos * featureSize;
    const double cellWidth = cell.widthInFeatureSize * featureSize;
    const double offCurrent = MatchlineOffCurrent(cellPort, tech, temperature);

    params.nominalResCellAccessOff = tech.vdd() / offCurrent / featureSize / cellPort.widthCmos * cellPort.numCmos;
    if (cell.isNVMdischarge) {
        params.nominalResCellAccessOff /= 4;
    }

    params.nominalResMatchTranOff = cell.isNVMdischarge ? cell.resistanceOff / 2 : 0;
    params.resMatchTran = cell.isNVMdischarge ? cell.resistanceOn : 0;

    const double accessMultiplier = (cellPort.isNMOS && cell.isNVMdischarge) ? 1 : cellPort.numCmos;
    params.resCellAccess = CalculateOnResistance(cmosWidth, mosType, temperature, tech) * accessMultiplier;
    params.capCellAccess = CalculateDrainCap(cmosWidth, mosType, cellWidth, tech) * cellPort.numCmos
        + CalculateGateCap(cmosWidth, tech);

    params.resMemCellOff = params.nominalResCellAccessOff + params.nominalResMatchTranOff;
    params.resMemCellOn = params.resCellAccess + params.resMatchTran;
    return params;
}

MatchlineElectricalParams BuildFloatingMatchlineParams(
        const CAMPort &cellPort,
        const MemCell &cell,
        const Technology &fefetTech) {
    MatchlineElectricalParams params;

    if (cell.memCellType == FEFETRAM) {
        params.capCellAccess = CalculateDrainCap(
                cellPort.widthCmos * fefetTech.featureSize(),
                NMOS,
                cell.widthInFeatureSize * fefetTech.featureSize(),
                fefetTech) * cellPort.numCmos;
    }

    params.nominalResMatchTranOff = cell.resistanceOff / cellPort.numCmos;
    params.resMemCellOff = params.nominalResMatchTranOff;
    params.resMatchTran = cell.resistanceOn;
    params.resMemCellOn = params.resMatchTran;
    return params;
}

double CombineStdev(double a, double b) {
    return std::sqrt(a * a + b * b);
}

uint32_t MixVariationSeed(uint32_t baseSeed, uint32_t sampleIndex, uint32_t streamOffset) {
    // Deterministically derive a per-sample, per-stream seed without relying on
    // ad hoc stride constants.
    uint32_t z = baseSeed;
    z ^= sampleIndex + 0x9e3779b9u + (z << 6) + (z >> 2);
    z ^= streamOffset + 0x85ebca6bu + (z << 6) + (z >> 2);
    return z;
}

CAMMetricStats BuildMetricStats(const std::vector<double> &samples, double nominal) {
    CAMMetricStats stats;
    if (samples.empty()) {
        return stats;
    }

    stats.available = true;
    stats.nominal = nominal;
    stats.sample = samples.front();
    stats.min = *std::min_element(samples.begin(), samples.end());
    stats.max = *std::max_element(samples.begin(), samples.end());

    double sum = 0;
    for (double value : samples)
        sum += value;
    stats.mean = sum / samples.size();

    double sqError = 0;
    for (double value : samples) {
        const double delta = value - stats.mean;
        sqError += delta * delta;
    }
    stats.stddev = std::sqrt(sqError / samples.size());

    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    const size_t p95Index = static_cast<size_t>(std::ceil(0.95 * sorted.size())) - 1;
    stats.p95 = sorted[std::min(p95Index, sorted.size() - 1)];
    return stats;
}

CAMMetricStats BuildSinglePointMetric(double sample, double nominal) {
    CAMMetricStats stats;
    stats.available = true;
    stats.nominal = nominal;
    stats.sample = sample;
    return stats;
}

}  // namespace


void CAM_SubArray::Initialize(
        long long _numRow, 
        long long _numColumn, 
        bool _multipleRowPerSet, 
        bool _split,
        int _muxSenseAmp, 
        bool _internalSenseAmp, 
        int _muxOutputLev1, 
        int _muxOutputLev2,
        BufferDesignTarget _DecMergeOptLevel, 
        BufferDesignTarget _DriverOptLevel,
        bool _withInputEnc, 
        TypeOfInputEncoder _typeInputEnc, 
        bool _customInputEnc,
        TypeOfSenseAmp _typeSenseAmp, 
        bool _customSenseAmp, 
        bool _withWriteDriver,
        bool _withOutputAcc, 
        bool _withPriorityEnc, 
        BufferDesignTarget _PriorityOptLevel,
        bool _withInputBuf, 
        bool _withOutputBuf, 
        CAMType _camType, 
        SearchFunction _searchFunction, 
        bool _withVariation, 
        std::shared_ptr<EvaCamConfig> _config, 
        const Wire &_localWire,
        const CAM_Opt &_CAM_opt) {

    auto &logger = _config->logger;
    if (initialized) logger.Verbose() << "[CAM_SubArray] Warning: Already initialized!";

    const auto &input = _config->input;
    const auto &runtimeSizing = _config->runtimeSizing;
    const auto &geometry = _config->exploration.geometry;
    const auto &peripherals = _config->peripherals;
    const auto &cell = *_config->technology.cell;
    const auto &tech = *_config->technology.tech;
    const auto &fefetTech = *_config->technology.fefetTech;

    numRow = _numRow;
    numColumn = _numColumn;
    multipleRowPerSet = _multipleRowPerSet;
    split = _split;
    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;
    internalSenseAmp = _internalSenseAmp;
    DecMergeOptLevel = _DecMergeOptLevel;
    DriverOptLevel = _DriverOptLevel;
    withInputEnc = _withInputEnc;
    typeInputEnc = _typeInputEnc;

    customInputEnc = _customInputEnc;
    withWriteDriver = _withWriteDriver;
    typeSenseAmp = _typeSenseAmp;
    customSenseAmp = _customSenseAmp;
    withOutputAcc = _withOutputAcc;
    withPriorityEnc = _withPriorityEnc;
    PriorityOptLevel = _PriorityOptLevel;
    withInputBuf = _withInputBuf;
    withOutputBuf = _withOutputBuf;
    camType = _camType;
    searchFunction = _searchFunction;
    withVariation = _withVariation;
    config = _config;
    localWire = _localWire;
    CAM_opt = _CAM_opt;

    rowDecoder = std::make_unique<RowDecoder>();
    bitlineMuxDecoder = std::make_unique<RowDecoder>();
    senseAmpMuxLev1Decoder = std::make_unique<RowDecoder>();
    senseAmpMuxLev2Decoder = std::make_unique<RowDecoder>();
    bitlineMux = std::make_unique<Mux>();

    inputBuf = std::make_unique<CAM_DataBuffer>();
    outputBuf = std::make_unique<CAM_DataBuffer>();
    inputLS = std::make_unique<CAM_LevelShifter>();
    outputLS = std::make_unique<CAM_LevelShifter>();
    inputEnc = std::make_unique<CAM_InputEncoder>();
    RowDecMergeNand = std::make_unique<CAM_RowNand>();

    precharger = std::make_unique<CAM_Precharger>();

    senseAmp = std::make_unique<CAM_SenseAmp>();

    ColDecMergeNand = std::make_unique<RowDecoder>();

    senseAmpMuxLev1Nand = std::make_unique<RowDecoder>();
    senseAmpMuxLev2Nand = std::make_unique<RowDecoder>();
    senseAmpMuxLev1 = std::make_unique<Mux>();
    senseAmpMuxLev2 = std::make_unique<Mux>();

    outputAcc = std::make_unique<CAM_OutputAccumulator>();
    priorityEnc = std::make_unique<CAM_PriorityEncoder>();

    RowDriver.resize(MAX_PORT);
    WriteDriver.resize(MAX_PORT);
    ColMux.resize(MAX_PORT);
    Row.resize(MAX_PORT);
    Col.resize(MAX_PORT);

    /****************************************************************************
     * Input Check
     ****************************************************************************/

    if (!runtimeSizing.hasFixedSubarrayDimensions
            && runtimeSizing.realCapacity != input.capacity
            && runtimeSizing.realCapacity != 0) {

        // deal with the ASP-DAC12 72-bit word
        numRow = runtimeSizing.realCapacity / geometry.numRowSubarray.Min()
            / geometry.numColumnSubarray.Min()
            / geometry.numActiveMatPerRow.Min()
            / geometry.numActiveMatPerColumn.Min() / numColumn;
    }

    /*****************************************************************************
     * Derived parameters
     *****************************************************************************/

    numSenseAmp = numColumn / muxSenseAmp;
    lenRow = (double)numColumn * cell.widthInFeatureSize * tech.featureSize();
    lenCol = (double)numRow * cell.heightInFeatureSize * tech.featureSize();

    // Add stitching overhead if necessary
    if (cell.stitching) {
        lenRow += ((numColumn - 1) / cell.stitching + 1) 
            * STITCHING_OVERHEAD * tech.featureSize();
    }

    /*****************************************************************************
     * Calculation for SA
     *****************************************************************************/

    // 1. setting SA sense mode
    // TODO: note that cmos-based/doide-based has to be current-in voltage sensing
    // 5. calc precharge voltage
    // 6. calc for sensing: resMemCellOff, voltageMemCellOff

    voltageSense = bool(cell.readMode > 0);
    senseVoltage = cell.minSenseVoltage;


    if (!internalSenseAmp && cell.memCellType != SRAM) {
        invalid = true;
        logger.Verbose() << "[CAM_SubArray] nvTCAM does not support external sense amplifiers.";
        return;
    }

    for(int i = 0; i < cell.camNumCol; i++) {
        Col[i].Initialize(false, i, lenCol, numRow, config, localWire);
    }

    /*****************************************************************************
     * Calculate CAM cell resistance and capacitance on ML
     * moved to helper functions for increased clarity
     *****************************************************************************/

    indexMatchline = -1;
    resCellAccess = 0;
    resMatchTran = 0;
    nominalResCellAccessOff = 0;
    nominalResMatchTranOff = 0;

    for (int i = 0; i < cell.camNumCol; i++) {
        const auto &cellPort = Col[i].CellPort;

        if (cellPort.Type == Matchline || cellPort.Type == Matchline_Bitline) {
            indexMatchline = i;

            if (cellPort.ConnectedRegion == gate) {
                invalid = true;
                logger.Log() << "[Warning]: Impractical matchline connection (gate).";
                return;
            }

            MatchlineElectricalParams params;
            if (cellPort.ConnectedRegion == drain || cellPort.ConnectedRegion == source) {
                params = BuildDrainSourceMatchlineParams(cellPort, cell, tech, input.temperature);
            } else if (cellPort.ConnectedRegion == diode) {
                params = BuildDiodeMatchlineParams(cellPort, cell, tech, input.temperature);
            } else if (cellPort.ConnectedRegion == none) {
                params = BuildFloatingMatchlineParams(cellPort, cell, fefetTech);
            } else {
                invalid = true;
                logger.Verbose() << "[CAM_SubArray] Unsupported access type.";
                return;
            }

            nominalResCellAccessOff = params.nominalResCellAccessOff;
            nominalResMatchTranOff = params.nominalResMatchTranOff;
            resMemCellOff = params.resMemCellOff;
            resCellAccess = params.resCellAccess;
            resMatchTran = params.resMatchTran;
            resMemCellOn = params.resMemCellOn;
            capCellAccess = params.capCellAccess;
            break;

            // need to account for other cases for some variables to be initialized
        } else if (cellPort.Type == Bitline) {
            invalid = true;
            logger.Verbose() << "[CAM_SubArray] Unsupported column bitline topology before matchline modeling is established.";
            return;
        }
    }

    if (indexMatchline < 0) {
        invalid = true;
        logger.Verbose() << "[CAM_SubArray] No matchline found.";
        return;
    }

    nominalMatchlineWireRes = Col[indexMatchline].res;
    nominalResCellAccess    = resCellAccess;
    nominalResMatchTran     = resMatchTran;
    nominalResMemCellOn     = resMemCellOn;
    nominalResMemCellOff    = resMemCellOff;
    sampledResistance       = BuildResistanceSample();
    matchlineWireRes        = sampledResistance.mlWireRes;
    resMemCellOn            = sampledResistance.cellResOn;
    resMemCellOff           = sampledResistance.cellResOff;

    /* Assume the precharge voltage as Vdd to be more simplified and generalized, 
     * some other precharge voltage designs also exist
     * E.g. diode access design many just need to charge to Vmatch+vth
     * CMOS access design could be half swing
     * None access design could be n*volatgeMemOff
     */

    voltagePrecharge = tech.vdd();

    /*****************************************************************************
     * Calculation for driver
     *****************************************************************************
        2. line resistance caclualtion
        3. Mux load calculation: extended from signel bl to cols
        4. transistor's impact on res and cap of the lines
    */

    for (int i = 0; i < cell.camNumRow; i++) {
        Row[i].Initialize(true, i, lenRow, numColumn, config, localWire);
    }
    for (int i = 0; i < cell.camNumCol; i++) {
        Col[i].Initialize(false, i, lenCol, numRow, config, localWire);
        if (Col[i].minMuxWidth > input.maxNmosSize * tech.featureSize()) {
            invalid = true;
            logger.Verbose() << "[CAM_SubArray] Column mux width exceeds the supported maximum.";
            return;
        }
    }

    /*****************************************************************************
     * Peripherals initialization
     *****************************************************************************/
    
    double capNandInput, tmp;
    const double hvNmosWidth = 2 * MIN_NMOS_SIZE * tech.featureSize();
    const double hvPmosWidth = tech.pnSizeRatio() * MIN_NMOS_SIZE * tech.featureSize();
    const double hvCellHeight = tech.featureSize() * MAX_TRANSISTOR_HEIGHT * 3;
    const double outputBufNmosWidth = 8 * MIN_NMOS_SIZE * tech.featureSize();

    CalculateGateCapacitance(
            NAND,
            2,
            hvNmosWidth,
            hvPmosWidth,
            hvCellHeight,
            tech,
            &capNandInput,
            &tmp);

    if (withInputBuf) {
        inputBuf->Initialize(true /*TODO*/, capNandInput, 0, config);
    }

    inputLS->Initialize(true /*TODO*/, capNandInput, 0, config);
    outputLS->Initialize(true /*TODO*/, capNandInput, 0, config);

    if (withInputEnc) {
        inputEnc->Initialize(encoding_two_bit, false, capNandInput, 0, config/* TODO*/);
        inputEnc->CalculateRC();
    }

    // this NAND merges pre-decoder's result, output the WL activation signal

    auto rowDecMergeNandNumRow = cell.memCellType != SRAM ? numRow * 2 : numRow;
    RowDecMergeNand->Initialize(
                rowDecMergeNandNumRow,
                capNandInput,
                0,
                false /*TODO*/,
                true,
                DecMergeOptLevel,
                0,
                config /*TODO*/);
    
    // those NAND merges the WL and SL signal

    for (int i = 0; i < cell.camNumRow; i++) {
        RowDriver[i] = std::make_unique<CAM_RowNand>();
        RowDriver[i]->Initialize(
                numRow,
                Row[i].cap * 1.6, // TODO: verify the 1.6 scaling constant, it may need to be different for FeFET
                                  //       previous maintainer tried 10 at one point for FeFET 
                Row[i].res,
                false/*TODO*/,
                false,
                DriverOptLevel,
                Row[i].maxCurrent,
                config);
    }

    precharger = std::make_unique<CAM_Precharger>();
    precharger->Initialize(voltagePrecharge, numColumn, Col[indexMatchline].cap, matchlineWireRes, config, localWire);

    // Model the shared column-decoder merge line against the worst-case column mux width.
    double maxColumnMuxWidth = 0.0;
    for (int i = 0; i < cell.camNumCol; i++) {
        maxColumnMuxWidth = MAX(maxColumnMuxWidth, Col[i].minMuxWidth);
    }

    Row[cell.camNumRow].Initialize(lenRow, numColumn, maxColumnMuxWidth, config, localWire);
    ColDecMergeNand->Initialize(
            cell.camNumCol * muxSenseAmp,
            Row[cell.camNumRow].cap,
            Row[cell.camNumRow].res,
            false,
            DecMergeOptLevel,
            0,
            config);

    senseAmpMuxLev1Nand->Initialize(
            muxOutputLev1,
            Row[cell.camNumRow].cap * 1.6,
            Row[cell.camNumRow].res,
            false,
            DecMergeOptLevel,
            0,
            config);

    senseAmpMuxLev2Nand->Initialize(
            muxOutputLev2,
            Row[cell.camNumRow].cap * 1.6,
            Row[cell.camNumRow].res,
            false,
            DecMergeOptLevel,
            0,
            config);

    // MUX
    senseAmpMuxLev2->Initialize(
            muxOutputLev2,
            numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2,
            0,
            0,
            Col[indexMatchline].maxCurrent,
            config);

    senseAmpMuxLev1->Initialize(
            muxOutputLev1,
            numColumn / muxSenseAmp / muxOutputLev1,
            senseAmpMuxLev2->capForPreviousDelayCalculation,
            senseAmpMuxLev2->capForPreviousPowerCalculation,
            Col[indexMatchline].maxCurrent,
            config);

    if (internalSenseAmp) {
        senseAmp->Initialize(
                numColumn / muxSenseAmp,
                typeSenseAmp,
                customSenseAmp,
                senseVoltage,
                lenRow / numColumn * muxSenseAmp,
                peripherals.fileCustomSA,
                config);

        for (int i = 0; i < cell.camNumCol; i++) {
            ColMux[i] = std::make_unique<Mux>();
            ColMux[i]->Initialize(
                    muxSenseAmp,
                    numColumn / muxSenseAmp,
                    senseAmp->capLoad,
                    senseAmp->capLoad,
                    Col[i].maxCurrent,
                    config);
        }

    } else {
        for (int i = 0; i < cell.camNumCol; i++) {
            ColMux[i]->Initialize(
                    muxSenseAmp,
                    numColumn / muxSenseAmp,
                    senseAmpMuxLev1->capForPreviousDelayCalculation,
                    senseAmpMuxLev1->capForPreviousPowerCalculation,
                    Col[i].maxCurrent,
                    config);
        }
    }

    if (withWriteDriver) {
        for (int i = 0; i < cell.camNumCol; i++) {
            if (cell.camPort[1][i].Type == Matchline) {
                WriteDriver[i] = std::make_unique<RowDecoder>();
            } else {
                WriteDriver[i] = std::make_unique<RowDecoder>();
                WriteDriver[i]->Initialize(
                        numColumn / muxSenseAmp,
                        Col[i].cap,
                        Col[i].res,
                        false,
                        DriverOptLevel,
                        Col[i].maxCurrent,
                        config);
            }
        }
    }

    if (withPriorityEnc) {
        priorityEnc->Initialize(numColumn, PriorityOptLevel, 0, 0, config /*TODO: no output driver*/);
    }

    if (withOutputAcc) {
        if (withPriorityEnc)
            outputAcc->Initialize(priorityEnc->MMR.BasicMMR.capIn, 0, config);
        else
            outputAcc->Initialize(0, 0, config /*TODO: no output driver*/);
    }

    if (withOutputBuf) {
        CalculateGateCapacitance(
                NAND,
                2,
                outputBufNmosWidth,
                hvPmosWidth,
                hvCellHeight,
                tech,
                &capNandInput,
                &tmp);
        outputBuf->Initialize(true /*TODO*/, capNandInput, 0, config);
    }

    initialized = true;
    CalculateArea();
}

void CAM_SubArray::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[CAM_SubArray]");
    } else if (invalid) {
        height = width = area = 1e41;
    } else {
        double addWidthArea = 0, addHeightArea = 0;


        width = lenRow;
        height = lenCol;
        area = height * width;

        if (withInputBuf) {
            area += (inputBuf->area * numRow * 4);
            addWidthArea += (inputBuf->area * numRow * 4);
        }
        area += (inputLS->area * numRow);
        addWidthArea += (inputBuf->area * numRow);

        area += (outputLS->area * numRow);
        addWidthArea += (outputLS->area * numColumn * 2);

        if (withInputEnc) {
            inputEnc->CalculateArea();
            area += (inputEnc->area * numRow);
            addWidthArea += (inputEnc->area * numRow);
        }

        area += (RowDecMergeNand->area * 4);
        addWidthArea += (RowDecMergeNand->area * 4);


        for (int i = 0; i < config->technology.cell->camNumRow; i++) {
            area += (RowDriver[i]->area * 4);
            addWidthArea += (RowDriver[i]->area * 4);
        }

        // column decoder signal merge
        area += (ColDecMergeNand->area * 4);
        addWidthArea += (ColDecMergeNand->area * 4);

        area += (senseAmpMuxLev1Nand->area );
        addWidthArea += (senseAmpMuxLev1Nand->area );
        area += (senseAmpMuxLev2Nand->area );
        addWidthArea += (senseAmpMuxLev2Nand->area);

        area += (precharger->area);
        addHeightArea += (precharger->area);

        for (int i = 0; i < config->technology.cell->camNumCol; i++) {
            area += (ColMux[i]->area);
            addHeightArea += (ColMux[i]->area* 3);
        }

        // MUX
        area += (senseAmpMuxLev2->area* 3);
        addHeightArea += (senseAmpMuxLev2->area);
        area += (senseAmpMuxLev1->area* 9);
        addHeightArea += (senseAmpMuxLev1->area);

        if (internalSenseAmp) {
            area += (senseAmp->area);
            addHeightArea += (senseAmp->area);
        }

        WriteDriverArea = 0;
        if (withWriteDriver) {
            for (int i = 0; i < config->technology.cell->camNumCol; i++) {
                if (WriteDriver[i]->initialized) {
                    if (i > 0 && Col[i].CellPort.Type == Bitline && Col[i-1].CellPort.Type == Bitline) {
                        WriteDriverArea += (WriteDriver[i]->outputDriver.area);
                        WriteDriverArea += (WriteDriver[i]->area* 4);
                    } else {
                        WriteDriverArea += (WriteDriver[i]->area* 4);
                    }
                }
            }
        }
        area += WriteDriverArea;
        addHeightArea += WriteDriverArea;

        if (withOutputAcc) {
            area += (outputAcc->area * numColumn / muxSenseAmp);
            addHeightArea += (outputAcc->area * numColumn / muxSenseAmp);
        }

        if (withPriorityEnc) {
            area += (priorityEnc->area);
            addHeightArea += (priorityEnc->area);
        }

        if (withOutputBuf) {
            area += (outputBuf->area * numColumn / muxSenseAmp);
            addWidthArea += (outputBuf->area * numColumn / muxSenseAmp);
        }

        // TODO: a prefect layout
        width = addWidthArea / lenCol + lenCol;
        height = area / width;
    }
}

void CAM_SubArray::CalculateLatency(double _rampInput) {
    const auto &cell = *config->technology.cell;
    const auto &tech = *config->technology.tech;
    const auto &input = config->input;
    const auto &peripherals = config->peripherals;
    auto &logger = config->logger;

    if (!initialized) {
        throw std::runtime_error("[CAM_SubArray] Error: Require initialization first!");
    } else if (invalid) {
        searchLatency = readLatency = writeLatency = 1e41;
    } else {
        if (withInputBuf) {
            inputBuf->CalculateLatency(_rampInput);
        } else {
            inputBuf->readLatency = 0;
            inputBuf->rampOutput = _rampInput;
        }
        inputLS->CalculateLatency(_rampInput);
        outputLS->CalculateLatency(_rampInput);
        if (withInputEnc) {
            inputEnc->CalculateLatency(_rampInput);
        } else {
            inputEnc->readLatency = 0;
            inputEnc->rampOutput = _rampInput;
        }

        // this NAND merges pre-decoder's result, output the WL activation signal
        RowDecMergeNand->CalculateLatency(_rampInput);

        // those NAND merges the WL and SL signal
        double maxRowDriver = 0;
        int indexMaxRowDriver = 0;
        for (int i=0; i<cell.camNumRow; i++) {
            RowDriver[i]->CalculateLatency(MAX(inputEnc->rampOutput, RowDecMergeNand->rampOutput));

            if (RowDriver[i]->readLatency > maxRowDriver) {
                maxRowDriver = RowDriver[i]->readLatency;
                indexMaxRowDriver = i;
            }
        }

        // precharge
        precharger->CalculateLatency(_rampInput);

        // colmn decoder signal merge
        ColDecMergeNand->CalculateLatency(_rampInput);

        senseAmpMuxLev1Nand->CalculateLatency(_rampInput);
        senseAmpMuxLev2Nand->CalculateLatency(_rampInput);

        columnDecoderLatency = MAX(MAX(ColDecMergeNand->readLatency, senseAmpMuxLev1Nand->readLatency), senseAmpMuxLev2Nand->readLatency);
        decoderLatency = MAX(RowDecMergeNand->readLatency + maxRowDriver, columnDecoderLatency);

        /*****************************************************************************
         * Calculate matchline latency
         *****************************************************************************/

        if (cell.camType == TCAM) {
            double tau, gm, beta;
            // Estimate the ML latency for 1-miss case

            resTotalCell = (resMemCellOn * resMemCellOff) / ((CAM_opt.BitSerialWidth-1)*resMemCellOn + resMemCellOff);
            capTotalCell = capCellAccess * CAM_opt.BitSerialWidth;

            tau = resTotalCell 
                * (capTotalCell 
                        + ColMux[indexMatchline]->capForPreviousDelayCalculation 
                        + peripherals.addCapOnML 
                        + precharger->capOutputBitlinePrecharger 
                        + senseAmp->capLoad) 
                + matchlineWireRes 
                * (ColMux[indexMatchline]->capForPreviousDelayCalculation 
                        + peripherals.addCapOnML 
                        + precharger->capOutputBitlinePrecharger 
                        + senseAmp->capLoad 
                        + Col[indexMatchline].cap / 2);

            // tau = resTotalCell * capTotalCell + matchlineWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation);
            // referDelay = tau * log((voltagePrecharge) / (config->technology.cell->readVoltage)); // Too hard for user to provide read voltage
            // referDelay = tau * log(2);
            // beta = resMemCellOff / CAM_opt->BitSerialWidth / resTotalCell;
            gm = CalculateTransconductance(Col[indexMatchline].CellPort.widthCmos*tech.featureSize(), NMOS, tech);
            beta = 1 / gm / resTotalCell;
            matchlineDelay = horowitz(tau, beta, RowDriver[indexMaxRowDriver]->rampOutput, &matchlineRamp);
            logger.Verbose() << "matchlineDelay = " << matchlineDelay * 1e12 << " ps";

            // Estimate the ML latency for all-match case
            resTotalCell = resMemCellOff / CAM_opt.BitSerialWidth;//  
            tau = resTotalCell * (Col[indexMatchline].cap + ColMux[indexMatchline]->capForPreviousDelayCalculation)
                + matchlineWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation + Col[indexMatchline].cap / 2);
            //TODO: Need to get referDelay to be an expected value, took the following line from a commented out line above
            referDelay = tau * log(2);
            volMatchDrop = voltagePrecharge - voltagePrecharge * exp(-referDelay * tau);

            // Primary sense margin check
            senseMargin = voltagePrecharge/2 - volMatchDrop;    
            if (senseMargin < senseVoltage) {
                invalid = true;
                logger.Verbose() << "[CAM_SubArray] Matchline is too long to be sensed.";
                searchLatency = readLatency = writeLatency = 1e41;
                return;
            }

            for (int i=0; i<cell.camNumCol; i++) {
                ColMux[i]->CalculateLatency(matchlineRamp);
            }
            if (internalSenseAmp) {
                senseAmp->CalculateLatency(ColMux[indexMatchline]->rampOutput);
                senseAmpMuxLev1->CalculateLatency(1e20);
                senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
            } else {
                senseAmpMuxLev1->CalculateLatency(ColMux[indexMatchline]->rampOutput);
                senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
            }

            if (withOutputAcc) {
                outputAcc->CalculateLatency(1e20);
            } else {
                outputAcc->readLatency = 0;
                outputAcc->rampOutput = 1e20;
            }

            if (withPriorityEnc) {
                priorityEnc->CalculateLatency(outputAcc->rampOutput);
                rampOutput = priorityEnc->rampOutput;
            } else {
                priorityEnc->readLatency = 0;
                priorityEnc->rampOutput = outputAcc->rampOutput;
                rampOutput = priorityEnc->rampOutput;
            }

            if (withOutputBuf) {
                outputBuf->CalculateLatency(outputAcc->rampOutput);
            } else {
                outputBuf->readLatency = 0;
            }

            // searchLatency = inputBuf->readLatency + precharger->readLatency + maxRowDriver + inputEnc->readLatency + matchlineDelay
            //         + ColMux[indexMatchline]->readLatency + senseAmp->readLatency + outputAcc->readLatency + priorityEnc->readLatency
            //         + outputBuf->readLatency;

            searchLatency = inputBuf->readLatency 
                + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) 
                + matchlineDelay
                + ColMux[indexMatchline]->readLatency 
                + senseAmp->readLatency 
                + senseAmpMuxLev1->readLatency 
                + senseAmpMuxLev2->readLatency
                + outputAcc->readLatency 
                + priorityEnc->readLatency 
                + outputBuf->readLatency 
                + inputLS->readLatency 
                + outputLS->readLatency;

            senseAmpLatency = senseAmp->readLatency;

            readLatency = inputBuf->readLatency 
                + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) 
                + matchlineDelay
                + ColMux[indexMatchline]->readLatency 
                + senseAmp->readLatency 
                + senseAmpMuxLev1->readLatency 
                + senseAmpMuxLev2->readLatency
                + outputAcc->readLatency 
                + priorityEnc->readLatency 
                + outputBuf->readLatency 
                + outputLS->readLatency;

            UpdateMonteCarloTimingSummary();



            // Hamming distance-based approximate match
            if (input.searchFunction == BE || input.searchFunction == TH) {
                for (int k = 1; k < CAM_opt.BitSerialWidth; k++) {

                    double resTemp0 = (resMemCellOn * resMemCellOff) / ((CAM_opt.BitSerialWidth-k)*resMemCellOn + resMemCellOff*k);
                    double resTemp1 = (resMemCellOn * resMemCellOff) / ((CAM_opt.BitSerialWidth-k-1)*resMemCellOn + resMemCellOff*(k+1));

                    capTotalCell = capCellAccess * CAM_opt.BitSerialWidth;

                    double tauTemp0 = resTemp0 * (capTotalCell + ColMux[indexMatchline]->capForPreviousDelayCalculation + peripherals.addCapOnML + precharger->capOutputBitlinePrecharger + senseAmp->capLoad)
                        + matchlineWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation + peripherals.addCapOnML + precharger->capOutputBitlinePrecharger + senseAmp->capLoad + Col[indexMatchline].cap / 2);
                    double tauTemp1 = resTemp1 * (capTotalCell + ColMux[indexMatchline]->capForPreviousDelayCalculation + peripherals.addCapOnML + precharger->capOutputBitlinePrecharger + senseAmp->capLoad)
                        + matchlineWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation + peripherals.addCapOnML + precharger->capOutputBitlinePrecharger + senseAmp->capLoad + Col[indexMatchline].cap / 2);

                    gm = CalculateTransconductance(Col[indexMatchline].CellPort.widthCmos*tech.featureSize(), NMOS, tech);

                    double beta0 = 1 / gm / resTemp0;
                    double beta1 = 1 / gm / resTemp1;
                    double matchlineRamptemp;

                    double delayTemp0 = horowitz(tauTemp0, beta0, RowDriver[indexMaxRowDriver]->rampOutput, &matchlineRamp);
                    double delayTemp1 = horowitz(tauTemp1, beta1, RowDriver[indexMaxRowDriver]->rampOutput, &matchlineRamptemp);

                    if (delayTemp0 - delayTemp1 >= peripherals.matchlineSenseMargin) {
                        matchlineDelayForApprox[k] = delayTemp0;
                        MaxDetectCellNumber = k;
                        searchLatencyForApprox[k] = inputBuf->readLatency + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) + matchlineDelayForApprox[k]
                            + ColMux[indexMatchline]->readLatency + senseAmp->readLatency + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency
                            + outputAcc->readLatency + priorityEnc->readLatency + outputBuf->readLatency;
                        continue;
                    } else {
                        break;
                    }    
                }    
            }
        } else if (cell.camType == MCAM) {
            if (cell.memCellType != FEFETRAM) {
                invalid = true;
                throw std::runtime_error("Only 2FeFET MCAM design has limited support.");
            } else {
                logger.Log() << "Warning: 2FeFET MCAM design is not properly supported and will return inaccurate results for some metrics.";
                // TODO: fix this placeholder
                capTotalCell = 0.001;
                searchLatency = 0.001;
                senseAmpLatency = 0.001;
            }

        } else if (cell.camType == ACAM) {
            throw std::runtime_error("ACAM is not supported at this time.");
        }
    }

    // for write
    double capPassTransistor = ColMux[indexMatchline]->capNMOSPassTransistor +
        senseAmpMuxLev1->capNMOSPassTransistor + senseAmpMuxLev2->capNMOSPassTransistor;
    double resPassTransistor = ColMux[indexMatchline]->resNMOSPassTransistor +
        senseAmpMuxLev1->resNMOSPassTransistor + senseAmpMuxLev2->resNMOSPassTransistor;
    double tauChargeLatency = resPassTransistor * (capPassTransistor + Col[indexMatchline].cap) +
        matchlineWireRes * Col[indexMatchline].cap / 2;
    chargeLatency = horowitz(tauChargeLatency, 0, 1e20, NULL);

    WriteDriverLatency = 0;
    if (input.writeScheme == write_and_verify) {
        /*TODO: write and verify programming */
    } else {
        if (withWriteDriver) {
            for (int i=0; i<cell.camNumCol; i++) {
                if (WriteDriver[i]->initialized) {
                    WriteDriver[i]->CalculateLatency(1e20);
                    WriteDriverLatency = MAX(WriteDriver[i]->writeLatency, WriteDriverLatency);
                }
            }
        }
        writeLatency = MAX(decoderLatency, columnDecoderLatency + WriteDriverLatency + chargeLatency);
        resetLatency = (writeLatency + cell.resetPulse)*numColumn*2 * muxSenseAmp * muxOutputLev1 * muxOutputLev2;
        setLatency = (writeLatency + cell.setPulse)*numColumn*2 * muxSenseAmp * muxOutputLev1 * muxOutputLev2;
        writeLatency += MAX(cell.resetPulse, cell.setPulse);
    }
}


void CAM_SubArray::CalculatePower() {
        if (!initialized) {
            ThrowInitializationError("[CAM_SubArray]");
        } else if (invalid) {
            readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
        } else {

            const auto &cell = *config->technology.cell;
            const auto &tech = *config->technology.tech;
            auto &logger = config->logger;

            /*****************************************************************************
             * Calculate components
             *****************************************************************************/

            readDynamicEnergy = writeDynamicEnergy = leakage = 0;

            double inputBufReadEnergy = 0;
            double inputBufLeakage = 0;

            if (withInputBuf) {
                inputBuf->CalculatePower();
                inputBufReadEnergy = inputBuf->readDynamicEnergy;
                inputBufLeakage = inputBuf->leakage;
            }

            inputLS->CalculatePower();
            outputLS->CalculatePower();
            double inputEncReadEnergy = 0;
            double inputEncLeakage = 0;

            if (withInputEnc) {
                inputEnc->CalculatePower();
                inputEncReadEnergy = inputEnc->readDynamicEnergy;
                inputEncLeakage = inputEnc->leakage;
            }

            // this NAND merges pre-decoder's result, output the WL activation signal
            RowDecMergeNand->CalculatePower();
            // those NAND merges the WL and SL signal
            
            for (int i = 0; i < cell.camNumRow; i++) {
                RowDriver[i]->CalculatePower();
            }

            // precharge
            precharger->CalculatePower();

            // colmn decoder signal merge
            ColDecMergeNand->CalculatePower();

            senseAmpMuxLev1Nand->CalculatePower();
            senseAmpMuxLev2Nand->CalculatePower();

            if (internalSenseAmp) {
                senseAmp->CalculatePower();
            }
            senseAmpMuxLev1->CalculatePower();
            senseAmpMuxLev2->CalculatePower();

            if (withOutputAcc) {
                outputAcc->CalculatePower();
            }

            if (withPriorityEnc) {
                priorityEnc->CalculatePower();
            }

            double outputBufReadEnergy = 0;
            double outputBufLeakage = 0;

            if (withOutputBuf) {
                outputBuf->CalculatePower();
                outputBufReadEnergy = outputBuf->readDynamicEnergy;
                outputBufLeakage = outputBuf->leakage;
            }

            /*****************************************************************************
             * Calculate read and search
             *****************************************************************************/
            if (typeSenseAmp == discharge) {
                searchDynamicEnergy = (Col[indexMatchline].cap 
                        + ColMux[indexMatchline]->capForPreviousPowerCalculation + capTotalCell)
                        * (voltagePrecharge * voltagePrecharge - cell.readVoltage * cell.readVoltage) 
                        * numColumn / muxSenseAmp;
            } else {
                if (UsesSramStyleCamModel(cell.memCellType)) {
                    /* Codes below calculate the SRAM matchline power */
                    searchDynamicEnergy = (Col[indexMatchline].cap
                            + ColMux[indexMatchline]->capForPreviousPowerCalculation + capTotalCell)
                            * voltagePrecharge * voltagePrecharge * numColumn / muxSenseAmp;

                } else if (UsesResistiveCamModel(cell.memCellType)) {
                    if (cell.readMode == false) {    /* current-sensing */
                        /* Use ICCAD 2009 model */
                        double resMatchlineMux = ColMux[indexMatchline]->resNMOSPassTransistor;
                        double vpreMin = cell.readVoltage * resMatchlineMux / (resMatchlineMux + matchlineWireRes + resMemCellOn);
                        double vpreMax = cell.readVoltage * (resMatchlineMux + matchlineWireRes) /
                            (resMatchlineMux + matchlineWireRes + resMemCellOn);
                        searchDynamicEnergy = capTotalCell * vpreMax * vpreMax + ColMux[indexMatchline]->capForPreviousPowerCalculation
                            * vpreMin * vpreMin + Col[indexMatchline].cap * (vpreMax * vpreMax + vpreMin * vpreMin + vpreMax * vpreMin) / 3;
                        searchDynamicEnergy *= numColumn / muxSenseAmp;

                    } else {                /* voltage-sensing */
                        /*std::cout << "[CAM_Subarray]" << capTotalCell << std::endl;
                          std::cout << "[CAM_Subarray]" << Col[indexMatchline].cap << std::endl;
                          std::cout << "[CAM_Subarray]" << ColMux[indexMatchline]->capForPreviousPowerCalculation << std::endl;
                          std::cout << "[CAM_Subarray]" << voltagePrecharge << std::endl;
                          std::cout << "[CAM_Subarray]" << voltageMemCellOn << std::endl;
                          std::cout << "[CAM_Subarray]" << numColumn << std::endl;
                          std::cout << "[CAM_Subarray]" << muxSenseAmp << std::endl;*/
                        searchDynamicEnergy = (capTotalCell 
                                + Col[indexMatchline].cap 
                                + ColMux[indexMatchline]->capForPreviousPowerCalculation) 
                            * (voltagePrecharge * voltagePrecharge - voltageMemCellOn 
                                    * voltageMemCellOn ) * numColumn / muxSenseAmp;
                    }
                    // } else if (cell.memCellType ==FEFETRAM) {
                    //     double FEFETCap = CalculateDrainCap(cell.widthAccessCMOS * config->technology.fefetTech->featureSize(), NMOS, cell.widthInFeatureSize * config->technology.fefetTech->featureSize(), *config->technology.fefetTech);
                    //             (voltagePrecharge * voltagePrecharge) * numColumn / muxSenseAmp;
            }// }
        }
            if (cell.readEnergy != 0) {
                cellReadEnergy = cell.readEnergy * CAM_opt.BitSerialWidth;
            } else if (cell.readPower != 0) {
                cellReadEnergy = cell.readPower 
                * CAM_opt.BitSerialWidth 
                * (senseAmp->readLatency + matchlineDelay);
            } else if (cell.memCellType == SRAM) {
                cellReadEnergy = capCellAccess * voltagePrecharge * voltagePrecharge * CAM_opt.BitSerialWidth;
            } else if (cell.readMode) {    /* voltage-sensing */
                if (cell.readVoltage == 0) { /* Current-in voltage sensing */
                    cellReadEnergy = tech.vdd() * cell.readCurrent * (senseAmp->readLatency + matchlineDelay) * CAM_opt.BitSerialWidth;
                }
                if (cell.readCurrent == 0) { /*Voltage-divider sensing */
                    double resInSerialForSenseAmp, maxMatchlineCurrent;
                    resInSerialForSenseAmp = sqrt(cell.resistanceOn * cell.resistanceOff);
                    maxMatchlineCurrent = (cell.readVoltage - cell.voltageDropAccessDevice) / (cell.resistanceOn + resInSerialForSenseAmp);
                    cellReadEnergy = tech.vdd() * maxMatchlineCurrent * (senseAmp->readLatency + matchlineDelay) * CAM_opt.BitSerialWidth;
                }
            } else { /* current-sensing */
                double maxMatchlineCurrent = (cell.readVoltage - cell.voltageDropAccessDevice) / cell.resistanceOn;
                cellReadEnergy = tech.vdd() * maxMatchlineCurrent * (senseAmp->readLatency + matchlineDelay) * CAM_opt.BitSerialWidth;
            }
        cellReadEnergy *= numColumn / muxSenseAmp;

        energyDriveSearch0 = 0;
        energyDriveSearch1 = 0;

        for (int i = 0; i < cell.camNumRow; i++) {

            energyDriveSearch0 += RowDriver[i]->readDynamicEnergy / tech.vdd() 
                / tech.vdd() * Row[i].CellPort.volSearch0 * Row[i].CellPort.volSearch0;

            energyDriveSearch1 += RowDriver[i]->readDynamicEnergy / tech.vdd() 
                / tech.vdd() * Row[i].CellPort.volSearch1 * Row[i].CellPort.volSearch1;
        }

        searchDynamicEnergy += (energyDriveSearch0 + energyDriveSearch1)/2;
        UpdateMonteCarloPowerSummary();

        readDynamicEnergy = searchDynamicEnergy + inputBufReadEnergy * numRow 
            + inputEncReadEnergy * numRow
            + cellReadEnergy + ColDecMergeNand->readDynamicEnergy + precharger->readDynamicEnergy
            + senseAmpMuxLev1Nand->readDynamicEnergy + senseAmpMuxLev2Nand->readDynamicEnergy
            + ColMux[indexMatchline]->readDynamicEnergy + senseAmp->readDynamicEnergy 
            + senseAmpMuxLev1->readDynamicEnergy + senseAmpMuxLev2->readDynamicEnergy
            + outputAcc->readDynamicEnergy + priorityEnc->readDynamicEnergy 
            + outputBufReadEnergy * numColumn
            + inputLS->readDynamicEnergy * numRow + outputLS->readDynamicEnergy * numColumn;

        searchDynamicEnergy +=  inputBufReadEnergy * numRow + inputEncReadEnergy * numRow
            + cellReadEnergy + ColDecMergeNand->readDynamicEnergy + precharger->readDynamicEnergy
            + ColMux[indexMatchline]->readDynamicEnergy
            + senseAmp->readDynamicEnergy
            + outputAcc->readDynamicEnergy + priorityEnc->readDynamicEnergy 
            + outputBuf->readDynamicEnergy * numColumn;
        /*****************************************************************************
         * Calculate write
         *****************************************************************************/
        numBitline = 0;
        indexBitline = 0;
        // default status is using ML as the BL, as it is in the case of JSSC 2t2r
        for (int i = 0; i < cell.camNumCol; i++) {
            if (Col[i].CellPort.Type == Bitline) {
                indexBitline = i;
                numBitline++;
            }
        }
        if (cell.memCellType == SRAM) {
            // since the SRAM cell is not flexible, we can make the coding simpler
            double capSRAMin;
            double capSRAMout;
            CalculateGateCapacitance(INV, 1, cell.widthSRAMCellNMOS, cell.widthSRAMCellPMOS,
                    config->technology.tech->featureSize()*MAX_TRANSISTOR_HEIGHT, *config->technology.tech, &capSRAMin, &capSRAMout);
            cellResetEnergy = (capSRAMin + capSRAMout) * config->technology.tech->vdd() * config->technology.tech->vdd();
            cellResetEnergy += (Col[indexBitline].cap + ColMux[indexBitline]->capForPreviousPowerCalculation)
                * config->technology.tech->vdd()  * config->technology.tech->vdd();
            cellSetEnergy = cellResetEnergy;
        } else if (UsesResistiveCamModel(cell.memCellType) || cell.memCellType == FEFETRAM) {
            /* Ignore the dynamic transition during the SET/RESET operation */
            /* Assume that the cell resistance keeps high for worst-case power estimation */
            // TODO: MLC MRS set
            resetEnergyPerBit = cell.resetEnergy;
            setEnergyPerBit = cell.setEnergy;
            for (int i = 0; i < cell.camNumCol; i++) {
                // since each line has a description of the set/reset voltage already, we do not need setMode and resetMode in original nvsim anymore
                // for current set/reset mode, it has to be converted to the description of voltage set/reset in the cell configuration file
                // for example, the matchline voltage will be zero when writing in ISSCC'15 3t1r
                setEnergyPerBit += (capCellAccess + Col[indexBitline].cap + ColMux[i]->capForPreviousPowerCalculation)
                    * Col[i].CellPort.volSetLRS * Col[i].CellPort.volSetLRS;
                resetEnergyPerBit += (capCellAccess + Col[indexBitline].cap + ColMux[i]->capForPreviousPowerCalculation)
                    * Col[i].CellPort.volReset * Col[i].CellPort.volReset;

            }


            if (cell.memCellType == PCRAM) { //PCRAM write energy
                if (config->input.writeScheme == write_and_verify) {
                    /*TODO: write and verify programming */
                } else {
                    cellResetEnergy = resetEnergyPerBit / SHAPER_EFFICIENCY_CONSERVATIVE;
                    cellSetEnergy = setEnergyPerBit / SHAPER_EFFICIENCY_CONSERVATIVE;  /* Due to the shaper inefficiency */
                }
            } else { //MRAM and memristor + FEFET write energy

                cellResetEnergy = resetEnergyPerBit / SHAPER_EFFICIENCY_AGGRESSIVE;
                cellSetEnergy = setEnergyPerBit / SHAPER_EFFICIENCY_AGGRESSIVE;  /* Due to the shaper inefficiency */
            }
            leakage = 0;                       //TODO: cell leaks during read/write operation
        }
        writeDynamicEnergy = MAX(cellResetEnergy, cellSetEnergy);
        cellResetEnergy *= numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
        cellSetEnergy *= numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
        writeDynamicEnergy *= numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;

        // TODO: does not calculate MLC case
        setDynamicEnergy = resetDynamicEnergy = 0;
        for (int i = 0; i < cell.camNumRow; i++) {
            setDynamicEnergy += RowDriver[i]->writeDynamicEnergy / config->technology.tech->vdd() / config->technology.tech->vdd()
                * Row[i].CellPort.volSetLRS * Row[i].CellPort.volSetLRS;
            resetDynamicEnergy += RowDriver[i]->writeDynamicEnergy / config->technology.tech->vdd() / config->technology.tech->vdd()
                * Row[i].CellPort.volReset * Row[i].CellPort.volReset;
            writeDynamicEnergy += RowDriver[i]->writeDynamicEnergy /*/ config->technology.tech->vdd() / config->technology.tech->vdd()
                                                                    * (Row[i].CellPort.volReset + Row[i].CellPort.volSetLRS) * (Row[i].CellPort.volSetLRS + Row[i].CellPort.volReset)*/;

        }
        for (int i = 0; i < cell.camNumCol; i++) {
            if (indexBitline == i && i > 0) {
                // this is skipping the matchline that is not used in writing
                continue;
            }
            setDynamicEnergy   += ColMux[i]->writeDynamicEnergy;
            resetDynamicEnergy += ColMux[i]->writeDynamicEnergy;
            writeDynamicEnergy += ColMux[i]->writeDynamicEnergy;
        }

        if (indexBitline == 0) {
            // ML is also used in the writing operations
            setDynamicEnergy   += senseAmp->writeDynamicEnergy;
            resetDynamicEnergy += senseAmp->writeDynamicEnergy;
            writeDynamicEnergy += senseAmp->writeDynamicEnergy;
        }

        WriteDriverDyn = 0;
        WriteDriverLeakage = 0;
        if (withWriteDriver) {
            for (int i = 0; i < cell.camNumCol; i++) {
                if (WriteDriver[i]->initialized) {
                    WriteDriver[i]->CalculatePower();
                    WriteDriverDyn += WriteDriver[i]->writeDynamicEnergy;
                    WriteDriverLeakage += WriteDriver[i]->leakage;
                }
            }
        }

        writeDynamicEnergy += ColDecMergeNand->writeDynamicEnergy + WriteDriverDyn
            + senseAmpMuxLev1Nand->writeDynamicEnergy + senseAmpMuxLev2Nand->writeDynamicEnergy
            + senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy
            + inputLS->writeDynamicEnergy * numRow ;
        /* for assymetric RESET and SET latency calculation only */
        setDynamicEnergy += cellSetEnergy + ColDecMergeNand->writeDynamicEnergy
            + senseAmpMuxLev1Nand->writeDynamicEnergy + senseAmpMuxLev2Nand->writeDynamicEnergy
            + senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy;
        resetDynamicEnergy += setDynamicEnergy + ColDecMergeNand->writeDynamicEnergy
            + senseAmpMuxLev1Nand->writeDynamicEnergy + senseAmpMuxLev2Nand->writeDynamicEnergy
            + senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy;
        /*****************************************************************************
         * Calculate leakage
         *****************************************************************************/

        // leakage inside the cell
        if (cell.memCellType == SRAM) {
            leakage = CalculateGateLeakage(INV, 1, cell.widthSRAMCellNMOS * config->technology.tech->featureSize(),
                    cell.widthSRAMCellPMOS * config->technology.tech->featureSize(), config->input.temperature, *config->technology.tech)
                * config->technology.tech->vdd() * 2;    /* two inverters per SRAM cell */
            leakage += CalculateGateLeakage(INV, 1, cell.widthAccessCMOS * config->technology.tech->featureSize(), 0,
                    config->input.temperature, *config->technology.tech) * config->technology.tech->vdd();    /* two accesses NMOS, but combined as one with vdd crossed */
            leakage += CalculateGateLeakage(INV, 1, cell.widthAccessCMOS * config->technology.tech->featureSize(), 0,
                    config->input.temperature, *config->technology.tech) * config->technology.tech->vdd();    /* two accesses NMOS, but combined as one with vdd crossed */
            leakage += CalculateGateLeakage(INV, 1, cell.camWidthMatchTran * config->technology.tech->featureSize(), 0,
                    config->input.temperature, *config->technology.tech) * config->technology.tech->vdd();    /* two accesses NMOS, but combined as one with vdd crossed */
            leakage *= 2;
        } else if (UsesResistiveCamModel(cell.memCellType) || cell.memCellType == FEFETRAM) {
            // basically count the transistors in the cell
            // the trick here is that every transistor in the cell should be connected by some line to the gate to control it
            // the exception is the matchline transistor in cmos-access, like ISSCC'15 3t1r
            leakage = 0;
            for (int i=0; i<cell.camNumRow; i++) {
                if (Row[i].CellPort.ConnectedRegion == gate && Row[i].CellPort.leak) {
                    if (Row[i].CellPort.isNMOS)
                        leakage += CalculateGateLeakage(INV, 1, 
                                Row[i].CellPort.widthCmos * config->technology.tech->featureSize(),
                                0, 
                                config->input.temperature, 
                                *config->technology.tech) * config->technology.tech->vdd();
                    else
                        leakage += CalculateGateLeakage(INV, 1, 0, 
                                Row[i].CellPort.widthCmos * config->technology.tech->featureSize(),
                                config->input.temperature, 
                                *config->technology.tech) * config->technology.tech->vdd();
                }
            }
            for (int i=0; i<cell.camNumCol; i++) {
                if (Col[i].CellPort.ConnectedRegion == gate && Col[i].CellPort.leak) {
                    if (Col[i].CellPort.isNMOS)
                        leakage += CalculateGateLeakage(INV, 1, Col[i].CellPort.widthCmos * config->technology.tech->featureSize(), 0,
                                config->input.temperature, *config->technology.tech) * config->technology.tech->vdd();
                    else
                        leakage += CalculateGateLeakage(INV, 1, 0, Col[i].CellPort.widthCmos * config->technology.tech->featureSize(),
                                config->input.temperature, *config->technology.tech) * config->technology.tech->vdd();
                }
            }
            if (cell.accessType == CMOS_access) {
                leakage += CalculateGateLeakage(INV, 1, cell.camWidthMatchTran * config->technology.tech->featureSize(),  0,
                        config->input.temperature, *config->technology.tech) * config->technology.tech->vdd();
            }
        } else {
            invalid = true;
            logger.Verbose() << "[CAM_SubArray] Error: cell type input error";
            return;
        }
        leakage *= numRow * numColumn;

        /*****************************************************************************
         * Leakage totals
         *****************************************************************************/
        double leak = 0;
        for (int i = 0; i < cell.camNumRow; i++) {
            leakage += RowDriver[i]->leakage;
            leak += RowDriver[i]->leakage;
        }
        for (int i = 0; i < cell.camNumCol; i++) {
            leakage += ColMux[i]->leakage;
            leak += ColMux[i]->leakage;
        }

        leakage += inputBufLeakage * numColumn + inputEncLeakage + precharger->leakage + senseAmpMuxLev1Nand->leakage
            + senseAmpMuxLev2Nand->leakage + ColDecMergeNand->leakage + WriteDriverLeakage + senseAmp->leakage
            + senseAmpMuxLev1->leakage + senseAmpMuxLev2->leakage + outputAcc->leakage + priorityEnc->leakage
            + outputBufLeakage * numColumn;
        leak += inputBufLeakage * numColumn + inputEncLeakage + precharger->leakage + senseAmpMuxLev1Nand->leakage
            + senseAmpMuxLev2Nand->leakage + ColDecMergeNand->leakage + WriteDriverLeakage + senseAmp->leakage
            + senseAmpMuxLev1->leakage + senseAmpMuxLev2->leakage + outputAcc->leakage + priorityEnc->leakage
            + outputBufLeakage * numColumn;
    }

}

void CAM_SubArray::PrintProperty() {
    std::cout << "CAMSubarray Properties:" << std::endl;
    //FunctionUnit::PrintProperty();
    std::cout << "numRow:" << numRow << " numColumn:" << numColumn << std::endl;
    // area wise
    std::cout << "Input Encoder Area:" << inputEnc->area*1e12 << " um^2 (" << inputEnc->area/area*100 << "%)" << std::endl;
    for (int i=0; i<config->technology.cell->camNumRow; i++) {
        std::cout << "Row Driver " << i << " Area:" << RowDriver[i]->area*1e12 << " um^2 (" << RowDriver[i]->area/area*100 << "%)" <<std::endl;
    }
    std::cout << "lenWordline * lenBitline = " << lenRow*1e6 << " um * " << lenCol*1e6 << " um = " << lenRow * lenCol * 1e12
        << " um^2 (" << lenRow * lenCol/area*100 << "%)" << std::endl;
    std::cout << "MergeDecoderNand Area:" << RowDecMergeNand->area*1e12 << " um^2 (" << RowDecMergeNand->area/area*100 << "%)" << std::endl;
    for (int i=0; i<config->technology.cell->camNumCol; i++) {
        std::cout << "Col Mux " << i << " Area:" << ColMux[i]->area*1e12 << " um^2 (" << ColMux[i]->area/area*100 << "%)" << std::endl;
    }
    std::cout << "Write Driver Area:" << WriteDriverArea*1e12 << " um^2 (" << WriteDriverArea/area*100 << "%)" << std::endl;
    std::cout << "Mux Area:" << inputEnc->area*1e12 << " um^2" << std::endl;
    std::cout << "Sense Amplifier Area:" << senseAmp->area*1e12 << " um^2 (" << senseAmp->area/area*100 << "%)" << std::endl;
    std::cout << "Output Acc Area:" << outputAcc->area*1e12 << " um^2 (" << outputAcc->area/area*100 << "%)" << std::endl;
    std::cout << "Priority Encoder Area:" << priorityEnc->area*1e12 << " um^2 (" << priorityEnc->area/area*100 << "%)" << std::endl;

    // TODO: not done with debug interface yet
    std::cout << "matchlineDelay: " << matchlineDelay*1e12 << "ps" << std::endl;
    std::cout << "chargeLatency: " << chargeLatency*1e12 << "ps" << std::endl;
    std::cout << "columnDecoderLatency: " << columnDecoderLatency*1e12 << "ps" << std::endl;
    std::cout << "errors exist here!" << std::endl;
}

EvaCAMMatchResult CAM_SubArray::EvaluateBinaryMatch(const std::vector<int> &stored, const std::vector<int> &query) const {
    const auto &cell = *config->technology.cell;
    const auto &tech = *config->technology.tech;
    const auto &input = config->input;
    const auto &peripherals = config->peripherals;

    if (!initialized)
        throw std::runtime_error("[CAM_SubArray] Error: Require initialization first!");
    if (invalid)
        throw std::runtime_error("[CAM_SubArray] Error: subarray is invalid.");
    if (input.searchFunction != EX)
        throw std::runtime_error("[CAM_SubArray] Error: binary matcher currently supports exact search only.");
    if (cell.camType != TCAM)
        throw std::runtime_error("[CAM_SubArray] Error: binary matcher currently supports TCAM only.");
    if (CAM_opt.BitSerialWidth <= 0)
        throw std::runtime_error("[CAM_SubArray] Error: CAM options are not initialized.");
    if (stored.size() != static_cast<size_t>(CAM_opt.BitSerialWidth)
            || query.size() != static_cast<size_t>(CAM_opt.BitSerialWidth)) {
        throw std::invalid_argument("[CAM_SubArray] Error: binary match vectors must match BitSerialWidth.");
    }

    int mismatchCount = 0;
    for (size_t i = 0; i < stored.size(); i++) {
        if (stored[i] != query[i])
            mismatchCount++;
    }

    double effectiveMatchlineDelay = referDelay;
    double effectiveSearchLatency = searchLatency - matchlineDelay + referDelay;
    double effectiveSenseMargin = senseMargin;
    double effectiveSearchEnergy = searchDynamicEnergy;

    if (mismatchCount > 0) {
        double capTotalCellTemp = capCellAccess * CAM_opt.BitSerialWidth;
        double resTemp = (resMemCellOn * resMemCellOff)
            / ((CAM_opt.BitSerialWidth - mismatchCount) * resMemCellOn + resMemCellOff * mismatchCount);
        double tauTemp = resTemp * (capTotalCellTemp
                + ColMux[indexMatchline]->capForPreviousDelayCalculation
                + peripherals.addCapOnML
                + precharger->capOutputBitlinePrecharger
                + senseAmp->capLoad)
            + matchlineWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation
                    + peripherals.addCapOnML
                    + precharger->capOutputBitlinePrecharger
                    + senseAmp->capLoad
                    + Col[indexMatchline].cap / 2);

        double gm = CalculateTransconductance(
                Col[indexMatchline].CellPort.widthCmos * tech.featureSize(),
                NMOS, tech);
        double beta = 1 / gm / resTemp;
        double rampTemp = 0;
        effectiveMatchlineDelay = horowitz(tauTemp, beta, RowDriver[0]->rampOutput, &rampTemp);
        effectiveSearchLatency = searchLatency - matchlineDelay + effectiveMatchlineDelay;

        // Evaluate the miss voltage at the all-match sensing instant to expose a per-query margin.
        double actualMatchlineVoltage = voltagePrecharge * exp(-referDelay / tauTemp);
        effectiveSenseMargin = voltagePrecharge / 2 - actualMatchlineVoltage;

        // Approximate extra discharge energy from deeper ML discharge as mismatches increase.
        double missRatio = static_cast<double>(mismatchCount) / static_cast<double>(CAM_opt.BitSerialWidth);
        effectiveSearchEnergy = searchDynamicEnergy * (1.0 + missRatio);
    }

    EvaCAMMatchResult result{};
    result.hit = (mismatchCount == 0) && (senseMargin >= senseVoltage);
    result.searchLatency = effectiveSearchLatency;
    result.searchDynamicEnergy = effectiveSearchEnergy;
    result.matchlineDelay = effectiveMatchlineDelay;
    result.senseMargin = effectiveSenseMargin;
    return result;
}

CAMResistanceSample CAM_SubArray::BuildResistanceSample(unsigned int sampleIndex) const {
    const auto &variation = config->variation;
    CAMResistanceSample sample;
    // Stream offsets give each resistance category a stable deterministic RNG stream.
    sample.mlWireRes = SampleVariationResistance(
            nominalMatchlineWireRes,
            variation.mlWireResStdev,
            3,
            sampleIndex);

    sample.accessRes = SampleVariationResistance(
            nominalResCellAccess,
            variation.deviceAccessResStdev,
            1,
            sampleIndex);
    sample.matchRes = SampleVariationResistance(
            nominalResMatchTran,
            CombineStdev(variation.memoryDeviceResOnStdev, variation.deviceMatchResStdev),
            2,
            sampleIndex);
    sample.cellResOn = sample.accessRes + sample.matchRes;

    sample.accessResOff = SampleVariationResistance(
            nominalResCellAccessOff,
            variation.deviceAccessResStdev,
            4,
            sampleIndex);
    sample.matchResOff = SampleVariationResistance(
            nominalResMatchTranOff,
            CombineStdev(variation.memoryDeviceResOffStdev, variation.deviceMatchResStdev),
            5,
            sampleIndex);
    sample.cellResOff = sample.accessResOff + sample.matchResOff;
    return sample;
}

double CAM_SubArray::SampleVariationResistance(
        double nominal,
        double stdevFrac,
        unsigned int streamOffset,
        unsigned int sampleIndex) const {
    const auto &variation = config->variation;
    if (!(withVariation || variation.enabled) || stdevFrac <= 0.0) {
        return nominal;
    }

    VariationSampler sampler(MixVariationSeed(
            variation.seed,
            sampleIndex,
            streamOffset));
    return sampler.SampleResistance(nominal, stdevFrac);
}

double CAM_SubArray::EffectiveDeviceResistanceStdev() const {
    const auto &variation = config->variation;
    return CombineStdev(variation.deviceAccessResStdev, variation.deviceMatchResStdev);
}

void CAM_SubArray::UpdateMonteCarloTimingSummary() {

    const auto &variation = config->variation;
    const auto &cell = *config->technology.cell;
    const auto &tech = *config->technology.tech;
    const auto &peripherals = config->peripherals;

    monteCarloSummary = CAMMonteCarloSummary{};
    if (!variation.enabled) {
        return;
    }
    if (variation.mode == "single_point") {
        const CAMResistanceSample sample = BuildResistanceSample(0);
        const double searchBase = searchLatency - matchlineDelay;
        const double readBase = readLatency - matchlineDelay;
        int indexMaxRowDriver = 0;
        double maxRowDriverLatency = 0;
        for (int i = 0; i < cell.camNumRow; i++) {
            if (RowDriver[i]->readLatency > maxRowDriverLatency) {
                maxRowDriverLatency = RowDriver[i]->readLatency;
                indexMaxRowDriver = i;
            }
        }

        const double sampleCapTotalCell = capCellAccess * CAM_opt.BitSerialWidth;
        const double sampleResTotalCell =
                (sample.cellResOn * sample.cellResOff)
                / ((CAM_opt.BitSerialWidth - 1) * sample.cellResOn + sample.cellResOff);
        const double sampleTau =
                sampleResTotalCell * (sampleCapTotalCell
                        + ColMux[indexMatchline]->capForPreviousDelayCalculation
                        + peripherals.addCapOnML
                        + precharger->capOutputBitlinePrecharger
                        + senseAmp->capLoad)
                + sample.mlWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation
                        + peripherals.addCapOnML
                        + precharger->capOutputBitlinePrecharger
                        + senseAmp->capLoad
                        + Col[indexMatchline].cap / 2);
        const double gm = CalculateTransconductance(
                Col[indexMatchline].CellPort.widthCmos * tech.featureSize(),
                NMOS,
                tech);
        const double beta = 1 / gm / sampleResTotalCell;
        double sampleRamp = 0;
        double sampleMatchlineDelay = horowitz(
                sampleTau,
                beta,
                RowDriver[indexMaxRowDriver]->rampOutput,
                &sampleRamp);

        const double sampleAllMatchRes = sample.cellResOff / CAM_opt.BitSerialWidth;
        const double sampleAllMatchTau =
                sampleAllMatchRes * (Col[indexMatchline].cap + ColMux[indexMatchline]->capForPreviousDelayCalculation)
                + sample.mlWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation + Col[indexMatchline].cap / 2);
        const double sampleReferDelay = sampleAllMatchTau * log(2);
        const double sampleVolMatchDrop = voltagePrecharge - voltagePrecharge * exp(-sampleReferDelay * sampleAllMatchTau);
        const double sampleSenseMargin = voltagePrecharge / 2 - sampleVolMatchDrop;

        double sampleSearchLatency = searchBase + sampleMatchlineDelay;
        if (sampleSenseMargin < senseVoltage) {
            sampleMatchlineDelay = 1e41;
            sampleSearchLatency = 1e41;
        }

        monteCarloSummary.enabled = true;
        monteCarloSummary.mode = "single_point";
        monteCarloSummary.samples = 1;
        monteCarloSummary.matchlineDelay = BuildSinglePointMetric(sampleMatchlineDelay, matchlineDelay);
        monteCarloSummary.searchLatency = BuildSinglePointMetric(sampleSearchLatency, searchLatency);
        monteCarloSummary.senseMargin = BuildSinglePointMetric(sampleSenseMargin, senseMargin);

        matchlineDelay = sampleMatchlineDelay;
        searchLatency = sampleSearchLatency;
        readLatency = readBase + matchlineDelay;
        senseMargin = sampleSenseMargin;
        referDelay = sampleReferDelay;
        return;
    }
    if (variation.mode != "monte_carlo" || variation.samples <= 1) {
        return;
    }

    std::vector<double> matchlineDelays;
    std::vector<double> searchLatencies;
    std::vector<double> senseMargins;
    std::vector<double> referDelays;
    matchlineDelays.reserve(variation.samples);
    searchLatencies.reserve(variation.samples);
    senseMargins.reserve(variation.samples);
    referDelays.reserve(variation.samples);

    const double searchBase = searchLatency - matchlineDelay;
    const double readBase = readLatency - matchlineDelay;
    int indexMaxRowDriver = 0;
    double maxRowDriverLatency = 0;
    for (int i = 0; i < cell.camNumRow; i++) {
        if (RowDriver[i]->readLatency > maxRowDriverLatency) {
            maxRowDriverLatency = RowDriver[i]->readLatency;
            indexMaxRowDriver = i;
        }
    }

    for (int sampleIndex = 0; sampleIndex < variation.samples; sampleIndex++) {
        const CAMResistanceSample sample = BuildResistanceSample(sampleIndex);
        const double sampleCapTotalCell = capCellAccess * CAM_opt.BitSerialWidth;
        const double sampleResTotalCell =
                (sample.cellResOn * sample.cellResOff)
                / ((CAM_opt.BitSerialWidth - 1) * sample.cellResOn + sample.cellResOff);
        const double sampleTau =
                sampleResTotalCell * (sampleCapTotalCell
                        + ColMux[indexMatchline]->capForPreviousDelayCalculation
                        + peripherals.addCapOnML
                        + precharger->capOutputBitlinePrecharger
                        + senseAmp->capLoad)
                + sample.mlWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation
                        + peripherals.addCapOnML
                        + precharger->capOutputBitlinePrecharger
                        + senseAmp->capLoad
                        + Col[indexMatchline].cap / 2);
        const double gm = CalculateTransconductance(
                Col[indexMatchline].CellPort.widthCmos * tech.featureSize(),
                NMOS,
                tech);
        const double beta = 1 / gm / sampleResTotalCell;
        double sampleRamp = 0;
        double sampleMatchlineDelay = horowitz(
                sampleTau,
                beta,
                RowDriver[indexMaxRowDriver]->rampOutput,
                &sampleRamp);

        const double sampleAllMatchRes = sample.cellResOff / CAM_opt.BitSerialWidth;
        const double sampleAllMatchTau =
                sampleAllMatchRes * (Col[indexMatchline].cap + ColMux[indexMatchline]->capForPreviousDelayCalculation)
                + sample.mlWireRes * (ColMux[indexMatchline]->capForPreviousDelayCalculation + Col[indexMatchline].cap / 2);
        const double sampleReferDelay = sampleAllMatchTau * log(2);
        const double sampleVolMatchDrop = voltagePrecharge - voltagePrecharge * exp(-sampleReferDelay * sampleAllMatchTau);
        const double sampleSenseMargin = voltagePrecharge / 2 - sampleVolMatchDrop;

        double sampleSearchLatency = searchBase + sampleMatchlineDelay;
        if (sampleSenseMargin < senseVoltage) {
            sampleMatchlineDelay = 1e41;
            sampleSearchLatency = 1e41;
        }

        matchlineDelays.push_back(sampleMatchlineDelay);
        searchLatencies.push_back(sampleSearchLatency);
        senseMargins.push_back(sampleSenseMargin);
        referDelays.push_back(sampleReferDelay);
    }

    monteCarloSummary.enabled = true;
    monteCarloSummary.mode = "monte_carlo";
    monteCarloSummary.samples = variation.samples;
    monteCarloSummary.matchlineDelay = BuildMetricStats(matchlineDelays, matchlineDelay);
    monteCarloSummary.searchLatency = BuildMetricStats(searchLatencies, searchLatency);
    monteCarloSummary.senseMargin = BuildMetricStats(senseMargins, senseMargin);

    matchlineDelay = monteCarloSummary.matchlineDelay.mean;
    searchLatency = monteCarloSummary.searchLatency.mean;
    readLatency = readBase + matchlineDelay;
    senseMargin = monteCarloSummary.senseMargin.mean;
    if (!referDelays.empty()) {
        double referDelaySum = 0;
        for (double value : referDelays)
            referDelaySum += value;
        referDelay = referDelaySum / referDelays.size();
    }
}

void CAM_SubArray::UpdateMonteCarloPowerSummary() {

    const auto &cell = *config->technology.cell;

    if (!monteCarloSummary.enabled) {
        return;
    }

    if (monteCarloSummary.mode == "single_point") {
        const CAMResistanceSample sample = BuildResistanceSample(0);
        const double sampleCapTotalCell = capCellAccess * CAM_opt.BitSerialWidth;
        double sampleSearchDynamicEnergy = 0;

        if (typeSenseAmp == discharge) {
            sampleSearchDynamicEnergy = (Col[indexMatchline].cap
                    + ColMux[indexMatchline]->capForPreviousPowerCalculation + sampleCapTotalCell)
                * (voltagePrecharge * voltagePrecharge - cell.readVoltage * cell.readVoltage)
                * numColumn / muxSenseAmp;
        } else {
            if (UsesSramStyleCamModel(cell.memCellType)) {
                sampleSearchDynamicEnergy = (Col[indexMatchline].cap + ColMux[indexMatchline]->capForPreviousPowerCalculation + sampleCapTotalCell)
                    * voltagePrecharge * voltagePrecharge * numColumn / muxSenseAmp;
            } else if (UsesResistiveCamModel(cell.memCellType)) {
                if (cell.readMode == false) {
                    const double resMatchlineMux = ColMux[indexMatchline]->resNMOSPassTransistor;
                    const double vpreMin = cell.readVoltage * resMatchlineMux / (resMatchlineMux + sample.mlWireRes + sample.cellResOn);
                    const double vpreMax = cell.readVoltage * (resMatchlineMux + sample.mlWireRes)
                        / (resMatchlineMux + sample.mlWireRes + sample.cellResOn);
                    sampleSearchDynamicEnergy = sampleCapTotalCell * vpreMax * vpreMax
                        + ColMux[indexMatchline]->capForPreviousPowerCalculation * vpreMin * vpreMin
                        + Col[indexMatchline].cap * (vpreMax * vpreMax + vpreMin * vpreMin + vpreMax * vpreMin) / 3;
                    sampleSearchDynamicEnergy *= numColumn / muxSenseAmp;
                } else {
                    sampleSearchDynamicEnergy = (sampleCapTotalCell
                            + Col[indexMatchline].cap
                            + ColMux[indexMatchline]->capForPreviousPowerCalculation)
                        * (voltagePrecharge * voltagePrecharge - voltageMemCellOn * voltageMemCellOn)
                        * numColumn / muxSenseAmp;
                }
            }
        }

        sampleSearchDynamicEnergy += (energyDriveSearch0 + energyDriveSearch1) / 2;
        monteCarloSummary.searchDynamicEnergy = BuildSinglePointMetric(sampleSearchDynamicEnergy, searchDynamicEnergy);
        searchDynamicEnergy = sampleSearchDynamicEnergy;
        return;
    }

    std::vector<double> searchEnergies;
    searchEnergies.reserve(monteCarloSummary.samples);

    for (int sampleIndex = 0; sampleIndex < monteCarloSummary.samples; sampleIndex++) {
        const CAMResistanceSample sample = BuildResistanceSample(sampleIndex);
        const double sampleCapTotalCell = capCellAccess * CAM_opt.BitSerialWidth;
        double sampleSearchDynamicEnergy = 0;

        if (typeSenseAmp == discharge) {
            sampleSearchDynamicEnergy = (Col[indexMatchline].cap
                    + ColMux[indexMatchline]->capForPreviousPowerCalculation + sampleCapTotalCell)
                * (voltagePrecharge * voltagePrecharge - cell.readVoltage * cell.readVoltage)
                * numColumn / muxSenseAmp;
        } else {
            if (UsesSramStyleCamModel(cell.memCellType)) {
                sampleSearchDynamicEnergy = (Col[indexMatchline].cap + ColMux[indexMatchline]->capForPreviousPowerCalculation + sampleCapTotalCell)
                    * voltagePrecharge * voltagePrecharge * numColumn / muxSenseAmp;
            } else if (UsesResistiveCamModel(cell.memCellType)) {
                if (cell.readMode == false) {
                    const double resMatchlineMux = ColMux[indexMatchline]->resNMOSPassTransistor;
                    const double vpreMin = cell.readVoltage * resMatchlineMux / (resMatchlineMux + sample.mlWireRes + sample.cellResOn);
                    const double vpreMax = cell.readVoltage * (resMatchlineMux + sample.mlWireRes)
                        / (resMatchlineMux + sample.mlWireRes + sample.cellResOn);
                    sampleSearchDynamicEnergy = sampleCapTotalCell * vpreMax * vpreMax
                        + ColMux[indexMatchline]->capForPreviousPowerCalculation * vpreMin * vpreMin
                        + Col[indexMatchline].cap * (vpreMax * vpreMax + vpreMin * vpreMin + vpreMax * vpreMin) / 3;
                    sampleSearchDynamicEnergy *= numColumn / muxSenseAmp;
                } else {
                    sampleSearchDynamicEnergy = (sampleCapTotalCell
                            + Col[indexMatchline].cap
                            + ColMux[indexMatchline]->capForPreviousPowerCalculation)
                        * (voltagePrecharge * voltagePrecharge - voltageMemCellOn * voltageMemCellOn)
                        * numColumn / muxSenseAmp;
                }
            }
        }

        sampleSearchDynamicEnergy += (energyDriveSearch0 + energyDriveSearch1) / 2;
        searchEnergies.push_back(sampleSearchDynamicEnergy);
    }

    monteCarloSummary.searchDynamicEnergy = BuildMetricStats(searchEnergies, searchDynamicEnergy);
    searchDynamicEnergy = monteCarloSummary.searchDynamicEnergy.mean;
}
