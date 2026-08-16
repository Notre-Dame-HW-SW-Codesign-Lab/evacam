#include <iostream>

#include "Result.h"
#include "UnitFormatter.h"

namespace {

double LocalSearchLatency(const Result &result) {
    const auto &bank = result.bank;
    const auto &subarray = bank->mat->subarray;
    if (result.config->peripherals.noPrechargeInc) {
        return subarray->matchlineDelay
            + subarray->ColMux[subarray->indexMatchline]->readLatency
            + subarray->senseAmpLatency
            + subarray->outputAcc->readLatency;
    }

    double latency = subarray->searchLatency * bank->mat->muxSenseAmp
        - subarray->inputBuf->readLatency * (bank->mat->muxSenseAmp - 1);
    if (result.config->peripherals.withOutputAcc) {
        latency *= result.config->input.wordWidth / bank->CAM_opt.BitSerialWidth;
    }
    return latency;
}

}  // namespace

void Result::print() {
    // std::cout << "Bank Area: " << bank->area * 1e12 << std::endl;
    // std::cout << "Bank Search Latency: " << bank->searchLatency * 1e9 << std::endl;
    // std::cout << "Bank Search Dynamic Energy: " << bank->searchDynamicEnergy * 1e12 << std::endl;
    // std::cout << "Bank Write Dynamic Energy: " << bank->writeDynamicEnergy * 1e12 << std::endl;
    // std::cout << "Bank Leakage: " << bank->leakage * 1e6 << std::endl;
    // std::cout << "==============================" <<std::endl;
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
    std::cout << "Local Wire:" << std::endl;
    std::cout << " - Wire Type : ";
    switch (localWire.wireType) {
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
    switch (localWire.wireRepeaterType) {
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
    if (localWire.isLowSwing)
        std::cout << "Yes" << std::endl;
    else
        std::cout << "No" << std::endl;
    std::cout << "Global Wire:" << std::endl;
    std::cout << " - Wire Type : ";
    switch (globalWire.wireType) {
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
    switch (globalWire.wireRepeaterType) {
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
    if (globalWire.isLowSwing)
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
    if (bank->mat->subarray->variationSummary.enabled) {
        std::cout << "Variation:" << std::endl;
        std::cout << " - Mode                         : " << config->variation.mode << std::endl;
        std::cout << " - Base Seed                    : " << config->variation.seed << std::endl;
        std::cout << " - Samples                      : " << config->variation.samples << std::endl;
        std::cout << " - Memory Device On Stddev      : " << config->variation.memoryDeviceResOnStdev * 100 << "%" << std::endl;
        std::cout << " - Memory Device Off Stddev     : " << config->variation.memoryDeviceResOffStdev * 100 << "%" << std::endl;
    }

    std::cout << std::endl;

    std::cout << "==============================================" << std::endl << "               SUMMARY RESULT" << std::endl << "==============================================" << std::endl;

    std::cout << "Area:" << std::endl;

    std::cout << " - Total Area = " << ToMeter(bank->height) << " x " << ToMeter(bank->width)
        << " = " << ToSquareMeter(bank->area) << std::endl;
    std::cout << " |--- Mat Area      = " << ToMeter(bank->mat->height) << " x " << ToMeter(bank->mat->width)
        << " = " << ToSquareMeter(bank->mat->area) << "   (" << config->technology.cell->area * config->technology.tech->featureSize() * config->technology.tech->featureSize()
        * bank->capacity / bank->numRowMat / bank->numColumnMat / bank->mat->area * 100 << "%)" << std::endl;
    std::cout << " |--- Subarray Area = " << ToMeter(bank->mat->subarray->height) << " x "
        << ToMeter(bank->mat->subarray->width) << " = " << ToSquareMeter(bank->mat->subarray->area) << "   ("
        << config->technology.cell->area * config->technology.tech->featureSize() * config->technology.tech->featureSize() * bank->capacity / bank->numRowMat
        / bank->numColumnMat / bank->numRowSubarray / bank->numColumnSubarray
        / bank->mat->subarray->area * 100 << "%)" <<std::endl;
    std::cout << " |--- Subarray Dimensions = " << bank->mat->subarray->numRow
        << " Rows x " << bank->mat->subarray->numColumn << " Columns" << std::endl;
    std::cout << " - Area Efficiency = " << config->technology.cell->area * config->technology.tech->featureSize() * config->technology.tech->featureSize()
        * bank->capacity / bank->area * 100 << "%" << std::endl;
    std::cout << "Timing:" << std::endl;

    const double localSearchLatency = LocalSearchLatency(*this);
    std::cout << " -  Search Latency = " << ToSecond(bank->searchLatency) << std::endl;
    if (config->input.routingMode == h_tree)
        std::cout << " |--- H-Tree Latency = " << ToSecond(bank->searchLatency - localSearchLatency) << std::endl;
    else
        std::cout << " |--- Non-H-Tree Latency = " << ToSecond(bank->searchLatency - localSearchLatency) << std::endl;
    std::cout << " |--- Mat Latency    = " << ToSecond(localSearchLatency) << std::endl;
    std::cout << "    |--- Predecoder Latency = " << ToSecond(bank->mat->predecoderLatency) << std::endl;
    // std::cout << "       |--- Row Decoder Latency = " << ToSecond(bank->mat->subarray->rowDecoder->readLatency) << std::endl;
    // std::cout << "       |--- Matchline Latency     = " << ToSecond(bank->mat->subarray->MatchlineDelay) << std::endl;
    // if (config->input.internalSensing)
    // 	std::cout << "       |--- Senseamp Latency    = " << ToSecond(bank->mat->subarray->senseAmp.readLatency) << std::endl;
    // std::cout << "       |--- Mux Latency         = " << ToSecond(bank->mat->subarray->bitlineMux->readLatency
    // 												+ bank->mat->subarray->senseAmpMuxLev1.readLatency
    // 												+ bank->mat->subarray->senseAmpMuxLev2.readLatency) << std::endl;
    // std::cout << "       |--- Precharge Latency   = " << ToSecond(bank->mat->subarray->precharger.readLatency) << std::endl;

    if (config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == FBRAM || config->technology.cell->memCellType == FEFETRAM ||
            (config->technology.cell->memCellType == memristor && (config->technology.cell->accessType == CMOS_access || config->technology.cell->accessType == BJT_access))) {
        std::cout << " - RESET Latency = " << ToSecond(bank->resetLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << ToSecond(bank->resetLatency - bank->mat->resetLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << ToSecond(bank->resetLatency - bank->mat->resetLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << ToSecond(bank->mat->resetLatency) << std::endl;
        std::cout << "    |--- Predecoder Latency = " << ToSecond(bank->mat->predecoderLatency) << std::endl;
        std::cout << "    |--- Subarray Latency   = " << ToSecond(bank->mat->subarray->resetLatency) << std::endl;
        // std::cout << "       |--- RESET Pulse Duration = " << ToSecond(config->technology.cell->resetPulse) << std::endl;
        // std::cout << "       |--- Row Decoder Latency  = " << ToSecond(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
        // std::cout << "       |--- Charge Latency   = " << ToSecond(bank->mat->subarray->chargeLatency) << std::endl;
        // std::cout << " - SET Latency   = " << ToSecond(bank->setLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << ToSecond(bank->setLatency - bank->mat->setLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << ToSecond(bank->setLatency - bank->mat->setLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << ToSecond(bank->mat->setLatency) << std::endl;
        std::cout << "    |--- Predecoder Latency = " << ToSecond(bank->mat->predecoderLatency) << std::endl;
        std::cout << "    |--- Subarray Latency   = " << ToSecond(bank->mat->subarray->setLatency) << std::endl;
        // std::cout << "       |--- SET Pulse Duration   = " << ToSecond(config->technology.cell->setPulse) << std::endl;
        // std::cout << "       |--- Row Decoder Latency  = " << ToSecond(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
        // std::cout << "       |--- Charger Latency      = " << ToSecond(bank->mat->subarray->chargeLatency) << std::endl;
    } else if (config->technology.cell->memCellType == SLCNAND) {
        std::cout << " - Erase Latency = " << ToSecond(bank->resetLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << ToSecond(bank->resetLatency - bank->mat->resetLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << ToSecond(bank->resetLatency - bank->mat->resetLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << ToSecond(bank->mat->resetLatency) << std::endl;
        std::cout << " - Programming Latency   = " << ToSecond(bank->setLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << ToSecond(bank->setLatency - bank->mat->setLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << ToSecond(bank->setLatency - bank->mat->setLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << ToSecond(bank->mat->setLatency) << std::endl;
    } else {
        std::cout << " - Write Latency = " << ToSecond(bank->writeLatency) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Latency = " << ToSecond(bank->writeLatency - bank->mat->writeLatency) << std::endl;
        else
            std::cout << " |--- Non-H-Tree Latency = " << ToSecond(bank->writeLatency - bank->mat->writeLatency) << std::endl;
        std::cout << " |--- Mat Latency    = " << ToSecond(bank->mat->writeLatency) << std::endl;
        std::cout << "    |--- Predecoder Latency = " << ToSecond(bank->mat->predecoderLatency) << std::endl;
        std::cout << "    |--- Subarray Latency   = " << ToSecond(bank->mat->subarray->writeLatency) << std::endl;
        // if (config->technology.cell->memCellType == MRAM)
        // 	std::cout << "       |--- Write Pulse Duration = " << ToSecond(config->technology.cell->resetPulse) << std::endl;	// MRAM reset/set is equal
        // std::cout << "       |--- Row Decoder Latency = " << ToSecond(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
        // std::cout << "       |--- Charge Latency      = " << ToSecond(bank->mat->subarray->chargeLatency) << std::endl;
    }

    double readBandwidth = (double)bank->blockSize /
        (bank->mat->subarray->readLatency - bank->mat->subarray->rowDecoder->readLatency
         + bank->mat->subarray->precharger->readLatency) / 8;
    std::cout << " - Read Bandwidth  = " << ToBps(readBandwidth) << std::endl;
    double writeBandwidth = (double)bank->blockSize /
        (bank->mat->subarray->writeLatency) / 8;
    std::cout << " - Write Bandwidth = " << ToBps(writeBandwidth) << std::endl;

    std::cout << "Power:" << std::endl;

    std::cout << " -  Read Dynamic Energy = " << ToJoule(bank->readDynamicEnergy) << std::endl;
    if (config->input.routingMode == h_tree)
        std::cout << " |--- H-Tree Dynamic Energy = " << ToJoule(bank->readDynamicEnergy - bank->mat->readDynamicEnergy
                * bank->numActiveMatPerColumn * bank->numActiveMatPerRow) 
            << std::endl;
    else
        std::cout << " |--- Non-H-Tree Dynamic Energy = " << ToJoule(bank->readDynamicEnergy - bank->mat->readDynamicEnergy
                * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
            << std::endl;
    std::cout << " |--- Mat Dynamic Energy    = " << ToJoule(bank->mat->readDynamicEnergy) << " per mat" << std::endl;
    std::cout << "    |--- Predecoder Dynamic Energy = " << ToJoule(bank->mat->readDynamicEnergy - bank->mat->subarray->readDynamicEnergy
            * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
        << std::endl;
    std::cout << "    |--- Subarray Dynamic Energy   = " << ToJoule(bank->mat->subarray->readDynamicEnergy) << " per active subarray" << std::endl;
    // std::cout << "       |--- Row Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->rowDecoder->readDynamicEnergy) << std::endl;
    // std::cout << "       |--- Mux Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->bitlineMuxDecoder->readDynamicEnergy
    // 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->readDynamicEnergy
    // 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->readDynamicEnergy) << std::endl;
    // if (config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == FBRAM || config->technology.cell->memCellType == MRAM || config->technology.cell->memCellType == memristor || config->technology.cell->memCellType == FEFETRAM	 ) {
    // 	std::cout << "       |--- Bitline & Cell Read Energy = " << ToJoule(bank->mat->subarray->cellReadEnergy) << std::endl;
    // }
    // if (config->input.internalSensing)
    // 	std::cout << "       |--- Senseamp Dynamic Energy    = " << ToJoule(bank->mat->subarray->senseAmp.readDynamicEnergy) << std::endl;
    // std::cout << "       |--- Mux Dynamic Energy         = " << ToJoule(bank->mat->subarray->bitlineMux->readDynamicEnergy
    // 												+ bank->mat->subarray->senseAmpMuxLev1.readDynamicEnergy
    // 												+ bank->mat->subarray->senseAmpMuxLev2.readDynamicEnergy) << std::endl;
    // std::cout << "       |--- Precharge Dynamic Energy   = " << ToJoule(bank->mat->subarray->precharger.readDynamicEnergy) << std::endl;

    if (config->technology.cell->memCellType == PCRAM || config->technology.cell->memCellType == FBRAM ||
            (config->technology.cell->memCellType == memristor && (config->technology.cell->accessType == CMOS_access || config->technology.cell->accessType == BJT_access || config->technology.cell->memCellType == FEFETRAM))) {
        std::cout << " - RESET Dynamic Energy = " << ToJoule(bank->resetDynamicEnergy) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << ToJoule(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- H-Tree Dynamic Energy = " << ToJoule(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << ToJoule(bank->mat->resetDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << ToJoule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << ToJoule(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        // std::cout << "       |--- Row Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Dynamic Energy         = " << ToJoule(bank->mat->subarray->bitlineMux->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Cell RESET Dynamic Energy  = " << ToJoule(bank->mat->subarray->cellResetEnergy) << std::endl;
        std::cout << " - SET Dynamic Energy = " << ToJoule(bank->setDynamicEnergy) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << ToJoule(bank->setDynamicEnergy - bank->mat->setDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- Non-H-Tree Dynamic Energy = " << ToJoule(bank->setDynamicEnergy - bank->mat->setDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << ToJoule(bank->mat->setDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << ToJoule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << ToJoule(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        // std::cout << "       |--- Row Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Dynamic Energy         = " << ToJoule(bank->mat->subarray->bitlineMux->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Cell SET Dynamic Energy    = " << ToJoule(bank->mat->subarray->cellSetEnergy) << std::endl;
    } else if (config->technology.cell->memCellType == SLCNAND) {
        std::cout << " - Erase Dynamic Energy = " << ToJoule(bank->resetDynamicEnergy) << " per block" << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << ToJoule(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- Non-H-Tree Dynamic Energy = " << ToJoule(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << ToJoule(bank->mat->resetDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << ToJoule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << ToJoule(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        // std::cout << "       |--- Row Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Dynamic Energy         = " << ToJoule(bank->mat->subarray->bitlineMux->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
        // std::cout << " - Programming Dynamic Energy = " << ToJoule(bank->setDynamicEnergy) << " per page" << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << ToJoule(bank->setDynamicEnergy - bank->mat->setDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- Non-H-Tree Dynamic Energy = " << ToJoule(bank->setDynamicEnergy - bank->mat->setDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << ToJoule(bank->mat->setDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << ToJoule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << ToJoule(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        // std::cout << "       |--- Row Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Dynamic Energy         = " << ToJoule(bank->mat->subarray->bitlineMux->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
    } else {
        std::cout << " - Write Dynamic Energy = " << ToJoule(bank->writeDynamicEnergy) << std::endl;
        if (config->input.routingMode == h_tree)
            std::cout << " |--- H-Tree Dynamic Energy = " << ToJoule(bank->writeDynamicEnergy - bank->mat->writeDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        else
            std::cout << " |--- Non-H-Tree Dynamic Energy = " << ToJoule(bank->writeDynamicEnergy - bank->mat->writeDynamicEnergy
                    * bank->numActiveMatPerColumn * bank->numActiveMatPerRow)
                << std::endl;
        std::cout << " |--- Mat Dynamic Energy    = " << ToJoule(bank->mat->writeDynamicEnergy) << " per mat" << std::endl;
        std::cout << "    |--- Predecoder Dynamic Energy = " << ToJoule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy
                * bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn)
            << std::endl;
        std::cout << "    |--- Subarray Dynamic Energy   = " << ToJoule(bank->mat->subarray->writeDynamicEnergy) << " per active subarray" << std::endl;
        // std::cout << "       |--- Row Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Decoder Dynamic Energy = " << ToJoule(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
        // std::cout << "       |--- Mux Dynamic Energy         = " << ToJoule(bank->mat->subarray->bitlineMux->writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
        // 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
        // if (config->technology.cell->memCellType == MRAM) {
        // 	std::cout << "       |--- Bitline & Cell Write Energy= " << ToJoule(bank->mat->subarray->cellResetEnergy) << std::endl;
        // }
    }

    std::cout << " - Leakage Power = " << ToWatt(bank->leakage) << std::endl;
    if (config->input.routingMode == h_tree)
        std::cout << " |--- H-Tree Leakage Power = " << ToWatt(bank->leakage - bank->mat->leakage
                * bank->numColumnMat * bank->numRowMat)
            << std::endl;
    else
        std::cout << " |--- Non-H-Tree Leakage Power = " << ToWatt(bank->leakage - bank->mat->leakage
                * bank->numColumnMat * bank->numRowMat)
            << std::endl;
    std::cout << " |--- Mat Leakage Power    = " << ToWatt(bank->mat->leakage) << " per mat" << std::endl;
    std::cout << "    |--- Predecoder Leakage Power = " << ToWatt(bank->mat->rowPredecoderBlock1->leakage + bank->mat->rowPredecoderBlock2->leakage +
            bank->mat->bitlineMuxPredecoderBlock1->leakage + bank->mat->bitlineMuxPredecoderBlock2->leakage +
            bank->mat->senseAmpMuxLev1PredecoderBlock1->leakage + bank->mat->senseAmpMuxLev1PredecoderBlock2->leakage +
            bank->mat->senseAmpMuxLev2PredecoderBlock1->leakage + bank->mat->senseAmpMuxLev2PredecoderBlock2->leakage ) << std::endl;
    std::cout << "    |--- Subarray Leakage Power   = " << ToWatt(bank->mat->subarray->leakage) << std::endl;

    std::cout << std::endl;        
    std::cout << "==============================================" << std::endl << "         RESULT BREAKDOWN (SUBARRAY)" << std::endl << "==============================================" << std::endl;
    bank->printbreakdown();

    // std::cout << "==============================" <<std::endl;
    // std::cout << bank->numBitSerial << std::endl;
    // std::cout << bank->mat->areaOptimizationLevel << std::endl;
    // std::cout << bank->mat->subarray->DriverOptLevel << std::endl;

    // std::cout << bank->area * 1e12 << std::endl;
    // std::cout << bank->searchLatency * 1e9 << std::endl;
    // std::cout << bank->searchDynamicEnergy * 1e12 << std::endl;
    // std::cout << bank->writeDynamicEnergy * 1e12 << std::endl;
    // std::cout << bank->leakage * 1e6 << std::endl;
    // std::cout << "==============================" <<std::endl;
}
