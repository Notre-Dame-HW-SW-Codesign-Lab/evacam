#include <cmath>
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
        const auto capacitances = input["capacitances_f"].as<std::vector<double>>();
        const auto shunts = input["shunt_conductances_s"]
                ? input["shunt_conductances_s"].as<std::vector<double>>()
                : std::vector<double>(capacitances.size(), 0);
        const NandRcOptions solver{options["max_step_s"].as<double>(),
                options["tolerance_v"].as<double>(), options["max_steps"].as<int>()};
        if (input["phases"]) {
            const auto resistances = input["series_resistances_ohm"].as<std::vector<double>>();
            auto voltages = input["initial_voltages_v"].as<std::vector<double>>();
            std::vector<std::size_t> observed;
            if (input["observed_nodes"]) {
                observed = input["observed_nodes"].as<std::vector<std::size_t>>();
                for (auto index : observed) if (index >= capacitances.size()) {
                    throw std::invalid_argument("observed node outside circuit");
                }
            }
            YAML::Node output;
            double elapsed = 0, leftCharge = 0, rightCharge = 0, shuntCharge = 0;
            int attempts = 0;
            for (const auto &phase : input["phases"]) {
                const auto left = Boundary(phase["left"]), right = Boundary(phase["right"]);
                double previous = 0;
                const double duration = phase["duration_s"].as<double>();
                const auto times = phase["observation_times_s"].as<std::vector<double>>();
                if (!std::isfinite(duration) || duration < 0 || times.empty()
                        || times.front() != 0 || times.back() != duration) {
                    throw std::invalid_argument("phase requires finite duration and observations at both endpoints");
                }
                for (std::size_t index = 0; index < times.size(); index++) {
                    const double time = times[index];
                    if (!std::isfinite(time) || time < 0 || time > duration
                            || (index > 0 && time <= previous)) {
                        throw std::invalid_argument("observation times must be finite and strictly increasing");
                    }
                    auto remaining = solver;
                    remaining.maxSteps -= attempts;
                    if (remaining.maxSteps <= 0) throw std::runtime_error("waveform maxSteps exceeded");
                    const auto result = NandRcLadder::Solve(capacitances, resistances, voltages,
                            time - previous, left, right, remaining, shunts);
                    voltages = result.voltages;
                    attempts += result.acceptedSteps + result.rejectedSteps;
                    leftCharge += result.leftSourceCharge;
                    rightCharge += result.rightSourceCharge;
                    shuntCharge += result.groundShuntCharge;
                    YAML::Node sample;
                    sample["time_s"] = elapsed + time;
                    sample["phase_time_s"] = time;
                    if (observed.empty()) sample["voltages_v"] = voltages;
                    else for (auto node : observed) sample["voltages_v"].push_back(voltages[node]);
                    sample["left_current_a"] = left.connected ? (left.voltage - voltages.front()) / left.resistance : 0;
                    sample["right_current_a"] = right.connected ? (right.voltage - voltages.back()) / right.resistance : 0;
                    sample["left_source_charge_c"] = leftCharge;
                    sample["right_source_charge_c"] = rightCharge;
                    sample["ground_shunt_charge_c"] = shuntCharge;
                    output["samples"].push_back(sample);
                    previous = time;
                }
                elapsed += duration;
            }
            output["attempted_steps"] = attempts;
            output["final_voltages_v"] = voltages;
            YAML::Emitter emitter;
            emitter.SetDoublePrecision(17);
            emitter << output;
            if (!emitter.good()) throw std::runtime_error("probe serialization failed");
            std::cout << emitter.c_str() << '\n';
            return 0;
        }
        const auto result = NandRcLadder::Solve(
            input["capacitances_f"].as<std::vector<double>>(),
            input["series_resistances_ohm"].as<std::vector<double>>(),
            input["initial_voltages_v"].as<std::vector<double>>(),
            input["duration_s"].as<double>(), Boundary(input["left"]), Boundary(input["right"]),
            {options["max_step_s"].as<double>(), options["tolerance_v"].as<double>(),
             options["max_steps"].as<int>()}, shunts);
        YAML::Node output;
        output["voltages_v"] = result.voltages;
        output["elapsed_s"] = result.elapsed;
        output["capacitor_charge_change_c"] = result.capacitorChargeChange;
        output["initial_stored_energy_j"] = result.initialStoredEnergy;
        output["final_stored_energy_j"] = result.finalStoredEnergy;
        output["left_source_charge_c"] = result.leftSourceCharge;
        output["right_source_charge_c"] = result.rightSourceCharge;
        output["ground_shunt_charge_c"] = result.groundShuntCharge;
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
