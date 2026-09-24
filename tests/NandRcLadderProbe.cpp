#include <iostream>
#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>

#include "model/NandRcLadder.h"

namespace {

NandRcBoundary Boundary(const YAML::Node &node) {
    if (!node || !node["connected"].as<bool>(false)) return {};
    return {true, node["resistance_ohm"].as<double>(), node["voltage_v"].as<double>()};
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: NandRcLadderProbe <circuit.yaml>\n";
        return 2;
    }
    try {
        const auto input = YAML::LoadFile(argv[1]);
        const auto options = input["solver"];
        const auto result = NandRcLadder::Solve(
            input["capacitances_f"].as<std::vector<double>>(),
            input["series_resistances_ohm"].as<std::vector<double>>(),
            input["initial_voltages_v"].as<std::vector<double>>(),
            input["duration_s"].as<double>(), Boundary(input["left"]), Boundary(input["right"]),
            {options["max_step_s"].as<double>(), options["tolerance_v"].as<double>(),
             options["max_steps"].as<int>()});
        YAML::Node output;
        output["voltages_v"] = result.voltages;
        output["elapsed_s"] = result.elapsed;
        output["capacitor_charge_change_c"] = result.capacitorChargeChange;
        output["initial_stored_energy_j"] = result.initialStoredEnergy;
        output["final_stored_energy_j"] = result.finalStoredEnergy;
        output["left_source_charge_c"] = result.leftSourceCharge;
        output["right_source_charge_c"] = result.rightSourceCharge;
        output["maximum_estimated_local_error_v"] = result.maximumEstimatedLocalError;
        output["accepted_steps"] = result.acceptedSteps;
        output["rejected_steps"] = result.rejectedSteps;
        YAML::Emitter emitter;
        emitter.SetDoublePrecision(17);
        emitter << output;
        if (!emitter.good()) throw std::runtime_error("probe serialization failed");
        std::cout << emitter.c_str() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "NandRcLadderProbe: " << error.what() << '\n';
        return 1;
    }
}
