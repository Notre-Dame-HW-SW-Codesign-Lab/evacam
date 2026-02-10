#include "../include/InputParameter.h"
#include "../include/global.h"
#include "../include/constant.h"
#include "../include/formula.h"
#include "../include/macros.h"
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <yaml.h>
//#include <yaml-cpp/yaml.h>
//#include <magic_enum.hpp>

InputParameter::InputParameter() {
	// TODO Auto-generated constructor stub
	designTarget = cache;
	searchFunction = EX;
	optimizationTarget = read_latency_optimized;
	processNode = 90;
	maxDriverCurrent = 0;

	maxNmosSize = MAX_NMOS_SIZE;

	minNumRowMat = 1;
	maxNumRowMat = 16;
	minNumColumnMat = 1;
	maxNumColumnMat = 16;
	minNumActiveMatPerRow = 1;
	maxNumActiveMatPerRow = maxNumColumnMat;
	minNumActiveMatPerColumn = 1;
	maxNumActiveMatPerColumn = maxNumRowMat;
	minNumRowSubarray = 1;
	maxNumRowSubarray = 16;
	minNumColumnSubarray = 1;
	maxNumColumnSubarray = 16;
	minNumActiveSubarrayPerRow = 1;
	maxNumActiveSubarrayPerRow = maxNumColumnSubarray;
	minNumActiveSubarrayPerColumn = 1;
	maxNumActiveSubarrayPerColumn = maxNumRowSubarray;
	minNumRow = 16;
	maxNumRow = 512;
	minNumColumn = 16;
	maxNumColumn = 512;
	minMuxSenseAmp = 1;
	maxMuxSenseAmp = 32;
	minMuxOutputLev1 = 1;
	maxMuxOutputLev1 = 32;
	minMuxOutputLev2 = 1;
	maxMuxOutputLev2 = 32;
	minNumRowPerSet = 1;
	maxNumRowPerSet = 32;
	minAreaOptimizationLevel = latency_first;
	maxAreaOptimizationLevel = area_first;
	minLocalWireType = local_aggressive;
	maxLocalWireType = local_conservative;
	minGlobalWireType = global_aggressive;
	maxGlobalWireType = global_conservative;
	minLocalWireRepeaterType = repeated_none;
	maxLocalWireRepeaterType = repeated_50;		/* The limit is repeated_50 */
	minGlobalWireRepeaterType = repeated_none;
	maxGlobalWireRepeaterType = repeated_50;	/* The limit is repeated_50 */
	minIsLocalWireLowSwing = false;
	maxIsLocalWireLowSwing = true;
	minIsGlobalWireLowSwing = false;
	maxIsGlobalWireLowSwing = true;

	associativity = 1;				/* Default value for non-cache design */
	routingMode = h_tree;
	internalSensing = true;

	useCactiAssumption = false;

	writeScheme = normal_write;
	cacheAccessMode = normal_access_mode;

	readLatencyConstraint = 1e41;
	writeLatencyConstraint = 1e41;
	readDynamicEnergyConstraint = 1e41;
	writeDynamicEnergyConstraint = 1e41;
	leakageConstraint = 1e41;
	areaConstraint = 1e41;
	readEdpConstraint = 1e41;
	writeEdpConstraint = 1e41;
	isConstraintApplied = false;
	isPruningEnabled = false;

	pageSize = 0;
	flashBlockSize = 0;

	outputFilePrefix = "output";	/* Default output file name */
	// TODO: added new initial
	withWriteDriver = false;
	withInputBuffer = false;
	withOutputBuffer = false;

	realCapacity = 0;
	NoPrechargeInc = false;
	IncludeLeakge = false;
	scaledVoltage = 0;
	UseUpdatedLib = false;

	minBitSerialWidth = 8;
	maxBitSerialWidth = 1024;
	minRowDriverOptLevel = latency_first;
	maxRowDriverOptLevel = area_first;
	minPriorityOptLevel = latency_first;
	maxPriorityOptLevel = area_first;

	MatchlineSenseMargin = 3e-8;

        // TODO: ensure these are right
        AddCapOnML = 0;

}
/*
InputParameter::~InputParameter() {
	// TODO Auto-generated destructor stub
}
*/

void InputParameter::RestoreSearchSize() {
        minNumRowMat = 1; 
	maxNumRowMat = 4; 
	minNumColumnMat = 1; 
	maxNumColumnMat = 4; 
	minNumActiveMatPerRow = 1; 
	maxNumActiveMatPerRow = maxNumColumnMat; 
	minNumActiveMatPerColumn = 1; 
	maxNumActiveMatPerColumn = maxNumRowMat; 
	minNumRowSubarray = 1; 
	maxNumRowSubarray = 8; 
	minNumColumnSubarray = 1; 
	maxNumColumnSubarray = 8; 
	minNumActiveSubarrayPerRow = 1; 
	maxNumActiveSubarrayPerRow = maxNumColumnSubarray; 
	minNumActiveSubarrayPerColumn = 1; 
	maxNumActiveSubarrayPerColumn = maxNumRowSubarray; 
	minMuxSenseAmp = 1; 
	maxMuxSenseAmp = 32; 
	minMuxOutputLev1 = 1; 
	maxMuxOutputLev1 = 32; 
	minMuxOutputLev2 = 1; 
	maxMuxOutputLev2 = 32; 
	minNumRowPerSet = 1; 
	maxNumRowPerSet = associativity; 
	minAreaOptimizationLevel = latency_first; 
	maxAreaOptimizationLevel = area_first; 
	minLocalWireType = local_aggressive; 
	maxLocalWireType = semi_conservative; 
	minGlobalWireType = semi_aggressive; 
	maxGlobalWireType = global_conservative; 
	minLocalWireRepeaterType = repeated_none; 
	maxLocalWireRepeaterType = repeated_50;		/* The limit is repeated_50 */ 
	minGlobalWireRepeaterType = repeated_none; 
	maxGlobalWireRepeaterType = repeated_50;	/* The limit is repeated_50 */ 
	minIsLocalWireLowSwing = false; 
	maxIsLocalWireLowSwing = true; 
	minIsGlobalWireLowSwing = false; 
	maxIsGlobalWireLowSwing = true; 
}

void InputParameter::ReduceSearchSize() {
        minNumRowMat = 1; 
	maxNumRowMat = 64; 
	minNumColumnMat = 1; 
	maxNumColumnMat = 64; 
	minNumActiveMatPerRow = 1; 
	maxNumActiveMatPerRow = maxNumColumnMat; 
	minNumActiveMatPerColumn = 1; 
	maxNumActiveMatPerColumn = maxNumRowMat; 
	minNumRowSubarray = 1; 
	maxNumRowSubarray = 16; 
	minNumColumnSubarray = 1; 
	maxNumColumnSubarray = 16; 
	minNumActiveSubarrayPerRow = 1; 
	maxNumActiveSubarrayPerRow = maxNumColumnSubarray; 
	minNumActiveSubarrayPerColumn = 1; 
	maxNumActiveSubarrayPerColumn = maxNumRowSubarray; 
	minMuxSenseAmp = 1; 
	maxMuxSenseAmp = 64; 
	minMuxOutputLev1 = 1; 
	maxMuxOutputLev1 = 64; 
	minMuxOutputLev2 = 1; 
	maxMuxOutputLev2 = 64; 
	minNumRowPerSet = 1; 
	maxNumRowPerSet = 1; 
	minAreaOptimizationLevel = latency_first; 
	maxAreaOptimizationLevel = area_first;	
	minLocalWireType = local_aggressive; 
	maxLocalWireType = local_conservative; 
	minGlobalWireType = global_aggressive; 
	maxGlobalWireType = global_conservative; 
	minLocalWireRepeaterType = repeated_none; 
	maxLocalWireRepeaterType = repeated_opt; 
	minGlobalWireRepeaterType = repeated_none; 
	maxGlobalWireRepeaterType = repeated_opt; 
	minIsLocalWireLowSwing = false; 
	maxIsLocalWireLowSwing = true; 
	minIsGlobalWireLowSwing = false; 
	maxIsGlobalWireLowSwing = true; 
}

void InputParameter::ReadInputParameterFromFile(const std::string &inputFile) {

    const YAML::Node config = YAML::LoadFile(inputFile);
 
    // Design
    
    auto design = child_required(config, "design");

    std::string tmp = read_required<std::string>(design, "target");

    if (tmp != "CAM") 
        throw std::runtime_error("[Error]: Only support CAM design.");
    else {
        designTarget = CAM_chip;
	minNumRowPerSet = 1;
	maxNumRowPerSet = 256;
    }

    tmp = read_required<std::string>(design, "search_function");

    if      (tmp == "BE") searchFunction = BE;
    else if (tmp == "TH") searchFunction = TH;
    else                  searchFunction = EX;

    processNode = read_required<int>(design, "process_node");

    constexpr std::array<int, 9> supportedProcessNodes = {7, 10, 14, 22, 32, 45, 90, 120, 200};
    if (!std::binary_search(supportedProcessNodes.begin(), supportedProcessNodes.end(), processNode))
        PRINT_VERBOSE( "[Warning]: Possible error as " << processNode << "nm proccess node is not supported.");
    if (processNode <= 45) {
        std::string ptm = "[Warning]: When using PTM model, ";
	PRINT_VERBOSE(ptm << "only HP/LSTP model is provided.");
	PRINT_VERBOSE(ptm << "interpolation between 45/32/22/14/10/7nm may lead to error.");
    }

    tmp = read_required<std::string>(design, "device_roadmap");

    if      (tmp == "HP")   deviceRoadmap = HP;
    else if (tmp == "LSTP") deviceRoadmap = LSTP;
    else                    deviceRoadmap = LOP;

    temperature = read_required<int>(design, "temperature");

    // Memory
    
    auto memory = child_required(config, "memory");

    fileMemCell = read_required<std::string>(memory, "cell_file");

    capacity = read_required<int>(memory, "capacity");

    wordWidth = read_required<int>(memory, "word_width");

    // Routing

    auto routing = child_required(config, "routing");
    
    routingMode = read_required<std::string>(routing, "type") == "H-tree" ? h_tree : non_h_tree;

    if (routingMode == non_h_tree) throw std::runtime_error("[Input] Error: non H-tree is under development!");
    
    // Peripherals

    auto peripherals = child_required(config, "peripherals");

    withWriteDriver = read_required<bool>(peripherals, "write_driver");
    
    auto pinput = child_required(peripherals, "input");
    
    withInputBuffer = read_required<bool>(pinput, "buffer");
    withInputEnc    = read_required<bool>(pinput, "encoder");
    customInputEnc  = read_required<bool>(pinput, "custom_encoder");

    auto poutput = child_required(peripherals, "output");
    
    withOutputBuffer = read_required<bool>(poutput, "buffer");
    withPriorityEnc  = read_required<bool>(poutput, "priority_encoder");
    withOutputAcc    = read_required<bool>(poutput, "accumulator");

    if (!withOutputAcc) minBitSerialWidth = maxBitSerialWidth;

    // Sensing
    
    auto sensing = child_required(config, "sensing");

    internalSensing = read_required<bool>(sensing, "internal");
    customSenseAmp  = read_required<bool>(sensing, "custom_sense_amp");
    
    tmp = config["sensing"]["amplifier_type"].as<std::string>();

    if      (tmp == "nvsim_cur")  typeSenseAmp = nvsim_current_sense;
    else if (tmp == "self_clock") typeSenseAmp = self_clock_sense;
    else if (tmp == "dual_the")   typeSenseAmp = dual_threshold_sense;
    else if (tmp == "nvsim_vol")  typeSenseAmp = nvsim_voltage_sense;
    else if (tmp == "discharge")  typeSenseAmp = discharge;
    else                          throw std::runtime_error("Sense Amp Type not supported.");

    // Optimization
    
    tmp = config["optimization"]["target"].as<std::string>();

    if      (tmp == "ReadLatency")        optimizationTarget = read_latency_optimized;
    else if (tmp == "WriteLatency")       optimizationTarget = write_latency_optimized;
    else if (tmp == "ReadDynamicEnergy")  optimizationTarget = read_energy_optimized;
    else if (tmp == "WriteDynamicEnergy") optimizationTarget = write_energy_optimized;
    else if (tmp == "ReadEDP")            optimizationTarget = read_edp_optimized;
    else if (tmp == "WriteEDP")           optimizationTarget = write_edp_optimized;
    else if (tmp == "LeakagePower")       optimizationTarget = leakage_optimized;
    else if (tmp == "Area")               optimizationTarget = area_optimized;
    else if (tmp == "SearchLatency")      optimizationTarget = search_latency_optimized;
    else if (tmp == "SearchEnergy")       optimizationTarget = search_energy_optimized;
    else if (tmp == "SearchEDP")          optimizationTarget = search_edp_optimized;
    else                                  optimizationTarget = full_exploration;

    tmp = config["optimization"]["buffer_design"].as<std::string>();

    if      (tmp == "latency") minAreaOptimizationLevel = maxAreaOptimizationLevel = 0;
    else if (tmp == "area")    minAreaOptimizationLevel = maxAreaOptimizationLevel = 2;
    else                       minAreaOptimizationLevel = maxAreaOptimizationLevel = 1;

    tmp = config["optimization"]["row_driver"].as<std::string>();

    if      (tmp == "latency") minRowDriverOptLevel = maxRowDriverOptLevel = 0;
    else if (tmp == "area")    minRowDriverOptLevel = maxRowDriverOptLevel = 2;
    else                       minRowDriverOptLevel = maxRowDriverOptLevel = 1;

    tmp = config["optimization"]["row_driver"].as<std::string>();

    if      (tmp == "latency") minPriorityOptLevel = maxPriorityOptLevel = 0;
    else if (tmp == "area")    minPriorityOptLevel = maxPriorityOptLevel = 2;
    else                       minPriorityOptLevel = maxPriorityOptLevel = 1;
          	
    // Wires
    
    tmp = config["wires"]["local"]["type"].as<std::string>();
    
    if      (tmp == "LocalAggressive")    minLocalWireType = maxLocalWireType = local_aggressive;
    else if (tmp == "LocalConservative")  minLocalWireType = maxLocalWireType = local_conservative;
    else if (tmp == "SemiAggressive")     minLocalWireType = maxLocalWireType = semi_aggressive;
    else if (tmp == "SemiConservative")   minLocalWireType = maxLocalWireType = semi_conservative;
    else if (tmp == "GlobalAggressive")   minLocalWireType = maxLocalWireType = global_aggressive;
    else if (tmp == "GlobalConservative") minLocalWireType = maxLocalWireType = global_conservative;
    else	                          minLocalWireType = maxLocalWireType = dram_wordline; // not supported yet

    tmp = config["wires"]["local"]["repeater"].as<std::string>();

    if      (tmp == "RepeatedOpt")         minLocalWireRepeaterType = maxLocalWireRepeaterType = repeated_opt;
    else if (tmp == "Repeated5%Penalty")   minLocalWireRepeaterType = maxLocalWireRepeaterType = repeated_5;
    else if (tmp == "Repeated10%Penalty")  minLocalWireRepeaterType = maxLocalWireRepeaterType = repeated_10;
    else if (tmp == "Repeated20%Penalty")  minLocalWireRepeaterType = maxLocalWireRepeaterType = repeated_20;
    else if (tmp == "Repeated30%Penalty")  minLocalWireRepeaterType = maxLocalWireRepeaterType = repeated_30;
    else if (tmp == "Repeated40%Penalty")  minLocalWireRepeaterType = maxLocalWireRepeaterType = repeated_40;
    else if (tmp == "Repeated50%Penalty")  minLocalWireRepeaterType = maxLocalWireRepeaterType = repeated_50;
    else                                   minLocalWireRepeaterType = maxLocalWireRepeaterType = repeated_none;

    minIsLocalWireLowSwing = maxIsLocalWireLowSwing = config["wires"]["local"]["low_swing"].as<bool>();

    tmp = config["wires"]["global"]["type"].as<std::string>();
    
    if      (tmp == "LocalAggressive")    minGlobalWireType = maxGlobalWireType = local_aggressive;
    else if (tmp == "LocalConservative")  minGlobalWireType = maxGlobalWireType = local_conservative;
    else if (tmp == "SemiAggressive")     minGlobalWireType = maxGlobalWireType = semi_aggressive;
    else if (tmp == "SemiConservative")   minGlobalWireType = maxGlobalWireType = semi_conservative;
    else if (tmp == "GlobalAggressive")   minGlobalWireType = maxGlobalWireType = global_aggressive;
    else if (tmp == "GlobalConservative") minGlobalWireType = maxGlobalWireType = global_conservative;
    else	                          minGlobalWireType = maxGlobalWireType = dram_wordline; // not supported yet

    tmp = config["wires"]["global"]["repeater"].as<std::string>();
    if      (tmp == "RepeatedOpt")         minGlobalWireRepeaterType = maxGlobalWireRepeaterType = repeated_opt;
    else if (tmp == "Repeated5%Penalty")   minGlobalWireRepeaterType = maxGlobalWireRepeaterType = repeated_5;
    else if (tmp == "Repeated10%Penalty")  minGlobalWireRepeaterType = maxGlobalWireRepeaterType = repeated_10;
    else if (tmp == "Repeated20%Penalty")  minGlobalWireRepeaterType = maxGlobalWireRepeaterType = repeated_20;
    else if (tmp == "Repeated30%Penalty")  minGlobalWireRepeaterType = maxGlobalWireRepeaterType = repeated_30;
    else if (tmp == "Repeated40%Penalty")  minGlobalWireRepeaterType = maxGlobalWireRepeaterType = repeated_40;
    else if (tmp == "Repeated50%Penalty")  minGlobalWireRepeaterType = maxGlobalWireRepeaterType = repeated_50;
    else                                   minGlobalWireRepeaterType = maxGlobalWireRepeaterType = repeated_none;

    minIsGlobalWireLowSwing = maxIsGlobalWireLowSwing = config["wires"]["global"]["low_swing"].as<bool>();

    // Array

    minNumRowMat             = maxNumRowMat             = config["array"]["banks"]["total"] [0].as<int>();
    minNumColumnMat          = maxNumColumnMat          = config["array"]["banks"]["total"] [1].as<int>();
    minNumActiveMatPerColumn = maxNumActiveMatPerColumn = config["array"]["banks"]["active"][0].as<int>();
    minNumActiveMatPerRow    = maxNumActiveMatPerRow    = config["array"]["banks"]["active"][1].as<int>();
     
    minNumRowSubarray = maxNumRowSubarray = config["array"]["mats"]["total"][0].as<int>();
    minNumColumnSubarray = maxNumColumnSubarray = config["array"]["mats"]["total"][1].as<int>();
    minNumActiveSubarrayPerColumn = maxNumActiveSubarrayPerColumn = config["array"]["mats"]["active"][0].as<int>();
    minNumActiveSubarrayPerRow = maxNumActiveSubarrayPerRow = config["array"]["mats"]["active"][1].as<int>();

    maxMuxSenseAmp = minMuxSenseAmp = config["array"]["mux"]["sense_amp"].as<int>();

    maxMuxOutputLev1 = minMuxOutputLev1 = config["array"]["mux"]["output_level1"].as<int>();

    maxMuxOutputLev2 = minMuxOutputLev2 = config["array"]["mux"]["output_level2"].as<int>();

    // Matchline
    
    AddCapOnML = config["matchline"]["additional_cap"].as<int>() / 1e15;

/*
		} else if (starts_with(line, "-WorstCaseSenseMargin")) {
                        sensemargin = parse_double_after_colon(line);
			MatchlineSenseMargin = sensemargin;
		
                                               optimizationTarget = full_exploration;

		} else if (starts_with(line, "-OutputFilePrefix")) {
			outputFilePrefix = parse_string_after_colon(line);
	
		} else if (starts_with(line, "-Associativity")) {
                        associativity = parse_int_after_colon(line);

		} else if (starts_with(line, "-MaxDriverCurrent")) {
                        maxDriverCurrent = parse_double_after_colon(line);

		} else if (starts_with(line, "-WriteScheme")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "SetBeforeReset")
				writeScheme = set_before_reset;
			else if (tmp == "ResetBeforeSet")
				writeScheme = reset_before_set;
			else if (tmp == "EraseBeforeSet")
				writeScheme = erase_before_set;
			else if (tmp == "EraseBeforeReset")
				writeScheme = erase_before_reset;
			else if (tmp == "WriteAndVerify")
				writeScheme = write_and_verify;
			else
				writeScheme = normal_write;

		} else if (starts_with(line, "-CacheAccessMode")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Sequential")
				cacheAccessMode = sequential_access_mode;
			else if (tmp == "Fast")
				cacheAccessMode = fast_access_mode;
			else
				cacheAccessMode = normal_access_mode;

			} else if (starts_with(line, "-MaxNmosSize")) {
                        maxNmosSize = parse_double_after_colon(line);

			} else if (starts_with(line, "-UseCactiAssumption")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes") {
				useCactiAssumption = true;
				minNumActiveMatPerRow = maxNumColumnMat;
				maxNumActiveMatPerRow = maxNumColumnMat;
				minNumActiveMatPerColumn = 1;
				maxNumActiveMatPerColumn = 1;
				minNumRowSubarray = 2;
				maxNumRowSubarray = 2;
				minNumColumnSubarray = 2;
				maxNumColumnSubarray = 2;
				minNumActiveSubarrayPerRow = 2;
				maxNumActiveSubarrayPerRow = 2;
				minNumActiveSubarrayPerColumn = 2;
				maxNumActiveSubarrayPerColumn = 2;
			} else
				useCactiAssumption = false;
		
                } else if (starts_with(line, "-EnablePruning")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				isPruningEnabled = true;
			else
				isPruningEnabled = false;

		} else if (starts_with(line, "-FlashPageSize")) {
                        pageSize = parse_long_after_colon(line) * 8;

		} else if (starts_with(line, "-FlashBlockSize")) {
                        flashBlockSize = parse_long_after_colon(line) * 8 * 1024;
	
                } else if (starts_with(line, "-ApplyReadLatencyConstraint")) {
                        readLatencyConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyWriteLatencyConstraint")) {
                        writeLatencyConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyReadDynamicEnergyConstraint")) {
                        readDynamicEnergyConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyWriteDynamicEnergyConstraint")) {
                        writeDynamicEnergyConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyLeakageConstraint")) {
                        leakageConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyAreaConstraint")) {
                        areaConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyReadEdpConstraint")) {
                        readEdpConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyWriteEdpConstraint")) {
                        writeEdpConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;
		}
                } else if (starts_with(line, "-InputEncType")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "encoding_two_bit") {
				typeInputEnc = encoding_two_bit;
			} else {
                                throw std::runtime_error("Input Encoding not supported.");
		
		} else if (starts_with(line, "-ExcludePrecharngeLatency")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				NoPrechargeInc = true;
			else
				NoPrechargeInc = false;

		} else if (starts_with(line, "-IncludeLeakage")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				IncludeLeakge = true;
			else
				IncludeLeakge = false;
		//////////////////Design for Approximate Match/////////////////////////////
                ///////////////////////////////////////////////////////////////////////////////////////////////////
                
                } else if (starts_with(line, "-ScaledVoltage")) {
                        scaledVoltage = parse_double_after_colon(line);

		} else if (starts_with(line, "-RealCapacity (B)")) {
                        realCapacity = parse_long_after_colon(line);

		} else if (starts_with(line, "-RealCapacity (KB)")) {
                        realCapacity = parse_long_after_colon(line) * 1024;

		} else if (starts_with(line, "-RealCapacity (MB)")) {
                        realCapacity = parse_long_after_colon(line) * 1024 * 1024;

		} else if (starts_with(line, "-BitSerialWidth")) {
                        maxBitSerialWidth = minBitSerialWidth = parse_int_after_colon(line);
                        
		} else if (starts_with(line, "-CustomSAInputFile")) {
                        tmp = parse_string_after_colon(line);
			fileCustomSA = std::string(tmp);

		} else if (starts_with(line, "-UseUpdatedLib")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				UseUpdatedLib = true;
			else
				UseUpdatedLib = false;
		}
        }
*/
		


		
/*
    std::ifstream inFile(inputFile);

        if (!inFile) throw std::runtime_error("Could not open configuration file.");

        std::string line, tmp;

        while (std::getline(inFile, line)) {
                    
                // Skip comments
                if (starts_with(line, "//") || starts_with(line, "#")) continue;

                if (starts_with(line, "-DesignTarget")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "cache") {
			        throw std::runtime_error("[Error]: Only support CAM design.");
			}	
			else if (tmp == "RAM") {
				designTarget = RAM_chip;
				throw std::runtime_error("[Error]: Only support CAM design.");
			} else {
				designTarget = CAM_chip;
				minNumRowPerSet = 1;
				maxNumRowPerSet = 256;
			}

		} else if (starts_with(line, "-SearchFunction")) {
                        tmp = parse_string_after_colon(line);
			if      (tmp == "BE") searchFunction = BE;
			else if (tmp == "TH") searchFunction = TH;
			else                  searchFunction = EX;

		} else if (starts_with(line, "-WorstCaseSenseMargin")) {
                        sensemargin = parse_double_after_colon(line);
			MatchlineSenseMargin = sensemargin;

		} else if (starts_with(line, "-OptimizationTarget")) {
                        tmp = parse_string_after_colon(line);
			if      (tmp == "ReadLatency")        optimizationTarget = read_latency_optimized;
			else if (tmp == "WriteLatency")       optimizationTarget = write_latency_optimized;
			else if (tmp == "ReadDynamicEnergy")  optimizationTarget = read_energy_optimized;
			else if (tmp == "WriteDynamicEnergy") optimizationTarget = write_energy_optimized;
			else if (tmp == "ReadEDP")            optimizationTarget = read_edp_optimized;
			else if (tmp == "WriteEDP")           optimizationTarget = write_edp_optimized;
			else if (tmp == "LeakagePower")       optimizationTarget = leakage_optimized;
			else if (tmp == "Area")               optimizationTarget = area_optimized;
			else if (tmp == "SearchLatency")      optimizationTarget = search_latency_optimized;
			else if (tmp == "SearchEnergy")       optimizationTarget = search_energy_optimized;
			else if (tmp == "SearchEDP")          optimizationTarget = search_edp_optimized;
			else                                  optimizationTarget = full_exploration;

		} else if (starts_with(line, "-OutputFilePrefix")) {
			outputFilePrefix = parse_string_after_colon(line);

		} else if (starts_with(line, "-ProcessNode")) {
                        constexpr std::array<int, 9> supportedProcessNodes = {7, 10, 14, 22, 32, 45, 90, 120, 200};
                        processNode = parse_int_after_colon(line);
                        if (!std::binary_search(supportedProcessNodes.begin(), supportedProcessNodes.end(), processNode)) {
                                PRINT_VERBOSE( "[Warning]: Possible error as " << processNode 
                                    << "nm proccess node is not supported.");
                        }
			if (processNode <= 45) {
                            std::string ptm = "[Warning]: When using PTM model, ";
				PRINT_VERBOSE(ptm << "only HP/LSTP model is provided.");
				PRINT_VERBOSE(ptm << "interpolation between 45/32/22/14/10/7nm may lead to error.");
			}

		} else if (starts_with(line, "-Capacity (B)")) {
                        capacity = parse_long_after_colon(line);

		} else if (starts_with(line, "-Capacity (KB)")) {
			capacity = parse_long_after_colon(line) * 1024;

		} else if (starts_with(line, "-Capacity (MB)")) {
			capacity = parse_long_after_colon(line) * 1024 * 1024;

		} else if (starts_with(line, "-WordWidth")) {
                        wordWidth = parse_long_after_colon(line);
			maxBitSerialWidth = wordWidth;

		} else if (starts_with(line, "-Associativity")) {
                        associativity = parse_int_after_colon(line);

		} else if (starts_with(line, "-Temperature")) {
                        temperature = parse_int_after_colon(line);

		} else if (starts_with(line, "-MaxDriverCurrent")) {
                        maxDriverCurrent = parse_double_after_colon(line);

		} else if (starts_with(line, "-DeviceRoadmap")) {
                        tmp = parse_string_after_colon(line);
			if      (tmp == "HP")   deviceRoadmap = HP;
			else if (tmp == "LSTP") deviceRoadmap = LSTP;
			else                    deviceRoadmap = LOP;

		} else if (starts_with(line, "-WriteScheme")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "SetBeforeReset")
				writeScheme = set_before_reset;
			else if (tmp == "ResetBeforeSet")
				writeScheme = reset_before_set;
			else if (tmp == "EraseBeforeSet")
				writeScheme = erase_before_set;
			else if (tmp == "EraseBeforeReset")
				writeScheme = erase_before_reset;
			else if (tmp == "WriteAndVerify")
				writeScheme = write_and_verify;
			else
				writeScheme = normal_write;

		} else if (starts_with(line, "-CacheAccessMode")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Sequential")
				cacheAccessMode = sequential_access_mode;
			else if (tmp == "Fast")
				cacheAccessMode = fast_access_mode;
			else
				cacheAccessMode = normal_access_mode;

		} else if (starts_with(line, "-LocalWireType")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "LocalAggressive") {
				minLocalWireType = local_aggressive;
				maxLocalWireType = local_aggressive;
			} else if (tmp == "LocalConservative") {
				minLocalWireType = local_conservative;
				maxLocalWireType = local_conservative;
			} else if (tmp == "SemiAggressive") {
				minLocalWireType = semi_aggressive;
				maxLocalWireType = semi_aggressive;
			} else if (tmp == "SemiConservative") {
				minLocalWireType = semi_conservative;
				maxLocalWireType = semi_conservative;
			} else if (tmp == "GlobalAggressive") {
				minLocalWireType = global_aggressive;
				maxLocalWireType = global_aggressive;
			} else if (tmp == "GlobalConservative") {
				minLocalWireType = global_conservative;
				maxLocalWireType = global_conservative;
			} else {	// not supported yet
				minLocalWireType = dram_wordline;
				maxLocalWireType = dram_wordline;
			}

		} else if (starts_with(line, "-LocalWireRepeaterType")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "RepeatedOpt") {
				minLocalWireRepeaterType = repeated_opt;
				maxLocalWireRepeaterType = repeated_opt;
			} else if (tmp == "Repeated5%Penalty") {
				minLocalWireRepeaterType = repeated_5;
				maxLocalWireRepeaterType = repeated_5;
			} else if (tmp == "Repeated10%Penalty") {
				minLocalWireRepeaterType = repeated_10;
				maxLocalWireRepeaterType = repeated_10;
			} else if (tmp == "Repeated20%Penalty") {
				minLocalWireRepeaterType = repeated_20;
				maxLocalWireRepeaterType = repeated_20;
			} else if (tmp == "Repeated30%Penalty") {
				minLocalWireRepeaterType = repeated_30;
				maxLocalWireRepeaterType = repeated_30;
			} else if (tmp == "Repeated40%Penalty") {
				minLocalWireRepeaterType = repeated_40;
				maxLocalWireRepeaterType = repeated_40;
			} else if (tmp == "Repeated50%Penalty") {
				minLocalWireRepeaterType = repeated_50;
				maxLocalWireRepeaterType = repeated_50;
			} else {
				minLocalWireRepeaterType = repeated_none;
				maxLocalWireRepeaterType = repeated_none;
			}

		} else if (starts_with(line, "-LocalWireUseLowSwing")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes") {
				minIsLocalWireLowSwing = 1;
				maxIsLocalWireLowSwing = 1;
			} else {
				minIsLocalWireLowSwing = 0;
				maxIsLocalWireLowSwing = 0;
			}

		} else if (starts_with(line, "-GlobalWireType")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "LocalAggressive") {
				minGlobalWireType = local_aggressive;
				maxGlobalWireType = local_aggressive;
			} else if (tmp == "LocalConservative") {
				minGlobalWireType = local_conservative;
				maxGlobalWireType = local_conservative;
			} else if (tmp == "SemiAggressive") {
				minGlobalWireType = semi_aggressive;
				maxGlobalWireType = semi_aggressive;
			} else if (tmp == "SemiConservative") {
				minGlobalWireType = semi_conservative;
				maxGlobalWireType = semi_conservative;
			} else if (tmp == "GlobalAggressive") {
				minGlobalWireType = global_aggressive;
				maxGlobalWireType = global_aggressive;
			} else if (tmp == "GlobalConservative") {
				minGlobalWireType = global_conservative;
				maxGlobalWireType = global_conservative;
			} else {	// not supported yet
				minGlobalWireType = dram_wordline;
				maxGlobalWireType = dram_wordline;
			}

                } else if (starts_with(line, "-GlobalWireRepeaterType")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "RepeatedOpt") {
				minGlobalWireRepeaterType = repeated_opt;
				maxGlobalWireRepeaterType = repeated_opt;
			} else if (tmp == "Repeated5%Penalty") {
				minGlobalWireRepeaterType = repeated_5;
				maxGlobalWireRepeaterType = repeated_5;
			} else if (tmp == "Repeated10%Penalty") {
				minGlobalWireRepeaterType = repeated_10;
				maxGlobalWireRepeaterType = repeated_10;
			} else if (tmp == "Repeated20%Penalty") {
				minGlobalWireRepeaterType = repeated_20;
				maxGlobalWireRepeaterType = repeated_20;
			} else if (tmp == "Repeated30%Penalty") {
				minGlobalWireRepeaterType = repeated_30;
				maxGlobalWireRepeaterType = repeated_30;
			} else if (tmp == "Repeated40%Penalty") {
				minGlobalWireRepeaterType = repeated_40;
				maxGlobalWireRepeaterType = repeated_40;
			} else if (tmp == "Repeated50%Penalty") {
				minGlobalWireRepeaterType = repeated_50;
				maxGlobalWireRepeaterType = repeated_50;
			} else {
				minGlobalWireRepeaterType = repeated_none;
				maxGlobalWireRepeaterType = repeated_none;
			}

		} else if (starts_with(line, "-GlobalWireUseLowSwing")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes") {
				minIsGlobalWireLowSwing = 1;
				maxIsGlobalWireLowSwing = 1;
			} else {
				minIsGlobalWireLowSwing = 0;
				maxIsGlobalWireLowSwing = 0;
			}

		} else if (starts_with(line, "-Routing")) {
                        tmp = parse_string_after_colon(line);
                        if (tmp == "H-tree")
				routingMode = h_tree;
			else {
				throw std::runtime_error("[Input] Error: non H-tree is under development!");
				routingMode = non_h_tree;
			}

		} else if (starts_with(line, "-InternalSensing")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "true")
				internalSensing = true;
			else
				internalSensing = false;

		} else if (starts_with(line, "-MemoryCellInputFile")) {
                        tmp = parse_string_after_colon(line);
			fileMemCell = std::string(tmp);

		} else if (starts_with(line, "-MaxNmosSize")) {
                        maxNmosSize = parse_double_after_colon(line);

		} else if (starts_with(line, "-ForceBank")) {
                        tmp = parse_string_after_colon(line);
                        replace(tmp.begin(), tmp.end(), 'x', ' ');
                        replace(tmp.begin(), tmp.end(), ',', ' ');
                        std::istringstream tmp_stream(tmp);
                        tmp_stream >> minNumRowMat >> minNumColumnMat 
                            >> minNumActiveMatPerColumn >> minNumActiveMatPerRow;
			maxNumRowMat = minNumRowMat;
			maxNumColumnMat = minNumColumnMat;
			maxNumActiveMatPerColumn = minNumActiveMatPerColumn;
			maxNumActiveMatPerRow = minNumActiveMatPerRow;

		} else if (starts_with(line, "-ForceMat")) {
                        tmp = parse_string_after_colon(line);
                        replace(tmp.begin(), tmp.end(), 'x', ' ');
                        replace(tmp.begin(), tmp.end(), ',', ' ');
                        std::istringstream tmp_stream(tmp);
                        tmp_stream >> minNumRowSubarray >> minNumColumnSubarray
                           >>  minNumActiveSubarrayPerColumn >> minNumActiveSubarrayPerRow;
			maxNumRowSubarray = minNumRowSubarray;
			maxNumColumnSubarray = minNumColumnSubarray;
			maxNumActiveSubarrayPerColumn = minNumActiveSubarrayPerColumn;
			maxNumActiveSubarrayPerRow = minNumActiveSubarrayPerRow;

		} else if (starts_with(line, "-ForceMat")) {
                        tmp = parse_string_after_colon(line);
                        replace(tmp.begin(), tmp.end(), 'x', ' ');
                        replace(tmp.begin(), tmp.end(), ',', ' ');
                        std::istringstream tmp_stream(tmp);
                        tmp_stream >> minNumRow >> minNumColumn 
                            >> minNumActiveSubarrayPerColumn >> minNumActiveSubarrayPerRow;
			maxNumRowSubarray = minNumRowSubarray;
			maxNumColumnSubarray = minNumColumnSubarray;
			maxNumActiveSubarrayPerColumn = minNumActiveSubarrayPerColumn;
			maxNumActiveSubarrayPerRow = minNumActiveSubarrayPerRow;

		} else if (starts_with(line, "-ForceMuxSenseAmp")) {
                        maxMuxSenseAmp = minMuxSenseAmp = parse_int_after_colon(line);

		} else if (starts_with(line, "-ForceMuxOutputLev1")) {
			maxMuxOutputLev1 = minMuxOutputLev1 = parse_int_after_colon(line);

		} else if (starts_with(line, "-ForceMuxOutputLev2")) {
			maxMuxOutputLev2 = minMuxOutputLev2 = parse_int_after_colon(line);

		} else if (starts_with(line, "-UseCactiAssumption")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes") {
				useCactiAssumption = true;
				minNumActiveMatPerRow = maxNumColumnMat;
				maxNumActiveMatPerRow = maxNumColumnMat;
				minNumActiveMatPerColumn = 1;
				maxNumActiveMatPerColumn = 1;
				minNumRowSubarray = 2;
				maxNumRowSubarray = 2;
				minNumColumnSubarray = 2;
				maxNumColumnSubarray = 2;
				minNumActiveSubarrayPerRow = 2;
				maxNumActiveSubarrayPerRow = 2;
				minNumActiveSubarrayPerColumn = 2;
				maxNumActiveSubarrayPerColumn = 2;
			} else
				useCactiAssumption = false;
		
                } else if (starts_with(line, "-EnablePruning")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				isPruningEnabled = true;
			else
				isPruningEnabled = false;

		} else if (starts_with(line, "-BufferDesignOptimization")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "latency") {
				minAreaOptimizationLevel = 0;
				maxAreaOptimizationLevel = 0;
			} else if (tmp == "area") {
				minAreaOptimizationLevel = 2;
				maxAreaOptimizationLevel = 2;
			} else {
				minAreaOptimizationLevel = 1;
				maxAreaOptimizationLevel = 1;
			}

		} else if (starts_with(line, "-FlashPageSize")) {
                        pageSize = parse_long_after_colon(line) * 8;

		} else if (starts_with(line, "-FlashBlockSize")) {
                        flashBlockSize = parse_long_after_colon(line) * 8 * 1024;
		
                } else if (starts_with(line, "-ApplyReadLatencyConstraint")) {
                        readLatencyConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyWriteLatencyConstraint")) {
                        writeLatencyConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyReadDynamicEnergyConstraint")) {
                        readDynamicEnergyConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyWriteDynamicEnergyConstraint")) {
                        writeDynamicEnergyConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyLeakageConstraint")) {
                        leakageConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyAreaConstraint")) {
                        areaConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyReadEdpConstraint")) {
                        readEdpConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;

		} else if (starts_with(line, "-ApplyWriteEdpConstraint")) {
                        writeEdpConstraint = parse_double_after_colon(line);
			isConstraintApplied = true;
		}

		//////////////////Design for Approximate Match/////////////////////////////
                else if (starts_with(line, "-AdditionalCapOnML (fF)")) {
                        AddCapOnML = parse_double_after_colon(line) / 1e15;

		}
///////////////////////////////////////////////////////////////////////////////////////////////////
                else if (starts_with(line, "-RowDriverOptimization")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "latency") {
				minRowDriverOptLevel = 0;
				maxRowDriverOptLevel = 0;
			} else if (tmp == "area") {
				minRowDriverOptLevel = 2;
				maxRowDriverOptLevel = 2;
			} else {
				minRowDriverOptLevel = 1;
				maxRowDriverOptLevel = 1;
			}

		} else if (starts_with(line, "-PriorityEncOptimization")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "latency") {
				minPriorityOptLevel = 0;
				maxPriorityOptLevel = 0;
			} else if (tmp == "area") {
				minPriorityOptLevel = 2;
				maxPriorityOptLevel = 2;
			} else {
				minPriorityOptLevel = 1;
				maxPriorityOptLevel = 1;
			}

		} else if (starts_with(line, "-WithInputEnc")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				withInputEnc = true;
			else
				withInputEnc = false;

		} else if (starts_with(line, "-WithWriteDriver")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				withWriteDriver = true;
			else
				withWriteDriver = false;

		} else if (starts_with(line, "-InputEncType")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "encoding_two_bit") {
				typeInputEnc = encoding_two_bit;
			} else {
                                throw std::runtime_error("Input Encoding not supported.");
			}

		} else if (starts_with(line, "-CustomInputEnc")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				customInputEnc = true;
			else
				customInputEnc = false;

		} else if (starts_with(line, "-TypeSenseAmp")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "nvsim_cur") {
				typeSenseAmp = nvsim_current_sense;
			} else if (tmp == "self_clock") {
				typeSenseAmp = self_clock_sense;
			} else if (tmp == "dual_the") {
				typeSenseAmp = dual_threshold_sense;
			} else if (tmp == "nvsim_vol") {
				typeSenseAmp = nvsim_voltage_sense;
			} else if (tmp == "discharge") {
				typeSenseAmp = discharge;
			} else {
                                throw std::runtime_error("Sense Amp Type not supported.");
			}

		} else if (starts_with(line, "-CustomSenseAmp")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				customSenseAmp = true;
			else
				customSenseAmp = false;

		} else if (starts_with(line, "-WithOutputAcc")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				withOutputAcc = true;
			else {
				withOutputAcc = false;
				minBitSerialWidth = maxBitSerialWidth;
			}

		} else if (starts_with(line, "-WithPriorityEnc")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				withPriorityEnc = true;
			else
				withPriorityEnc = false;

		} else if (starts_with(line, "-WithInputBuffer")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				withInputBuffer = true;
			else
				withInputBuffer = false;

		} else if (starts_with(line, "-WithOutputBuffer")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				withOutputBuffer = true;
			else
				withOutputBuffer = false;

		} else if (starts_with(line, "-ExcludePrecharngeLatency")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				NoPrechargeInc = true;
			else
				NoPrechargeInc = false;

		} else if (starts_with(line, "-IncludeLeakage")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				IncludeLeakge = true;
			else
				IncludeLeakge = false;

		} else if (starts_with(line, "-ScaledVoltage")) {
                        scaledVoltage = parse_double_after_colon(line);

		} else if (starts_with(line, "-RealCapacity (B)")) {
                        realCapacity = parse_long_after_colon(line);

		} else if (starts_with(line, "-RealCapacity (KB)")) {
                        realCapacity = parse_long_after_colon(line) * 1024;

		} else if (starts_with(line, "-RealCapacity (MB)")) {
                        realCapacity = parse_long_after_colon(line) * 1024 * 1024;

		} else if (starts_with(line, "-BitSerialWidth")) {
                        maxBitSerialWidth = minBitSerialWidth = parse_int_after_colon(line);
                        
		} else if (starts_with(line, "-CustomSAInputFile")) {
                        tmp = parse_string_after_colon(line);
			fileCustomSA = std::string(tmp);

		} else if (starts_with(line, "-UseUpdatedLib")) {
                        tmp = parse_string_after_colon(line);
			if (tmp == "Yes")
				UseUpdatedLib = true;
			else
				UseUpdatedLib = false;
		}
        }
*/
        // C style reading, better to use C++ style reading as this is C++
        /*
	FILE *fp = fopen(inputFile.c_str(), "r");
	char line[5000];
	char tmp[5000];

	if (!fp) {
		std::cout << inputFile << " cannot be found!\n";
                throw std::runtime_error("Could not open file");
	}

	while (fscanf(fp, "%[^\n]\n", line) != EOF) {
		if (!strncmp("-DesignTarget", line, strlen("-DesignTarget"))) {
			sscanf(line, "-DesignTarget: %s", tmp);
			if (!strcmp(tmp, "cache")){
				std::cout << "[Error]: Only support CAM design." << std::endl;
				exit(-1);
			}	
			else if (!strcmp(tmp, "RAM")) {
				designTarget = RAM_chip;
				std::cout << "[Error]: Only support CAM design." << std::endl;
				exit(-1);
			} else {
				designTarget = CAM_chip;
				minNumRowPerSet = 1;
				maxNumRowPerSet = 256;
			}
			continue;
		}
		if (!strncmp("-SearchFunction", line, strlen("-SearchFunction"))) {
			sscanf(line, "-SearchFunction: %s", tmp);
			if (!strcmp(tmp, "BE")){
				searchFunction = BE;
			} else if (!strcmp(tmp, "TH")){
				searchFunction = TH;
			} else {
				searchFunction = EX;
			}
			continue;
		}
		if (!strncmp("-WorstCaseSenseMargin", line, strlen("-WorstCaseSenseMargin"))) {
			sscanf(line, "-WorstCaseSenseMargin: %lf", &sensemargin);
			MatchlineSenseMargin = sensemargin;
			continue;
		}

		if (!strncmp("-OptimizationTarget", line, strlen("-OptimizationTarget"))) {
			sscanf(line, "-OptimizationTarget: %s", tmp);
			if (!strcmp(tmp, "ReadLatency"))
				optimizationTarget = read_latency_optimized;
			else if (!strcmp(tmp, "WriteLatency"))
				optimizationTarget = write_latency_optimized;
			else if (!strcmp(tmp, "ReadDynamicEnergy"))
				optimizationTarget = read_energy_optimized;
			else if (!strcmp(tmp, "WriteDynamicEnergy"))
				optimizationTarget = write_energy_optimized;
			else if (!strcmp(tmp, "ReadEDP"))
				optimizationTarget = read_edp_optimized;
			else if (!strcmp(tmp, "WriteEDP"))
				optimizationTarget = write_edp_optimized;
			else if (!strcmp(tmp, "LeakagePower"))
				optimizationTarget = leakage_optimized;
			else if (!strcmp(tmp, "Area"))
				optimizationTarget = area_optimized;
			else if (!strcmp(tmp, "SearchLatency"))
				optimizationTarget = search_latency_optimized;
			else if (!strcmp(tmp, "SearchEnergy"))
				optimizationTarget = search_energy_optimized;
			else if (!strcmp(tmp, "SearchEDP"))
				optimizationTarget = search_edp_optimized;
			else
				optimizationTarget = full_exploration;
			continue;
		}

		if (!strncmp("-OutputFilePrefix", line, strlen("-OutputFilePrefix"))) {
			sscanf(line, "-OutputFilePrefix: %s", tmp);
			outputFilePrefix = (string)tmp;
			continue;
		}

		if (!strncmp("-ProcessNode", line, strlen("-ProcessNode"))) {
                        constexpr array<int, 9> supportedProcessNodes = {7, 10, 14, 22, 32, 45, 90, 120, 200};
			sscanf(line, "-ProcessNode: %d", &processNode);
                        if (!binary_search(supportedProcessNodes.begin(), supportedProcessNodes.end(), processNode)) {
                                //throw std::runtime_error("Process node not supported.");
                                std::cout << "[Warning]: Possible error as " << processNode 
                                    << "nm proccess node is not supported." << std::endl;
                        }
			if (processNode <= 45) {
				std::cout<<"[Warning]: When using PTM model, only HP/LSTP model is provided."<<std::endl;
				std::cout<<"[Warning]: When using PTM model, interpolation between 45/32/22/14/10/7nm may lead to error."<<std::endl;
			}
			continue;
		}
		if (!strncmp("-Capacity (B)", line, strlen("-Capacity (B)"))) {
			long cap;
			sscanf(line, "-Capacity (B): %ld", &cap);
			capacity = cap;
			continue;
		}
		if (!strncmp("-Capacity (KB)", line, strlen("-Capacity (KB)"))) {
			long cap;
			sscanf(line, "-Capacity (KB): %ld", &cap);
			capacity = cap * 1024;
			continue;
		}
		if (!strncmp("-Capacity (MB)", line, strlen("-Capacity (MB)"))) {
			long cap;
			sscanf(line, "-Capacity (MB): %ld", &cap);
			capacity = cap * 1024*1024;
			continue;
		}
		if (!strncmp("-WordWidth", line, strlen("-WordWidth"))) {
			sscanf(line, "-WordWidth (bit): %ld", &wordWidth);
			maxBitSerialWidth = wordWidth;
			continue;
		}
		if (!strncmp("-Associativity", line, strlen("-Associativity"))) {
			sscanf(line, "-Associativity (for cache only): %d", &associativity);
			continue;
		}
		if (!strncmp("-Temperature", line, strlen("-Temperature"))) {
			sscanf(line, "-Temperature (K): %d", &temperature);
			continue;
		}
		if (!strncmp("-MaxDriverCurrent", line, strlen("-MaxDriverCurrent"))) {
			sscanf(line, "-MaxDriverCurrent (uA): %lf", &maxDriverCurrent);
			continue;
		}
		if (!strncmp("-DeviceRoadmap", line, strlen("-DeviceRoadmap"))) {
			sscanf(line, "-DeviceRoadmap: %s", tmp);
			if (!strcmp(tmp, "HP"))
				deviceRoadmap = HP;
			else if (!strcmp(tmp, "LSTP"))
				deviceRoadmap = LSTP;
			else
				deviceRoadmap = LOP;
			continue;
		}

		if (!strncmp("-WriteScheme", line, strlen("-WriteScheme"))) {
			sscanf(line, "-WriteScheme: %s", tmp);
			if (!strcmp(tmp, "SetBeforeReset"))
				writeScheme = set_before_reset;
			else if (!strcmp(tmp, "ResetBeforeSet"))
				writeScheme = reset_before_set;
			else if (!strcmp(tmp, "EraseBeforeSet"))
				writeScheme = erase_before_set;
			else if (!strcmp(tmp, "EraseBeforeReset"))
				writeScheme = erase_before_reset;
			else if (!strcmp(tmp, "WriteAndVerify"))
				writeScheme = write_and_verify;
			else
				writeScheme = normal_write;
			continue;
		}

		if (!strncmp("-CacheAccessMode", line, strlen("-CacheAccessMode"))) {
			sscanf(line, "-CacheAccessMode: %s", tmp);
			if (!strcmp(tmp, "Sequential"))
				cacheAccessMode = sequential_access_mode;
			else if (!strcmp(tmp, "Fast"))
				cacheAccessMode = fast_access_mode;
			else
				cacheAccessMode = normal_access_mode;
			continue;
		}

		if (!strncmp("-LocalWireType", line, strlen("-LocalWireType"))) {
			sscanf(line, "-LocalWireType: %s", tmp);
			if (!strcmp(tmp, "LocalAggressive")) {
				minLocalWireType = local_aggressive;
				maxLocalWireType = local_aggressive;
			} else if (!strcmp(tmp, "LocalConservative")) {
				minLocalWireType = local_conservative;
				maxLocalWireType = local_conservative;
			} else if (!strcmp(tmp, "SemiAggressive")) {
				minLocalWireType = semi_aggressive;
				maxLocalWireType = semi_aggressive;
			} else if (!strcmp(tmp, "SemiConservative")) {
				minLocalWireType = semi_conservative;
				maxLocalWireType = semi_conservative;
			} else if (!strcmp(tmp, "GlobalAggressive")) {
				minLocalWireType = global_aggressive;
				maxLocalWireType = global_aggressive;
			} else if (!strcmp(tmp, "GlobalConservative")) {
				minLocalWireType = global_conservative;
				maxLocalWireType = global_conservative;
			} else {	// not supported yet
				minLocalWireType = dram_wordline;
				maxLocalWireType = dram_wordline;
			}
			continue;
		}
		if (!strncmp("-LocalWireRepeaterType", line, strlen("-LocalWireRepeaterType"))) {
			sscanf(line, "-LocalWireRepeaterType: %s", tmp);
			if (!strcmp(tmp, "RepeatedOpt")) {
				minLocalWireRepeaterType = repeated_opt;
				maxLocalWireRepeaterType = repeated_opt;
			} else if (!strcmp(tmp, "Repeated5%Penalty")) {
				minLocalWireRepeaterType = repeated_5;
				maxLocalWireRepeaterType = repeated_5;
			} else if (!strcmp(tmp, "Repeated10%Penalty")) {
				minLocalWireRepeaterType = repeated_10;
				maxLocalWireRepeaterType = repeated_10;
			} else if (!strcmp(tmp, "Repeated20%Penalty")) {
				minLocalWireRepeaterType = repeated_20;
				maxLocalWireRepeaterType = repeated_20;
			} else if (!strcmp(tmp, "Repeated30%Penalty")) {
				minLocalWireRepeaterType = repeated_30;
				maxLocalWireRepeaterType = repeated_30;
			} else if (!strcmp(tmp, "Repeated40%Penalty")) {
				minLocalWireRepeaterType = repeated_40;
				maxLocalWireRepeaterType = repeated_40;
			} else if (!strcmp(tmp, "Repeated50%Penalty")) {
				minLocalWireRepeaterType = repeated_50;
				maxLocalWireRepeaterType = repeated_50;
			} else {
				minLocalWireRepeaterType = repeated_none;
				maxLocalWireRepeaterType = repeated_none;
			}
			continue;
		}
		if (!strncmp("-LocalWireUseLowSwing", line, strlen("-LocalWireUseLowSwing"))) {
			sscanf(line, "-LocalWireUseLowSwing: %s", tmp);
			if (!strcmp(tmp, "Yes")) {
				minIsLocalWireLowSwing = 1;
				maxIsLocalWireLowSwing = 1;
			} else {
				minIsLocalWireLowSwing = 0;
				maxIsLocalWireLowSwing = 0;
			}
			continue;
		}

		if (!strncmp("-GlobalWireType", line, strlen("-GlobalWireType"))) {
			sscanf(line, "-GlobalWireType: %s", tmp);
			if (!strcmp(tmp, "LocalAggressive")) {
				minGlobalWireType = local_aggressive;
				maxGlobalWireType = local_aggressive;
			} else if (!strcmp(tmp, "LocalConservative")) {
				minGlobalWireType = local_conservative;
				maxGlobalWireType = local_conservative;
			} else if (!strcmp(tmp, "SemiAggressive")) {
				minGlobalWireType = semi_aggressive;
				maxGlobalWireType = semi_aggressive;
			} else if (!strcmp(tmp, "SemiConservative")) {
				minGlobalWireType = semi_conservative;
				maxGlobalWireType = semi_conservative;
			} else if (!strcmp(tmp, "GlobalAggressive")) {
				minGlobalWireType = global_aggressive;
				maxGlobalWireType = global_aggressive;
			} else if (!strcmp(tmp, "GlobalConservative")) {
				minGlobalWireType = global_conservative;
				maxGlobalWireType = global_conservative;
			} else {	// not supported yet
				minGlobalWireType = dram_wordline;
				maxGlobalWireType = dram_wordline;
			}
			continue;
		}
		if (!strncmp("-GlobalWireRepeaterType", line, strlen("-GlobalWireRepeaterType"))) {
			sscanf(line, "-GlobalWireRepeaterType: %s", tmp);
			if (!strcmp(tmp, "RepeatedOpt")) {
				minGlobalWireRepeaterType = repeated_opt;
				maxGlobalWireRepeaterType = repeated_opt;
			} else if (!strcmp(tmp, "Repeated5%Penalty")) {
				minGlobalWireRepeaterType = repeated_5;
				maxGlobalWireRepeaterType = repeated_5;
			} else if (!strcmp(tmp, "Repeated10%Penalty")) {
				minGlobalWireRepeaterType = repeated_10;
				maxGlobalWireRepeaterType = repeated_10;
			} else if (!strcmp(tmp, "Repeated20%Penalty")) {
				minGlobalWireRepeaterType = repeated_20;
				maxGlobalWireRepeaterType = repeated_20;
			} else if (!strcmp(tmp, "Repeated30%Penalty")) {
				minGlobalWireRepeaterType = repeated_30;
				maxGlobalWireRepeaterType = repeated_30;
			} else if (!strcmp(tmp, "Repeated40%Penalty")) {
				minGlobalWireRepeaterType = repeated_40;
				maxGlobalWireRepeaterType = repeated_40;
			} else if (!strcmp(tmp, "Repeated50%Penalty")) {
				minGlobalWireRepeaterType = repeated_50;
				maxGlobalWireRepeaterType = repeated_50;
			} else {
				minGlobalWireRepeaterType = repeated_none;
				maxGlobalWireRepeaterType = repeated_none;
			}
			continue;
		}
		if (!strncmp("-GlobalWireUseLowSwing", line, strlen("-GlobalWireUseLowSwing"))) {
			sscanf(line, "-GlobalWireUseLowSwing: %s", tmp);
			if (!strcmp(tmp, "Yes")) {
				minIsGlobalWireLowSwing = 1;
				maxIsGlobalWireLowSwing = 1;
			} else {
				minIsGlobalWireLowSwing = 0;
				maxIsGlobalWireLowSwing = 0;
			}
			continue;
		}

		if (!strncmp("-Routing", line, strlen("-Routing"))) {
			sscanf(line, "-Routing: %s", tmp);
			if (!strcmp(tmp, "H-tree"))
				routingMode = h_tree;
			else {
				std::cout << "[Input} Error: non H-tree is under development!" <<std::endl;
				exit(-1);
				routingMode = non_h_tree;
			}
			continue;
		}

		if (!strncmp("-InternalSensing", line, strlen("-InternalSensing"))) {
			sscanf(line, "-InternalSensing: %s", tmp);
			if (!strcmp(tmp, "true"))
				internalSensing = true;
			else
				internalSensing = false;
			continue;
		}

		if (!strncmp("-MemoryCellInputFile", line, strlen("-MemoryCellInputFile"))) {
			sscanf(line, "-MemoryCellInputFile: %s", tmp);
			fileMemCell = string(tmp);
			continue;
		}

		if (!strncmp("-MaxNmosSize", line, strlen("-MaxNmosSize"))) {
			sscanf(line, "-MaxNmosSize (F): %lf", &maxNmosSize);
			continue;
		}

		if (!strncmp("-ForceBank", line, strlen("-ForceBank"))) {
			sscanf(line, "-ForceBank (Total AxB, Active CxD): %dx%d, %dx%d",
					&minNumRowMat, &minNumColumnMat, &minNumActiveMatPerColumn, &minNumActiveMatPerRow);
			maxNumRowMat = minNumRowMat;
			maxNumColumnMat = minNumColumnMat;
			maxNumActiveMatPerColumn = minNumActiveMatPerColumn;
			maxNumActiveMatPerRow = minNumActiveMatPerRow;
			continue;
		}

		if (!strncmp("-ForceMat", line, strlen("-ForceMat"))) {
			sscanf(line, "-ForceMat (Total AxB, Active CxD): %dx%d, %dx%d",
					&minNumRowSubarray, &minNumColumnSubarray, &minNumActiveSubarrayPerColumn, &minNumActiveSubarrayPerRow);
			maxNumRowSubarray = minNumRowSubarray;
			maxNumColumnSubarray = minNumColumnSubarray;
			maxNumActiveSubarrayPerColumn = minNumActiveSubarrayPerColumn;
			maxNumActiveSubarrayPerRow = minNumActiveSubarrayPerRow;
			continue;
		}

		if (!strncmp("-ForceMat", line, strlen("-ForceMat"))) {
			sscanf(line, "-ForceMat (Total AxB, Active CxD): %dx%d, %dx%d",
					&minNumRow, &minNumColumn, &minNumActiveSubarrayPerColumn, &minNumActiveSubarrayPerRow);
			maxNumRowSubarray = minNumRowSubarray;
			maxNumColumnSubarray = minNumColumnSubarray;
			maxNumActiveSubarrayPerColumn = minNumActiveSubarrayPerColumn;
			maxNumActiveSubarrayPerRow = minNumActiveSubarrayPerRow;
			continue;
		}	

		if (!strncmp("-ForceMuxSenseAmp", line, strlen("-ForceMuxSenseAmp"))) {
			sscanf(line, "-ForceMuxSenseAmp: %d", &minMuxSenseAmp);
			maxMuxSenseAmp = minMuxSenseAmp;
			continue;
		}

		if (!strncmp("-ForceMuxOutputLev1", line, strlen("-ForceMuxOutputLev1"))) {
			sscanf(line, "-ForceMuxOutputLev1: %d", &minMuxOutputLev1);
			maxMuxOutputLev1 = minMuxOutputLev1;
			continue;
		}

		if (!strncmp("-ForceMuxOutputLev2", line, strlen("-ForceMuxOutputLev2"))) {
			sscanf(line, "-ForceMuxOutputLev2: %d", &minMuxOutputLev2);
			maxMuxOutputLev2 = minMuxOutputLev2;
			continue;
		}

		if (!strncmp("-UseCactiAssumption", line, strlen("-UseCactiAssumption"))) {
			sscanf(line, "-UseCactiAssumption: %s", tmp);
			if (!strcmp(tmp, "Yes")) {
				useCactiAssumption = true;
				minNumActiveMatPerRow = maxNumColumnMat;
				maxNumActiveMatPerRow = maxNumColumnMat;
				minNumActiveMatPerColumn = 1;
				maxNumActiveMatPerColumn = 1;
				minNumRowSubarray = 2;
				maxNumRowSubarray = 2;
				minNumColumnSubarray = 2;
				maxNumColumnSubarray = 2;
				minNumActiveSubarrayPerRow = 2;
				maxNumActiveSubarrayPerRow = 2;
				minNumActiveSubarrayPerColumn = 2;
				maxNumActiveSubarrayPerColumn = 2;
			} else
				useCactiAssumption = false;
			continue;
		}

		if (!strncmp("-EnablePruning", line, strlen("-EnablePruning"))) {
			sscanf(line, "-EnablePruning: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				isPruningEnabled = true;
			else
				isPruningEnabled = false;
			continue;
		}

		if (!strncmp("-BufferDesignOptimization", line, strlen("-BufferDesignOptimization"))) {
			sscanf(line, "-BufferDesignOptimization: %s", tmp);
			if (!strcmp(tmp, "latency")) {
				minAreaOptimizationLevel = 0;
				maxAreaOptimizationLevel = 0;
			} else if (!strcmp(tmp, "area")) {
				minAreaOptimizationLevel = 2;
				maxAreaOptimizationLevel = 2;
			} else {
				minAreaOptimizationLevel = 1;
				maxAreaOptimizationLevel = 1;
			}
		}

		if (!strncmp("-FlashPageSize", line, strlen("-FlashPageSize"))) {
			sscanf(line, "-FlashPageSize (Byte): %ld", &pageSize);
			pageSize *= 8;	//  Byte to bit
			continue;
		}

		if (!strncmp("-FlashBlockSize", line, strlen("-FlashBlockSize"))) {
			sscanf(line, "-FlashBlockSize (KB): %ld", &flashBlockSize);
			flashBlockSize *= (8 * 1024);	// KB to bit 
			continue;
		}

		if (!strncmp("-ApplyReadLatencyConstraint", line, strlen("-ApplyReadLatencyConstraint"))) {
			sscanf(line, "-ApplyReadLatencyConstraint: %lf", &readLatencyConstraint);
			isConstraintApplied = true;
			continue;
		}

		if (!strncmp("-ApplyWriteLatencyConstraint", line, strlen("-ApplyWriteLatencyConstraint"))) {
			sscanf(line, "-ApplyWriteLatencyConstraint: %lf", &writeLatencyConstraint);
			isConstraintApplied = true;
			continue;
		}

		if (!strncmp("-ApplyReadDynamicEnergyConstraint", line, strlen("-ApplyReadDynamicEnergyConstraint"))) {
			sscanf(line, "-ApplyReadDynamicEnergyConstraint: %lf", &readDynamicEnergyConstraint);
			isConstraintApplied = true;
			continue;
		}

		if (!strncmp("-ApplyWriteDynamicEnergyConstraint", line, strlen("-ApplyWriteDynamicEnergyConstraint"))) {
			sscanf(line, "-ApplyWriteDynamicEnergyConstraint: %lf", &writeDynamicEnergyConstraint);
			isConstraintApplied = true;
			continue;
		}

		if (!strncmp("-ApplyLeakageConstraint", line, strlen("-ApplyLeakageConstraint"))) {
			sscanf(line, "-ApplyLeakageConstraint: %lf", &leakageConstraint);
			isConstraintApplied = true;
			continue;
		}

		if (!strncmp("-ApplyAreaConstraint", line, strlen("-ApplyAreaConstraint"))) {
			sscanf(line, "-ApplyAreaConstraint: %lf", &areaConstraint);
			isConstraintApplied = true;
			continue;
		}

		if (!strncmp("-ApplyReadEdpConstraint", line, strlen("-ApplyReadEdpConstraint"))) {
			sscanf(line, "-ApplyReadEdpConstraint: %lf", &readEdpConstraint);
			isConstraintApplied = true;
			continue;
		}

		if (!strncmp("-ApplyWriteEdpConstraint", line, strlen("-ApplyWriteEdpConstraint"))) {
			sscanf(line, "-ApplyWriteEdpConstraint: %lf", &writeEdpConstraint);
			isConstraintApplied = true;
			continue;
		}

		//////////////////Design for Approximate Match/////////////////////////////
		if (!strncmp("-AdditionalCapOnML (fF)", line, strlen("-AdditionalCapOnML (fF)"))) {
			sscanf(line, "-AdditionalCapOnML (fF): %lf", &AddCapOnML);
			AddCapOnML /= 1e15;

			continue;
		}
///////////////////////////////////////////////////////////////////////////////////////////////////
		if (!strncmp("-RowDriverOptimization", line, strlen("-RowDriverOptimization"))) {
			sscanf(line, "-RowDriverOptimization: %s", tmp);
			if (!strcmp(tmp, "latency")) {
				minRowDriverOptLevel = 0;
				maxRowDriverOptLevel = 0;
			} else if (!strcmp(tmp, "area")) {
				minRowDriverOptLevel = 2;
				maxRowDriverOptLevel = 2;
			} else {
				minRowDriverOptLevel = 1;
				maxRowDriverOptLevel = 1;
			}
		}
		if (!strncmp("-PriorityEncOptimization", line, strlen("-PriorityEncOptimization"))) {
			sscanf(line, "-PriorityEncOptimization: %s", tmp);
			if (!strcmp(tmp, "latency")) {
				minPriorityOptLevel = 0;
				maxPriorityOptLevel = 0;
			} else if (!strcmp(tmp, "area")) {
				minPriorityOptLevel = 2;
				maxPriorityOptLevel = 2;
			} else {
				minPriorityOptLevel = 1;
				maxPriorityOptLevel = 1;
			}
		}
		if (!strncmp("-WithInputEnc", line, strlen("-WithInputEnc"))) {
			sscanf(line, "-WithInputEnc: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				withInputEnc = true;
			else
				withInputEnc = false;
			continue;
		}
		if (!strncmp("-WithWriteDriver", line, strlen("-WithWriteDriver"))) {
			sscanf(line, "-WithWriteDriver: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				withWriteDriver = true;
			else
				withWriteDriver = false;
			continue;
		}
		if (!strncmp("-InputEncType", line, strlen("-InputEncType"))) {
			sscanf(line, "-InputEncType: %s", tmp);
			if (!strcmp(tmp, "encoding_two_bit")) {
				typeInputEnc = encoding_two_bit;
			} else {

			}
		}
		if (!strncmp("-CustomInputEnc", line, strlen("-CustomInputEnc"))) {
			sscanf(line, "-CustomInputEnc: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				customInputEnc = true;
			else
				customInputEnc = false;
			continue;
		}
		if (!strncmp("-TypeSenseAmp", line, strlen("-TypeSenseAmp"))) {
			sscanf(line, "-TypeSenseAmp: %s", tmp);
			if (!strcmp(tmp, "nvsim_cur")) {
				typeSenseAmp = nvsim_current_sense;
			} else if (!strcmp(tmp, "self_clock")){
				typeSenseAmp = self_clock_sense;
			} else if (!strcmp(tmp, "dual_the")){
				typeSenseAmp = dual_threshold_sense;
			} else if (!strcmp(tmp, "nvsim_vol")){
				typeSenseAmp = nvsim_voltage_sense;
			} else if (!strcmp(tmp, "discharge")){
				typeSenseAmp = discharge;
			} else {

			}
		}
		if (!strncmp("-CustomSenseAmp", line, strlen("-CustomSenseAmp"))) {
			sscanf(line, "-CustomSenseAmp: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				customSenseAmp = true;
			else
				customSenseAmp = false;
			continue;
		}
		if (!strncmp("-WithOutputAcc", line, strlen("-WithOutputAcc"))) {
			sscanf(line, "-WithOutputAcc: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				withOutputAcc = true;
			else {
				withOutputAcc = false;
				minBitSerialWidth = maxBitSerialWidth;
			}
			continue;
		}
		if (!strncmp("-WithPriorityEnc", line, strlen("-WithPriorityEnc"))) {
			sscanf(line, "-WithPriorityEnc: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				withPriorityEnc = true;
			else
				withPriorityEnc = false;
			continue;
		}
		if (!strncmp("-WithInputBuffer", line, strlen("-WithInputBuffer"))) {
			sscanf(line, "-WithInputBuffer: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				withInputBuffer = true;
			else
				withInputBuffer = false;
			continue;
		}
		if (!strncmp("-WithOutputBuffer", line, strlen("-WithOutputBuffer"))) {
			sscanf(line, "-WithOutputBuffer: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				withOutputBuffer = true;
			else
				withOutputBuffer = false;
			continue;
		}
		if (!strncmp("-ExcludePrecharngeLatency", line, strlen("-ExcludePrecharngeLatency"))) {
			sscanf(line, "-ExcludePrecharngeLatency: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				NoPrechargeInc = true;
			else
				NoPrechargeInc = false;
			continue;
		}
		if (!strncmp("-IncludeLeakage", line, strlen("-IncludeLeakage"))) {
			sscanf(line, "-IncludeLeakage: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				IncludeLeakge = true;
			else
				IncludeLeakge = false;
			continue;
		}
		if (!strncmp("-ScaledVoltage", line, strlen("-ScaledVoltage"))) {
			sscanf(line, "-ScaledVoltage: %lf", &scaledVoltage);
			continue;
		}
		if (!strncmp("-RealCapacity (B)", line, strlen("-RealCapacity (B)"))) {
			long cap;
			sscanf(line, "-RealCapacity (B): %ld", &cap);
			realCapacity = cap;
			continue;
		}
		if (!strncmp("-RealCapacity (KB)", line, strlen("-RealCapacity (KB)"))) {
			long cap;
			sscanf(line, "-RealCapacity (KB): %ld", &cap);
			realCapacity = cap * 1024;
			continue;
		}
		if (!strncmp("-RealCapacity (MB)", line, strlen("-RealCapacity (MB)"))) {
			long cap;
			sscanf(line, "-RealCapacity (MB): %ld", &cap);
			realCapacity = cap * 1024*1024;
			continue;
		}
		if (!strncmp("-BitSerialWidth", line, strlen("-BitSerialWidth"))) {
			sscanf(line, "-BitSerialWidth: %d", &minBitSerialWidth);
			maxBitSerialWidth = minBitSerialWidth;
			continue;
		}
		if (!strncmp("-CustomSAInputFile", line, strlen("-CustomSAInputFile"))) {
			sscanf(line, "-CustomSAInputFile: %s", tmp);
			fileCustomSA = string(tmp);
			continue;
		}
		if (!strncmp("-UseUpdatedLib", line, strlen("-UseUpdatedLib"))) {
			sscanf(line, "-UseUpdatedLib: %s", tmp);
			if (!strcmp(tmp, "Yes"))
				UseUpdatedLib = true;
			else
				UseUpdatedLib = false;
			continue;
		}
	}

	fclose(fp);
        */

        TechSetup();
        MemCellSetup();
        FEFETTechSetup();
}

void InputParameter::ApplyConstraint() {
        if (designTarget != cache && associativity > 1) {
                PRINT_VERBOSE("[WARNING] Associativity setting is ignored for non-cache designs.");
                associativity = 1;
        }
        
        if (!isPow2(associativity))
                throw std::runtime_error("[ERROR] The associativity value has to be a power of 2 in this version.");

        if (routingMode == h_tree && internalSensing == false)
                throw std::runtime_error("[ERROR] H-tree does not support external sensing scheme in this version.");

}

void InputParameter::TechSetup() {
        tech = std::make_shared<Technology>();
        tech->Initialize(processNode, deviceRoadmap, UseUpdatedLib);
}

void InputParameter::MemCellSetup() {
        cell = std::make_shared<MemCell>();
        cell->ReadCellFromFile(fileMemCell, designTarget, tech->vdd);
}

void InputParameter::FEFETTechSetup() {
        FEFET_tech = std::make_shared<Technology>();
        FEFET_tech->Initialize(processNode, FEFET, UseUpdatedLib);
}

void InputParameter::PrintInputParameter() {
	std::cout << std::endl << "====================" << std::endl << "DESIGN SPECIFICATION" << std::endl << "====================" << std::endl;
	std::cout << "Design Target: ";
	switch (designTarget) {
	case cache:
		std::cout << "Cache" << std::endl;
		break;
	case RAM_chip:
		std::cout << "Random Access Memory" << std::endl;
		break;
	default:	/* CAM */
		std::cout << "Content Addressable Memory" << std::endl;
	}

	std::cout << "Capacity   : ";
        if (capacity < 1024)
                std::cout << capacity << "B" << std::endl;
        else if (capacity < 1024 * 1024)
		std::cout << capacity / 1024 << "KB" << std::endl;
	else if (capacity < 1024 * 1024 * 1024)
		std::cout << capacity / 1024 / 1024 << "MB" << std::endl;
	else
		std::cout << capacity / 1024 / 1024 / 1024 << "GB" << std::endl;

	if (designTarget == cache) {
		std::cout << "Cache Line Size: " << wordWidth / 8 << "Bytes" << std::endl;
		std::cout << "Cache Associativity: " << associativity << " Ways" << std::endl;
	} else {
		std::cout << "Data Width : " << wordWidth << "Bits";
		if (wordWidth % 8 == 0)
			std::cout << " (" << wordWidth / 8 << "Bytes)" << std::endl;
		else
			std::cout << std::endl;
	}
	if (designTarget == RAM_chip && (cell->memCellType == SLCNAND || cell->memCellType == MLCNAND)) {
		std::cout << "Page Size  : " << pageSize / 8 << "Bytes" << std::endl;
		std::cout << "Block Size : " << flashBlockSize / 8 / 1024 << "KB" << std::endl;
	}
	// TODO: tedious work here!!!

	if (optimizationTarget == full_exploration) {
		std::cout << std::endl << "Full design space exploration ... might take hours" << std::endl;
	} else {
		std::cout << std::endl << "Searching for the best solution that is optimized for ";
		switch (optimizationTarget) {
		case read_latency_optimized:
			std::cout << "read latency ..." << std::endl;
			break;
		case write_latency_optimized:
			std::cout << "write latency ..." << std::endl;
			break;
		case read_energy_optimized:
			std::cout << "read energy ..." << std::endl;
			break;
		case write_energy_optimized:
			std::cout << "write energy ..." << std::endl;
			break;
		case read_edp_optimized:
			std::cout << "read energy-delay-product ..." << std::endl;
			break;
		case write_edp_optimized:
			std::cout << "write energy-delay-product ..." << std::endl;
			break;
		case leakage_optimized:
			std::cout << "leakage power ..." << std::endl;
			break;
		default:	/* area */
			std::cout << "area ..." << std::endl;
		}
	}
}
