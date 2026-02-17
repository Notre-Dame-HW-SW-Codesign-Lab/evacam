#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <math.h>
#include <memory>
#include <vector>
#include <omp.h>
#include <filesystem>
#include <yaml.h>
#include "../include/InputParameter.h"
#include "../include/MemCell.h"
#include "../include/RowDecoder.h"
#include "../include/Precharger.h"
#include "../include/OutputDriver.h"
#include "../include/SenseAmp.h"
#include "../include/Technology.h"
#include "../include/BasicDecoder.h"
#include "../include/PredecodeBlock.h"
#include "../include/SubArray.h"
#include "../include/Mat.h"
#include "../include/BankWithHtree.h"
#include "../include/BankWithoutHtree.h"
#include "../include/Wire.h"
#include "../include/Result.h"
#include "../include/formula.h"
#include "../include/macros.h"
#include "../include/CAM_Result.h"
#include "../include/global.h"
#include "../include/ResultsYaml.h"

// Define the verbose boolean
bool verbose = false;

void usage(int exit_code);
void applyConstraint(std::shared_ptr<InputParameter> inputParameter);

int main(int argc, char *argv[]) {
            
	std::setw(10);
	std::cout << std::fixed << std::setprecision(3);

        std::string_view arg;
        std::string inputFileName;
        std::string outputYamlFileName;
        int threads = omp_get_num_procs();
        bool deepExploration = false;
        
        // TODO: Is having a default input file necessary? For CAMASim integration yes it is
        if (argc == 1) {
                usage(1);
                //inputFileName = "Eva-CAM.cfg";
                //std::cout << "Default configuration file (nvsim.cfg) is loaded" << std::endl;
        } 

        // Read command line arguments
        for (int i = 1; i < argc; i++) {
                arg = std::string_view(argv[i]);
                if (arg == "-h" || arg == "--help") {
                        usage(0);

                } else if (arg == "-t" || arg == "--threads") {
                        try { 
                                threads = std::stoi(argv[++i]); 
                        } catch (const std::exception& e) {
                                std::cerr << "Number of threads must be a integer." << std::endl;
                                usage(1);
                        }

                } else if (arg == "-v" || arg == "--verbose") {
                        verbose = true;
                        PRINT_VERBOSE("Verbose output enabled");

                } else if (arg == "-d" || arg == "--deep-exploration") {
                        deepExploration = true;

                } else if (arg == "-o" || arg == "--output") {
                        if (i + 1 >= argc) {
                                std::cerr << "Missing output file after " << arg << std::endl;
                                usage(1);
                        }
                        outputYamlFileName = std::string(argv[++i]);

                } else if (i == argc - 1) {
                        inputFileName = std::string(argv[argc - 1]);

                } else {
                        usage(1);
                }
        }
 
        PRINT_VERBOSE("User-defined configuration file (" << inputFileName << ") is loaded");

        if (!std::filesystem::exists(inputFileName)) {
                std::cout << "Config file: " << inputFileName << " does not exist." << std::endl;
                usage(1);
        }
        if (outputYamlFileName.empty()) {
                std::filesystem::path inputPath(inputFileName);
                std::string base = inputPath.stem().string();
                const std::string suffix1 = "_config";
                const std::string suffix2 = "-config";
                if (base.size() > suffix1.size() &&
                    base.compare(base.size() - suffix1.size(), suffix1.size(), suffix1) == 0) {
                        base.resize(base.size() - suffix1.size());
                } else if (base.size() > suffix2.size() &&
                           base.compare(base.size() - suffix2.size(), suffix2.size(), suffix2) == 0) {
                        base.resize(base.size() - suffix2.size());
                }
                outputYamlFileName = (std::filesystem::path("results") / (base + "_results.yaml")).string();
        }

        omp_set_num_threads(threads);

        PRINT_VERBOSE(""); 

        // Wrapped entire main function in a try catch statement to allow easy use of exceptions
        // TODO: Should be reworked to not have literally everything aside from cli args in the same try block
    int exitCode = 0;
    try {
        /* Read input files */
	auto inputParameter = std::make_shared<InputParameter>();

        // TODO: double check if FEFET_tech is a completely redundant class due to tech
        // already being FEFET if the config is FEFET

        // Set selected search parameters
        if (deepExploration) inputParameter->ReduceSearchSize();
        else inputParameter->RestoreSearchSize();

        // Make all the classes read tech from inputParameter, as tech is an input parameter
	inputParameter->ReadInputParameterFromFile(inputFileName);
        
	inputParameter->cell->PrintCell();
        
        /*if (cell->memCellType == FEFETRAM) {
            FEFET_tech->Initialize(inputParameter->processNode, FEFET, inputParameter->UseUpdatedLib);
        }*/

	auto techHigh = std::make_shared<Technology>();
	double alpha = 0;

        /* TODO Make this work instead of the if/else if tree below it
        
        switch (inputParameter->processNode) {
                case 200: alpha = (inputParameter->processNode - 120.0) / 60; break;
                case 120: alpha = (inputParameter->processNode - 90.0)  / 30; break;
                case 90 : alpha = (inputParameter->processNode - 65.0)  / 25; break;
                case 65 : alpha = (inputParameter->processNode - 45.0)  / 20; break;
                case 45 : alpha = (inputParameter->processNode - 32.0)  / 13; break;
                case 32 : alpha = (inputParameter->processNode - 22.0)  / 10; break;
                case 22 : alpha = (inputParameter->processNode - 14.0)  / 8 ; break;
                case 14 : alpha = (inputParameter->processNode - 10.0)  / 4 ; break;
                case 10 : alpha = (inputParameter->processNode - 10.0)  / 3 ; break;
                case 7  : alpha = 1; break;
                default : throw std::runtime_error("Error: Unsupported Process Node."); 
        };

        techHigh->Initialize(inputParameter->processNode, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
        */

	if (inputParameter->processNode > 200) {
		// TODO: technology node > 200 nm
                throw std::runtime_error("[Error] Technology node above 200nm not supported.");
	} else if (inputParameter->processNode > 120) { // 120 nm < technology node <= 200 nm
		techHigh->Initialize(200, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 120.0) / 60;
	} else if (inputParameter->processNode > 90) { // 90 nm < technology node <= 120 nm
		techHigh->Initialize(120, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 90.0) / 30;
	} else if (inputParameter->processNode > 65) { // 65 nm < technology node <= 90 nm
		techHigh->Initialize(90, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 65.0) / 25;
	} else if (inputParameter->processNode > 45) { // 45 nm < technology node <= 65 nm
		techHigh->Initialize(65, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 45.0) / 20;
	} else if (inputParameter->processNode > 32) { // 32 nm < technology node <= 45 nm
		techHigh->Initialize(45, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 32.0) / 13;
	} else if (inputParameter->processNode > 22) { // 22 nm < technology node <= 32 nm
		techHigh->Initialize(32, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 22.0) / 10;
	} else if (inputParameter->processNode > 14) { // 14 nm < technology node <= 22 nm
		techHigh->Initialize(22, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 14.0) / 8;
	} else if (inputParameter->processNode > 10) { // 10 nm < technology node <= 14 nm
		techHigh->Initialize(14, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 10.0) / 4;
	} else if (inputParameter->processNode > 7) { // 7 nm < technology node <= 10 nm
		techHigh->Initialize(10, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = (inputParameter->processNode - 10.0) / 3;
	} else if (inputParameter->processNode == 7) { // 7 nm < technology node <= 10 nm
		techHigh->Initialize(7, inputParameter->deviceRoadmap, inputParameter->UseUpdatedLib);
		alpha = 1;
	} else {
		throw std::runtime_error("[Error]: Technology node below 7nm is not supported.");
	}

	inputParameter->tech->InterpolateWith(techHigh, alpha);
        std::ofstream outputFile;
	std::string outputFileName;

	if (inputParameter->optimizationTarget == full_exploration) {
		std::stringstream temp;
		temp << inputParameter->outputFilePrefix << "_" << inputParameter->capacity / 1024 
                    << "K_" << inputParameter->wordWidth << "_" << inputParameter->associativity;

		if (inputParameter->internalSensing) temp << "_IN";
		else                                 temp << "_EX";

		if (inputParameter->cell->readMode) temp << "_VOL";
		else                                temp << "_CUR";

		temp << ".csv";
			outputFileName = temp.str();
                        std::filesystem::path csvPath(outputFileName);
                        if (!csvPath.parent_path().empty()) {
                                std::filesystem::create_directories(csvPath.parent_path());
                        }
			outputFile.open(outputFileName.c_str(), std::ofstream::app);
		}

	applyConstraint(inputParameter);
	//int numRowMat, numColumnMat, numActiveMatPerRow, numActiveMatPerColumn;
	//int numRowSubarray, numColumnSubarray, numActiveSubarrayPerRow, numActiveSubarrayPerColumn;
	//int muxSenseAmp, muxOutputLev1, muxOutputLev2, numRowPerSet;
	//int areaOptimizationLevel;				/* actually BufferDesignTarget */
	int localWireType, globalWireType;			/* actually WireType */
	int localWireepeaterType, globalWireepeaterType;	/* actually WireRepeaterType */
	int isLocalWireLowSwing, isGlobalWireLowSwing;		/* actually boolean value */

	long long capacity;
	long blockSize;
	int associativity;

	/* for cache data array, memory array */
        
        // Vectorize
        // TODO: Check if this should be atomic
        std::vector<std::shared_ptr<CAM_Result>> bestDataResults;
        bestDataResults.resize((int)full_exploration);

        for (int i = 0; i < (int)full_exploration; i++){
                bestDataResults[i] = std::make_shared<CAM_Result>();
                bestDataResults[i]->Initialize(inputParameter);
		bestDataResults[i]->optimizationTarget = (OptimizationTarget)i;
		// std::cout << __FILE__ << ": " << __LINE__ << ": " << "capacity: " << bestDataResults[i]->bank->capacity << std::endl;
	}

	auto localWire = std::make_shared<Wire>();
	auto globalWire = std::make_shared<Wire>();

        std::atomic<long long> numSolution = 0;

	inputParameter->PrintInputParameter();

	/* adjust cache data array parameters according to the access mode */
	capacity = (long long)inputParameter->capacity * 8;
	blockSize = inputParameter->capacity * 8 / inputParameter->wordWidth;
	associativity = 1;
	// std::cout << inputParameter->capacity << "::::" << inputParameter->wordWidth << std::endl;
	// std::cout << capacity << "::" << blockSize << "::" << associativity << std::endl;
	//INITIAL_BASIC_WIRE;

        WireType basicWireType;
	WireRepeaterType basicWireRepeaterType;
	bool isBasicLowSwing;

	if (inputParameter->minLocalWireType == inputParameter->maxLocalWireType)
		basicWireType = (WireType)inputParameter->minLocalWireType;
	else
		basicWireType = local_aggressive;

	if (inputParameter->minLocalWireRepeaterType == inputParameter->maxLocalWireRepeaterType)
		basicWireRepeaterType = (WireRepeaterType)inputParameter->minLocalWireRepeaterType;
	else
		basicWireRepeaterType = repeated_none;

	if (inputParameter->minIsLocalWireLowSwing == inputParameter->maxIsLocalWireLowSwing)
		isBasicLowSwing = inputParameter->minIsLocalWireLowSwing;
	else
		isBasicLowSwing = false;

	localWire->Initialize(inputParameter->processNode, basicWireType, basicWireRepeaterType, 
                inputParameter->temperature, isBasicLowSwing, inputParameter);

	if (inputParameter->minGlobalWireType == inputParameter->maxGlobalWireType)
		basicWireType = (WireType)inputParameter->minGlobalWireType;
	else
		basicWireType = global_aggressive;

	if (inputParameter->minGlobalWireRepeaterType == inputParameter->maxGlobalWireRepeaterType)
		basicWireRepeaterType = (WireRepeaterType)inputParameter->minGlobalWireRepeaterType;
	else
		basicWireRepeaterType = repeated_none;

	if (inputParameter->minIsGlobalWireLowSwing == inputParameter->maxIsGlobalWireLowSwing)
		isBasicLowSwing = inputParameter->minIsGlobalWireLowSwing;
	else
		isBasicLowSwing = false;

	globalWire->Initialize(inputParameter->processNode, basicWireType, basicWireRepeaterType, 
                inputParameter->temperature, isBasicLowSwing, inputParameter);

        auto CAM_opt = std::make_shared<CAM_Opt>();

	//CAM_BIGFOR
        // Delcare new loop variables to enable collapsing the loop
        int maxNumRowMat_par = intLog2(inputParameter->maxNumRowMat);
        int maxNumColumnMat_par = intLog2(inputParameter->maxNumColumnMat);
        int maxNumRowSubarray_par = intLog2(inputParameter->maxNumRowSubarray);

        if (maxNumRowMat_par == 0) maxNumRowMat_par = 1;
        if (maxNumColumnMat_par == 0) maxNumColumnMat_par = 1;
        if (maxNumRowSubarray_par == 0) maxNumRowSubarray_par = 1;


        long long total = maxNumRowMat_par * maxNumColumnMat_par * maxNumRowSubarray_par;
        
        std::atomic<long long> loops_complete = 0;

        #pragma omp parallel firstprivate(CAM_opt) 
{
        #pragma omp for collapse(3)
        for (int bigLoopItr0 = inputParameter->minNumRowMat; 
                bigLoopItr0 <= maxNumRowMat_par; 
                bigLoopItr0++)
	for (int bigLoopItr1 = inputParameter->minNumColumnMat; 
                bigLoopItr1 <= maxNumColumnMat_par; 
                bigLoopItr1++) 
        for (int bigLoopItr2 = inputParameter->minNumRowSubarray; 
                bigLoopItr2 <= maxNumRowSubarray_par; 
                bigLoopItr2++) {

                int numRowMat;
                int numColumnMat;   
                int numRowSubarray;

                numRowMat      = inputParameter->minNumRowMat << bigLoopItr0;
                numColumnMat   = inputParameter->minNumColumnMat << bigLoopItr1;
                numRowSubarray = inputParameter->minNumRowSubarray << bigLoopItr2;

                if (bigLoopItr0 == 1) numRowMat = 1;
                if (bigLoopItr1 == 1) numColumnMat = 1;
                if (bigLoopItr2 == 1) numRowSubarray = 1;

                loops_complete += 1;

                long long percentage = loops_complete*100/total;
                
                if (total > 1) {
                        #pragma omp critical
                        std::cout << "\rProgress: " << percentage << "%" << std::flush;
                }

	for (int numActiveMatPerRow = MIN(numColumnMat, inputParameter->minNumActiveMatPerRow); 
                numActiveMatPerRow <= MIN(numColumnMat, inputParameter->maxNumActiveMatPerRow); 
                numActiveMatPerRow *= 2) 
	for (int numActiveMatPerColumn = MIN(numRowMat, inputParameter->minNumActiveMatPerColumn); 
                numActiveMatPerColumn <= MIN(numRowMat, inputParameter->maxNumActiveMatPerColumn); 
                numActiveMatPerColumn *= 2) 
	for (int numColumnSubarray = inputParameter->minNumColumnSubarray; 
                numColumnSubarray <= inputParameter->maxNumColumnSubarray; 
                numColumnSubarray *= 2) 
	for (int numActiveSubarrayPerRow = MIN(numColumnSubarray, inputParameter->minNumActiveSubarrayPerRow); 
                numActiveSubarrayPerRow <= MIN(numColumnSubarray, inputParameter->maxNumActiveSubarrayPerRow); 
                numActiveSubarrayPerRow *=2) 
	for (int numActiveSubarrayPerColumn = MIN(numRowSubarray, inputParameter->minNumActiveSubarrayPerColumn); 
                numActiveSubarrayPerColumn <= MIN(numRowSubarray, inputParameter->maxNumActiveSubarrayPerColumn); 
                numActiveSubarrayPerColumn *= 2) 
	for (int muxSenseAmp = inputParameter->minMuxSenseAmp; 
                muxSenseAmp <= inputParameter->maxMuxSenseAmp; 
                muxSenseAmp *= 2) 
	for (int muxOutputLev1 = inputParameter->minMuxOutputLev1; 
                muxOutputLev1 <= inputParameter->maxMuxOutputLev1; 
                muxOutputLev1 *= 2) 
	for (int muxOutputLev2 = inputParameter->minMuxOutputLev2; 
                muxOutputLev2 <= inputParameter->maxMuxOutputLev2; 
                muxOutputLev2 *= 2) 
	for (int numRowPerSet = inputParameter->minNumRowPerSet; 
                numRowPerSet <= MIN(inputParameter->maxNumRowPerSet, inputParameter->associativity); 
                numRowPerSet *= 2) 
	for (int areaOptimizationLevel = inputParameter->minAreaOptimizationLevel; 
                areaOptimizationLevel <= inputParameter->maxAreaOptimizationLevel; 
                areaOptimizationLevel++) 
	for (CAM_opt->RowDriver = inputParameter->minRowDriverOptLevel; 
                CAM_opt->RowDriver <= inputParameter->maxRowDriverOptLevel; 
                CAM_opt->RowDriver++) 
	for (CAM_opt->Proirity = inputParameter->minPriorityOptLevel; 
                CAM_opt->Proirity <= inputParameter->maxPriorityOptLevel; 
                CAM_opt->Proirity++) 
	for (CAM_opt->BitSerialWidth = inputParameter->minBitSerialWidth; 
                CAM_opt->BitSerialWidth <= inputParameter->maxBitSerialWidth; 
                CAM_opt->BitSerialWidth *= 2) {

		if (blockSize / (numActiveMatPerRow * numActiveMatPerColumn 
                            * numActiveSubarrayPerRow * numActiveSubarrayPerColumn) == 0) {
			/* To aggressive partitioning */
			continue;
		}
                
                std::shared_ptr<Bank> dataBank;

	        if (inputParameter->routingMode == h_tree) {
		    dataBank = std::make_shared<BankWithHtree>(); 
                } else { 
		    dataBank = std::make_shared<BankWithoutHtree>();
                }
	        dataBank->Initialize(numRowMat, numColumnMat, capacity, blockSize, associativity,
				numRowPerSet, numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
				inputParameter->internalSensing, muxOutputLev1, muxOutputLev2, numRowSubarray, 
                                numColumnSubarray, numActiveSubarrayPerRow, numActiveSubarrayPerColumn, 
                                (BufferDesignTarget)areaOptimizationLevel, mem_data, inputParameter->cell->camType, 
                                inputParameter->searchFunction, inputParameter, localWire, globalWire, CAM_opt); 
	        //dataBank->CalculateArea(); 
	        //dataBank->CalculateRC(); 
	        //dataBank->CalculateLatencyAndPower();
                //std::cout << dataBank->readDynamicEnergy << std::endl;

	// for (int i = 0; i < (int)full_exploration; i++)
	// std::cout << __FILE__ << ": " << __LINE__ << ": " << "capacity" << bestDataResults[i]->bank->capacity << std::endl;
    // std::cout << __FILE__ << ": " << __LINE__ << ": " << capacity << "::" << blockSize << "::" << associativity << std::endl;

                if (!dataBank->invalid && !dataBank->mat->subarray->invalid) {
			std::shared_ptr<Result> tempResult;
                        tempResult = std::make_shared<Result>();
                        tempResult->Initialize(inputParameter);
			//VERIFY_DATA_CAPACITY;
                        if ((long long)dataBank->mat->subarray->numColumn * dataBank->mat->subarray->numRow
                                        * dataBank->numColumnMat * dataBank->numRowMat * dataBank->numColumnSubarray 
                                        * dataBank->numRowSubarray  != capacity) {
                                std::cout << 
                                "numcolumn x numrow x numcolumnmat x numrowmat x numcolumnsubarry x numrowsubarray" 
                                    << dataBank->mat->subarray->numColumn  << ": " << dataBank->mat->subarray->numRow   
                                    << ": " << dataBank->numColumnMat << ": " << dataBank->numRowMat << ": " 
                                    <<  dataBank->numColumnSubarray << ": " <<  dataBank->numRowSubarray << std::endl;
                                std::cout << "1 Bank = " << dataBank->numRowMat << "x" 
                                    << dataBank->numColumnMat << " Mats" << std::endl;
                                std::cout << "Activation - " << dataBank->numActiveMatPerColumn 
                                    << "x" << dataBank->numActiveMatPerRow << " Mats" << std::endl;
                                std::cout << "1 Mat  = " << dataBank->numRowSubarray 
                                    << "x" << dataBank->numColumnSubarray << " Subarrays" << std::endl;
                                std::cout << "Activation - " << dataBank->numActiveSubarrayPerColumn 
                                    << "x" << dataBank->numActiveSubarrayPerRow << " Subarrays" << std::endl;
                                std::cout << "Mux Degree - " << dataBank->muxSenseAmp << " x " 
                                    << dataBank->muxOutputLev1 << " x " << dataBank->muxOutputLev2 << std::endl;
                                throw std::runtime_error("ERROR: DATA capacity violation. Shouldn't happen"); 
                    }

			numSolution++;
			// for (int i = 0; i < (int)full_exploration; i++)
				// std::cout << __FILE__ << ": " << __LINE__ << ": " << "capacity" << bestDataResults[i]->bank->capacity << std::endl;

			//UPDATE_BEST_DATA;
                        tempResult->bank = dataBank;
	                tempResult->localWire = localWire;
	                tempResult->globalWire = globalWire;
	                for (int i = 0; i < (int)full_exploration; i++){
		                bestDataResults[i]->compareAndUpdate(tempResult);
	                }
			// for (int i = 0; i < (int)full_exploration; i++)
				// std::cout << __FILE__ << ": " << __LINE__ << ": " << "capacity" << bestDataResults[i]->bank->capacity << std::endl;

			if (inputParameter->optimizationTarget == full_exploration && !inputParameter->isPruningEnabled) {
				//OUTPUT_TO_FILE;
		                tempResult->printToCsvFile(outputFile); 
		                outputFile << std::endl; 
			// for (int i = 0; i < (int)full_exploration; i++)
				// std::cout << __FILE__ << ": " << __LINE__ << ": " << "capacity" << bestDataResults[i]->bank->capacity << std::endl;

			}
		}
	}

        }
}
	// for (int i = 0; i < (int)full_exploration; i++)
		// std::cout << __FILE__ << ": " << __LINE__ << ": " << "capacity: " << bestDataResults[i]->bank->capacity << std::endl;

	// std::cout << __FILE__ << ": " << __LINE__ << ": " << capacity << "::" << blockSize << "::" << associativity << std::endl;
	if (numSolution > 0) {
                std::cout << std::endl;
		std::cout << "*** There are " << numSolution << " Solutions. ***" << std::endl;
		std::shared_ptr<Bank> trialBank;
		std::shared_ptr<Result> tempResult;
                tempResult = std::make_shared<Result>();
                tempResult->Initialize(inputParameter);
		/* refine local wire type */
		// std::cout << __FILE__ << ": " << __LINE__ << ": " << capacity << "::" << blockSize << "::" << associativity << std::endl;
		//REFINE_LOCAL_WIRE_FORLOOP 
                for (localWireType = inputParameter->minLocalWireType; 
                        localWireType <= inputParameter->maxLocalWireType; 
                        localWireType++) 
	            for (localWireepeaterType = inputParameter->minLocalWireRepeaterType; 
                            localWireepeaterType <= inputParameter->maxLocalWireRepeaterType; 
                            localWireepeaterType++) 
	                for (isLocalWireLowSwing = inputParameter->minIsLocalWireLowSwing; 
                                isLocalWireLowSwing <= inputParameter->maxIsLocalWireLowSwing; 
                                isLocalWireLowSwing++) 
	                    if ((WireRepeaterType)localWireepeaterType == repeated_none 
                                    || (bool)isLocalWireLowSwing == false) {

			localWire->Initialize(inputParameter->processNode, 
                                (WireType)localWireType,
				(WireRepeaterType)localWireepeaterType, 
                                inputParameter->temperature,
				(bool)isLocalWireLowSwing, 
                                inputParameter);
			for (int i = 0; i < (int)full_exploration; i++) {
				// std::cout << __FILE__ << ": " << __LINE__ << "i: " << i << std::endl;
                                
                                // TODO: Verify this, if the bestDataResult has no initialized bank,
                                //       there is no reason to check if it is the best result
                                if (!bestDataResults[i]->bank->initialized) continue;

                                globalWire->Initialize(inputParameter->processNode, 
                                        bestDataResults[i]->globalWire->wireType, 
                                        bestDataResults[i]->globalWire->wireRepeaterType,
                                        inputParameter->temperature, 
                                        bestDataResults[i]->globalWire->isLowSwing,
                                        inputParameter);

				//LOAD_GLOBAL_WIRE(bestDataResults[i]);
				// std::cout << __FILE__ << ": " << __LINE__ <<": " << capacity << "::" << blockSize << "::" << associativity << std::endl;
				CAM_opt->BitSerialWidth = bestDataResults[i]->bank->numBitSerial;
				CAM_opt->Proirity = bestDataResults[i]->bank->mat->subarray->PriorityOptLevel;
				CAM_opt->RowDriver = bestDataResults[i]->bank->mat->subarray->DriverOptLevel;

// std::cout << __FILE__ << ": " << __LINE__ << ": " << capacity << "::" << blockSize << "::" << associativity << std::endl;

				//TRY_AND_UPDATE(bestDataResults[i], mem_data);
	                        if (inputParameter->routingMode == h_tree)
		                        trialBank = std::make_shared<BankWithHtree>();
	                        else
		                        trialBank = std::make_shared<BankWithoutHtree>();

	                        trialBank->Initialize(bestDataResults[i]->bank->numRowMat, 
                                        bestDataResults[i]->bank->numColumnMat, 
                                        bestDataResults[i]->bank->capacity, 
                                        bestDataResults[i]->bank->blockSize, 
                                        bestDataResults[i]->bank->associativity,
                                        bestDataResults[i]->bank->numRowPerSet, 
                                        bestDataResults[i]->bank->numActiveMatPerRow, 
                                        bestDataResults[i]->bank->numActiveMatPerColumn, 
                                        bestDataResults[i]->bank->muxSenseAmp,
                                        inputParameter->internalSensing, 
                                        bestDataResults[i]->bank->muxOutputLev1, 
                                        bestDataResults[i]->bank->muxOutputLev2, 
                                        bestDataResults[i]->bank->numRowSubarray, 
                                        bestDataResults[i]->bank->numColumnSubarray,
                                        bestDataResults[i]->bank->numActiveSubarrayPerRow,
                                        bestDataResults[i]->bank->numActiveSubarrayPerColumn, 
                                        bestDataResults[i]->bank->areaOptimizationLevel, 
                                        mem_data, 
                                        inputParameter->cell->camType, 
                                        inputParameter->searchFunction, 
                                        inputParameter,
                                        localWire,
                                        globalWire,
                                        CAM_opt);
                        	bestDataResults[i]->bank = trialBank;
                        	//trialBank->CalculateArea(); 
                        	//trialBank->CalculateRC(); 
                        	//trialBank->CalculateLatencyAndPower(); 
                        	tempResult->bank = trialBank;
                        	tempResult->localWire = localWire;
                        	tempResult->globalWire = globalWire;
                        	bestDataResults[i]->compareAndUpdate(tempResult); 

    // std::cout << __FILE__ << ": " << __LINE__ << ": " << capacity << "::" << blockSize << "::" << associativity << std::endl;
			}
			// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
			if (inputParameter->optimizationTarget == full_exploration 
                                && !inputParameter->isPruningEnabled) {
				//OUTPUT_TO_FILE;
		                tempResult->printToCsvFile(outputFile); 
	                	outputFile << std::endl; 
			}
		}
		// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
		/* refine global wire type */
		//REFINE_GLOBAL_WIRE_FORLOOP 
                for (globalWireType = inputParameter->minGlobalWireType; 
                        globalWireType <= inputParameter->maxGlobalWireType; 
                        globalWireType++) 
	            for (globalWireepeaterType = inputParameter->minGlobalWireRepeaterType; 
                            globalWireepeaterType <= inputParameter->maxGlobalWireRepeaterType; 
                            globalWireepeaterType++) 
	                for (isGlobalWireLowSwing = inputParameter->minIsGlobalWireLowSwing; 
                                isGlobalWireLowSwing <= inputParameter->maxIsGlobalWireLowSwing; 
                                isGlobalWireLowSwing++) 
	                    if ((WireRepeaterType)globalWireepeaterType == repeated_none 
                                    || (bool)isGlobalWireLowSwing == false) {

			    globalWire->Initialize(inputParameter->processNode, (WireType)globalWireType,
					(WireRepeaterType)globalWireepeaterType, inputParameter->temperature,
					(bool)isGlobalWireLowSwing, inputParameter);
			for (int i = 0; i < (int)full_exploration; i++) {
                                
                            // TODO: Verify this, if the bestDataResult has no initialized bank,
                                //       there is no reason to check if it is the best result
                                if (!bestDataResults[i]->bank->initialized) continue;

				//LOAD_LOCAL_WIRE(bestDataResults[i]);
                                localWire->Initialize(inputParameter->processNode, 
                                        bestDataResults[i]->localWire->wireType, 
                                        bestDataResults[i]->localWire->wireRepeaterType,
                                        inputParameter->temperature, 
                                        bestDataResults[i]->localWire->isLowSwing, inputParameter);

				CAM_opt->BitSerialWidth = bestDataResults[i]->bank->numBitSerial;
				CAM_opt->Proirity = bestDataResults[i]->bank->mat->subarray->PriorityOptLevel;
				CAM_opt->RowDriver = bestDataResults[i]->bank->mat->subarray->DriverOptLevel;
				//TRY_AND_UPDATE(bestDataResults[i], mem_data);
                                if (inputParameter->routingMode == h_tree)
		                        trialBank = std::make_shared<BankWithHtree>();
	                        else
		                        trialBank = std::make_shared<BankWithoutHtree>();
	                        trialBank->Initialize(bestDataResults[i]->bank->numRowMat, 
                                        bestDataResults[i]->bank->numColumnMat, 
                                        bestDataResults[i]->bank->capacity, 
                                        bestDataResults[i]->bank->blockSize, 
                                        bestDataResults[i]->bank->associativity,
                                        bestDataResults[i]->bank->numRowPerSet, 
                                        bestDataResults[i]->bank->numActiveMatPerRow, 
                                        bestDataResults[i]->bank->numActiveMatPerColumn, 
                                        bestDataResults[i]->bank->muxSenseAmp,
                                        inputParameter->internalSensing, 
                                        bestDataResults[i]->bank->muxOutputLev1, 
                                        bestDataResults[i]->bank->muxOutputLev2, 
                                        bestDataResults[i]->bank->numRowSubarray, 
                                        bestDataResults[i]->bank->numColumnSubarray,
                                        bestDataResults[i]->bank->numActiveSubarrayPerRow,
                                        bestDataResults[i]->bank->numActiveSubarrayPerColumn, 
                                        bestDataResults[i]->bank->areaOptimizationLevel, 
                                        mem_data, 
                                        inputParameter->cell->camType, 
                                        inputParameter->searchFunction, 
                                        inputParameter,
                                        localWire,
                                        globalWire,
                                        CAM_opt);
                        	bestDataResults[i]->bank = trialBank;
                                //trialBank->CalculateArea(); 
                        	//trialBank->CalculateRC(); 
                        	//trialBank->CalculateLatencyAndPower(); 
                        	tempResult->bank = trialBank;
                        	tempResult->localWire = localWire;
                        	tempResult->globalWire = globalWire;
                        	bestDataResults[i]->compareAndUpdate(tempResult); 

			}
			if (inputParameter->optimizationTarget == full_exploration && !inputParameter->isPruningEnabled) {
				//OUTPUT_TO_FILE;
		                tempResult->printToCsvFile(outputFile); \
		                outputFile << std::endl; \
			}
		}
	}
	// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
	if (inputParameter->optimizationTarget == full_exploration && inputParameter->isPruningEnabled) {
		/* pruning is enabled */

/*
		Result **** pruningResults;
		// pruningResults[x][y][z] points to the result which is optimized for x, with constraint on y with z overhead
		pruningResults = new Result***[(int)full_exploration];	// full_exploration is always set as the last element in the enum, so if full_exploration is 8, what we want here is a 0-7 array, which is correct
		for (int i = 0; i < (int)full_exploration; i++) {
			pruningResults[i] = new Result**[(int)full_exploration];
			for (int j = 0; j < (int)full_exploration; j++) {
				pruningResults[i][j] = new Result*[3];		// 10%, 20%, and 30% overhead
				for (int k = 0; k < 3; k++)
					pruningResults[i][j][k] = new Result;
			}
		}
*/
 
                /* New vectorized smart pointers for this, not leaky C style pointers 
                 * Ideally not have a super nested vector of pointers, but it works without leaks */
                std::vector<std::vector<std::vector<std::unique_ptr<Result>>>> pruningResults;
                pruningResults.resize((int)full_exploration);
                for (auto& v1 : pruningResults) {
                        v1.resize((int)full_exploration);
                        for (auto& v2 : v1) {
                                v2.resize(3);
                                for (auto& res : v2) {
                                        res = std::make_unique<Result>();
                                        res->Initialize(inputParameter);
                                }
                        }
                }

		// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
		/* assign the constraints */
                //#pragma omp parallel for
		for (int i = 0; i < (int)full_exploration; i++) {
			for (int j = 0; j < (int)full_exploration; j++) {
				for (int k = 0; k < 3; k++) {
					pruningResults[i][j][k]->optimizationTarget = (OptimizationTarget)i;
					*(pruningResults[i][j][k]->localWire) = *(bestDataResults[i]->localWire);
					*(pruningResults[i][j][k]->globalWire) = *(bestDataResults[i]->globalWire);
					switch ((OptimizationTarget)j) {
					case read_latency_optimized:
						pruningResults[i][j][k]->limitReadLatency = bestDataResults[j]->bank->readLatency * (1 + (k + 1.0) / 10);
						break;
					case write_latency_optimized:
						pruningResults[i][j][k]->limitWriteLatency = bestDataResults[j]->bank->writeLatency * (1 + (k + 1.0) / 10);
						break;
					case read_energy_optimized:
						pruningResults[i][j][k]->limitReadDynamicEnergy = bestDataResults[j]->bank->readDynamicEnergy * (1 + (k + 1.0) / 10);
						break;
					case write_energy_optimized:
						pruningResults[i][j][k]->limitWriteDynamicEnergy = bestDataResults[j]->bank->writeDynamicEnergy * (1 + (k + 1.0) / 10);
						break;
					case read_edp_optimized:
						pruningResults[i][j][k]->limitReadEdp = bestDataResults[j]->bank->readLatency * bestDataResults[j]->bank->readDynamicEnergy * (1 + (k + 1.0) / 10);
						break;
					case write_edp_optimized:
						pruningResults[i][j][k]->limitWriteEdp = bestDataResults[j]->bank->writeLatency * bestDataResults[j]->bank->writeDynamicEnergy * (1 + (k + 1.0) / 10);
						break;
					case area_optimized:
						pruningResults[i][j][k]->limitArea = bestDataResults[j]->bank->area * (1 + (k + 1.0) / 10);
						break;
					case leakage_optimized:
						pruningResults[i][j][k]->limitLeakage = bestDataResults[j]->bank->leakage * (1 + (k + 1.0) / 10);
						break;
					default:
						/* nothing should happen here */
						PRINT_VERBOSE("Warning: should not happen");
					}
				}
                        }
                }
	}

	/* If design constraint is applied */
	if (inputParameter->optimizationTarget != full_exploration && inputParameter->isConstraintApplied) {

		double allowedDataReadLatency = bestDataResults[read_latency_optimized]->bank->readLatency 
                    * (inputParameter->readLatencyConstraint + 1);
		double allowedDataWriteLatency = bestDataResults[write_latency_optimized]->bank->writeLatency 
                    * (inputParameter->writeLatencyConstraint + 1);
		double allowedDataReadDynamicEnergy = bestDataResults[read_energy_optimized]->bank->readDynamicEnergy 
                    * (inputParameter->readDynamicEnergyConstraint + 1);
		double allowedDataWriteDynamicEnergy = bestDataResults[write_energy_optimized]->bank->writeDynamicEnergy 
                    * (inputParameter->writeDynamicEnergyConstraint + 1);
		double allowedDataLeakage = bestDataResults[leakage_optimized]->bank->leakage 
                    * (inputParameter->leakageConstraint + 1);
		double allowedDataArea = bestDataResults[area_optimized]->bank->area 
                    * (inputParameter->areaConstraint + 1);
		double allowedDataReadEdp = bestDataResults[read_edp_optimized]->bank->readLatency
                    * bestDataResults[read_edp_optimized]->bank->readDynamicEnergy 
                    * (inputParameter->readEdpConstraint + 1);
		double allowedDataWriteEdp = bestDataResults[write_edp_optimized]->bank->writeLatency
		    * bestDataResults[write_edp_optimized]->bank->writeDynamicEnergy 
                    * (inputParameter->writeEdpConstraint + 1);
		
                for (int i = 0; i < (int)full_exploration; i++) {
			//APPLY_LIMIT(bestDataResults[i]);
                        bestDataResults[i]->reset(); 
                	bestDataResults[i]->limitReadLatency = allowedDataReadLatency; 
                	bestDataResults[i]->limitWriteLatency = allowedDataWriteLatency; 
                	bestDataResults[i]->limitReadDynamicEnergy = allowedDataReadDynamicEnergy; 
                	bestDataResults[i]->limitWriteDynamicEnergy = allowedDataWriteDynamicEnergy; 
                	bestDataResults[i]->limitReadEdp = allowedDataReadEdp; 
                	bestDataResults[i]->limitWriteEdp = allowedDataWriteEdp; 
                	bestDataResults[i]->limitArea = allowedDataArea; 
                	bestDataResults[i]->limitLeakage = allowedDataLeakage; 
		}

		numSolution = 0;
		//INITIAL_BASIC_WIRE;
                WireType basicWireType;
                WireRepeaterType basicWireRepeaterType;
                bool isBasicLowSwing;
                if (inputParameter->minLocalWireType == inputParameter->maxLocalWireType)
                        basicWireType = (WireType)inputParameter->minLocalWireType;
                else
                        basicWireType = local_aggressive;
                if (inputParameter->minLocalWireRepeaterType == inputParameter->maxLocalWireRepeaterType)
                        basicWireRepeaterType = (WireRepeaterType)inputParameter->minLocalWireRepeaterType;
                else
                        basicWireRepeaterType = repeated_none;
                if (inputParameter->minIsLocalWireLowSwing == inputParameter->maxIsLocalWireLowSwing)
                        isBasicLowSwing = inputParameter->minIsLocalWireLowSwing;
                else
                        isBasicLowSwing = false;
                localWire->Initialize(inputParameter->processNode, basicWireType, basicWireRepeaterType, 
                        inputParameter->temperature, isBasicLowSwing, inputParameter);
                if (inputParameter->minGlobalWireType == inputParameter->maxGlobalWireType)
                        basicWireType = (WireType)inputParameter->minGlobalWireType;
                else
                        basicWireType = global_aggressive;
                if (inputParameter->minGlobalWireRepeaterType == inputParameter->maxGlobalWireRepeaterType)
                        basicWireRepeaterType = (WireRepeaterType)inputParameter->minGlobalWireRepeaterType;
                else
                        basicWireRepeaterType = repeated_none;
                if (inputParameter->minIsGlobalWireLowSwing == inputParameter->maxIsGlobalWireLowSwing)
                        isBasicLowSwing = inputParameter->minIsGlobalWireLowSwing;
                else
                        isBasicLowSwing = false;
                globalWire->Initialize(inputParameter->processNode, basicWireType, basicWireRepeaterType, 
                        inputParameter->temperature, isBasicLowSwing, inputParameter);

                // TODO: Make multithreaded 
                for (int numRowMat = inputParameter->minNumRowMat; 
                        numRowMat <= inputParameter->maxNumRowMat; 
                        numRowMat *= 2)
	        for (int numColumnMat = inputParameter->minNumColumnMat; 
                        numColumnMat <= inputParameter->maxNumColumnMat; 
                        numColumnMat *= 2) 
                for (int numActiveMatPerRow = MIN(numColumnMat, inputParameter->minNumActiveMatPerRow); 
                        numActiveMatPerRow <= MIN(numColumnMat, inputParameter->maxNumActiveMatPerRow); 
                        numActiveMatPerRow *= 2) 
                for (int numActiveMatPerColumn = MIN(numRowMat, inputParameter->minNumActiveMatPerColumn); 
                        numActiveMatPerColumn <= MIN(numRowMat, inputParameter->maxNumActiveMatPerColumn); 
                        numActiveMatPerColumn *= 2) 
                for (int numRowSubarray = inputParameter->minNumRowSubarray; 
                        numRowSubarray <= inputParameter->maxNumRowSubarray; 
                        numRowSubarray *= 2) 
                for (int numColumnSubarray = inputParameter->minNumColumnSubarray; 
                        numColumnSubarray <= inputParameter->maxNumColumnSubarray; 
                        numColumnSubarray *= 2) 
                for (int numActiveSubarrayPerRow = MIN(numColumnSubarray, inputParameter->minNumActiveSubarrayPerRow); 
                        numActiveSubarrayPerRow <= MIN(numColumnSubarray, inputParameter->maxNumActiveSubarrayPerRow); 
                        numActiveSubarrayPerRow *=2) 
                for (int numActiveSubarrayPerColumn = MIN(numRowSubarray, inputParameter->minNumActiveSubarrayPerColumn); 
                        numActiveSubarrayPerColumn <= MIN(numRowSubarray, inputParameter->maxNumActiveSubarrayPerColumn);
                        numActiveSubarrayPerColumn *= 2)
                for (int muxSenseAmp = inputParameter->minMuxSenseAmp; 
                        muxSenseAmp <= inputParameter->maxMuxSenseAmp; 
                        muxSenseAmp *= 2) 
                for (int muxOutputLev1 = inputParameter->minMuxOutputLev1; 
                        muxOutputLev1 <= inputParameter->maxMuxOutputLev1; 
                        muxOutputLev1 *= 2) 
                for (int muxOutputLev2 = inputParameter->minMuxOutputLev2; 
                        muxOutputLev2 <= inputParameter->maxMuxOutputLev2; 
                        muxOutputLev2 *= 2) 
                for (int numRowPerSet = inputParameter->minNumRowPerSet; 
                        numRowPerSet <= MIN(inputParameter->maxNumRowPerSet, inputParameter->associativity); 
                        numRowPerSet *= 2) 
                for (int areaOptimizationLevel = inputParameter->minAreaOptimizationLevel; 
                        areaOptimizationLevel <= inputParameter->maxAreaOptimizationLevel; 
                        areaOptimizationLevel++) {

                    if (blockSize / (numActiveMatPerRow * numActiveMatPerColumn * numActiveSubarrayPerRow * numActiveSubarrayPerColumn) == 0) {
				/* To aggressive partitioning */
				continue;
			}

                        std::shared_ptr<Bank> dataBank;

                        if (inputParameter->routingMode == h_tree) {
		            dataBank = std::make_unique<BankWithHtree>(); 
                        } else { 
		            dataBank = std::make_unique<BankWithoutHtree>(); 
                        }
	                dataBank->Initialize(numRowMat, numColumnMat, capacity, blockSize, associativity,
				numRowPerSet, numActiveMatPerRow, numActiveMatPerColumn, muxSenseAmp,
				inputParameter->internalSensing, muxOutputLev1, muxOutputLev2, numRowSubarray, 
                                numColumnSubarray, numActiveSubarrayPerRow, numActiveSubarrayPerColumn, 
                                (BufferDesignTarget)areaOptimizationLevel, mem_data, inputParameter->cell->camType, 
                                inputParameter->searchFunction, inputParameter, localWire, globalWire, CAM_opt);
	                //dataBank->CalculateArea(); 
	                //dataBank->CalculateRC(); 
	                //dataBank->CalculateLatencyAndPower(); 

		        if (!dataBank->invalid 
                                && dataBank->readLatency <= allowedDataReadLatency 
                                && dataBank->writeLatency <= allowedDataWriteLatency
				&& dataBank->readDynamicEnergy <= allowedDataReadDynamicEnergy 
                                && dataBank->writeDynamicEnergy <= allowedDataWriteDynamicEnergy
				&& dataBank->leakage <= allowedDataLeakage 
                                && dataBank->area <= allowedDataArea
				&& dataBank->readLatency * dataBank->readDynamicEnergy <= allowedDataReadEdp 
                                && dataBank->writeLatency * dataBank->writeDynamicEnergy <= allowedDataWriteEdp) {

				std::shared_ptr<Result> tempResult;
				//VERIFY_DATA_CAPACITY;
                                if ((long long)dataBank->mat->subarray->numColumn * dataBank->mat->subarray->numRow
                                        * dataBank->numColumnMat * dataBank->numRowMat * dataBank->numColumnSubarray 
                                        * dataBank->numRowSubarray  != capacity) {
                                    std::cout << 
                                    "numcolumn x numrow x numcolumnmat x numrowmat x numcolumnsubarry x numrowsubarray" 
                                        << dataBank->mat->subarray->numColumn  << ": " << dataBank->mat->subarray->numRow
                                        << ": " << dataBank->numColumnMat << ": " << dataBank->numRowMat << ": " 
                                        <<  dataBank->numColumnSubarray << ": " <<  dataBank->numRowSubarray << std::endl;
                                    std::cout << "1 Bank = " << dataBank->numRowMat << "x" 
                                        << dataBank->numColumnMat << " Mats" << std::endl;
                                    std::cout << "Activation - " << dataBank->numActiveMatPerColumn 
                                        << "x" << dataBank->numActiveMatPerRow << " Mats" << std::endl;
                                    std::cout << "1 Mat  = " << dataBank->numRowSubarray 
                                        << "x" << dataBank->numColumnSubarray << " Subarrays" << std::endl;
                                    std::cout << "Activation - " << dataBank->numActiveSubarrayPerColumn 
                                        << "x" << dataBank->numActiveSubarrayPerRow << " Subarrays" << std::endl;
                                    std::cout << "Mux Degree - " << dataBank->muxSenseAmp << " x " 
                                        << dataBank->muxOutputLev1 << " x " << dataBank->muxOutputLev2 << std::endl;
				    throw std::runtime_error("ERROR: DATA capacity violation. Shouldn't happen"); 
			        }

				numSolution++;
				//UPDATE_BEST_DATA;
                                tempResult->bank = dataBank->clone_bank(); 
	                        tempResult->localWire = localWire; 
	                        tempResult->globalWire = globalWire; 
	                        for (int i = 0; i < (int)full_exploration; i++){ 
		                        bestDataResults[i]->compareAndUpdate(tempResult); 
	                        } 

			}
		}
	}

	if (inputParameter->optimizationTarget != full_exploration) {

		if (numSolution > 0) bestDataResults[inputParameter->optimizationTarget]->print();
		else std::cout << "No valid solutions." << std::endl;

		std::cout << std::endl << "Finished!" << std::endl;

	} else {
		std::cout << std::endl << outputFileName << " generated successfully!" << std::endl;
		if (inputParameter->isPruningEnabled) {
			std::cout << "The results are pruned" << std::endl;
		}

		for (int i = 0; i < (int)full_exploration; i++) {
			std::cout << "[" << std::left << std::setw(2) << i << "]" << " ";
			std::cout << std::left << std::setw(8) << bestDataResults[i]->bank->readLatency * 1e12 << "    ";
		    std::cout << std::left << std::setw(8) << bestDataResults[i]->bank->writeLatency * 1e12 << "    ";
		    std::cout << std::left << std::setw(5) << bestDataResults[i]->bank->readDynamicEnergy * 1e12 << "    ";
		    std::cout << std::left << std::setw(5) << bestDataResults[i]->bank->writeDynamicEnergy * 1e12 << "    ";
			std::cout << std::left << std::setw(9) << bestDataResults[i]->bank->readLatency * bestDataResults[i]->bank->readDynamicEnergy * 1e24 << "    ";
		    std::cout << std::left << std::setw(9) << bestDataResults[i]->bank->writeLatency * bestDataResults[i]->bank->writeDynamicEnergy * 1e24 << "    ";
		    std::cout << std::left << std::setw(5) << bestDataResults[i]->bank->leakage * 1e6 << "    ";
			std::cout << std::left << std::setw(8) << bestDataResults[i]->bank->area * 1e12 << "    ";
		    std::cout << std::left << std::setw(8) << bestDataResults[i]->bank->searchLatency * 1e12 << "    ";
		    std::cout << std::left << std::setw(5) << bestDataResults[i]->bank->searchDynamicEnergy * 1e12 << "    ";
		    std::cout << std::left << std::setw(8) << bestDataResults[i]->bank->searchDynamicEnergy * bestDataResults[i]->bank->searchLatency * 1e24 << std::endl;
		}
		std::cout 	<< std::left << std::setw(8) << bestDataResults[7]->bank->numBitSerial << "	"
				<< std::left << std::setw(8) << bestDataResults[8]->bank->numBitSerial << "	"
				<< std::left << std::setw(8) << bestDataResults[9]->bank->numBitSerial << "	"
				<< std::left << std::setw(8) << bestDataResults[3]->bank->numBitSerial << "	"
				<< std::left << std::setw(8) << bestDataResults[6]->bank->numBitSerial << "	"
				<< std::endl;
		std::cout 	<< std::left << std::setw(8) << bestDataResults[7]->bank->mat->areaOptimizationLevel << "	"
				<< std::left << std::setw(8) << bestDataResults[8]->bank->mat->areaOptimizationLevel << "	"
				<< std::left << std::setw(8) << bestDataResults[9]->bank->mat->areaOptimizationLevel << "	"
				<< std::left << std::setw(8) << bestDataResults[3]->bank->mat->areaOptimizationLevel << "	"
				<< std::left << std::setw(8) << bestDataResults[6]->bank->mat->areaOptimizationLevel << "	"
				<< std::endl;
		std::cout 	<< std::left << std::setw(8) << bestDataResults[7]->bank->mat->subarray->DriverOptLevel << "	"
				<< std::left << std::setw(8) << bestDataResults[8]->bank->mat->subarray->DriverOptLevel << "	"
				<< std::left << std::setw(8) << bestDataResults[9]->bank->mat->subarray->DriverOptLevel << "	"
				<< std::left << std::setw(8) << bestDataResults[3]->bank->mat->subarray->DriverOptLevel << "	"
				<< std::left << std::setw(8) << bestDataResults[6]->bank->mat->subarray->DriverOptLevel << "	"
				<< std::endl;
	    std::cout 	<< std::left << std::setw(8) << bestDataResults[7]->bank->area * 1e12 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[8]->bank->area * 1e12 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[9]->bank->area * 1e12 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[3]->bank->area * 1e12 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[6]->bank->area * 1e12 << "	"
	    		<< std::endl;
	    std::cout 	<< std::left << std::setw(8) << bestDataResults[7]->bank->searchLatency * 1e9 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[8]->bank->searchLatency * 1e9 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[9]->bank->searchLatency * 1e9 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[3]->bank->searchLatency * 1e9 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[6]->bank->searchLatency * 1e9 << "	"
	    		<< std::endl;
	    std::cout 	<< std::left << std::setw(8) << bestDataResults[7]->bank->searchDynamicEnergy * 1e12 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[8]->bank->searchDynamicEnergy * 1e12 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[9]->bank->searchDynamicEnergy * 1e12 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[3]->bank->searchDynamicEnergy * 1e12 << "	"
	    	 	<< std::left << std::setw(8) << bestDataResults[6]->bank->searchDynamicEnergy * 1e12 << "	"
	    		<< std::endl;
	    std::cout	<< std::left << std::setw(8) << bestDataResults[7]->bank->writeDynamicEnergy * 1e12 << "	"
	    		<< std::left << std::setw(8) << bestDataResults[8]->bank->writeDynamicEnergy * 1e12 << "	"
	    		<< std::left << std::setw(8) << bestDataResults[9]->bank->writeDynamicEnergy * 1e12 << "	"
	    		<< std::left << std::setw(8) << bestDataResults[3]->bank->writeDynamicEnergy * 1e12 << "	"
	    		<< std::left << std::setw(8) << bestDataResults[6]->bank->writeDynamicEnergy * 1e12 << "	"
	    		<< std::endl;
	    std::cout	<< std::left << std::setw(8) << bestDataResults[7]->bank->leakage * 1e6 << "	"
	    		<< std::left << std::setw(8) << bestDataResults[8]->bank->leakage * 1e6 << "	"
	    		<< std::left << std::setw(8) << bestDataResults[9]->bank->leakage * 1e6 << "	"
	    		<< std::left << std::setw(8) << bestDataResults[3]->bank->leakage * 1e6 << "	"
	    		<< std::left << std::setw(8) << bestDataResults[6]->bank->leakage * 1e6 << "	"
	    		<< std::endl;
	}

	if (outputFile.is_open())
		outputFile.close();

        {
                std::filesystem::path outPath(outputYamlFileName);
                if (!outPath.parent_path().empty()) {
                        std::filesystem::create_directories(outPath.parent_path());
                }
                std::ofstream yamlOut(outputYamlFileName);
                if (!yamlOut) {
                        throw std::runtime_error("Failed to open YAML output file: " + outputYamlFileName);
                }
                if (numSolution <= 0) {
                        WriteResultsYamlNoSolutions(yamlOut);
                } else if (inputParameter->optimizationTarget == full_exploration) {
                        std::vector<std::shared_ptr<Result>> results;
                        results.reserve((int)full_exploration);
                        for (int i = 0; i < (int)full_exploration; i++)
                                results.push_back(bestDataResults[i]);
                        WriteResultsYamlMulti(yamlOut, results);
                } else {
                        WriteResultsYaml(yamlOut, *bestDataResults[inputParameter->optimizationTarget]);
                }
        }
    
    } catch (const YAML::Exception& e) {

        if (e.mark.line != -1) {
            std::cerr << "YAML error at line " 
                  << (e.mark.line + 1)
                  << ", column "
                  << (e.mark.column + 1)
                  << ": "
                  << e.what()
                  << std::endl;
        } else {
            std::cerr << "YAML error: " << e.what() << "\n";
        }
        exitCode = 1;

    } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            exitCode = 1;
    }

        return exitCode;
}

void usage(int exit_code) {

        std::cout << std::endl << "Usage: ./EvaCAM [OPTIONS] <cfg_file>" << std::endl               << std::endl;
        std::cout << "Options:"                                                                     << std::endl;
        std::cout << "  -t, --threads N           Number of parallel threads (default: all cores)"  << std::endl;
        std::cout << "  -v, --verbose             Enable verbose output"                            << std::endl;
        std::cout << "  -d, --deep-exploration    Test more options when performing optimization"   << std::endl;
        std::cout << "  -o, --output FILE         YAML output file (default: results/<cfg>_results.yaml)" << std::endl;
        std::cout << "  -h, --help                Show this help and exit"                          << std::endl;

        std::exit(exit_code);
}

// TODO: This should be inside of the InputParameter and MemCell class.
void applyConstraint(std::shared_ptr<InputParameter> inputParameter) {
	/* Check functions that are not yet implemented */
	if (inputParameter->cell->memCellType == DRAM) {
		throw std::runtime_error("[ERROR] DRAM model is still under development");
	}
	if (inputParameter->cell->memCellType == eDRAM) {
		throw std::runtime_error("[ERROR] Embedded DRAM model is still under development");
	}
	if (inputParameter->cell->memCellType == MLCNAND) {
		throw std::runtime_error("[ERROR] MLC NAND flash model is still under development");
	}

	/*if (inputParameter->designTarget != cache && inputParameter->associativity > 1) {
		std::cout << "[WARNING] Associativity setting is ignored for non-cache designs" << std::endl;
		inputParameter->associativity = 1;
	}

	if (!isPow2(inputParameter->associativity)) {
		throw std::runtime_error("[ERROR] The associativity value has to be a power of 2 in this version");
	}

	if (inputParameter->routingMode == h_tree && inputParameter->internalSensing == false) {
		throw std::runtime_error("[ERROR] H-tree does not support external sensing scheme in this version");
	}*/
/*
	if (inputParameter->globalWireepeaterType != repeated_none && inputParameter->internalSensing == false) {
		std::cout << "[ERROR] Repeated global wire does not support external sensing scheme" << std::endl;
		exit(-1);
	}
*/

	/* TODO: more rules to add not here but in the input reading classes where they belong */
}


// Unused function, no need to have it keep causing compiler errors, so commented it out
/*
void mat_only_print(std::shared_ptr<Mat> mat){
void mat_only_print(std::shared_ptr<Mat> mat){
void mat_only_print(std::shared_ptr<Mat> mat){
void mat_only_print(std::shared_ptr<Mat> mat){
	// all in um, ps, uw

	///////////////       area              //////////////
	std::cout << mat->height * 1e6 << std::endl << mat->width * 1e6 << std::endl;
	std::cout << mat->area * 1e12 << std::endl;
	std::cout << (mat->rowPredecoderBlock1->area + mat->rowPredecoderBlock2->area) * 1e12 << std::endl;
	std::cout << (mat->bitlineMuxPredecoderBlock1->area + mat->bitlineMuxPredecoderBlock2->area +
			mat->senseAmpMuxLev1PredecoderBlock1->area + mat->senseAmpMuxLev1PredecoderBlock2->area +
			mat->senseAmpMuxLev2PredecoderBlock1->area + mat->senseAmpMuxLev2PredecoderBlock2->area ) * 1e12 << std::endl;
	std::cout << mat->subarray->area * 1e12 << std::endl;
	std::cout << mat->subarray->inputEnc->area *1e12 << std::endl;
	std::cout << (mat->subarray->RowDecMergeNand->area +mat->subarray->senseAmpMuxLev1Nand->area +mat->subarray->senseAmpMuxLev2Nand->area) * 1e12 << std::endl;
	double area = 0;
	for(int i=0;i<cell->camNumRow;i++){
		area += mat->subarray->RowDriver[i]->area;
	}
	std::cout << area * 1e12 << std::endl;
	std::cout << mat->subarray->precharger->area * 1e12 << std::endl;
	std::cout << mat->subarray->lenRow * mat->subarray->lenCol * 1e12 << std::endl;
	area = 0;
	for(int i=0;i<cell->camNumCol;i++){
		area += mat->subarray->ColMux[i]->area;
	}
	std::cout << area * 1e12 << std::endl;
	std::cout << mat->subarray->senseAmp->area * 1e12 << std::endl;
	std::cout << (mat->subarray->senseAmpMuxLev1->area + mat->subarray->senseAmpMuxLev2->area) * 1e12 << std::endl;
	std::cout << mat->subarray->outputAcc->area * 1e12 << std::endl;
	std::cout << mat->subarray->priorityEnc->area * 1e12 << std::endl;

	///////////////       latency              //////////////
	std::cout << mat->readLatency * 1e12 << std::endl;
	std::cout << (mat->rowPredecoderBlock1->readLatency + mat->rowPredecoderBlock2->readLatency) * 1e12 << std::endl;
	std::cout << (mat->bitlineMuxPredecoderBlock1->readLatency + mat->bitlineMuxPredecoderBlock2->readLatency +
			mat->senseAmpMuxLev1PredecoderBlock1->readLatency + mat->senseAmpMuxLev1PredecoderBlock2->readLatency +
			mat->senseAmpMuxLev2PredecoderBlock1->readLatency + mat->senseAmpMuxLev2PredecoderBlock2->readLatency ) * 1e12 << std::endl;
	std::cout << mat->subarray->readLatency * 1e12 << std::endl;
	std::cout << mat->subarray->inputEnc->readLatency *1e12 << std::endl;
	std::cout << (mat->subarray->RowDecMergeNand->readLatency +mat->subarray->senseAmpMuxLev1Nand->readLatency +mat->subarray->senseAmpMuxLev2Nand->readLatency) * 1e12 << std::endl;
	double readLatency = 0;
	for(int i=0;i<cell->camNumRow;i++){
		readLatency = MAX(mat->subarray->RowDriver[i]->readLatency, readLatency);
	}
	std::cout << readLatency * 1e12 << std::endl;
	std::cout << mat->subarray->precharger->readLatency * 1e12 << std::endl;
	std::cout << mat->subarray->matchlineDelay * 1e12 << std::endl;
	readLatency = 0;
	for(int i=0;i<cell->camNumCol;i++){
		readLatency = MAX(mat->subarray->ColMux[i]->readLatency, readLatency);
	}
	std::cout << readLatency * 1e12 << std::endl;
	std::cout << mat->subarray->senseAmp->readLatency * 1e12 << std::endl;
	std::cout << (mat->subarray->senseAmpMuxLev1->readLatency + mat->subarray->senseAmpMuxLev2->readLatency) * 1e12 << std::endl;
	std::cout << mat->subarray->outputAcc->readLatency * 1e12 << std::endl;
	std::cout << mat->subarray->priorityEnc->readLatency * 1e12 << std::endl;

	///////////////       energy              //////////////
	std::cout << mat->readDynamicEnergy * 1e12 << std::endl;
	std::cout << (mat->rowPredecoderBlock1->readDynamicEnergy + mat->rowPredecoderBlock2->readDynamicEnergy) * 1e12 << std::endl;
	std::cout << (mat->bitlineMuxPredecoderBlock1->readDynamicEnergy + mat->bitlineMuxPredecoderBlock2->readDynamicEnergy +
			mat->senseAmpMuxLev1PredecoderBlock1->readDynamicEnergy + mat->senseAmpMuxLev1PredecoderBlock2->readDynamicEnergy +
			mat->senseAmpMuxLev2PredecoderBlock1->readDynamicEnergy + mat->senseAmpMuxLev2PredecoderBlock2->readDynamicEnergy ) * 1e12 << std::endl;
	std::cout << mat->subarray->readDynamicEnergy * 1e12 << std::endl;
	std::cout << mat->subarray->inputEnc->readDynamicEnergy *1e12 << std::endl;
	std::cout << (mat->subarray->RowDecMergeNand->readDynamicEnergy +mat->subarray->senseAmpMuxLev1Nand->readDynamicEnergy
			+mat->subarray->senseAmpMuxLev2Nand->readDynamicEnergy) * 1e12 << std::endl;
	double readDynamicEnergy = 0;
	for(int i=0;i<cell->camNumRow;i++){
		readDynamicEnergy += mat->subarray->RowDriver[i]->readDynamicEnergy;
	}
	std::cout << readDynamicEnergy * 1e12 << std::endl;
	std::cout << mat->subarray->precharger->readDynamicEnergy * 1e12 << std::endl;
	// TODO: not really the breakdown
	std::cout << mat->subarray->cellReadEnergy * 1e12 << std::endl;
	readDynamicEnergy = 0;
	for(int i=0;i<cell->camNumCol;i++){
		readDynamicEnergy += mat->subarray->ColMux[i]->readDynamicEnergy;
	}
	std::cout << readDynamicEnergy * 1e12 << std::endl;
	std::cout << mat->subarray->senseAmp->readDynamicEnergy * 1e12 << std::endl;
	std::cout << (mat->subarray->senseAmpMuxLev1->readDynamicEnergy + mat->subarray->senseAmpMuxLev2->readDynamicEnergy) * 1e12 << std::endl;
	std::cout << mat->subarray->outputAcc->readDynamicEnergy * 1e12 << std::endl;
	std::cout << mat->subarray->priorityEnc->readDynamicEnergy * 1e12 << std::endl;

	///////////////       leakage              //////////////
	std::cout << mat->leakage * 1e6 << std::endl;
	std::cout << (mat->rowPredecoderBlock1->leakage + mat->rowPredecoderBlock2->leakage) * 1e6 << std::endl;
	std::cout << (mat->bitlineMuxPredecoderBlock1->leakage + mat->bitlineMuxPredecoderBlock2->leakage +
			mat->senseAmpMuxLev1PredecoderBlock1->leakage + mat->senseAmpMuxLev1PredecoderBlock2->leakage +
			mat->senseAmpMuxLev2PredecoderBlock1->leakage + mat->senseAmpMuxLev2PredecoderBlock2->leakage ) * 1e6 << std::endl;
	std::cout << mat->subarray->leakage * 1e6 << std::endl;
	std::cout << mat->subarray->inputEnc->leakage *1e6 << std::endl;
	std::cout << (mat->subarray->RowDecMergeNand->leakage +mat->subarray->senseAmpMuxLev1Nand->leakage +mat->subarray->senseAmpMuxLev2Nand->leakage) * 1e6 << std::endl;
	double leakage = 0;
	for(int i=0;i<cell->camNumRow;i++){
		leakage += mat->subarray->RowDriver[i]->leakage;
	}
	std::cout << leakage * 1e6 << std::endl;
	std::cout << mat->subarray->precharger->leakage * 1e6 << std::endl;
	std::cout << 0 << std::endl;
	leakage = 0;
	for(int i=0;i<cell->camNumCol;i++){
		leakage += mat->subarray->ColMux[i]->leakage;
	}
	std::cout << leakage * 1e6 << std::endl;
	std::cout << mat->subarray->senseAmp->leakage * 1e6 << std::endl;
	std::cout << (mat->subarray->senseAmpMuxLev1->leakage + mat->subarray->senseAmpMuxLev2->leakage) * 1e6 << std::endl;
	std::cout << mat->subarray->outputAcc->leakage * 1e6 << std::endl;
	std::cout << mat->subarray->priorityEnc->leakage * 1e6 << std::endl;
}
*/
