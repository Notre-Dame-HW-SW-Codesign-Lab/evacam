#include "FunctionUnit.h"

#include <iostream>

FunctionUnit::FunctionUnit() {
    height = width = 0;
    area = 0;
    readLatency = writeLatency = 0;
    readDynamicEnergy = writeDynamicEnergy = 0;
    leakage = 0;
    resetLatency = setLatency = 0;
    resetDynamicEnergy = setDynamicEnergy = 0;
    cellReadEnergy = 0;
    cellSetEnergy = cellResetEnergy = 0;
}

void FunctionUnit::PrintProperty() {
    std::cout << "Area = " << height * 1e6 << "um x " << width * 1e6 << "um = " << area * 1e6 << "mm^2" << std::endl;
    std::cout << "Timing:" << std::endl;
    std::cout << " -  Read Latency = " << readLatency*1e9 << "ns" << std::endl;
    std::cout << " - Write Latency = " << writeLatency*1e9 << "ns" << std::endl;
    std::cout << "Power:" << std::endl;
    std::cout << " -  Read Dynamic Energy = " << readDynamicEnergy * 1e12 << "pJ" << std::endl;
    std::cout << " -  Write Dynamic Energy = " << writeDynamicEnergy * 1e12 << "pJ" << std::endl;
    std::cout << " -  Leakage Power = " << leakage * 1e3 << "mW" << std::endl;
}
