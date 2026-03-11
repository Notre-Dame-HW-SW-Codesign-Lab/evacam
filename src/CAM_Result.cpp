#include "../include/CAM_Result.h"
#include "../include/Result.h"
#include "../include/global.h"
#include "../include/formula.h"
#include "../include/macros.h"

#include <iostream>
#include <fstream>

CAM_Result::CAM_Result() {
	// TODO Auto-generated constructor stub
       /* 
        inputParameter = _inputParameter;
        
	if (inputParameter->routingMode == h_tree)
		bank = make_unique<BankWithHtree>();
	else
		bank = make_unique<BankWithoutHtree>();

	localWire = make_shared<Wire>();
	globalWire = make_shared<Wire>();

	// initialize the worst case 
	bank->readLatency = 1e41;
	bank->writeLatency = 1e41;
	bank->readDynamicEnergy = 1e41;
	bank->writeDynamicEnergy = 1e41;
	bank->leakage = 1e41;
	bank->height = 1e41;
	bank->width = 1e41;
	bank->area = 1e41;
	bank->searchLatency = 1e41;
	bank->searchDynamicEnergy = 1e41;

	// No constraints 
	limitReadLatency = 1e41;
	limitWriteLatency = 1e41;
	limitReadDynamicEnergy = 1e41;
	limitWriteDynamicEnergy = 1e41;
	limitReadEdp = 1e41;
	limitWriteEdp = 1e41;
	limitArea = 1e41;
	limitLeakage = 1e41;

	// Default read latency optimization
	optimizationTarget = read_latency_optimized;*/
}


CAM_Result::~CAM_Result() {
	// TODO Auto-generated destructor stub
        //if (bank) delete bank;
        //if (localWire) delete localWire;
        //if (globalWire) delete globalWire;
}

void CAM_Result::reset() {
	bank->readLatency = 1e41;
	bank->writeLatency = 1e41;
	bank->readDynamicEnergy = 1e41;
	bank->writeDynamicEnergy = 1e41;
	bank->leakage = 1e41;
	bank->height = 1e41;
	bank->width = 1e41;
	bank->area = 1e41;
	bank->searchLatency = 1e41;
	bank->searchDynamicEnergy = 1e41;
}

void CAM_Result::compareAndUpdate(std::shared_ptr<Result> newResult) {
	// std::cout << "compare: " << newResult->bank->readLatency << ": " << newResult->bank->writeLatency << ": " << newResult->bank->readDynamicEnergy << ": " << newResult->bank->writeDynamicEnergy << ": " << newResult->bank->area << ": " <<newResult->bank->area << std::endl;
        /* 
        std::cout << "1: " << (newResult->bank->readLatency <= limitReadLatency) << std::endl;
        std::cout << "[CAM_Result] newResult->bank->readLatency: " << newResult->bank->readLatency << std::endl;
        std::cout << "[CAM_Result] limitReadLatency: " << limitReadLatency << std::endl << std::endl;

        std::cout << "2: " << (newResult->bank->writeLatency <= limitWriteLatency) << std::endl;
        std::cout << "[CAM_Result] newResult->bank->writeLatency: " << newResult->bank->writeLatency << std::endl;
        std::cout << "[CAM_Result] limitWriteLatency: " << limitWriteLatency << std::endl << std::endl;

        std::cout << "3: " << (newResult->bank->readDynamicEnergy <= limitReadDynamicEnergy) << std::endl;
        std::cout << "[CAM_Result] newResult->bank->readDynamicEnergy: " << newResult->bank->readDynamicEnergy << std::endl;
        std::cout << "[CAM_Result] limitReadDynamicEnergy: " << limitReadDynamicEnergy << std::endl << std::endl;

        std::cout << "4: " << (newResult->bank->writeDynamicEnergy <= limitWriteDynamicEnergy) << std::endl;
        std::cout << "[CAM_Result] newResult->bank->writeDynamicEnergy: " << newResult->bank->writeDynamicEnergy << std::endl;
        std::cout << "[CAM_Result] limitWriteDynamicEnergy: " << limitWriteDynamicEnergy << std::endl << std::endl;

        std::cout << "[CAM_Result] newResult->bank->area: " << newResult->bank->area << std::endl;
        std::cout << "[CAM_Result] newResult->bank->leakage: " << newResult->bank->leakage << std::endl;

        std::cout << "5: " << (newResult->bank->readLatency * newResult->bank->readDynamicEnergy <= limitReadEdp) << std::endl;
        std::cout << "[CAM_Result] newResult->bank->readLatency: " << newResult->bank->readLatency << std::endl;
        std::cout << "[CAM_Result] newResult->bank->readDynamicEnergy: " << newResult->bank->readDynamicEnergy << std::endl;
        std::cout << "[CAM_Result] limitReadEdp: " << limitReadEdp << std::endl << std::endl;

        std::cout << "6: " << (newResult->bank->writeLatency * newResult->bank->writeDynamicEnergy <= limitWriteEdp) << std::endl;
        std::cout << "7: " << (newResult->bank->area <= limitArea) << std::endl;
        std::cout << "8: " << (newResult->bank->leakage <= limitLeakage) << std::endl;
        */

        // Test to ensure the bank initialized right
	if (newResult->bank->readLatency <= limitReadLatency 
                && newResult->bank->writeLatency <= limitWriteLatency
		&& newResult->bank->readDynamicEnergy <= limitReadDynamicEnergy 
                && newResult->bank->writeDynamicEnergy <= limitWriteDynamicEnergy
		&& newResult->bank->readLatency * newResult->bank->readDynamicEnergy <= limitReadEdp
		&& newResult->bank->writeLatency * newResult->bank->writeDynamicEnergy <= limitWriteEdp
		&& newResult->bank->area <= limitArea 
                && newResult->bank->leakage <= limitLeakage) {

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
		case search_latency_optimized:
			if 	(newResult->bank->searchLatency < bank->searchLatency)
				toUpdate = true;
			break;
		case search_energy_optimized:
			if 	(newResult->bank->searchDynamicEnergy < bank->searchDynamicEnergy)
				toUpdate = true;
			break;
		case search_edp_optimized:
			if 	(newResult->bank->searchDynamicEnergy * newResult->bank->searchLatency < bank->searchDynamicEnergy * bank->searchLatency)
				toUpdate = true;
			break;
		default:	/* Exploration */
			/* should not happen */
			;
		}
		if (toUpdate) {
			bank = newResult->bank;
			localWire = newResult->localWire;
			globalWire = newResult->globalWire;
		}
	} else {
		std::cout << "mem_data initialization error."  << std::endl;
	}
}

void CAM_Result::print() {
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
        
        std::cout << std::endl;

	std::cout << "==============================================" << std::endl << "               SUMMARY RESULT" << std::endl << "==============================================" << std::endl;

	std::cout << "Area:" << std::endl;

	std::cout << " - Total Area = " << TO_METER(bank->height) << " x " << TO_METER(bank->width)
			<< " = " << TO_SQM(bank->area) << std::endl;
	std::cout << " |--- Mat Area      = " << TO_METER(bank->mat->height) << " x " << TO_METER(bank->mat->width)
			<< " = " << TO_SQM(bank->mat->area) << "   (" << inputParameter->cell->area * inputParameter->tech->featureSize * inputParameter->tech->featureSize
			* bank->capacity / bank->numRowMat / bank->numColumnMat / bank->mat->area * 100 << "%)" << std::endl;
		std::cout << " |--- Subarray Area = " << TO_METER(bank->mat->subarray->height) << " x "
				<< TO_METER(bank->mat->subarray->width) << " = " << TO_SQM(bank->mat->subarray->area) << "   ("
				<< inputParameter->cell->area * inputParameter->tech->featureSize * inputParameter->tech->featureSize * bank->capacity / bank->numRowMat
				/ bank->numColumnMat / bank->numRowSubarray / bank->numColumnSubarray
				/ bank->mat->subarray->area * 100 << "%)" <<std::endl;
		std::cout << " |--- Subarray Dimensions = " << bank->mat->subarray->numRow
				<< " Rows x " << bank->mat->subarray->numColumn << " Columns" << std::endl;
		std::cout << " - Area Efficiency = " << inputParameter->cell->area * inputParameter->tech->featureSize * inputParameter->tech->featureSize
				* bank->capacity / bank->area * 100 << "%" << std::endl;
	std::cout << "Timing:" << std::endl;

	std::cout << " -  Search Latency = " << TO_SECOND(bank->readLatency) << std::endl;
	if (inputParameter->routingMode == h_tree)
		std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->readLatency - bank->mat->readLatency) << std::endl;
	else
		std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->readLatency - bank->mat->readLatency) << std::endl;
	std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->readLatency) << std::endl;
	std::cout << "    |--- Predecoder Latency = " << TO_SECOND(bank->mat->predecoderLatency) << std::endl;
	// std::cout << "       |--- Row Decoder Latency = " << TO_SECOND(bank->mat->subarray->rowDecoder->readLatency) << std::endl;
	// std::cout << "       |--- Matchline Latency     = " << TO_SECOND(bank->mat->subarray->MatchlineDelay) << std::endl;
	// if (inputParameter->internalSensing)
	// 	std::cout << "       |--- Senseamp Latency    = " << TO_SECOND(bank->mat->subarray->senseAmp.readLatency) << std::endl;
	// std::cout << "       |--- Mux Latency         = " << TO_SECOND(bank->mat->subarray->bitlineMux->readLatency
	// 												+ bank->mat->subarray->senseAmpMuxLev1.readLatency
	// 												+ bank->mat->subarray->senseAmpMuxLev2.readLatency) << std::endl;
	// std::cout << "       |--- Precharge Latency   = " << TO_SECOND(bank->mat->subarray->precharger.readLatency) << std::endl;

	if (inputParameter->cell->memCellType == PCRAM || inputParameter->cell->memCellType == FBRAM || inputParameter->cell->memCellType == FEFETRAM ||
			(inputParameter->cell->memCellType == memristor && (inputParameter->cell->accessType == CMOS_access || inputParameter->cell->accessType == BJT_access))) {
		std::cout << " - RESET Latency = " << TO_SECOND(bank->resetLatency) << std::endl;
		if (inputParameter->routingMode == h_tree)
			std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->resetLatency - bank->mat->resetLatency) << std::endl;
		else
			std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->resetLatency - bank->mat->resetLatency) << std::endl;
		std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->resetLatency) << std::endl;
		std::cout << "    |--- Predecoder Latency = " << TO_SECOND(bank->mat->predecoderLatency) << std::endl;
		std::cout << "    |--- Subarray Latency   = " << TO_SECOND(bank->mat->subarray->resetLatency) << std::endl;
		// std::cout << "       |--- RESET Pulse Duration = " << TO_SECOND(inputParameter->cell->resetPulse) << std::endl;
		// std::cout << "       |--- Row Decoder Latency  = " << TO_SECOND(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
		// std::cout << "       |--- Charge Latency   = " << TO_SECOND(bank->mat->subarray->chargeLatency) << std::endl;
		// std::cout << " - SET Latency   = " << TO_SECOND(bank->setLatency) << std::endl;
		if (inputParameter->routingMode == h_tree)
			std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->setLatency - bank->mat->setLatency) << std::endl;
		else
			std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->setLatency - bank->mat->setLatency) << std::endl;
		std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->setLatency) << std::endl;
		std::cout << "    |--- Predecoder Latency = " << TO_SECOND(bank->mat->predecoderLatency) << std::endl;
		std::cout << "    |--- Subarray Latency   = " << TO_SECOND(bank->mat->subarray->setLatency) << std::endl;
		// std::cout << "       |--- SET Pulse Duration   = " << TO_SECOND(inputParameter->cell->setPulse) << std::endl;
		// std::cout << "       |--- Row Decoder Latency  = " << TO_SECOND(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
		// std::cout << "       |--- Charger Latency      = " << TO_SECOND(bank->mat->subarray->chargeLatency) << std::endl;
	} else if (inputParameter->cell->memCellType == SLCNAND) {
		std::cout << " - Erase Latency = " << TO_SECOND(bank->resetLatency) << std::endl;
		if (inputParameter->routingMode == h_tree)
			std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->resetLatency - bank->mat->resetLatency) << std::endl;
		else
			std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->resetLatency - bank->mat->resetLatency) << std::endl;
		std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->resetLatency) << std::endl;
		std::cout << " - Programming Latency   = " << TO_SECOND(bank->setLatency) << std::endl;
		if (inputParameter->routingMode == h_tree)
			std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->setLatency - bank->mat->setLatency) << std::endl;
		else
			std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->setLatency - bank->mat->setLatency) << std::endl;
		std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->setLatency) << std::endl;
	} else {
		std::cout << " - Write Latency = " << TO_SECOND(bank->writeLatency) << std::endl;
		if (inputParameter->routingMode == h_tree)
			std::cout << " |--- H-Tree Latency = " << TO_SECOND(bank->writeLatency - bank->mat->writeLatency) << std::endl;
		else
			std::cout << " |--- Non-H-Tree Latency = " << TO_SECOND(bank->writeLatency - bank->mat->writeLatency) << std::endl;
		std::cout << " |--- Mat Latency    = " << TO_SECOND(bank->mat->writeLatency) << std::endl;
		std::cout << "    |--- Predecoder Latency = " << TO_SECOND(bank->mat->predecoderLatency) << std::endl;
		std::cout << "    |--- Subarray Latency   = " << TO_SECOND(bank->mat->subarray->writeLatency) << std::endl;
		// if (inputParameter->cell->memCellType == MRAM)
		// 	std::cout << "       |--- Write Pulse Duration = " << TO_SECOND(inputParameter->cell->resetPulse) << std::endl;	// MRAM reset/set is equal
		// std::cout << "       |--- Row Decoder Latency = " << TO_SECOND(bank->mat->subarray->rowDecoder->writeLatency) << std::endl;
		// std::cout << "       |--- Charge Latency      = " << TO_SECOND(bank->mat->subarray->chargeLatency) << std::endl;
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
	if (inputParameter->routingMode == h_tree)
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
	// std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->readDynamicEnergy) << std::endl;
	// std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->readDynamicEnergy
	// 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->readDynamicEnergy
	// 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->readDynamicEnergy) << std::endl;
	// if (inputParameter->cell->memCellType == PCRAM || inputParameter->cell->memCellType == FBRAM || inputParameter->cell->memCellType == MRAM || inputParameter->cell->memCellType == memristor || inputParameter->cell->memCellType == FEFETRAM	 ) {
	// 	std::cout << "       |--- Bitline & Cell Read Energy = " << TO_JOULE(bank->mat->subarray->cellReadEnergy) << std::endl;
	// }
	// if (inputParameter->internalSensing)
	// 	std::cout << "       |--- Senseamp Dynamic Energy    = " << TO_JOULE(bank->mat->subarray->senseAmp.readDynamicEnergy) << std::endl;
	// std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->readDynamicEnergy
	// 												+ bank->mat->subarray->senseAmpMuxLev1.readDynamicEnergy
	// 												+ bank->mat->subarray->senseAmpMuxLev2.readDynamicEnergy) << std::endl;
	// std::cout << "       |--- Precharge Dynamic Energy   = " << TO_JOULE(bank->mat->subarray->precharger.readDynamicEnergy) << std::endl;

	if (inputParameter->cell->memCellType == PCRAM || inputParameter->cell->memCellType == FBRAM ||
			(inputParameter->cell->memCellType == memristor && (inputParameter->cell->accessType == CMOS_access || inputParameter->cell->accessType == BJT_access || inputParameter->cell->memCellType == FEFETRAM))) {
		std::cout << " - RESET Dynamic Energy = " << TO_JOULE(bank->resetDynamicEnergy) << std::endl;
		if (inputParameter->routingMode == h_tree)
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
		// std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Cell RESET Dynamic Energy  = " << TO_JOULE(bank->mat->subarray->cellResetEnergy) << std::endl;
		std::cout << " - SET Dynamic Energy = " << TO_JOULE(bank->setDynamicEnergy) << std::endl;
		if (inputParameter->routingMode == h_tree)
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
		// std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Cell SET Dynamic Energy    = " << TO_JOULE(bank->mat->subarray->cellSetEnergy) << std::endl;
	} else if (inputParameter->cell->memCellType == SLCNAND) {
		std::cout << " - Erase Dynamic Energy = " << TO_JOULE(bank->resetDynamicEnergy) << " per block" << std::endl;
		if (inputParameter->routingMode == h_tree)
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
		// std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
		// std::cout << " - Programming Dynamic Energy = " << TO_JOULE(bank->setDynamicEnergy) << " per page" << std::endl;
		if (inputParameter->routingMode == h_tree)
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
		// std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
	} else {
		std::cout << " - Write Dynamic Energy = " << TO_JOULE(bank->writeDynamicEnergy) << std::endl;
		if (inputParameter->routingMode == h_tree)
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
		// std::cout << "       |--- Row Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->rowDecoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Decoder Dynamic Energy = " << TO_JOULE(bank->mat->subarray->bitlineMuxDecoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1Decoder->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2Decoder->writeDynamicEnergy) << std::endl;
		// std::cout << "       |--- Mux Dynamic Energy         = " << TO_JOULE(bank->mat->subarray->bitlineMux->writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev1.writeDynamicEnergy
		// 												+ bank->mat->subarray->senseAmpMuxLev2.writeDynamicEnergy) << std::endl;
		// if (inputParameter->cell->memCellType == MRAM) {
		// 	std::cout << "       |--- Bitline & Cell Write Energy= " << TO_JOULE(bank->mat->subarray->cellResetEnergy) << std::endl;
		// }
	}

	std::cout << " - Leakage Power = " << TO_WATT(bank->leakage) << std::endl;
	if (inputParameter->routingMode == h_tree)
		std::cout << " |--- H-Tree Leakage Power = " << TO_WATT(bank->leakage - bank->mat->leakage
													* bank->numColumnMat * bank->numRowMat)
													<< std::endl;
	else
		std::cout << " |--- Non-H-Tree Leakage Power = " << TO_WATT(bank->leakage - bank->mat->leakage
													* bank->numColumnMat * bank->numRowMat)
													<< std::endl;
	std::cout << " |--- Mat Leakage Power    = " << TO_WATT(bank->mat->leakage) << " per mat" << std::endl;
	std::cout << "    |--- Predecoder Leakage Power = " << TO_WATT(bank->mat->rowPredecoderBlock1->leakage + bank->mat->rowPredecoderBlock2->leakage +
				bank->mat->bitlineMuxPredecoderBlock1->leakage + bank->mat->bitlineMuxPredecoderBlock2->leakage +
				bank->mat->senseAmpMuxLev1PredecoderBlock1->leakage + bank->mat->senseAmpMuxLev1PredecoderBlock2->leakage +
				bank->mat->senseAmpMuxLev2PredecoderBlock1->leakage + bank->mat->senseAmpMuxLev2PredecoderBlock2->leakage ) << std::endl;
	std::cout << "    |--- Subarray Leakage Power   = " << TO_JOULE(bank->mat->subarray->leakage) << std::endl;

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
