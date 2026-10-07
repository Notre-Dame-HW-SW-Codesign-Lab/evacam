"""Independent numerical checks of the experimental law, not paper calibration.

The reference integrates local conductance by quadrature, then solves KCL with
SciPy's finite-difference Powell hybrid solver. It uses neither the production
potential-difference evaluation nor its analytical Jacobian/damped Newton loop.
"""

import copy
import json
from pathlib import Path
import subprocess
import unittest

import numpy as np
from scipy.integrate import quad
from scipy.optimize import root
from scipy.special import expit
import yaml

PROBE = Path(__file__).resolve().parents[1] / "test-bin/NandNonlinearStringProbe"


def device(gate, threshold=1., beta=2e-5):
    return {"gate_v": gate, "threshold_v": threshold, "parameters": {
        "beta_a_per_v2": beta, "slope_factor": 1.5, "leakage_conductance_s": 1e-12,
        "temperature_k": 300., "channel_diameter_m": 47e-9, "gate_length_m": 50e-9,
        "minimum_voltage_v": -2., "maximum_voltage_v": 8.,
        "minimum_threshold_v": -1., "maximum_threshold_v": 5.}}


def current(cell, source, drain):
    p = cell["parameters"]
    scale = 2 * p["slope_factor"] * 8.617333262145e-5 * p["temperature_k"]

    def conductance(voltage):
        x = (cell["gate_v"] - cell["threshold_v"] - voltage) / scale
        return p["beta_a_per_v2"] * scale * np.logaddexp(0., x) * expit(x)

    integral, _ = quad(conductance, source, drain, epsabs=1e-24, epsrel=3e-12)
    return integral + p["leakage_conductance_s"] * (drain - source)


def reference(circuit):
    cells = circuit["devices"]
    source, drain = circuit["source_v"], circuit["drain_v"]
    scale = max(abs(current(cell, source, drain)) for cell in cells)

    def residual(interior):
        nodes = np.r_[source, interior, drain]
        currents = [current(cell, nodes[i], nodes[i + 1]) for i, cell in enumerate(cells)]
        return np.diff(currents) / scale

    initial = np.linspace(source, drain, len(cells) + 1)[1:-1]
    solution = root(residual, initial, method="hybr", options={"xtol": 1e-10})
    if np.max(np.abs(residual(solution.x))) > 1e-10:
        raise AssertionError(f"independent reference did not converge: {solution.message}")
    nodes = np.r_[source, solution.x, drain]
    return nodes, current(cells[-1], nodes[-2], nodes[-1])


class NandNonlinearNumericsTest(unittest.TestCase):
    def probe(self, circuit, success=True):
        process = subprocess.run([str(PROBE), "-"], input=json.dumps(circuit),
                                 capture_output=True, text=True, timeout=30)
        if success:
            self.assertEqual(process.returncode, 0, process.stderr)
            return yaml.safe_load(process.stdout)
        self.assertNotEqual(process.returncode, 0)
        self.assertEqual(process.stdout, "")
        return process.stderr

    def test_single_cell_matches_conductance_quadrature(self):
        for gate in [-.5, .8, 1., 1.2, 2., 6.]:
            for source, drain in [(0., .7), (.7, 0.), (.1, .1 + 1e-12)]:
                with self.subTest(gate=gate, source=source, drain=drain):
                    cell = device(gate)
                    actual = self.probe({"source_v": source, "drain_v": drain, "devices": [cell]})
                    expected = current(cell, source, drain)
                    np.testing.assert_allclose(actual["current_a"], expected, rtol=1e-10, atol=1e-25)
                    self.assertEqual(actual["calibration_status"], "uncalibrated")

    def test_heterogeneous_strings_match_independent_kcl_solution(self):
        for count in [2, 3, 8]:
            for position in sorted({0, count // 2, count - 1}):
                for gate in [.9, 1.4, 2.5]:
                    with self.subTest(count=count, position=position, gate=gate):
                        cells = [device(6., 1. + .02 * i, 2e-5 * (1 + .1 * i)) for i in range(count)]
                        cells[position]["gate_v"] = gate
                        circuit = {"source_v": 0., "drain_v": .7, "devices": cells,
                                   "solver": {"absolute_current_tolerance_a": 1e-17,
                                              "relative_current_tolerance": 1e-10,
                                              "voltage_tolerance_v": 1e-12,
                                              "max_iterations": 100, "max_backtracks": 40}}
                        expected_nodes, expected_current = reference(circuit)
                        actual = self.probe(circuit)
                        np.testing.assert_allclose(actual["voltages_v"], expected_nodes, atol=3e-9, rtol=0)
                        np.testing.assert_allclose(actual["current_a"], expected_current, atol=2e-15, rtol=2e-7)
                        self.assertLessEqual(actual["maximum_voltage_correction_v"], 1e-10)
                        reverse = {**circuit, "source_v": .7, "drain_v": 0., "devices": list(reversed(cells))}
                        opposite = self.probe(reverse)
                        np.testing.assert_allclose(opposite["voltages_v"], expected_nodes[::-1], atol=3e-9, rtol=0)
                        np.testing.assert_allclose(opposite["current_a"], -expected_current, atol=2e-15, rtol=2e-7)

    def test_failures_do_not_emit_success_results(self):
        circuit = {"source_v": 0., "drain_v": .7, "devices": [device(1.1), device(6.)]}
        for field, bad in [("devices", []), ("drain_v", 9.)]:
            invalid = copy.deepcopy(circuit)
            invalid[field] = bad
            self.probe(invalid, success=False)
        limited = copy.deepcopy(circuit)
        limited["solver"] = {"absolute_current_tolerance_a": 1e-14, "relative_current_tolerance": 1e-8,
                             "voltage_tolerance_v": 1e-10, "max_iterations": 1, "max_backtracks": 40}
        self.assertIn("iteration limit", self.probe(limited, success=False))
        missing = copy.deepcopy(circuit)
        del missing["devices"][0]["parameters"]["temperature_k"]
        self.probe(missing, success=False)


if __name__ == "__main__":
    unittest.main()
