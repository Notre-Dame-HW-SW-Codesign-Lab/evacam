#include "config/EvaCamConfigPrinter.h"

#include <iostream>

#include "EvaCamConfig.h"

namespace {

const char *ToString(DeviceRoadmap roadmap) {
    switch (roadmap) {
        case HP: return "HP";
        case LSTP: return "LSTP";
        case LOP: return "LOP";
        case FEFET: return "FEFET";
        case LP: return "LP";
        default: return "Unknown";
    }
}

const char *ToString(MemCellType type) {
    switch (type) {
        case SRAM: return "SRAM";
        case DRAM: return "DRAM";
        case eDRAM: return "eDRAM";
        case MRAM: return "MRAM";
        case PCRAM: return "PCRAM";
        case memristor: return "Memristor";
        case FBRAM: return "FBRAM";
        case SLCNAND: return "SLC NAND";
        case MLCNAND: return "MLC NAND";
        case FEFETRAM: return "FEFET RAM";
        default: return "Unknown";
    }
}

const char *ToString(SearchFunction function) {
    switch (function) {
        case EX: return "Exact match";
        case BE: return "Best match";
        case TH: return "Threshold match";
        default: return "Unknown";
    }
}

const char *ToString(WriteScheme scheme) {
    switch (scheme) {
        case set_before_reset: return "Set before reset";
        case reset_before_set: return "Reset before set";
        case erase_before_set: return "Erase before set";
        case erase_before_reset: return "Erase before reset";
        case write_and_verify: return "Write and verify";
        case normal_write: return "Normal write";
        default: return "Unknown";
    }
}

const char *ToString(TypeOfInputEncoder type) {
    switch (type) {
        case encoding_two_bit: return "Two-bit";
        default: return "Unknown";
    }
}

const char *ToString(TypeOfSenseAmp type) {
    switch (type) {
        case nvsim_voltage_sense: return "Voltage sense";
        case nvsim_current_sense: return "Current sense";
        case self_clock_sense: return "Self-clock sense";
        case dual_threshold_sense: return "Dual-threshold sense";
        case discharge: return "Discharge";
        default: return "Unknown";
    }
}

const char *ToString(BufferDesignTarget target) {
    switch (target) {
        case latency_first: return "Latency first";
        case latency_area_trade_off: return "Latency/area trade-off";
        case area_first: return "Area first";
        default: return "Unknown";
    }
}

void PrintConstraintIfSet(const char *label, double value) {
    if (value < 1e40)
        std::cout << " - " << label << ": " << value << std::endl;
}

}  // namespace

void EvaCamConfigPrinter::Print(const EvaCamConfig &config) {
    const auto &input = config.input;
    const auto &peripherals = config.peripherals;
    const auto &constraints = config.constraints;
    const auto &technology = config.technology;
    const auto &exploration = config.exploration;

    std::cout << std::endl << "====================" << std::endl
              << "DESIGN SPECIFICATION" << std::endl
              << "====================" << std::endl;
    std::cout << "Design Target: Content Addressable Memory" << std::endl;

    std::cout << "Capacity   : ";
    if (input.capacity < 1024)
        std::cout << input.capacity << "B" << std::endl;
    else if (input.capacity < 1024 * 1024)
        std::cout << input.capacity / 1024 << "KB" << std::endl;
    else if (input.capacity < 1024 * 1024 * 1024)
        std::cout << input.capacity / 1024 / 1024 << "MB" << std::endl;
    else
        std::cout << input.capacity / 1024 / 1024 / 1024 << "GB" << std::endl;

    std::cout << "Data Width : " << input.wordWidth << "Bits";
    if (input.wordWidth % 8 == 0)
        std::cout << " (" << input.wordWidth / 8 << "Bytes)" << std::endl;
    else
        std::cout << std::endl;
    std::cout << "System Process Node: " << input.processNode << "nm" << std::endl;
    std::cout << "Device Roadmap: " << ToString(input.deviceRoadmap) << std::endl;
    std::cout << "Temperature: " << input.temperature << "K" << std::endl;
    std::cout << "Memory Cell: " << ToString(technology.cell->memCellType) << std::endl;
    std::cout << "Cell File  : " << input.fileMemCell << std::endl;

    std::cout << "Search Function: " << ToString(input.searchFunction) << std::endl;

    std::cout << "Write Scheme: " << ToString(input.writeScheme) << std::endl;

    std::cout << "Routing Mode: " << (input.routingMode == h_tree ? "H-tree" : "Non-H-tree") << std::endl;
    std::cout << "Sensing: " << (input.internalSensing ? "Internal" : "External")
              << ", " << ToString(peripherals.typeSenseAmp);
    if (peripherals.customSenseAmp)
        std::cout << " (custom)";
    std::cout << std::endl;

    std::cout << "Peripherals:" << std::endl;
    std::cout << " - Write Driver: " << (peripherals.withWriteDriver ? "enabled" : "disabled") << std::endl;
    if (peripherals.withInputBuffer || peripherals.withInputEnc) {
        std::cout << " - Input Path:";
        if (peripherals.withInputBuffer)
            std::cout << " buffer";
        if (peripherals.withInputEnc) {
            std::cout << (peripherals.withInputBuffer ? "," : "") << " encoder=" << ToString(peripherals.typeInputEnc);
            if (peripherals.customInputEnc)
                std::cout << " (custom)";
        }
        std::cout << std::endl;
    }
    if (peripherals.withOutputBuffer || peripherals.withPriorityEnc || peripherals.withOutputAcc) {
        std::cout << " - Output Path:";
        bool needSeparator = false;
        if (peripherals.withOutputBuffer) {
            std::cout << " buffer";
            needSeparator = true;
        }
        if (peripherals.withPriorityEnc) {
            std::cout << (needSeparator ? "," : "") << " priority encoder";
            needSeparator = true;
        }
        if (peripherals.withOutputAcc) {
            std::cout << (needSeparator ? "," : "") << " accumulator";
        }
        std::cout << std::endl;
    }

    std::cout << "Optimization:" << std::endl;
    std::cout << " - Buffer Design: "
        << ToString((BufferDesignTarget)exploration.cam.areaOptimizationLevel.Min()) << std::endl;
    std::cout << " - Row Driver: "
        << ToString((BufferDesignTarget)exploration.cam.rowDriverOptLevel.Min()) << std::endl;
    if (peripherals.withPriorityEnc)
        std::cout << " - Priority Encoder: "
            << ToString((BufferDesignTarget)exploration.cam.priorityOptLevel.Min()) << std::endl;
    if (peripherals.withOutputAcc)
        std::cout << " - Bit Serial Width: " << exploration.cam.bitSerialWidth.Min() << std::endl;

    if (constraints.enabled) {
        std::cout << "Constraints:" << std::endl;
        PrintConstraintIfSet("Read Latency", constraints.readLatency);
        PrintConstraintIfSet("Write Latency", constraints.writeLatency);
        PrintConstraintIfSet("Read Dynamic Energy", constraints.readDynamicEnergy);
        PrintConstraintIfSet("Write Dynamic Energy", constraints.writeDynamicEnergy);
        PrintConstraintIfSet("Read EDP", constraints.readEdp);
        PrintConstraintIfSet("Write EDP", constraints.writeEdp);
        PrintConstraintIfSet("Area", constraints.area);
        PrintConstraintIfSet("Leakage", constraints.leakage);
    }

    if (peripherals.useUpdatedLib || peripherals.noPrechargeInc || peripherals.includeLeakage || peripherals.scaledVoltage != 0
            || !peripherals.fileCustomSA.empty() || constraints.pruningEnabled) {
        std::cout << "Advanced Options:" << std::endl;
        if (peripherals.useUpdatedLib)
            std::cout << " - Updated Library Enabled" << std::endl;
        if (peripherals.noPrechargeInc)
            std::cout << " - Excluding Precharge Latency" << std::endl;
        if (peripherals.includeLeakage)
            std::cout << " - Including Leakage in Results" << std::endl;
        if (peripherals.scaledVoltage != 0)
            std::cout << " - Scaled Voltage: " << peripherals.scaledVoltage << std::endl;
        if (!peripherals.fileCustomSA.empty())
            std::cout << " - Custom Sense Amp File: " << peripherals.fileCustomSA << std::endl;
        if (constraints.pruningEnabled)
            std::cout << " - Exploration Pruning Enabled" << std::endl;
    }

    if (input.optimizationTarget == full_exploration) {
        std::cout << std::endl << "Full design space exploration ... might take hours" << std::endl;
    } else {
        std::cout << std::endl << "Searching for the best solution that is optimized for ";
        switch (input.optimizationTarget) {
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
            default:
                std::cout << "area ..." << std::endl;
        }
    }
}
