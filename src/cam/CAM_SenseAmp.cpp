#include "CAM_SenseAmp.h"
#include "EvaCamConfig.h"
#include "formula.h"
#include "input/CustomSenseAmpYamlLoader.h"

#include <iostream>
#include <stdexcept>

CAM_SenseAmp::CAM_SenseAmp() {
    initialized = false;
    invalid = false;
    typeSA = nvsim_voltage_sense;
    isCustom = false;
    senseVoltage = 0;
    capLoad = 0;
    pitchSenseAmp = 0;
}

void CAM_SenseAmp::Initialize(
        long long _numColumn, 
        TypeOfSenseAmp _typeSA, 
        bool _isCustom, 
        double _senseVoltage, 
        double _pitchSenseAmp, 
        std::string _fileCustomSA, 
        std::shared_ptr<EvaCamConfig> _config) {

    config = _config;

    if (initialized)
        config->logger.Verbose() << "[CAM_SenseAmp] Warning: Already initialized!";

    typeSA = _typeSA;
    isCustom = _isCustom;
    senseVoltage = _senseVoltage;
    pitchSenseAmp = _pitchSenseAmp;
    numColumn = _numColumn;
    fileCustomSA = _fileCustomSA;

    if (pitchSenseAmp <= config->technology.tech->featureSize() * 2) {
        /* too small, cannot do the layout */
        config->logger.Log() << "[CAM_SenseAmp] Warning: Pitch size too small, cannot do the layout!";
        invalid = true;
    }

    normalSenseAmp = std::make_unique<SenseAmp>();
    normalSenseAmp->Initialize(numColumn, typeSA == nvsim_current_sense, senseVoltage, pitchSenseAmp, config);
    if (!isCustom && typeSA != nvsim_voltage_sense && typeSA != nvsim_current_sense
            && typeSA != discharge && typeSA != self_clock_sense
            && typeSA != dual_threshold_sense) {
        normalSenseAmp->invalid = true;
        throw std::runtime_error("[CAM_SenseAmp] Error: sensing type is not supported.");
    }
    if (isCustom) {
        customSA = std::make_unique<SenseAmp>();
        YamlHelpers::ReadCustomSenseAmpFromYaml(
                *customSA, fileCustomSA, config->technology.tech->featureSize());
    }
    initialized = true;
    CalculateArea();
    CalculateRC();
}

void CAM_SenseAmp::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[CAM_SenseAmp]");
    } else if (invalid) {
        height = width = area = 1e41;
    } else {
        if (isCustom && customSA->area > 0) {
            if (customSA->height * customSA->width > 0) {
                height = customSA->height;
                width = customSA->width * numColumn;
            } else {
                height = sqrt(customSA->area);
                width = height * numColumn;
            }
            area = width * height;
        }
        else if (typeSA == nvsim_voltage_sense || typeSA == nvsim_current_sense || typeSA == discharge) {
            //normalSenseAmp->CalculateArea();
            width = normalSenseAmp->width;
            height = normalSenseAmp->height;
            area = width * height;
        }
        else if (typeSA == self_clock_sense) {
            // TODO: self clock sense
            height = width = area = 1e41;
            config->logger.Log() << "[CAM_SenseAmp] Warning: Self clocked sensing is under development!";
        }
        else if (typeSA == dual_threshold_sense) {
            // TODO: self clock sense
            height = width = area = 1e41;
            config->logger.Log() << "[CAM_SenseAmp] Warning: Dual threshold sensing is under development!";
        }
        else {
            height = width = area = 1e41;
            throw std::runtime_error("[CAM_SenseAmp] Error: Sensing type entered is not supported yet!");
        }
    }
}

void CAM_SenseAmp::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[CAM_SenseAmp]");
    } else if (invalid) {
        capLoad = 1e41;
    } else {
        if (isCustom && customSA->capLoad > 0) {
            // custom design
            capLoad = customSA->capLoad;
        }
        else if (typeSA == nvsim_voltage_sense || typeSA == nvsim_current_sense || typeSA == discharge) {
            //normalSenseAmp->CalculateRC();
            capLoad = normalSenseAmp->capLoad;
        }
        else if (typeSA == self_clock_sense) {
            // TODO: self clock sense
            capLoad = 1e41;
            config->logger.Log() << "[CAM_SenseAmp] Warning: Self clocked sensing is under development!";
        }
        else if (typeSA == dual_threshold_sense) {
            // TODO: self clock sense
            capLoad = 1e41;
            config->logger.Log() << "[CAM_SenseAmp] Warning: Dual threshold sensing is under development!";
        }
        else {
            capLoad = 1e41;
            throw std::runtime_error("[CAM_SenseAmp] Error: sensing type is not supported.");
        }
    }
}

void CAM_SenseAmp::CalculateLatency() {
    if (!initialized) {
        ThrowInitializationError("[CAM_SenseAmp]");
    } else {
        readLatency = writeLatency = 0;
        if (isCustom && customSA->readLatency > 0) {
            readLatency = customSA->readLatency;
            writeLatency = readLatency;
        }
        else if (typeSA == nvsim_voltage_sense || typeSA == nvsim_current_sense || typeSA == discharge) {
            normalSenseAmp->CalculateLatency();
            readLatency = normalSenseAmp->readLatency;
            writeLatency = normalSenseAmp->writeLatency;
        }
        else if (typeSA == self_clock_sense) {
            // TODO: self clock sense
            readLatency = writeLatency = 0;
            config->logger.Log() << "[CAM_SenseAmp] Warning: Self clocked sensing is under development!";
        }
        else if (typeSA == dual_threshold_sense) {
            // TODO: self clock sense
            readLatency = writeLatency = 0;
            config->logger.Log() << "[CAM_SenseAmp] Warning: Dual threshold sensing is under development!";
        }
        else {
            readLatency = writeLatency = 0;
            throw std::runtime_error("[CAM_SenseAmp] Error: sensing type is not supported.");
        }
    }
}

void CAM_SenseAmp::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[CAM_SenseAmp]");
    } else if (invalid) {
        readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
    } else {
        readDynamicEnergy = writeDynamicEnergy = 0;
        leakage = 0;
        normalSenseAmp->CalculatePower();
        if (isCustom && customSA->readDynamicEnergy > 0) {
            readDynamicEnergy = customSA->readDynamicEnergy * numColumn;
            writeDynamicEnergy = readDynamicEnergy;
            if (customSA->leakage > 0) {
                leakage = customSA->leakage * numColumn;
            } else {
                leakage = normalSenseAmp->leakage;
            }
        }
        else if (typeSA == nvsim_voltage_sense || typeSA == nvsim_current_sense || typeSA == discharge) {
            readDynamicEnergy = normalSenseAmp->readDynamicEnergy;
            writeDynamicEnergy = normalSenseAmp->writeDynamicEnergy;
            leakage = normalSenseAmp->leakage;
        }
        else if (typeSA == self_clock_sense) {
            // TODO: self clock sense
            readDynamicEnergy = writeDynamicEnergy = leakage = 0;
            config->logger.Log() << "[CAM_SenseAmp] Warning: Self clocked sensing is under development!";
        }
        else if (typeSA == dual_threshold_sense) {
            // TODO: self clock sense
            readDynamicEnergy = writeDynamicEnergy = leakage = 0;
            config->logger.Log() << "[CAM_SenseAmp] Warning: Dual threshold sensing is under development!";
        }
        else {
            readDynamicEnergy = writeDynamicEnergy = leakage = 0;
            throw std::runtime_error("[CAM_SenseAmp] Error: sensing type is not supported.");
        }
    }
}

void CAM_SenseAmp::PrintProperty() {
    std::cout << "Sense Amplifier Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
