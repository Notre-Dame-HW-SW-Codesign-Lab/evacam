#include "SenseAmp.h"
#include "formula.h"
#include "input/SenseAmpYamlLoader.h"

#include <stdexcept>

namespace {

double LookupNodeValue(const std::vector<SenseAmpModel::NodeValue>& table, double featureSize) {
    for (const SenseAmpModel::NodeValue& item : table) {
        if (item.minFeatureSize == 0 || featureSize >= item.minFeatureSize) {
            return item.value;
        }
    }
    throw std::runtime_error("[Sense Amp] Missing node table entry.");
}

}  // namespace

void SenseAmp::Initialize(long long _numColumn, bool _currentSense, double _senseVoltage, double _pitchSenseAmp,
        std::shared_ptr<EvaCamConfig> _config) {
    if (initialized)
        _config->logger.Verbose() << "[Sense Amp] Warning: Already initialized!";

    numColumn = _numColumn;
    currentSense = _currentSense;
    senseVoltage = _senseVoltage;
    pitchSenseAmp = _pitchSenseAmp;
    config = _config;
    auto& tech = *config->technology.tech;
    if (!config->peripherals.fileSenseAmp.empty()) {
        model = YamlHelpers::ReadSenseAmpModelFromYaml(config->peripherals.fileSenseAmp);
    }

    if (pitchSenseAmp <= tech.featureSize() * model.minPitch) {
        /* too small, cannot do the layout */
        invalid = true;
        config->logger.Log() << "Sense Amp too small, cannot do the layout";
    }

    initialized = true;
    CalculateArea();
    CalculateRC();
}

void SenseAmp::CalculateArea() {
    if (!initialized) {
        ThrowInitializationError("[Sense Amp]");
    } else if (invalid) {
        height = width = area = 1e41;
    } else {
        height = width = area = 0;
        double tempHeight = 0;
        double tempWidth = 0;
        auto& tech = *config->technology.tech;

        if (currentSense) {	/* current-sensing needs IV converter */
            area += model.ivConverterArea * tech.featureSize() * tech.featureSize();
        }
        /* the following codes are transformed from CACTI 6.5 */


        CalculateGateArea(INV, 1, 0, model.pSenseWidth * tech.featureSize(),
                pitchSenseAmp, tech, &tempWidth, &tempHeight, config->peripherals.useUpdatedLib);	/* exchange width and height for senseamp layout */
        width = std::max(width, tempWidth);
        height += 2 * tempHeight;
        CalculateGateArea(INV, 1, 0, model.isolationWidth * tech.featureSize(),
                pitchSenseAmp, tech, &tempWidth, &tempHeight, config->peripherals.useUpdatedLib);	/* exchange width and height for senseamp layout */
        width = std::max(width, tempWidth);
        height += tempHeight;
        height += 2 * model.sameTypeDiffGap * tech.featureSize();

        CalculateGateArea(INV, 1, model.nSenseWidth * tech.featureSize(), 0,
                pitchSenseAmp, tech, &tempWidth, &tempHeight, config->peripherals.useUpdatedLib);	/* exchange width and height for senseamp layout */
        width = std::max(width, tempWidth);
        height += 2 * tempHeight;
        CalculateGateArea(INV, 1, model.enableWidth * tech.featureSize(), 0,
                pitchSenseAmp, tech, &tempWidth, &tempHeight, config->peripherals.useUpdatedLib);	/* exchange width and height for senseamp layout */
        width = std::max(width, tempWidth);
        height += tempHeight;
        height += 2 * model.sameTypeDiffGap * tech.featureSize();

        height += model.pToNDiffGap * tech.featureSize();

        /* transformation so that width meets the pitch */
        height = height * width / pitchSenseAmp;
        width = pitchSenseAmp;

        /* Add additional area if IV converter exists */
        height += area / width;
        width *= numColumn;

        area = height * width;
    }
}

void SenseAmp::CalculateRC() {
    if (!initialized) {
        ThrowInitializationError("[Sense Amp]");
    } else if (invalid) {
        capLoad = 1e41;
    } else {
        auto& tech = *config->technology.tech;
        capLoad = CalculateGateCap((model.pSenseWidth + model.nSenseWidth) * tech.featureSize(), tech)
            + CalculateDrainCap(model.nSenseWidth * tech.featureSize(), NMOS, pitchSenseAmp, tech)
            + CalculateDrainCap(model.pSenseWidth * tech.featureSize(), PMOS, pitchSenseAmp, tech)
            + CalculateDrainCap(model.isolationWidth * tech.featureSize(), PMOS, pitchSenseAmp, tech)
            + CalculateDrainCap(model.muxWidth * tech.featureSize(), NMOS, pitchSenseAmp, tech);
    }
}

void SenseAmp::CalculateLatency() {
    if (!initialized) {
        ThrowInitializationError("[Sense Amp]");
    } else {
        readLatency = writeLatency = 0;
        auto& tech = *config->technology.tech;
        if (currentSense) {	/* current-sensing needs IV converter */
            readLatency += LookupNodeValue(model.currentSenseLatency, tech.featureSize());
        }

        /* Voltage sense amplifier */
        double gm = CalculateTransconductance(model.nSenseWidth * tech.featureSize(), NMOS, tech)
            + CalculateTransconductance(model.pSenseWidth * tech.featureSize(), PMOS, tech);
        double tau = capLoad / gm;
        readLatency += tau * log(tech.vdd() / senseVoltage);
    }
}

void SenseAmp::CalculatePower() {
    if (!initialized) {
        ThrowInitializationError("[Sense Amp]");
    } else if (invalid) {
        readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
    } else {
        readDynamicEnergy = writeDynamicEnergy = 0;
        leakage = 0;
        auto& tech = *config->technology.tech;
        if (currentSense) {	/* current-sensing needs IV converter */
            readDynamicEnergy += LookupNodeValue(model.currentSenseEnergy, tech.featureSize());
            leakage += LookupNodeValue(model.currentSenseLeakage, tech.featureSize());
        }

        /* Voltage sense amplifier */
        readDynamicEnergy += capLoad * tech.vdd() * tech.vdd();
        double idleCurrent =  CalculateGateLeakage(INV, 1, model.enableWidth * tech.featureSize(), 0,
                config->input.temperature, tech) * tech.vdd();
        leakage += idleCurrent * tech.vdd();

        readDynamicEnergy *= numColumn;
        leakage *= numColumn;
    }
}

void SenseAmp::PrintProperty() {
    std::cout << "Sense Amplifier Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}
