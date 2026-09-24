"""Independent circuit identities for the numerical RC reference, no EvaCAM imports."""

import importlib.util
from pathlib import Path
import unittest

import numpy as np
from scipy.linalg import expm


MODULE_PATH = Path(__file__).resolve().parents[1] / "scripts" / "nand_rc_reference.py"
SPEC = importlib.util.spec_from_file_location("nand_rc_reference", MODULE_PATH)
REFERENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REFERENCE)
RcLadderReference = REFERENCE.RcLadderReference
elmore_last_node = REFERENCE.elmore_last_node


class RcReferenceTest(unittest.TestCase):
    def test_single_rc_analytic_limit(self):
        circuit = RcLadderReference([10e3], [20e-15])
        tau = 200e-12
        times = np.array([0, 0.1, 1, 2, 10]) * tau
        np.testing.assert_allclose(circuit.voltages(times, [0.8])[:, 0],
                                   0.8 * np.exp(-times / tau), rtol=1e-14)
        np.testing.assert_allclose(circuit.first_moments, [tau], rtol=1e-14)
        self.assertAlmostEqual(elmore_last_node([10e3], [20e-15]) / tau, 1)

    def test_two_equal_rc_known_two_pole_response(self):
        resistance, capacitance = 2e3, 1e-15
        circuit = RcLadderReference([resistance, resistance], [capacitance, capacitance])
        normalized_time = np.array([0, 0.01, 0.5, 1, 3, 10])
        root5 = np.sqrt(5)
        slow, fast = (3 - root5) / 2, (3 + root5) / 2
        expected = (fast * np.exp(-slow * normalized_time)
                    - slow * np.exp(-fast * normalized_time)) / root5
        actual = circuit.voltages(normalized_time * resistance * capacitance)[:, -1]
        np.testing.assert_allclose(actual, expected, rtol=1e-13, atol=1e-14)
        np.testing.assert_allclose(circuit.first_moments / (resistance * capacitance),
                                   [2, 3], rtol=1e-14)
        # The first moment is exact, but its one-pole waveform is not.
        self.assertGreater(abs(actual[3] - np.exp(-1 / 3)), 0.05)

    def test_independent_dense_matrix_exponential_and_nonuniform_initial_state(self):
        resistance = np.array([1000, 9000, 3000, 7000], dtype=float)
        capacitance = np.array([1, 2, 3, 20], dtype=float) * 1e-15
        circuit = RcLadderReference(resistance, capacitance)
        # Assemble G from edge incidences, independently of the reference's
        # diagonal formulas and symmetric modal coordinate transformation.
        incidence = np.eye(4)
        for edge in range(1, 4):
            incidence[edge, edge - 1] = -1
        conductance = incidence.T @ np.diag(1 / resistance) @ incidence
        generator = -np.diag(1 / capacitance) @ conductance
        initial = np.array([0.2, 0.8, 0.4, 0.6])
        times = np.array([0, 1e-12, 1e-10, 1e-9])
        expected = np.array([expm(generator * time) @ initial for time in times])
        np.testing.assert_allclose(circuit.voltages(times, initial), expected,
                                   rtol=1e-11, atol=1e-13)
        independent_moment = np.linalg.solve(conductance, capacitance)
        np.testing.assert_allclose(circuit.first_moments, independent_moment, rtol=1e-13)
        self.assertAlmostEqual(circuit.first_moments[-1]
                               / elmore_last_node(resistance, capacitance), 1)

    def test_floating_source_precharge_boundary_and_capacitor_energy(self):
        # Drain boundary--1kohm--0.05fF--5kohm--0.05fF--5kohm--1fF(source).
        circuit = RcLadderReference([1000, 5000, 5000], [0.05e-15, 0.05e-15, 1e-15])
        times = np.array([0, 1e-12, 1e-11, 1e-9])
        charging = circuit.charging_voltages(times, 0.8)
        np.testing.assert_allclose(charging[0], 0, atol=0)
        np.testing.assert_allclose(charging[-1], 0.8, rtol=1e-13)
        self.assertTrue(np.all(np.diff(charging, axis=0) >= -1e-14))
        self.assertGreater(charging[1, 0], charging[1, -1])
        decay = circuit.voltages(times, [0.8] * 3)
        stored_energy = 0.5 * np.sum(circuit.capacitances * decay ** 2, axis=1)
        self.assertTrue(np.all(np.diff(stored_energy) <= 0))

    def test_nand_moment_identity_and_initial_condition(self):
        cells = np.array([1e4, 5e3] * 34)
        resistance = np.concatenate(([1000], cells, [1000]))
        capacitance = np.concatenate(([1e-15], np.full(68, 0.05e-15), [20e-15]))
        circuit = RcLadderReference(resistance, capacitance)
        self.assertAlmostEqual(circuit.first_moments[-1]
                               / elmore_last_node(resistance, capacitance), 1, places=11)
        result = circuit.voltages([0, 50e-9], np.full(70, 0.8))
        np.testing.assert_array_equal(result[0], np.full(70, 0.8))
        self.assertTrue(np.all(result[1] >= 0))
        self.assertTrue(np.all(result[1] <= 0.8))

    def test_long_string_margin_counterexample_and_off_state_conditioning(self):
        # Controlled synthetic network: no wire parasitics, uniform 0.8 V
        # initial state. This validates the linear RC reference, not silicon.
        length = 512
        resistance = np.concatenate(([1000], np.tile([1e4, 5e3], length // 2), [1000]))
        capacitance = np.concatenate(([1e-15], np.full(length, 0.05e-15), [20e-15]))
        circuit = RcLadderReference(resistance, capacitance)
        initial = np.full(length + 2, 0.8)
        exact_match = circuit.voltages([50e-9], initial)[0, -1]
        approximate_match = 0.8 * np.exp(-50e-9 / elmore_last_node(resistance, capacitance))
        self.assertAlmostEqual(exact_match, 0.582867925882892, places=8)
        self.assertAlmostEqual(circuit.first_moments[-1]
                               / elmore_last_node(resistance, capacitance), 1, places=9)
        # The off-device case has a much wider span of modal rates. Check
        # finite monotone voltages and the independent first-moment solve.
        mismatch = np.full(length + 2, 5e3)
        mismatch[[0, -1]] = 1000
        mismatch[1] = 1e4  # Fixed valid pair; all other query bits masked.
        mismatch[-2] = 1e9  # Last data device is the single blocking device.
        blocked = RcLadderReference(mismatch, capacitance)
        blocked_voltage = blocked.voltages([0, 50e-9, 20e-6, 200e-6], initial)[:, -1]
        self.assertTrue(np.all(np.isfinite(blocked_voltage)))
        self.assertTrue(np.all(np.diff(blocked_voltage) < 0))
        self.assertAlmostEqual(blocked.first_moments[-1]
                               / elmore_last_node(mismatch, capacitance), 1, places=8)
        approximate_miss = 0.8 * np.exp(-50e-9 / elmore_last_node(mismatch, capacitance))
        reference = (approximate_match + approximate_miss) / 2
        approximate_margin = (approximate_miss - approximate_match) / 2 - 0.01
        tested_match_margin = reference - exact_match - 0.01
        self.assertGreater(approximate_margin, 0.1)
        self.assertLess(tested_match_margin, 0.1)
        self.assertGreater(tested_match_margin, 0)
        # A witness for this match pattern suffices to refute the advertised
        # margin; no exhaustive exact worst-pattern ordering is assumed.

    def test_fixed_precharge_time_does_not_scale_to_long_strings(self):
        source_voltages = []
        for length in (68, 512):
            circuit = RcLadderReference([1000] + [5000] * length,
                                       [0.05e-15] * length + [1e-15])
            source_voltages.append(circuit.charging_voltages([5e-9], 0.8)[0, -1])
        self.assertGreater(source_voltages[0], 0.99 * 0.8)
        self.assertLess(source_voltages[1], 0.02 * 0.8)

    def test_invalid_networks_and_arguments(self):
        for resistance, capacitance in [([], []), ([1, 2], [1]), ([0], [1]),
                                        ([1], [0]), ([-1], [1]), ([np.inf], [1]),
                                        ([1], [np.nan]), ([[1]], [1])]:
            with self.subTest(resistance=resistance, capacitance=capacitance):
                with self.assertRaises(ValueError):
                    RcLadderReference(resistance, capacitance)
                with self.assertRaises(ValueError):
                    elmore_last_node(resistance, capacitance)
        circuit = RcLadderReference([1], [1])
        for times in [[-1], [np.nan], [[1, 2]]]:
            with self.assertRaises(ValueError):
                circuit.voltages(times)
        with self.assertRaises(ValueError):
            circuit.voltages([1], [1, 2])
        with self.assertRaises(ValueError):
            circuit.charging_voltages([1], np.nan)


if __name__ == "__main__":
    unittest.main()
