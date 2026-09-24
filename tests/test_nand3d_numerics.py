"""Independent checks of the compiled transient solver against matrix exponentials.

These verify the stated linear circuit and do not calibrate physical flash.
The reference uses dense edge-incidence assembly and matrix exponentiation,
independently of the C++ adaptive implicit tridiagonal integration.
"""

import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np
from scipy.linalg import expm
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from nand_rc_reference import RcLadderReference

PROBE = ROOT / "test-bin/NandRcLadderProbe"
MODEL_PROBE = ROOT / "test-bin/NandValidationProbe"


def reference(circuit):
    c = np.array(circuit["capacitances_f"])
    count = len(c)
    incidence = np.zeros((len(circuit["series_resistances_ohm"]), count))
    for edge in range(len(incidence)):
        incidence[edge, edge] = 1
        incidence[edge, edge + 1] = -1
    g = incidence.T @ np.diag(1 / np.array(circuit["series_resistances_ohm"])) @ incidence
    current = np.zeros(count)
    # Additional states integrate the charge delivered by each boundary.
    generator = np.zeros((count + 3, count + 3))
    for source_index, side in enumerate(("left", "right")):
        boundary = circuit.get(side, {})
        if not boundary.get("connected", False):
            continue
        node = 0 if side == "left" else count - 1
        conductance = 1 / boundary["resistance_ohm"]
        g[node, node] += conductance
        current[node] += conductance * boundary["voltage_v"]
        generator[count + source_index, node] = -conductance
        generator[count + source_index, -1] = conductance * boundary["voltage_v"]
    generator[:count, :count] = -g / c[:, None]
    generator[:count, -1] = current / c
    initial = np.r_[circuit["initial_voltages_v"], 0., 0., 1.]
    return expm(generator * circuit["duration_s"]) @ initial


class CompiledNandNumericsTest(unittest.TestCase):
    def setUp(self):
        if not PROBE.is_file():
            self.fail("run make test-nand3d-numerics to build the compiled probe")
        self.directory = tempfile.TemporaryDirectory(prefix="nand3d-numerics-")
        self.addCleanup(self.directory.cleanup)

    def solve(self, circuit):
        path = Path(self.directory.name) / "circuit.yaml"
        path.write_text(yaml.safe_dump(circuit))
        process = subprocess.run([str(PROBE), str(path)], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        return yaml.safe_load(process.stdout)

    def circuit(self, capacitances, resistances, initial, duration, left=None, right=None):
        return {"capacitances_f": list(capacitances), "series_resistances_ohm": list(resistances),
                "initial_voltages_v": list(initial), "duration_s": duration,
                "left": left or {"connected": False}, "right": right or {"connected": False},
                "solver": {"max_step_s": duration / 10, "tolerance_v": 1e-9, "max_steps": 100000}}

    def test_dense_reference_with_two_driven_boundaries_and_nonuniform_initial_state(self):
        circuit = self.circuit([1e-15, 3e-15, 2e-15, 20e-15], [1e4, 5e3, 7e3],
                               [.2, .7, .1, .5], 2e-10,
                               {"connected": True, "resistance_ohm": 2e3, "voltage_v": .1},
                               {"connected": True, "resistance_ohm": 1e4, "voltage_v": .8})
        expected = reference(circuit)
        actual = self.solve(circuit)
        np.testing.assert_allclose(actual["voltages_v"], expected[:4], rtol=0, atol=2e-6)
        np.testing.assert_allclose([actual["left_source_charge_c"], actual["right_source_charge_c"]],
                                   expected[4:6], rtol=3e-5, atol=1e-20)
        charge = np.dot(circuit["capacitances_f"],
                        np.array(actual["voltages_v"]) - circuit["initial_voltages_v"])
        self.assertAlmostEqual((actual["left_source_charge_c"] + actual["right_source_charge_c"]
                                - charge) / 1e-15, 0, places=6)

    def test_floating_network_preserves_charge_and_relaxes_voltage(self):
        circuit = self.circuit([1e-15, 3e-15, 2e-15], [1e4, 5e3], [.2, .7, .1], 1e-9)
        actual = self.solve(circuit)
        expected = reference(circuit)
        np.testing.assert_allclose(actual["voltages_v"], expected[:3], rtol=0, atol=2e-6)
        self.assertAlmostEqual(actual["capacitor_charge_change_c"] / 1e-15, 0, places=6)
        self.assertLess(actual["final_stored_energy_j"], actual["initial_stored_energy_j"])

    def test_precharge_then_evaluate_carries_actual_node_state(self):
        count = 68
        c = [1e-15] + [.05e-15] * count + [20e-15]
        precharge = self.circuit(c, [5e3] * count + [1e3], [0.] * len(c), 5e-9,
                                right={"connected": True, "resistance_ohm": 2e3, "voltage_v": .8})
        exact_precharge = reference(precharge)[:len(c)]
        actual_precharge = self.solve(precharge)
        np.testing.assert_allclose(actual_precharge["voltages_v"], exact_precharge, rtol=0, atol=3e-6)
        # Supplying rail charge, rather than half C*V², captures source energy.
        expected_energy = .8 * np.dot(c, exact_precharge)
        self.assertAlmostEqual(actual_precharge["right_source_charge_c"] * .8
                               / expected_energy, 1, places=5)
        evaluation = self.circuit(c, [1e4, 5e3] * (count // 2) + [1e3],
                                 actual_precharge["voltages_v"], 50e-9,
                                 left={"connected": True, "resistance_ohm": 1e3, "voltage_v": 0.})
        reference_evaluation = copy.deepcopy(evaluation)
        reference_evaluation["initial_voltages_v"] = exact_precharge.tolist()
        expected = reference(reference_evaluation)[:len(c)]
        actual = self.solve(evaluation)
        np.testing.assert_allclose(actual["voltages_v"], expected, rtol=0, atol=3e-6)

    def test_512_wordline_margin_counterexample_and_tolerance_convergence(self):
        count = 512
        c = [1e-15] + [.05e-15] * count + [20e-15]
        r = [1e4, 5e3] * (count // 2) + [1e3]
        circuit = self.circuit(c, r, [.8] * len(c), 50e-9,
                               left={"connected": True, "resistance_ohm": 1e3, "voltage_v": 0.})
        expected = RcLadderReference([1e3] + r, c).voltages([50e-9], [.8] * len(c))[0, -1]
        fine = self.solve(circuit)
        coarse_input = copy.deepcopy(circuit)
        coarse_input["solver"]["tolerance_v"] = 1e-5
        coarse = self.solve(coarse_input)
        fine_error = abs(fine["voltages_v"][-1] - expected)
        coarse_error = abs(coarse["voltages_v"][-1] - expected)
        self.assertLess(fine_error, 3e-6)
        self.assertLess(fine_error, coarse_error)
        self.assertGreater(fine["accepted_steps"], coarse["accepted_steps"])
        self.assertLess(.6681125434317466 - fine["voltages_v"][-1] - .01, .1)

    def test_off_device_stiffness_matches_independent_modal_solution(self):
        count = 68
        c = [1e-15] + [.05e-15] * count + [20e-15]
        for position in (0, count - 1):
            with self.subTest(position=position):
                r = [5e3] * count + [1e3]
                r[position] = 1e9
                circuit = self.circuit(c, r, [.8] * len(c), 50e-9,
                                       left={"connected": True, "resistance_ohm": 1e3, "voltage_v": 0.})
                expected = RcLadderReference([1e3] + r, c).voltages([50e-9], [.8] * len(c))[0]
                actual = self.solve(circuit)
                np.testing.assert_allclose(actual["voltages_v"], expected, rtol=0, atol=3e-6)


class EncodedNand3dReferenceTest(unittest.TestCase):
    def test_actual_backend_encoding_precharge_rounds_and_margin(self):
        """Reassemble the encoded device from reported inputs, without C++ internals."""
        self.assertTrue(MODEL_PROBE.is_file(), "build with make test-nand3d-numerics")
        with tempfile.TemporaryDirectory(prefix="nand3d-model-reference-") as directory:
            path = Path(directory)
            base = ROOT / "config/NAND_3D_TCAM"
            for mux in (1, 2):
                with self.subTest(mux=mux):
                    architecture = yaml.safe_load((base / "NAND_3D_TCAM.architecture.yaml").read_text())
                    architecture["organization"]["mux"]["sense_amp"] = mux
                    (path / "architecture.yaml").write_text(yaml.safe_dump(architecture))
                    config = yaml.safe_load((base / "NAND_3D_TCAM.config.yaml").read_text())
                    config["architecture"] = str(path / "architecture.yaml")
                    config["cell"] = str(base / "NAND_3D_TCAM.cell.yaml")
                    config["technology"] = str(ROOT / "config/lib/technology/cmos.legacy.yaml")
                    (path / "config.yaml").write_text(yaml.safe_dump(config))
                    process = subprocess.run([str(MODEL_PROBE), str(path / "config.yaml")],
                                             capture_output=True, text=True)
                    self.assertEqual(process.returncode, 0, process.stderr)
                    report = yaml.safe_load(process.stdout)
                    self.assertEqual(report["metadata"]["model_backend"], "transient_rc")
                    self.assertEqual(report["metadata"]["device_validation"], "not_performed_by_evacam")
                    device, geometry = report["device"], report["geometry"]
                    layers = geometry["data_wordlines"] + device["dummy_layers"]
                    cap = [device["capacitance_source_f"]] + [device["capacitance_internal_f"]] * layers
                    cap.append(device["capacitance_bitline_f"])
                    all_pass = [device["resistance_pass_ohm"]] * layers + [device["resistance_select_ohm"]]
                    ground = {"connected": True, "resistance_ohm": device["resistance_select_ohm"], "voltage_v": 0.}
                    driver = {"connected": True, "resistance_ohm": device["precharge_driver_resistance_ohm"],
                              "voltage_v": device["voltage_precharge_v"]}
                    circuit = {"capacitances_f": cap, "series_resistances_ohm": all_pass}
                    first_charge = None
                    for pattern in report["patterns"]:
                        # Recreate physical L/H states and read/pass query biases,
                        # instead of selecting resistors with the C++ Encode formula.
                        low, high = device["threshold_low_v"], device["threshold_high_v"]
                        levels = [low if pattern["valid"] else high, high if pattern["valid"] else low]
                        biases = [device["voltage_read_v"], device["voltage_pass_v"]]
                        for stored, query in zip(pattern["stored"], pattern["query"]):
                            levels += [low, low] if stored == -1 else ([low, high] if stored == 0 else [high, low])
                            biases += [device["voltage_pass_v"] if query == -1 or rail != query
                                       else device["voltage_read_v"] for rail in (0, 1)]
                        encoded = [device["resistance_pass_ohm"] if bias == device["voltage_pass_v"] else
                                   device["resistance_read_on_ohm"] if bias > level else device["resistance_off_ohm"]
                                   for bias, level in zip(biases, levels)]
                        source_dummy = device["dummy_layers"] // 2
                        evaluation_resistance = all_pass.copy()
                        evaluation_resistance[source_dummy:source_dummy + len(encoded)] = encoded
                        state = np.zeros(len(cap))
                        decision_voltages = []
                        for _ in range(mux):
                            circuit.update(series_resistances_ohm=all_pass, initial_voltages_v=state.tolist(),
                                           duration_s=device["precharge_latency_s"], left={}, right=driver)
                            charged = reference(circuit)
                            if first_charge is None:
                                first_charge = charged[len(cap) + 1]
                            circuit.update(series_resistances_ohm=evaluation_resistance,
                                           initial_voltages_v=charged[:len(cap)].tolist(),
                                           duration_s=device["decision_time_s"], left=ground, right={})
                            evaluated = reference(circuit)[:len(cap)]
                            decision_voltages.append(evaluated[-1])
                            circuit.update(series_resistances_ohm=all_pass, initial_voltages_v=evaluated.tolist(),
                                           duration_s=device["recovery_latency_s"], left=ground,
                                           right={**driver, "voltage_v": 0.})
                            state = reference(circuit)[:len(cap)]
                        expected = (max if pattern["ideal_hit"] else min)(decision_voltages)
                        self.assertAlmostEqual(pattern["model_voltage_v"], expected, delta=3e-6)
                        expected_conductance = 1 / (sum(evaluation_resistance) + ground["resistance_ohm"])
                        self.assertAlmostEqual(pattern["model_conductance_s"] / expected_conductance, 1, places=12)
                        expected_margin = ((report["model"]["reference_voltage_v"] - expected) if pattern["ideal_hit"]
                                           else (expected - report["model"]["reference_voltage_v"])) - device["sense_offset_v"]
                        self.assertAlmostEqual(pattern["model_margin_v"], expected_margin, delta=3e-6)
                        self.assertFalse(pattern["time_constant_inference_available"])
                    if mux == 1:
                        expected_energy = geometry["entries"] * driver["voltage_v"] * first_charge / device["supply_efficiency"]
                        actual_energy = report["model"]["search_energy_breakdown_j"]["bitline_and_internal_precharge"]
                        self.assertAlmostEqual(actual_energy / expected_energy, 1, places=5)


if __name__ == "__main__":
    unittest.main()
