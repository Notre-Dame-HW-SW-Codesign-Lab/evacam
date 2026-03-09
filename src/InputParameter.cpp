#include "../include/InputParameter.h"
#include "../include/global.h"
#include "../include/constant.h"
#include "../include/formula.h"
#include "../include/macros.h"
#include "../include/YamlHelpers.h"
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <yaml.h>
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

	outputFilePrefix = "results/output";	/* Default output file name */
	// TODO: added new initial
	withWriteDriver = false;
	withInputBuffer = false;
	withOutputBuffer = false;
	withInputEnc = false;
	customInputEnc = false;
	typeInputEnc = encoding_two_bit;
	typeSenseAmp = nvsim_voltage_sense;
	customSenseAmp = false;
	withOutputAcc = false;
	withPriorityEnc = false;

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
    
    auto design = YamlHelpers::child_required(config, "design");

    designTarget = YamlHelpers::read_enum_required<DesignTarget>(design, "target");
    if (designTarget != CAM_chip)
        throw std::runtime_error("[Error]: Only support CAM design.");
    minNumRowPerSet = 1;
    maxNumRowPerSet = 256;

    searchFunction = YamlHelpers::read_enum_required<SearchFunction>(design, "search_function");

    double processNodeM =
        YamlHelpers::read_quantity_required(design, "process_node", YamlHelpers::LengthUnits(), 1e-9, "process_node");
    processNode = (int)std::lround(processNodeM / 1e-9);

    constexpr std::array<int, 9> supportedProcessNodes = {7, 10, 14, 22, 32, 45, 90, 120, 200};
    if (!std::binary_search(supportedProcessNodes.begin(), supportedProcessNodes.end(), processNode))
        PRINT_VERBOSE( "[Warning]: Possible error as " << processNode << "nm proccess node is not supported.");
    if (processNode <= 45) {
        std::string ptm = "[Warning]: When using PTM model, ";
	PRINT_VERBOSE(ptm << "only HP/LSTP model is provided.");
	PRINT_VERBOSE(ptm << "interpolation between 45/32/22/14/10/7nm may lead to error.");
    }

    deviceRoadmap = YamlHelpers::read_enum_required<DeviceRoadmap>(design, "device_roadmap");

    temperature =
        (int)std::lround(YamlHelpers::read_quantity_required(design, "temperature", YamlHelpers::TemperatureUnits(), 1.0, "temperature"));

    // Memory
    
    auto memory = YamlHelpers::child_required(config, "memory");

    fileMemCell = YamlHelpers::read_required<std::string>(memory, "cell_file");

    capacity = (int64_t)std::llround(
        YamlHelpers::read_quantity_required(memory, "capacity", YamlHelpers::DataSizeUnits(), 1.0, "capacity"));

    wordWidth = (long)std::lround(
        YamlHelpers::read_quantity_required(memory, "word_width", YamlHelpers::BitUnits(), 1.0, "word_width"));
    minBitSerialWidth = maxBitSerialWidth = (int)wordWidth;

    // Routing

    auto routing = YamlHelpers::child_required(config, "routing");
    
    routingMode = YamlHelpers::read_enum_required<RoutingMode>(routing, "type");

    if (routingMode == non_h_tree) throw std::runtime_error("[Input] Error: non H-tree is under development!");
    
    // Peripherals

    auto peripherals = YamlHelpers::child_required(config, "peripherals");

    withWriteDriver = YamlHelpers::read_required<bool>(peripherals, "write_driver");
    
    auto pinput = YamlHelpers::child_required(peripherals, "input");
    
    withInputBuffer = YamlHelpers::read_required<bool>(pinput, "buffer");
    withInputEnc    = YamlHelpers::read_required<bool>(pinput, "encoder");
    customInputEnc  = YamlHelpers::read_required<bool>(pinput, "custom_encoder");

    auto poutput = YamlHelpers::child_required(peripherals, "output");
    
    withOutputBuffer = YamlHelpers::read_required<bool>(poutput, "buffer");
    withPriorityEnc  = YamlHelpers::read_required<bool>(poutput, "priority_encoder");
    withOutputAcc    = YamlHelpers::read_required<bool>(poutput, "accumulator");

    if (!withOutputAcc) minBitSerialWidth = maxBitSerialWidth;

    // Sensing
    
    auto sensing = YamlHelpers::child_required(config, "sensing");

    internalSensing = YamlHelpers::read_required<bool>(sensing, "internal");
    customSenseAmp  = YamlHelpers::read_required<bool>(sensing, "custom_sense_amp");

    typeSenseAmp = YamlHelpers::read_enum_required<TypeOfSenseAmp>(sensing, "amplifier_type");

    // Optimization
    
    const auto opt = YamlHelpers::child_required(config, "optimization");
    optimizationTarget = YamlHelpers::read_enum_required<OptimizationTarget>(opt, "target");

    auto bufferDesign = YamlHelpers::read_enum_required<BufferDesignTarget>(opt, "buffer_design");
    minAreaOptimizationLevel = maxAreaOptimizationLevel = (int)bufferDesign;

    auto rowDriver = YamlHelpers::read_enum_required<BufferDesignTarget>(opt, "row_driver");
    minRowDriverOptLevel = maxRowDriverOptLevel = (int)rowDriver;

    auto priority = YamlHelpers::read_enum_required<BufferDesignTarget>(opt, "priority_encoder");
    minPriorityOptLevel = maxPriorityOptLevel = (int)priority;
          	
    // Wires
    
    auto wiresLocal = YamlHelpers::child_required(YamlHelpers::child_required(config, "wires"), "local");
    auto localType = YamlHelpers::read_enum_required<WireType>(wiresLocal, "type");
    minLocalWireType = maxLocalWireType = (int)localType;

    auto localRepeater = YamlHelpers::read_enum_required<WireRepeaterType>(wiresLocal, "repeater");
    minLocalWireRepeaterType = maxLocalWireRepeaterType = (int)localRepeater;

    minIsLocalWireLowSwing = maxIsLocalWireLowSwing = YamlHelpers::read_required<bool>(wiresLocal, "low_swing");

    auto wiresGlobal = YamlHelpers::child_required(YamlHelpers::child_required(config, "wires"), "global");
    auto globalType = YamlHelpers::read_enum_required<WireType>(wiresGlobal, "type");
    minGlobalWireType = maxGlobalWireType = (int)globalType;

    auto globalRepeater = YamlHelpers::read_enum_required<WireRepeaterType>(wiresGlobal, "repeater");
    minGlobalWireRepeaterType = maxGlobalWireRepeaterType = (int)globalRepeater;

    minIsGlobalWireLowSwing = maxIsGlobalWireLowSwing = YamlHelpers::read_required<bool>(wiresGlobal, "low_swing");

    // Array
    // If array is omitted, keep exploration ranges from RestoreSearchSize()/ReduceSearchSize().
    auto array = YamlHelpers::child_optional(config, "array");
    if (array) {
        auto banks = YamlHelpers::child_required(array, "banks");
        auto banks_total = YamlHelpers::child_required(banks, "total");
        auto banks_active = YamlHelpers::child_required(banks, "active");

        minNumRowMat             = maxNumRowMat             = YamlHelpers::read_required_index<int>(banks_total, 0, "array.banks.total[0]");
        minNumColumnMat          = maxNumColumnMat          = YamlHelpers::read_required_index<int>(banks_total, 1, "array.banks.total[1]");
        minNumActiveMatPerColumn = maxNumActiveMatPerColumn = YamlHelpers::read_required_index<int>(banks_active, 0, "array.banks.active[0]");
        minNumActiveMatPerRow    = maxNumActiveMatPerRow    = YamlHelpers::read_required_index<int>(banks_active, 1, "array.banks.active[1]");

        auto mats = YamlHelpers::child_required(array, "mats");
        auto mats_total = YamlHelpers::child_required(mats, "total");
        auto mats_active = YamlHelpers::child_required(mats, "active");

        minNumRowSubarray = maxNumRowSubarray = YamlHelpers::read_required_index<int>(mats_total, 0, "array.mats.total[0]");
        minNumColumnSubarray = maxNumColumnSubarray = YamlHelpers::read_required_index<int>(mats_total, 1, "array.mats.total[1]");
        minNumActiveSubarrayPerColumn = maxNumActiveSubarrayPerColumn = YamlHelpers::read_required_index<int>(mats_active, 0, "array.mats.active[0]");
        minNumActiveSubarrayPerRow = maxNumActiveSubarrayPerRow = YamlHelpers::read_required_index<int>(mats_active, 1, "array.mats.active[1]");

        auto mux = YamlHelpers::child_required(array, "mux");
        maxMuxSenseAmp = minMuxSenseAmp = YamlHelpers::read_required<int>(mux, "sense_amp");
        maxMuxOutputLev1 = minMuxOutputLev1 = YamlHelpers::read_required<int>(mux, "output_level1");
        maxMuxOutputLev2 = minMuxOutputLev2 = YamlHelpers::read_required<int>(mux, "output_level2");
    }

    // Matchline
    
	    auto matchline = YamlHelpers::child_optional(config, "matchline");
	    if (matchline) {
	        AddCapOnML =
	            YamlHelpers::read_quantity_required(matchline, "additional_cap", YamlHelpers::CapacitanceUnits(), 1e-15, "matchline.additional_cap");
	    }

	    auto constraints = YamlHelpers::child_optional(config, "constraints");
	    if (constraints) {
	        bool anyConstraintSet = false;
	        bool enabled = YamlHelpers::read_optional<bool>(constraints, "enabled", false);

	        if (YamlHelpers::child_optional(constraints, "read_latency")) {
	            readLatencyConstraint = YamlHelpers::read_quantity_required(
	                constraints, "read_latency", YamlHelpers::TimeUnits(), 1.0, "constraints.read_latency");
	            anyConstraintSet = true;
	        }
	        if (YamlHelpers::child_optional(constraints, "write_latency")) {
	            writeLatencyConstraint = YamlHelpers::read_quantity_required(
	                constraints, "write_latency", YamlHelpers::TimeUnits(), 1.0, "constraints.write_latency");
	            anyConstraintSet = true;
	        }
	        if (YamlHelpers::child_optional(constraints, "read_dynamic_energy")) {
	            readDynamicEnergyConstraint = YamlHelpers::read_quantity_required(
	                constraints, "read_dynamic_energy", YamlHelpers::EnergyUnits(), 1.0, "constraints.read_dynamic_energy");
	            anyConstraintSet = true;
	        }
	        if (YamlHelpers::child_optional(constraints, "write_dynamic_energy")) {
	            writeDynamicEnergyConstraint = YamlHelpers::read_quantity_required(
	                constraints, "write_dynamic_energy", YamlHelpers::EnergyUnits(), 1.0, "constraints.write_dynamic_energy");
	            anyConstraintSet = true;
	        }
	        if (YamlHelpers::child_optional(constraints, "leakage")) {
	            leakageConstraint = YamlHelpers::read_quantity_required(
	                constraints, "leakage", YamlHelpers::PowerUnits(), 1.0, "constraints.leakage");
	            anyConstraintSet = true;
	        }
	        if (YamlHelpers::child_optional(constraints, "area")) {
	            areaConstraint = YamlHelpers::read_required<double>(constraints, "area");
	            anyConstraintSet = true;
	        }
	        if (YamlHelpers::child_optional(constraints, "read_edp")) {
	            readEdpConstraint = YamlHelpers::read_required<double>(constraints, "read_edp");
	            anyConstraintSet = true;
	        }
	        if (YamlHelpers::child_optional(constraints, "write_edp")) {
	            writeEdpConstraint = YamlHelpers::read_required<double>(constraints, "write_edp");
	            anyConstraintSet = true;
	        }
	        isConstraintApplied = enabled || anyConstraintSet;
	    }

	    auto advanced = YamlHelpers::child_optional(config, "advanced");
	    if (advanced) {
	        if (YamlHelpers::child_optional(advanced, "max_nmos_size")) {
	            maxNmosSize = YamlHelpers::read_quantity_required(
	                advanced, "max_nmos_size", YamlHelpers::FeatureUnits(), 1.0, "advanced.max_nmos_size");
	        }
	        useCactiAssumption = YamlHelpers::read_optional<bool>(
	            advanced, "use_cacti_assumption", useCactiAssumption);
	        if (useCactiAssumption) {
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
	        }
	        isPruningEnabled = YamlHelpers::read_optional<bool>(
	            advanced, "enable_pruning", isPruningEnabled);
	        if (YamlHelpers::child_optional(advanced, "bit_serial_width")) {
	            const long bsw = (long)std::lround(YamlHelpers::read_quantity_required(
	                advanced, "bit_serial_width", YamlHelpers::BitUnits(), 1.0, "advanced.bit_serial_width"));
	            minBitSerialWidth = maxBitSerialWidth = (int)bsw;
	        }
	        if (YamlHelpers::child_optional(advanced, "input_encoder_type")) {
	            typeInputEnc = YamlHelpers::read_enum_required<TypeOfInputEncoder>(
	                advanced, "input_encoder_type", false);
	        }
	        fileCustomSA = YamlHelpers::read_optional<std::string>(
	            advanced, "custom_sa_input_file", fileCustomSA);
	        UseUpdatedLib = YamlHelpers::read_optional<bool>(
	            advanced, "use_updated_lib", UseUpdatedLib);
	        NoPrechargeInc = YamlHelpers::read_optional<bool>(
	            advanced, "exclude_precharge_latency", NoPrechargeInc);
	        IncludeLeakge = YamlHelpers::read_optional<bool>(
	            advanced, "include_leakage", IncludeLeakge);
	        scaledVoltage = YamlHelpers::read_optional<double>(
	            advanced, "scaled_voltage", scaledVoltage);
	    }

	    auto cacheCfg = YamlHelpers::child_optional(config, "cache");
	    if (cacheCfg) {
	        associativity = YamlHelpers::read_optional<int>(cacheCfg, "associativity", associativity);
	        if (YamlHelpers::child_optional(cacheCfg, "access_mode")) {
	            cacheAccessMode = YamlHelpers::read_enum_required<CacheAccessMode>(
	                cacheCfg, "access_mode", false);
	        }
	        if (YamlHelpers::child_optional(cacheCfg, "write_scheme")) {
	            writeScheme = YamlHelpers::read_enum_required<WriteScheme>(
	                cacheCfg, "write_scheme", false);
	        }
	    }

	    auto flash = YamlHelpers::child_optional(config, "flash");
	    if (flash) {
	        if (YamlHelpers::child_optional(flash, "page_size")) {
	            pageSize = (long)std::lround(YamlHelpers::read_quantity_required(
	                flash, "page_size", YamlHelpers::DataSizeUnits(), 1.0, "flash.page_size") * 8.0);
	        }
	        if (YamlHelpers::child_optional(flash, "block_size")) {
	            flashBlockSize = (long)std::lround(YamlHelpers::read_quantity_required(
	                flash, "block_size", YamlHelpers::DataSizeUnits(), 1.0, "flash.block_size") * 8.0);
	        }
	    }

	    auto extra = YamlHelpers::child_optional(config, "extra");
	    if (extra) {
	        outputFilePrefix = YamlHelpers::read_optional<std::string>(
	            extra, "output_file_prefix", outputFilePrefix);
	        if (YamlHelpers::child_optional(extra, "worst_case_sense_margin")) {
	            MatchlineSenseMargin = YamlHelpers::read_quantity_required(
	                extra, "worst_case_sense_margin", YamlHelpers::VoltageUnits(), 1.0, "extra.worst_case_sense_margin");
	            sensemargin = MatchlineSenseMargin;
	        }
	        if (YamlHelpers::child_optional(extra, "max_driver_current")) {
	            maxDriverCurrent = YamlHelpers::read_quantity_required(
	                extra, "max_driver_current", YamlHelpers::CurrentUnits(), 1.0, "extra.max_driver_current");
	        }
	        // Preferred YAML form, e.g. `real_capacity: 9KB`.
	        auto realCapacityNode = YamlHelpers::child_optional(extra, "real_capacity");
	        if (realCapacityNode) {
	            realCapacity = (int64_t)std::llround(
	                YamlHelpers::parse_quantity_node(
	                    realCapacityNode, YamlHelpers::DataSizeUnits(), 1.0, "extra.real_capacity"));
	        } else {
	            // Backward-compatible aliases used by older configs.
	            auto realCapacityB = YamlHelpers::child_optional(extra, "RealCapacity (B)");
	            auto realCapacityKB = YamlHelpers::child_optional(extra, "RealCapacity (KB)");
	            auto realCapacityMB = YamlHelpers::child_optional(extra, "RealCapacity (MB)");

	            if (realCapacityB) {
	                realCapacity = (int64_t)std::llround(
	                    YamlHelpers::read_scalar_required<double>(realCapacityB, "extra.RealCapacity (B)"));
	            } else if (realCapacityKB) {
	                realCapacity = (int64_t)std::llround(
	                    YamlHelpers::read_scalar_required<double>(realCapacityKB, "extra.RealCapacity (KB)") * 1024.0);
	            } else if (realCapacityMB) {
	                realCapacity = (int64_t)std::llround(
	                    YamlHelpers::read_scalar_required<double>(realCapacityMB, "extra.RealCapacity (MB)") * 1024.0 * 1024.0);
	            }
	        }
	    }

	    if (wordWidth <= 0) {
	        throw std::runtime_error("[Input] Error: word_width must be > 0.");
	    }
	    const bool isWordWidthPow2 = (wordWidth & (wordWidth - 1)) == 0;
	    if (!isWordWidthPow2 && realCapacity == 0) {
	        throw std::runtime_error(
	            "[Input] Error: non-power-of-two word_width requires extra.real_capacity to be set.");
	    }
	    if (realCapacity > 0) {
	        if (realCapacity < capacity) {
	            throw std::runtime_error(
	                "[Input] Error: extra.real_capacity must be >= memory.capacity.");
	        }
	        const long long denom =
	            (long long)minNumRowSubarray * minNumColumnSubarray *
	            minNumActiveMatPerRow * minNumActiveMatPerColumn;
	        if (denom <= 0) {
	            throw std::runtime_error(
	                "[Input] Error: invalid array geometry while validating extra.real_capacity.");
	        }
	        if ((realCapacity % denom) != 0) {
	            throw std::runtime_error(
	                "[Input] Error: extra.real_capacity is incompatible with array geometry.");
	        }
	        if (((realCapacity / denom) % wordWidth) != 0) {
	            throw std::runtime_error(
	                "[Input] Error: extra.real_capacity is incompatible with word_width.");
	        }
	    }


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
