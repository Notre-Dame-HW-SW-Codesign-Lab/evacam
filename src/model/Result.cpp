#include "Result.h"
#include "factories/BankFactory.h"
#include "factories/WireFactory.h"

void Result::Initialize(std::shared_ptr<EvaCamConfig> _config) {
    config = _config;

    bank = BankFactory::CreateBank(*config);
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
    bank->searchLatency = 1e41;
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
            case search_latency_optimized:
                if 	(newResult->bank->searchLatency < bank->searchLatency)
                    toUpdate = true;
                break;
            case search_energy_optimized:
                if 	(newResult->bank->searchDynamicEnergy < bank->searchDynamicEnergy)
                    toUpdate = true;
                break;
            case search_edp_optimized:
                if 	(newResult->bank->searchDynamicEnergy * newResult->bank->searchLatency
                        < bank->searchDynamicEnergy * bank->searchLatency)
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
    }
}

void Result::printToCsvFile(std::ostream &outputFile) {
    outputFile << bank->numRowMat << "," << bank->numColumnMat << "," << bank->numActiveMatPerColumn << "," << bank->numActiveMatPerRow << ",";
    outputFile << bank->numRowSubarray << "," << bank->numColumnSubarray << "," << bank->numActiveSubarrayPerColumn << "," << bank->numActiveSubarrayPerRow << ",";
    outputFile << bank->mat->subarray->numRow << "," << bank->mat->subarray->numColumn << ",";
    outputFile << bank->muxSenseAmp << "," << bank->muxOutputLev1 << "," << bank->muxOutputLev2 << ",";
    outputFile << "N/A,";

    switch (localWire.wireType) {
        case local_aggressive:
            outputFile << "Local Aggressive,";
            break;
        case local_conservative:
            outputFile << "Local Conservative,";
            break;
        case semi_aggressive:
            outputFile << "Semi-Global Aggressive,";
            break;
        case semi_conservative:
            outputFile << "Semi-Global Conservative,";
            break;
        case global_aggressive:
            outputFile << "Global Aggressive,";
            break;
        case global_conservative:
            outputFile << "Global Conservative,";
            break;
        default:
            outputFile << "DRAM Wire,";
    }

    switch (localWire.wireRepeaterType) {
        case repeated_none:
            outputFile << "No Repeaters,";
            break;
        case repeated_opt:
            outputFile << "Fully-Optimized Repeaters,";
            break;
        case repeated_5:
            outputFile << "Repeaters with 5% Overhead,";
            break;
        case repeated_10:
            outputFile << "Repeaters with 10% Overhead,";
            break;
        case repeated_20:
            outputFile << "Repeaters with 20% Overhead,";
            break;
        case repeated_30:
            outputFile << "Repeaters with 30% Overhead,";
            break;
        case repeated_40:
            outputFile << "Repeaters with 40% Overhead,";
            break;
        case repeated_50:
            outputFile << "Repeaters with 50% Overhead,";
            break;
        default:
            outputFile << "N/A,";
    }
    outputFile << (localWire.isLowSwing ? "Yes," : "No,");

    switch (globalWire.wireType) {
        case local_aggressive:
            outputFile << "Local Aggressive,";
            break;
        case local_conservative:
            outputFile << "Local Conservative,";
            break;
        case semi_aggressive:
            outputFile << "Semi-Global Aggressive,";
            break;
        case semi_conservative:
            outputFile << "Semi-Global Conservative,";
            break;
        case global_aggressive:
            outputFile << "Global Aggressive,";
            break;
        case global_conservative:
            outputFile << "Global Conservative,";
            break;
        default:
            outputFile << "DRAM Wire,";
    }

    switch (globalWire.wireRepeaterType) {
        case repeated_none:
            outputFile << "No Repeaters,";
            break;
        case repeated_opt:
            outputFile << "Fully-Optimized Repeaters,";
            break;
        case repeated_5:
            outputFile << "Repeaters with 5% Overhead,";
            break;
        case repeated_10:
            outputFile << "Repeaters with 10% Overhead,";
            break;
        case repeated_20:
            outputFile << "Repeaters with 20% Overhead,";
            break;
        case repeated_30:
            outputFile << "Repeaters with 30% Overhead,";
            break;
        case repeated_40:
            outputFile << "Repeaters with 40% Overhead,";
            break;
        case repeated_50:
            outputFile << "Repeaters with 50% Overhead,";
            break;
        default:
            outputFile << "N/A,";
    }
    outputFile << (globalWire.isLowSwing ? "Yes," : "No,");

    switch (bank->areaOptimizationLevel) {
        case latency_first:
            outputFile << "Latency-Optimized,";
            break;
        case area_first:
            outputFile << "Area-Optimized,";
            break;
        default:
            outputFile << "Balanced,";
    }

    outputFile << bank->height * 1e6 << "," << bank->width * 1e6 << "," << bank->area * 1e6 << ",";
    outputFile << bank->mat->height * 1e6 << "," << bank->mat->width * 1e6 << "," << bank->mat->area * 1e6 << ",";
    outputFile << bank->mat->subarray->height * 1e6 << "," << bank->mat->subarray->width * 1e6 << "," << bank->mat->subarray->area * 1e6 << ",";
    outputFile << config->technology.cell->area * config->technology.tech->featureSize()
            * config->technology.tech->featureSize() * bank->capacity / bank->area * 100 << ",";
    outputFile << bank->readLatency * 1e9 << "," << bank->writeLatency * 1e9 << ",";
    outputFile << bank->readDynamicEnergy * 1e12 << "," << bank->writeDynamicEnergy * 1e12 << ",";
    outputFile << bank->leakage * 1e3 << ",";
}
