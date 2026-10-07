#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "model/NandNonlinearString.h"

// Electrical validation protocol only; not an EvaCAM memory-device schema.
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: NandNonlinearStringProbe <circuit.yaml|->\n";
        return 2;
    }
    try {
        const auto input = std::string(argv[1]) == "-" ? YAML::Load(std::cin) : YAML::LoadFile(argv[1]);
        std::vector<NandNonlinearDevice> devices;
        if (!input["devices"].IsSequence()) throw std::invalid_argument("devices must be a sequence");
        for (const auto &device : input["devices"]) {
            const auto p = device["parameters"];
            const NandCellCurrentParameters parameters{
                p["beta_a_per_v2"].as<double>(), p["slope_factor"].as<double>(),
                p["leakage_conductance_s"].as<double>(), p["temperature_k"].as<double>(),
                p["channel_diameter_m"].as<double>(), p["gate_length_m"].as<double>(),
                p["minimum_voltage_v"].as<double>(), p["maximum_voltage_v"].as<double>(),
                p["minimum_threshold_v"].as<double>(), p["maximum_threshold_v"].as<double>()};
            devices.push_back({NandCellCurrentModel(parameters), device["gate_v"].as<double>(),
                    device["threshold_v"].as<double>()});
        }
        NandNonlinearDcOptions options;
        const auto solver = input["solver"];
        if (solver) {
            options.absoluteCurrentTolerance = solver["absolute_current_tolerance_a"].as<double>();
            options.relativeCurrentTolerance = solver["relative_current_tolerance"].as<double>();
            options.voltageTolerance = solver["voltage_tolerance_v"].as<double>();
            options.maxIterations = solver["max_iterations"].as<int>();
            options.maxBacktracks = solver["max_backtracks"].as<int>();
        }
        const auto result = NandNonlinearString::Solve(devices, input["source_v"].as<double>(),
                input["drain_v"].as<double>(), options);
        YAML::Node output;
        output["model"] = "experimental_smooth_potential_dc_v1";
        output["calibration_status"] = "uncalibrated";
        output["voltages_v"] = result.voltages;
        output["current_a"] = result.current;
        output["maximum_kcl_residual_a"] = result.maximumKclResidual;
        output["maximum_voltage_correction_v"] = result.maximumVoltageCorrection;
        output["iterations"] = result.iterations;
        output["backtracks"] = result.backtracks;
        YAML::Emitter emitter;
        emitter.SetDoublePrecision(17);
        emitter << output;
        if (!emitter.good()) throw std::runtime_error("probe serialization failed");
        std::cout << emitter.c_str() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "NandNonlinearStringProbe: " << error.what() << '\n';
        return 1;
    }
}
