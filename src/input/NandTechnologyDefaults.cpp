#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "config/EvaCamConfig.h"
#include "circuit/OutputDriver.h"
#include "circuit/SenseAmp.h"
#include "circuit/formula.h"
#include "input/NandTechnologyDefaults.h"

void ApplyNandTechnologyDefaults(MemCell &cell,
        const std::shared_ptr<EvaCamConfig> &context) {
    if (!cell.nandString) return;
    auto &spec = cell.memCellType == NAND3D ? cell.nand3d.electrical : cell.nand;
    if (spec.pendingTechnologyDefaults.empty()) return;
    if (!context || !context->technology.tech || !context->technology.tech->initialized()) {
        throw std::runtime_error("NAND defaults require an initialized technology library");
    }
    if (context->input.temperature < 300 || context->input.temperature > 400) {
        throw std::runtime_error("NAND technology defaults require temperature in 300..400 K");
    }
    const auto &tech = *context->technology.tech;
    const double feature = tech.featureSize();
    const auto needed = [&](const std::string &field) {
        return spec.pendingTechnologyDefaults.count(field) != 0;
    };
    const auto use = [&](const std::string &field, double &target, double value,
            const char *unit) {
        if (!needed(field)) return;
        if (!std::isfinite(value) || value < 0) {
            throw std::runtime_error("Invalid technology-library estimate for NAND " + field);
        }
        target = value;
        spec.technologyDefaults[field + "_" + unit] = value;
    };
    // NVSim's SLC string approximation uses a one-feature-width NMOS on
    // resistance. Its reuse here is an explicitly labeled CMOS proxy. The
    // off state uses the selected library's leakage at the same width/bias.
    const double onResistance = CalculateOnResistance(feature, NMOS, context->input.temperature, tech);
    const double gateCap = CalculateGateCap(feature, tech);
    const double drainCap = CalculateDrainCap(feature, NMOS, MAX_TRANSISTOR_HEIGHT * feature, tech);
    use("resistance.read_on", spec.resistanceReadOn, onResistance, "ohm");
    use("resistance.pass", spec.resistancePass, onResistance, "ohm");
    use("resistance.select", spec.resistanceSelect, onResistance, "ohm");
    if (needed("resistance.off")) {
        const double current = tech.currentOffNmos()[context->input.temperature - 300] * feature;
        if (!std::isfinite(current) || current <= 0) {
            throw std::runtime_error("Technology library has no positive off current; provide NAND resistance.off");
        }
        use("resistance.off", spec.resistanceOff, tech.vdd() / current, "ohm");
    }
    use("capacitance.gate", spec.capacitanceGate, gateCap, "f");
    use("capacitance.select", spec.capacitanceSelect, gateCap, "f");
    use("capacitance.internal", spec.capacitanceInternal, drainCap, "f");
    use("capacitance.source", spec.capacitanceSource, drainCap, "f");
    // Intrinsic termination load only: geometry-dependent wire C is already
    // added by each NAND model and must not be charged twice.
    use("capacitance.bitline", spec.capacitanceBitline, drainCap, "f");

    const auto needsPeripheral = [&](const char *name) {
        return needed(std::string(name) + ".area") || needed(std::string(name) + ".latency")
                || needed(std::string(name) + ".energy") || needed(std::string(name) + ".leakage");
    };
    const auto peripheral = [&](const char *name, NandPeripheralSpec &target,
            double area, double latency, double energy, double leakage) {
        use(std::string(name) + ".area", target.area, area, "m2");
        use(std::string(name) + ".latency", target.latency, latency, "s");
        use(std::string(name) + ".energy", target.energy, energy, "j");
        use(std::string(name) + ".leakage", target.leakage, leakage, "w");
    };
    if (needsPeripheral("sense")) {
        if (!std::isfinite(spec.minSenseMargin) || spec.minSenseMargin <= 0) {
            throw std::runtime_error("NAND sensing.min_margin must be positive for sense defaults");
        }
        SenseAmp sense;
        sense.Initialize(1, false, spec.minSenseMargin, MAX_TRANSISTOR_HEIGHT * feature, context);
        if (sense.invalid) throw std::runtime_error("Cannot derive NAND sense defaults from the selected CMOS model");
        sense.CalculateLatency();
        sense.CalculatePower();
        peripheral("sense", spec.sense, sense.area, sense.readLatency, sense.readDynamicEnergy, sense.leakage);
    }
    if (needsPeripheral("wordline_driver")) {
        const double strings = cell.memCellType == NAND3D
                ? static_cast<double>(cell.nand3d.stringRows) * cell.nand3d.stringColumns
                : context->input.pageSize;
        const double load = strings * spec.capacitanceGate;
        if (!std::isfinite(load) || load <= 0 || !std::isfinite(context->input.maxNmosSize)
                || context->input.maxNmosSize < MIN_NMOS_SIZE) {
            throw std::runtime_error("NAND driver defaults require positive gate load and valid CMOS driver sizing");
        }
        const double inputCap = CalculateGateCap(MIN_NMOS_SIZE * feature * (1 + tech.pnSizeRatio()), tech);
        OutputDriver driver;
        driver.Initialize(1, inputCap, load, 0, false, latency_first, 0, context);
        if (driver.invalid) throw std::runtime_error("Cannot derive NAND wordline_driver from the selected CMOS model");
        driver.CalculateArea();
        driver.CalculateRC();
        driver.CalculateLatency(1e20);
        driver.CalculatePower();
        // Load switching is already in wordline_capacitance at NAND voltages.
        // Keep only the CMOS chain's internal switching overhead here. This
        // fallback excludes high-voltage conversion and wire-loading delay.
        const double internalEnergy = std::max(0.0, driver.readDynamicEnergy - load * tech.vdd() * tech.vdd());
        peripheral("wordline_driver", spec.wordlineDriver,
                driver.area, driver.readLatency, internalEnergy, driver.leakage);
    }
    std::ostringstream fields;
    for (const auto &field : spec.pendingTechnologyDefaults) {
        if (fields.tellp() > 0) fields << ", ";
        fields << field;
    }
    spec.technologyDefaultSource = context->input.fileTechnology;
    if (spec.calibrationStatus == "calibrated") spec.calibrationStatus = "uncalibrated";
    spec.pendingTechnologyDefaults.clear();
    const char *roadmap = context->input.deviceRoadmap == HP ? "HP"
        : context->input.deviceRoadmap == LSTP ? "LSTP"
        : context->input.deviceRoadmap == LOP ? "LOP" : "FEFET";
    std::cerr << "[Input] Warning: missing NAND parameters: " << fields.str()
        << "; using technology library defaults from " << spec.technologyDefaultSource
        << " (requested " << context->input.processNode << "nm, roadmap " << roadmap
        << ", " << context->input.temperature << "K). CMOS estimates, not characterized NAND parameters.\n";
}
