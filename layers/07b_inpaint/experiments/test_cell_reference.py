"""Offline reference checks; model-dependent cases take QRSCAN_CELL_WEIGHTS."""
import os
import unittest
import numpy as np
from cell_reference import Scorer


def sample():
    return np.random.default_rng(20261123).integers(0, 256, size=(150, 150, 3), dtype=np.uint8)


class Contracts(unittest.TestCase):
    def test_reject_invalid_inputs_before_inference(self):
        reader = Scorer.__new__(Scorer)
        for im, n, polarity in [(sample(), 24, 1), (sample(), 25, 0), (sample()[:, :, 0], 25, 1)]:
            with self.subTest(n=n, polarity=polarity), self.assertRaises(ValueError):
                reader.predict(im, n, polarity)

    def test_missing_weights(self):
        with self.assertRaises(FileNotFoundError):
            Scorer('/nonexistent/qrscan_cell_reference_model.npz')

    @unittest.skipUnless(os.getenv('QRSCAN_CELL_WEIGHTS'), 'external model not configured')
    def test_frozen_reference(self):
        reader = Scorer(os.environ['QRSCAN_CELL_WEIGHTS'])
        p = reader.predict(sample(), 25, 1)
        self.assertEqual(p.shape, (25, 25))
        self.assertTrue(np.isfinite(p).all() and (p >= 0).all() and (p <= 1).all())
        # Captured from the released reader with this RGB input and external frozen weights.
        np.testing.assert_allclose(p[[0, 3, 12, 20, 24], [0, 8, 12, 15, 24]],
                                   [0.35572219, 0.14533639, 0.96400380, 0.84452462, 0.75177753], atol=3e-5)
        np.testing.assert_allclose(reader.predict(sample(), 25, -1), 1-p, atol=2e-6)


if __name__ == '__main__':
    unittest.main()
