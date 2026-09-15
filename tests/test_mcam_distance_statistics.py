import itertools
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT))
import evacam_py
from plot_mcam_distance_statistics import completion_counts, sample_vectors, summarize, varied_conductances
from plot_mcam_voltage import prepare_config


class DistanceStatisticsTests(unittest.TestCase):
    def test_conditional_counts_and_samples(self):
        counts = completion_counts(3)
        expected = np.zeros(counts.shape[1], dtype=np.int64)
        for vector in itertools.product(range(8), repeat=3):
            expected[sum(value * value for value in vector)] += 1
        np.testing.assert_array_equal(counts[-1], expected)
        vectors = sample_vectors(completion_counts(2), 25, 12000, np.random.default_rng(9))
        self.assertTrue(np.all(np.sum(vectors * vectors, axis=1) == 25))
        for pair in ((0, 5), (5, 0), (3, 4), (4, 3)):
            probability = np.mean(np.all(vectors == pair, axis=1))
            self.assertAlmostEqual(probability, 0.25, delta=0.025)
        with self.assertRaises(ValueError):
            sample_vectors(completion_counts(1), 2, 10, np.random.default_rng(1))

    def test_variation_and_output_statistics(self):
        vectors = np.zeros((20000, 1), dtype=np.int64)
        resistance = np.array([100.0])
        varied = varied_conductances(vectors, resistance, np.array([0.1]), np.random.default_rng(9))
        samples = 1 / varied
        self.assertTrue(np.all((samples >= 70) & (samples <= 130)))
        self.assertGreater(np.std(samples), 9)
        self.assertLess(np.std(samples), 11)
        nominal = varied_conductances(vectors, resistance, np.array([0.0]), np.random.default_rng(9))
        self.assertTrue(np.all(nominal == 0.01))
        stats = summarize(np.array([1.0, 2.0, 3.0]))
        self.assertEqual(stats["mean_voltage_v"], 2)
        self.assertEqual(stats["stddev_voltage_v"], 1)
        self.assertEqual(stats["lower_3sigma_voltage_v"], -1)

    def test_native_voltage_conversion(self):
        source = ROOT / "config/2FeFET_MCAM_variation/stdev05/2FeFET_MCAM_8x8.config.yaml"
        with tempfile.TemporaryDirectory(prefix="evacam-distance-statistics-") as temporary:
            config, _ = prepare_config(source, Path(temporary), samples=2)
            matcher = evacam_py.EvaCAMMatch(str(config))
            results = matcher.evaluate_distance_samples([1, 2, 3, 0, 0, 0, 0, 0], [0] * 8)
            self.assertEqual(len(results), 2)
            actual = matcher.sense_mcam_conductances([result.matchline_conductance for result in results])
            np.testing.assert_allclose(actual, [result.matchline_voltage for result in results], rtol=1e-13)
            self.assertEqual(matcher.sense_mcam_conductances([]), [])
            for value in (0, -1, float("nan"), float("inf")):
                with self.assertRaises(ValueError):
                    matcher.sense_mcam_conductances([value])


if __name__ == "__main__":
    unittest.main()
