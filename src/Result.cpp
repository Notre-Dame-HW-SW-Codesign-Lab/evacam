#include "Result.h"
#include "factories/BankFactory.h"
#include "factories/WireFactory.h"
#include "formula.h"
#include "macros.h"

#include <iostream>
#include <fstream>


void Result::Initialize(std::shared_ptr<EvaCamConfig> _config) {
    config = _config;

    bank = BankFactory::CreateBank(config);
    localWire = WireFactory::CreateDefaultLocalWire(config);
    globalWire = WireFactory::CreateDefaultGlobalWire(config);

    /* initialize the worst case */
    bank->readLatency = 1e41;
    bank->writeLatency = 1e41;
    bank->readDynamicEnergy = 1e41;
    bank->writeDynamicEnergy = 1e41;
    bank->searchLatency = 1e41;
    bank->searchDynamicEnergy = 1e41;
    bank->leakage = 1e41;
    bank->height = 1e41;
    bank->width = 1e41;
    bank->area = 1e41;

    /* No constraints */
    limitReadLatency = 1e41;
    limitWriteLatency = 1e41;
    limitReadDynamicEnergy = 1e41;
    limitWriteDynamicEnergy = 1e41;
    limitReadEdp = 1e41;
    limitWriteEdp = 1e41;
    limitArea = 1e41;
    limitLeakage = 1e41;

    /* Default read latency optimization */
    optimizationTarget = read_latency_optimized;
}

void Result::reset() {
    bank->readLatency = 1e41;
    bank->writeLatency = 1e41;
    bank->readDynamicEnergy = 1e41;
    bank->writeDynamicEnergy = 1e41;
    bank->searchDynamicEnergy = 1e41;
    bank->leakage = 1e41;
    bank->height = 1e41;
    bank->width = 1e41;
    bank->area = 1e41;
}

void Result::compareAndUpdate(std::shared_ptr<Result> newResult) {
    if (newResult->bank->readLatency <= limitReadLatency 
            && newResult->bank->writeLatency <= limitWriteLatency
            && newResult->bank->readDynamicEnergy <= limitReadDynamicEnergy 
            && newResult->bank->writeDynamicEnergy <= limitWriteDynamicEnergy
            && newResult->bank->readLatency * newResult->bank->readDynamicEnergy <= limitReadEdp
            && newResult->bank->writeLatency * newResult->bank->writeDynamicEnergy <= limitWriteEdp
            && newResult->bank->area <= limitArea && newResult->bank->leakage <= limitLeakage) {
        bool toUpdate = false;
        switch (optimizationTarget) {
            case read_latency_optimized:
                if 	(newResult->bank->readLatency < bank->readLatency)
                    toUpdate = true;
                break;
            case write_latency_optimized:
                if 	(newResult->bank->writeLatency < bank->writeLatency)
                    toUpdate = true;
                break;
            case read_energy_optimized:
                if 	(newResult->bank->readDynamicEnergy < bank->readDynamicEnergy)
                    toUpdate = true;
                break;
            case write_energy_optimized:
                if 	(newResult->bank->writeDynamicEnergy < bank->writeDynamicEnergy)
                    toUpdate = true;
                break;
            case read_edp_optimized:
                if 	(newResult->bank->readLatency * newResult->bank->readDynamicEnergy < bank->readLatency * bank->readDynamicEnergy)
                    toUpdate = true;
                break;
            case write_edp_optimized:
                if 	(newResult->bank->writeLatency * newResult->bank->writeDynamicEnergy < bank->writeLatency * bank->writeDynamicEnergy)
                    toUpdate = true;
                break;
            case area_optimized:
                if 	(newResult->bank->area < bank->area)
                    toUpdate = true;
                break;
            case leakage_optimized:
                if 	(newResult->bank->leakage < bank->leakage)
                    toUpdate = true;
                break;
            default:	/* Exploration */
                /* should not happen */
                ;
        }
        if (toUpdate) {
            *bank = *(newResult->bank);
            localWire = newResult->localWire;
            globalWire = newResult->globalWire;
        }
    }
}

void Result::print() {
    std::cout << "error is here Result::print" << std::endl;
    std::cout << std::endl << "=============" << std::endl << "CONFIGURATION" << std::endl << "=============" << std::endl;
    std::cout << "Bank Organization: " << bank->numRowMat << " x " << bank->numColumnMat << std::endl;
    std::cout << " - Row Activation   : " << bank->numActiveMatPerColumn << " / " << bank->numRowMat << std::endl;
    std::cout << " - Column Activation: " << bank->numActiveMatPerRow << " / " << bank->numColumnMat << std::endl;
    std::cout << "Mat Organization: " << bank->numRowSubarray << " x " << bank->numColumnSubarray << std::endl;
    std::cout << " - Row Activation   : " << bank->numActiveSubarrayPerColumn << " / " << bank->numRowSubarray << std::endl;
    std::cout << " - Column Activation: " << bank->numActiveSubarrayPerRow << " / " << bank->numColumnSubarray << std::endl;
    std::cout << " - Subarray Size    : " << bank->mat->subarray->numRow << " Rows x " << bank->mat->subarray->numColumn << " Columns" << std::endl;
    std::cout << "Mux Level:" << std::endl;
    std::cout << " - Senseamp Mux      : " << bank->muxSenseAmp << std::endl;
    std::cout << " - Output Level-1 Mux: " << bank->muxOutputLev1 << std::endl;
    std::cout << " - Output Level-2 Mux: " << bank->muxOutputLev2 << std::endl;
    if (config->input.designTarget == cache)
        std::cout << " - One set is partitioned into " << bank->numRowPerSet << " rows" << std::endl;
    std::cout << "Local Wire:" << std::endl;
    std::cout << " - Wire Type : ";
    switch (localWire->wireType) {
        case local_aggressive:
            std::cout << "Local Aggressive" << std::endl;
            break;
        case local_conservative:
            std::cout << "Local Conservative" << std::endl;
            break;
        case semi_aggressive:
            std::cout << "Semi-Global Aggressive" << std::endl;
            break;
        case semi_conservative:
            std::cout << "Semi-Global Conservative" << std::endl;
            break;
        case global_aggressive:
            std::cout << "Global Aggressive" << std::endl;
            break;
        case global_conservative:
            std::cout << "Global Conservative" << std::endl;
            break;
        default:
            std::cout << "DRAM Wire" << std::endl;
    }
    std::cout << " - Repeater Type: ";
    switch (localWire->wireRepeaterType) {
        case repeated_none:
            std::cout << "No Repeaters" << std::endl;
            break;
        case repeated_opt:
            std::cout << "Fully-Optimized Repeaters" << std::endl;
            break;
        case repeated_5:
            std::cout << "Repeaters with 5% Overhead" << std::endl;
            break;
        case repeated_10:
            std::cout << "Repeaters with 10% Overhead" << std::endl;
            break;
        case repeated_20:
            std::cout << "Repeaters with 20% Overhead" << std::endl;
            break;
        case repeated_30:
            std::cout << "Repeaters with 30% Overhead" << std::endl;
            break;
        case repeated_40:
            std::cout << "Repeaters with 40% Overhead" << std::endl;
            break;
        case repeated_50:
            std::cout << "Repeaters with 50% Overhead" << std::endl;
            break;
        default:
            std::cout << "Unknown" << std::endl;
    }
    std::cout << " - Low Swing : ";
    if (localWire->isLowSwing)
        std::cout << "Yes" << std::endl;
    else
        std::cout << "No" << std::endl;
    std::cout << "Global Wire:" << std::endl;
    std::cout << " - Wire Type : ";
    switch (globalWire->wireType) {
        case local_aggressive:
            std::cout << "Local Aggressive" << std::endl;
            break;
        case local_conservative:
            std::cout << "Local Conservative" << std::endl;
            break;
        case semi_aggressive:
            std::cout << "Semi-Global Aggressive" << std::endl;
            break;
        case semi_conservative:
            std::cout << "Semi-Global Conservative" << std::endl;
            break;
        case global_aggressive:
            std::cout << "Global Aggressive" << std::endl;
            break;
        case global_conservative:
            std::cout << "Global Conservative" << std::endl;
            break;
        default:
            std::cout << "DRAM Wire" << std::endl;
    }
    std::cout << " - Repeater Type: ";
    switch (globalWire->wireRepeaterType) {
        case repeated_none:
            std::cout << "No Repeaters" << std::endl;
            break;
        case repeated_opt:
            std::cout << "Fully-Optimized Repeaters" << std::endl;
            break;
        case repeated_5:
            std::cout << "Repeaters with 5% Overhead" << std::endl;
            break;
        case repeated_10:
            std::cout << "Repeaters with 10% Overhead" << std::endl;
            break;
        case repeated_20:
            std::cout << "Repeaters with 20% Overhead" << std::endl;
            break;
        case repeated_30:
            std::cout << "Repeaters with 30% Overhead" << std::endl;
            break;
        case repeated_40:
            std::cout << "Repeaters with 40% Overhead" << std::endl;
            break;
        case repeated_50:
            std::cout << "Repeaters with 50% Overhead" << std::endl;
            break;
        default:
            std::cout << "Unknown" << std::endl;
    }
    std::cout << " - Low Swing : ";
    if (globalWire->isLowSwing)
        std::cout << "Yes" << std::endl;
    else
        std::cout << "No" << std::endl;
    std::cout << "Buffer Design Style: ";
    switch (bank->areaOptimizationLevel) {
        case latency_first:
            std::cout << "Latency-Optimized" << std::endl;
            break;
        case area_first:
            std::cout << "Area-Optimized" << std::endl;
            break;
        default:	/* balance */
            std::cout << "Balanced" << std::endl;
    }

    std::cout << "=============" << std::endl << "   RESULT" << std::endl << "=============" << std::endl;

    std::cout << "Area:" << std::endl;

    std::cout << " - Total Area = " << TO_METER(bank->height) << " x " << TO_METER(bank->width)
        << " = " << TO_SQM(bank->area) << std::endl;
    std::cout << " |--- Mat Area      = " << TO_METER(bank->mat->height) << " x " << TO_METER(bank->mat->width)
        << " = " << TO_SQM(bank->mat->area) << "   (" << config->technology.cell->area * config->technology.tech->featureSize() * config->technology.tech->featureSize()
        * bank->capacity / bank->numRowMat / bank->numColumnMat / bank->mat->area * 100 << "%)" << std::endl;
    std::cout << " |--- Subarray Area = " << TO_METER(bank->mat->subarray->height) << " x "
        << TO_METER(bank->mat->subarray->width) << " = " << TO_SQM(bank->mat->subarray->area) << "   ("
        << config->technology.cell->area * config->technology.tech->featureSize() * config->technology.tech->featureSize() * bank->capacity / bank->numRowMat
        / bank->numColumnMat / bank->numRowSubarray / bank->numColumnSubarray
        / bank->mat->subarray->area * 100 << "%)" <<std::endl;
    std::cout << " |--- Subarray Dimensions = " << bank->mat->subarray->numRow
        << " Rows x " << bank->mat->subarray->numColumn << " Columns" << std::endl;
    std::cout << " - Area Efficiency = " << config->technology.cell->area * config->technology.tech->featureSize() * config->technology.tech->featureSize()
        * bank->capacity / bank->area * 100 << "%" << std::endl;

    std::cout << "Timing:" << std::endl;

    std::cout << " -  Read Latency = " << TO_SECOND(bank->readLatency) << std::endl;
    if (config->input.routingMode == h_tree)
        std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->readLatency - bank->mat->readLatency) << std::endl;
    else
        std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->readLatency - bank->mat->readLatency) << std::endl;
    std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->readLatency) << std::endl;
    std::cout << "    |--- Predecoder Latency = " << TO_SECOND(bank->mat->predecoderLatency) << std::endl;
    std::cout << "    |--- Subarray Latency   = " << TO_SECOND(bank->mat->subarray->readLatency) << std::endl;
    std::cout << "       |--- Row Decoder Latency = " << TO_SECOND(bank->mat->subarray->rowDecoder->readLatency) << std::endl;
    std::cout << "       |--- Matchline Latency     = " << TO_SECOND(bank->mat->subarray->matchlineDelay) << std::endl;
    if (config->input.internalSensing)
        std::cout << "       |--- Senseamp Latency    = " << TO_SECOND(bank->mat->subarray->senseAmp->readLatency) << std::endl;
    std::cout << "       |--- Mux Latency         = " << TO_SECOND(bank->mat->subarray->bitlineMux->readLatency
            + bank->mat->subarray->senseAmpMuxLev1->readLatency
            + bank->mat->subarray->senseAmpMuxLev2->readLatency) << std::endl;
    std::cout << "       |--- Precharge Latency   = " << TO_SECOND(bank->mat->subarray->precharger->readLatency) << std::endl;

    if (config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == FBRAM ||
            (config->technology.cell->memCellType == memristor && (config->technology.cell->accessType == CMOS_access || config->technology.cell->accessType == BJT_access))) {
        std::cout << " - RESET Latency = " << TO_SECOND(bank->resetLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->resetLatency - bank->mat->resetLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->resetLatency - bank->mat->resetLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->resetLatency) << std::endl;
        std::cout << "    |--- Predecoder Latency = " << TO_SECOND(bank->mat->predecoderLatency) << std::endl;
        std::cout << "    |--- Subarray Latency   = " << TO_SECOND(bank->mat->subarray->resetLatency) << std::endl;
        std::cout << "       |--- RESET Pulse Duration = " << TO_SECOND(config->technology.cell->resetPulse) << std::endl;
        std::cout << "       |--- Row Decoder Latency  = " << TO_SECOND(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
        std::cout << "       |--- Charge Latency   = " << TO_SECOND(bank->mat->subarray->chargeLatency) << std::endl;
        std::cout << " - SET Latency   = " << TO_SECOND(bank->setLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->setLatency - bank->mat->setLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->setLatency - bank->mat->setLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->setLatency) << std::endl;
        std::cout << "    |--- Predecoder Latency = " << TO_SECOND(bank->mat->predecoderLatency) << std::endl;
        std::cout << "    |--- Subarray Latency   = " << TO_SECOND(bank->mat->subarray->setLatency) << std::endl;
        std::cout << "       |--- SET Pulse Duration   = " << TO_SECOND(config->technology.cell->setPulse) << std::endl;
        std::cout << "       |--- Row Decoder Latency  = " << TO_SECOND(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
        std::cout << "       |--- Charger Latency      = " << TO_SECOND(bank->mat->subarray->chargeLatency) << std::endl;
    } else if (config->technology.cell->memCellType == SLCNAND) {
        std::cout << " - Erase Latency = " << TO_SECOND(bank->resetLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->resetLatency - bank->mat->resetLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->resetLatency - bank->mat->resetLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->resetLatency) << std::endl;
        std::cout << " - Programming Latency   = " << TO_SECOND(bank->setLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->setLatency - bank->mat->setLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->setLatency - bank->mat->setLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->setLatency) << std::endl;
    } else {
        std::cout << " - Write Latency = " << TO_SECOND(bank->writeLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->writeLatency - bank->mat->writeLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->writeLatency - bank->mat->writeLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->writeLatency) << std::endl;
        std::cout << "    |--- Predecoder Latency = " << TO_SECOND(bank->mat->predecoderLatency) << std::endl;
        std::cout << "    |--- Subarray Latency   = " << TO_SECOND(bank->mat->subarray->writeLatency) << std::endl;
        if (config->technology.cell->memCellType == MRAM)
            std::cout << "       |--- Write Pulse Duration = " << TO_SECOND(config->technology.cell->resetPulse) << std::endl;	// MRAM reset/set is equal
        std::cout << "       |--- Row Decoder Latency = " << TO_SECOND(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
        std::cout << "       |--- Charge Latency      = " << TO_SECOND(bank->mat->subarray->chargeLatency) << std::endl;
    }

    double readBandwidth = (double)bank->blockSize /
        (bank->mat->subarray->readLatency - bank->mat->subarray->rowDecoder->readLatency
         + bank->mat->subarray->precharger->readLatency) / 8;
    std::cout << " - Read Bandwidth  = " << TO_BPS(readBandwidth) << std::endl;
    double writeBandwidth = (double)bank->blockSize /
        (bank->mat->subarray->writeLatency) / 8;
    std::cout << " - Write Bandwidth = " << TO_BPS(writeBandwidth) << std::endl;

    std::cout << "Power:" << std::endl;

    std::cout << " -  Read Dynamic Energy = " << TO_JOULE(bank->readDynamicEnergy) << std::endl;
    if (config->input.routingMode == h_tree)
        std::cout << " |--- H-Tree Dynamic Energy = " << TO_JOULE(bank->readDynamicEnergy - bank->mat->readDynamicEnergy
                * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
            << std::endl;
    else
        std::cout << " |--- Non-H-Tree Dynamic Energy = " << TO_JOULE(bank->readDynamicEnergy - bank->mat->readDynamicEnergy
                * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
            << std::endl;
    std::cout << " |--- Mat Dynamic Energy    = " << TO_JOULE(bank->mat->readDynamicEnergy) << " per mat" << std::endl;
    std::cout << "    |--- Predecoder Dynamic Energy = " << TO_JOULE(bank->mat->readDynamicEnergy - bank->mat->subarray->readDynamicEnergy
            * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
        << std::endl;
    std::cout << "    |--- Subarray Dynamic Energy   = " << TO_JOULE(bank->mat->subarray->readDynamicEnergy) << " per active subarray" << std::endl;
    std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->readDynamicEnergy) << std::endl;
    std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->readDynamicEnergy
            + bank->mat->subarray->senseAmpMuxLev1Decoder->readDynamicEnergy
            + bank->mat->subarray->senseAmpMuxLev2Decoder->readDynamicEnergy) << std::endl;
    if (config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == FBRAM || config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == memristor || config->technology.cell->memCellType == FEFETRAM) {
        std::cout << "       |--- Bitline & Cell Read Energy = " << TO_JOULE(bank->mat->subarray->cellReadEnergy) << std::endl;
    }
    if (config->input.internalSensing)
        std::cout << "       |--- Senseamp Dynamic Energy    = " << TO_JOULE(bank->mat->subarray->senseAmp->readDynamicEnergy) << std::endl;
    std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->readDynamicEnergy
            + bank->mat->subarray->senseAmpMuxLev1->readDynamicEnergy
            + bank->mat->subarray->senseAmpMuxLev2->readDynamicEnergy) << std::endl;
    std::cout << "       |--- Precharge Dynamic Energy   = " << TO_JOULE(bank->mat->subarray->precharger->readDynamicEnergy) << std::endl;

    if (config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == FBRAM ||
            (config->technology.cell->memCellType == memristor && (config->technology.cell->accessType == CMOS_access || config->technology.cell->accessType == BJT_access))) {
        std::cout << " - RESET Dynamic Energy = " << TO_JOULE(bank->resetDynamicEnergy) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << TO_JOULE(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- H-Tree Dynamic Energy = " << TO_JOULE(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << TO_JOULE(bank->mat->resetDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << TO_JOULE(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << TO_JOULE(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Cell RESET Dynamic Energy  = " << TO_JOULE(bank->mat->subarray->cellResetEnergy) << std::endl;
        std::cout << " - SET Dynamic Energy = " << TO_JOULE(bank->setDynamicEnergy) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << TO_JOULE(bank->setDynamicEnergy - bank->mat->setDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- Non-H-Tree Dynamic Energy = " << TO_JOULE(bank->setDynamicEnergy - bank->mat->setDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << TO_JOULE(bank->mat->setDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << TO_JOULE(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << TO_JOULE(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Cell SET Dynamic Energy    = " << TO_JOULE(bank->mat->subarray->cellSetEnergy) << std::endl;
    } else if (config->technology.cell->memCellType == SLCNAND) {
        std::cout << " - Erase Dynamic Energy = " << TO_JOULE(bank->resetDynamicEnergy) << " per block" << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << TO_JOULE(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- Non-H-Tree Dynamic Energy = " << TO_JOULE(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << TO_JOULE(bank->mat->resetDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << TO_JOULE(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << TO_JOULE(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2->writeDynamicEnergy) << std::endl;
        std::cout << " - Programming Dynamic Energy = " << TO_JOULE(bank->setDynamicEnergy) << " per page" << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << TO_JOULE(bank->setDynamicEnergy - bank->mat->setDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- Non-H-Tree Dynamic Energy = " << TO_JOULE(bank->setDynamicEnergy - bank->mat->setDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << TO_JOULE(bank->mat->setDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << TO_JOULE(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << TO_JOULE(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2->writeDynamicEnergy) << std::endl;
    } else {
        std::cout << " - Write Dynamic Energy = " << TO_JOULE(bank->writeDynamicEnergy) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << TO_JOULE(bank->writeDynamicEnergy - bank->mat->writeDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- Non-H-Tree Dynamic Energy = " << TO_JOULE(bank->writeDynamicEnergy - bank->mat->writeDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << TO_JOULE(bank->mat->writeDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << TO_JOULE(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << TO_JOULE(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev1->writeDynamicEnergy
                + bank->mat->subarray->senseAmpMuxLev2->writeDynamicEnergy) << std::endl;
        if (config->technology.cell->memCellType == MRAM) {
            std::cout << "       |--- Bitline & Cell Write Energy= " << TO_JOULE(bank->mat->subarray->cellResetEnergy) << std::endl;
        }
    }

    std::cout << " - Leakage Power = " << TO_WATT(bank->leakage) << std::endl;
    if (config->input.routingMode == h_tree)
        std::cout << " |--- H-Tree Leakage Power = " << TO_WATT(bank->leakage - bank->mat->leakage
                * bank->numColumnMat * bank->numRowMat)
            << std::endl;
    else
        std::cout << " |--- Non-H-Tree Leakage Power = " << TO_WATT(bank->leakage - bank->mat->leakage
                * bank->numColumnMat * bank->numRowMat)
            << std::endl;
    std::cout << " |--- Mat Leakage Power    = " << TO_WATT(bank->mat->leakage) << " per mat" << std::endl;
}


void Result::printAsCache(Result &tagResult, CacheAccessMode cacheAccessMode) {
    if (bank->memoryType != mem_data || tagResult.bank->memoryType != tag) {
        std::cout << "This is not a valid cache configuration." << std::endl;
        return;
    } else {
        double cacheHitLatency, cacheMissLatency, cacheWriteLatency;
        double cacheHitDynamicEnergy, cacheMissDynamicEnergy, cacheWriteDynamicEnergy;
        double cacheLeakage;
        double cacheArea;
        if (cacheAccessMode == normal_access_mode) {
            /* Calculate latencies */
            cacheMissLatency = tagResult.bank->readLatency;		/* only the tag access latency */
            cacheHitLatency = MAX(tagResult.bank->readLatency, bank->mat->readLatency);	/* access tag and activate mem_data row in parallel */
            cacheHitLatency += bank->mat->subarray->columnDecoderLatency;		/* add column decoder latency after hit signal arrives */
            cacheHitLatency += bank->readLatency - bank->mat->readLatency;	/* H-tree in and out latency */
            cacheWriteLatency = MAX(tagResult.bank->writeLatency, bank->writeLatency);	/* Data and tag are written in parallel */
            /* Calculate power */
            cacheMissDynamicEnergy = tagResult.bank->readDynamicEnergy;	/* no matter what tag is always accessed */
            cacheMissDynamicEnergy += bank->readDynamicEnergy;	/* mem_data is also partially accessed, TODO: not accurate here */
            cacheHitDynamicEnergy = tagResult.bank->readDynamicEnergy + bank->readDynamicEnergy;
            cacheWriteDynamicEnergy = tagResult.bank->writeDynamicEnergy + bank->writeDynamicEnergy;
        } else if (cacheAccessMode == fast_access_mode) {
            /* Calculate latencies */
            cacheMissLatency = tagResult.bank->readLatency;
            cacheHitLatency = MAX(tagResult.bank->readLatency, bank->readLatency);
            cacheWriteLatency = MAX(tagResult.bank->writeLatency, bank->writeLatency);
            /* Calculate power */
            cacheMissDynamicEnergy = tagResult.bank->readDynamicEnergy;	/* no matter what tag is always accessed */
            cacheMissDynamicEnergy += bank->readDynamicEnergy;	/* mem_data is also partially accessed, TODO: not accurate here */
            cacheHitDynamicEnergy = tagResult.bank->readDynamicEnergy + bank->readDynamicEnergy;
            cacheWriteDynamicEnergy = tagResult.bank->writeDynamicEnergy + bank->writeDynamicEnergy;
        } else {		/* sequential access */
            /* Calculate latencies */
            cacheMissLatency = tagResult.bank->readLatency;
            cacheHitLatency = tagResult.bank->readLatency + bank->readLatency;
            cacheWriteLatency = MAX(tagResult.bank->writeLatency, bank->writeLatency);
            /* Calculate power */
            cacheMissDynamicEnergy = tagResult.bank->readDynamicEnergy;	/* no matter what tag is always accessed */
            cacheHitDynamicEnergy = tagResult.bank->readDynamicEnergy + bank->readDynamicEnergy;
            cacheWriteDynamicEnergy = tagResult.bank->writeDynamicEnergy + bank->writeDynamicEnergy;
        }
        /* Calculate leakage */
        cacheLeakage = tagResult.bank->leakage + bank->leakage;
        /* Calculate area */
        cacheArea = tagResult.bank->area + bank->area;	/* TODO: simply add them together here */

        /* start printing */
        std::cout << std::endl << "=======================" << std::endl << "CACHE DESIGN -- SUMMARY" << std::endl << "=======================" << std::endl;
        std::cout << "Access Mode: ";
        switch (cacheAccessMode) {
            case normal_access_mode:
                std::cout << "Normal" << std::endl;
                break;
            case fast_access_mode:
                std::cout << "Fast" << std::endl;
                break;
            default:	/* sequential */
                std::cout << "Sequential" << std::endl;
        }
        std::cout << "Area:" << std::endl;
        std::cout << " - Total Area = " << cacheArea * 1e6 << "mm^2" << std::endl;
        std::cout << " |--- Data Array Area = " << bank->height * 1e6 << "um x " << bank->width * 1e6 << "um = " << bank->area * 1e6 << "mm^2" << std::endl;
        std::cout << " |--- Tag Array Area  = " << tagResult.bank->height * 1e6 << "um x " << tagResult.bank->width * 1e6 << "um = " << tagResult.bank->area * 1e6 << "mm^2" << std::endl;
        std::cout << "Timing:" << std::endl;
        std::cout << " - Cache Hit Latency   = " << cacheHitLatency * 1e9 << "ns" << std::endl;
        std::cout << " - Cache Miss Latency  = " << cacheMissLatency * 1e9 << "ns" << std::endl;
        std::cout << " - Cache Write Latency = " << cacheWriteLatency * 1e9 << "ns" << std::endl;
        std::cout << "Power:" << std::endl;
        std::cout << " - Cache Hit Dynamic Energy   = " << cacheHitDynamicEnergy * 1e9 << "nJ per access" << std::endl;
        std::cout << " - Cache Miss Dynamic Energy  = " << cacheMissDynamicEnergy * 1e9 << "nJ per access" << std::endl;
        std::cout << " - Cache Write Dynamic Energy = " << cacheWriteDynamicEnergy * 1e9 << "nJ per access" << std::endl;
        std::cout << " - Cache Total Leakage Power  = " << cacheLeakage * 1e3 << "mW" << std::endl;
        std::cout << " |--- Cache Data Array Leakage Power = " << bank->leakage * 1e3 << "mW" << std::endl;
        std::cout << " |--- Cache Tag Array Leakage Power  = " << tagResult.bank->leakage * 1e3 << "mW" << std::endl;

        std::cout << std::endl << "CACHE DATA ARRAY";
        print();
        std::cout << std::endl << "CACHE TAG ARRAY";
        tagResult.print();
    }
}

void Result::printToCsvFile(std::ostream &outputFile) {
    outputFile << bank->numRowMat << "," << bank->numColumnMat << "," << bank->numActiveMatPerColumn << "," << bank->numActiveMatPerRow << ",";
    outputFile << bank->numRowSubarray << "," << bank->numColumnSubarray << "," << bank->numActiveSubarrayPerColumn << "," << bank->numActiveSubarrayPerRow << ",";
    outputFile << bank->mat->subarray->numRow << "," << bank->mat->subarray->numColumn << ",";
    outputFile << bank->muxSenseAmp << "," << bank->muxOutputLev1 << "," << bank->muxOutputLev2 << ",";
    if (config->input.designTarget == cache)
        outputFile << bank->numRowPerSet << ",";
    else
        outputFile << "N/A,";
    switch (localWire->wireType) {
        case local_aggressive:
            outputFile << "Local Aggressive" << ",";
            break;
        case local_conservative:
            outputFile << "Local Conservative" << ",";
            break;
        case semi_aggressive:
            outputFile << "Semi-Global Aggressive" << ",";
            break;
        case semi_conservative:
            outputFile << "Semi-Global Conservative" << ",";
            break;
        case global_aggressive:
            outputFile << "Global Aggressive" << ",";
            break;
        case global_conservative:
            outputFile << "Global Conservative" << ",";
            break;
        default:
            outputFile << "DRAM Wire" << ",";
    }
    switch (localWire->wireRepeaterType) {
        case repeated_none:
            outputFile << "No Repeaters" << ",";
            break;
        case repeated_opt:
            outputFile << "Fully-Optimized Repeaters" << ",";
            break;
        case repeated_5:
            outputFile << "Repeaters with 5% Overhead" << ",";
            break;
        case repeated_10:
            outputFile << "Repeaters with 10% Overhead" << ",";
            break;
        case repeated_20:
            outputFile << "Repeaters with 20% Overhead" << ",";
            break;
        case repeated_30:
            outputFile << "Repeaters with 30% Overhead" << ",";
            break;
        case repeated_40:
            outputFile << "Repeaters with 40% Overhead" << ",";
            break;
        case repeated_50:
            outputFile << "Repeaters with 50% Overhead" << ",";
            break;
        default:
            outputFile << "N/A" << ",";
    }
    if (localWire->isLowSwing)
        outputFile << "Yes" << ",";
    else
        outputFile << "No" << ",";
    switch (globalWire->wireType) {
        case local_aggressive:
            outputFile << "Local Aggressive" << ",";
            break;
        case local_conservative:
            outputFile << "Local Conservative" << ",";
            break;
        case semi_aggressive:
            outputFile << "Semi-Global Aggressive" << ",";
            break;
        case semi_conservative:
            outputFile << "Semi-Global Conservative" << ",";
            break;
        case global_aggressive:
            outputFile << "Global Aggressive" << ",";
            break;
        case global_conservative:
            outputFile << "Global Conservative" << ",";
            break;
        default:
            outputFile << "DRAM Wire" << ",";
    }
    switch (globalWire->wireRepeaterType) {
        case repeated_none:
            outputFile << "No Repeaters" << ",";
            break;
        case repeated_opt:
            outputFile << "Fully-Optimized Repeaters" << ",";
            break;
        case repeated_5:
            outputFile << "Repeaters with 5% Overhead" << ",";
            break;
        case repeated_10:
            outputFile << "Repeaters with 10% Overhead" << ",";
            break;
        case repeated_20:
            outputFile << "Repeaters with 20% Overhead" << ",";
            break;
        case repeated_30:
            outputFile << "Repeaters with 30% Overhead" << ",";
            break;
        case repeated_40:
            outputFile << "Repeaters with 40% Overhead" << ",";
            break;
        case repeated_50:
            outputFile << "Repeaters with 50% Overhead" << ",";
            break;
        default:
            outputFile << "N/A" << ",";
    }
    if (globalWire->isLowSwing)
        outputFile << "Yes" << ",";
    else
        outputFile << "No" << ",";
    switch (bank->areaOptimizationLevel) {
        case latency_first:
            outputFile << "Latency-Optimized" << ",";
            break;
        case area_first:
            outputFile << "Area-Optimized" << ",";
            break;
        default:	/* balance */
            outputFile << "Balanced" << ",";
    }
    outputFile << bank->height * 1e6 << "," << bank->width * 1e6 << "," << bank->area * 1e6 << ",";
    outputFile << bank->mat->height * 1e6 << "," << bank->mat->width * 1e6 << "," << bank->mat->area * 1e6 << ",";
    outputFile << bank->mat->subarray->height * 1e6 << "," << bank->mat->subarray->width * 1e6 << "," << bank->mat->subarray->area * 1e6 << ",";
    outputFile << config->technology.cell->area * config->technology.tech->featureSize() * config->technology.tech->featureSize() * bank->capacity / bank->area * 100 << ",";
    outputFile << bank->readLatency * 1e9 << "," << bank->writeLatency * 1e9 << ",";
    outputFile << bank->readDynamicEnergy * 1e12 << "," << bank->writeDynamicEnergy * 1e12 << ",";
    outputFile << bank->leakage * 1e3 << ",";
}

void Result::printAsCacheToCsvFile(Result &tagResult, CacheAccessMode cacheAccessMode, std::ostream &outputFile) {
    if (bank->memoryType != mem_data || tagResult.bank->memoryType != tag) {
        std::cout << "This is not a valid cache configuration." << std::endl;
        return;
    } else {
        double cacheHitLatency, cacheMissLatency, cacheWriteLatency;
        double cacheHitDynamicEnergy, cacheMissDynamicEnergy, cacheWriteDynamicEnergy;
        double cacheLeakage;
        double cacheArea;
        if (cacheAccessMode == normal_access_mode) {
            /* Calculate latencies */
            cacheMissLatency = tagResult.bank->readLatency;		/* only the tag access latency */
            cacheHitLatency = MAX(tagResult.bank->readLatency, bank->mat->readLatency);	/* access tag and activate mem_data row in parallel */
            cacheHitLatency += bank->mat->subarray->columnDecoderLatency;		/* add column decoder latency after hit signal arrives */
            cacheHitLatency += bank->readLatency - bank->mat->readLatency;	/* H-tree in and out latency */
            cacheWriteLatency = MAX(tagResult.bank->writeLatency, bank->writeLatency);	/* Data and tag are written in parallel */
            /* Calculate power */
            cacheMissDynamicEnergy = tagResult.bank->readDynamicEnergy;	/* no matter what tag is always accessed */
            cacheMissDynamicEnergy += bank->readDynamicEnergy;	/* mem_data is also partially accessed, TODO: not accurate here */
            cacheHitDynamicEnergy = tagResult.bank->readDynamicEnergy + bank->readDynamicEnergy;
            cacheWriteDynamicEnergy = tagResult.bank->writeDynamicEnergy + bank->writeDynamicEnergy;
        } else if (cacheAccessMode == fast_access_mode) {
            /* Calculate latencies */
            cacheMissLatency = tagResult.bank->readLatency;
            cacheHitLatency = MAX(tagResult.bank->readLatency, bank->readLatency);
            cacheWriteLatency = MAX(tagResult.bank->writeLatency, bank->writeLatency);
            /* Calculate power */
            cacheMissDynamicEnergy = tagResult.bank->readDynamicEnergy;	/* no matter what tag is always accessed */
            cacheMissDynamicEnergy += bank->readDynamicEnergy;	/* mem_data is also partially accessed, TODO: not accurate here */
            cacheHitDynamicEnergy = tagResult.bank->readDynamicEnergy + bank->readDynamicEnergy;
            cacheWriteDynamicEnergy = tagResult.bank->writeDynamicEnergy + bank->writeDynamicEnergy;
        } else {		/* sequential access */
            /* Calculate latencies */
            cacheMissLatency = tagResult.bank->readLatency;
            cacheHitLatency = tagResult.bank->readLatency + bank->readLatency;
            cacheWriteLatency = MAX(tagResult.bank->writeLatency, bank->writeLatency);
            /* Calculate power */
            cacheMissDynamicEnergy = tagResult.bank->readDynamicEnergy;	/* no matter what tag is always accessed */
            cacheHitDynamicEnergy = tagResult.bank->readDynamicEnergy + bank->readDynamicEnergy;
            cacheWriteDynamicEnergy = tagResult.bank->writeDynamicEnergy + bank->writeDynamicEnergy;
        }
        /* Calculate leakage */
        cacheLeakage = tagResult.bank->leakage + bank->leakage;
        /* Calculate area */
        cacheArea = tagResult.bank->area + bank->area;	/* TODO: simply add them together here */

        /* start printing */
        switch (cacheAccessMode) {
            case normal_access_mode:
                outputFile << "Normal" << ",";
                break;
            case fast_access_mode:
                outputFile << "Fast" << ",";
                break;
            default:	/* sequential */
                outputFile << "Sequential" << ",";
        }
        outputFile << cacheArea * 1e6 << ",";
        outputFile << cacheHitLatency * 1e9 << ",";
        outputFile << cacheMissLatency * 1e9 << ",";
        outputFile << cacheWriteLatency * 1e9 << ",";
        outputFile << cacheHitDynamicEnergy * 1e9 << ",";
        outputFile << cacheMissDynamicEnergy * 1e9 << ",";
        outputFile << cacheWriteDynamicEnergy * 1e9 << ",";
        outputFile << cacheLeakage * 1e3 << ",";
        printToCsvFile(outputFile);
        tagResult.printToCsvFile(outputFile);
        outputFile << std::endl;
    }
}
