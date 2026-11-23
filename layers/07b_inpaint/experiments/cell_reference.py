"""Offline cell-recovery reference. Inputs are already aligned; no decoding or acceptance."""

import argparse
from pathlib import Path
import cv2 as cv
import numpy as np


def sigmoid(x):
    return 1 / (1 + np.exp(-np.clip(x, -80, 80)))


class Scorer:
    def __init__(self, weights_path):
        with np.load(weights_path, allow_pickle=False) as f:
            self.weights = {k: f[k] for k in f.files}

    def conv(self, x, name, stride=1, activate=True):
        weights = self.weights[name + ".weight"]
        kernel = weights.shape[-1]
        channels = x.shape[-1]
        if kernel == 3:
            x = np.pad(x, ((1, 1), (1, 1), (0, 0)))
        windows = np.lib.stride_tricks.sliding_window_view(x, (kernel, kernel), axis=(0, 1))[
            ::stride, ::stride
        ]
        height, width = windows.shape[:2]
        patches = np.ascontiguousarray(windows.reshape(height * width, channels * kernel * kernel))
        # BLAS uses one thread in the desktop launcher. Channels precede kernel offsets.
        logits = (
            patches @ weights.reshape(len(weights), -1).T + self.weights[name + ".bias"]
        ).reshape(height, width, -1)
        return logits * sigmoid(logits) if activate else logits

    def stack(self, x, name):
        for i in (0, 2, 4):
            x = self.conv(x, f"{name}.{i}", activate=i != 4)
        return x

    def predict(self, rgb, n, pol, check=False):
        if not isinstance(n, int) or n < 21 or n > 177 or (n-21) % 4:
            raise ValueError("expected a standard QR side (21, 25, ..., 177 modules)")
        if pol not in (-1, 1):
            raise ValueError("polarity must be -1 or +1")
        if not isinstance(rgb, np.ndarray) or rgb.dtype != np.uint8 or rgb.ndim != 3 or rgb.shape[2] != 3 or not rgb.size:
            raise ValueError("expected a nonempty RGB uint8 patch")
        x = cv.resize(rgb, (6 * n, 6 * n), interpolation=cv.INTER_AREA).astype(np.float32) / 255
        cells = x.reshape(n, 6, n, 6, 3)
        center = cells[:, 1:5, :, 1:5].mean(axis=(1, 3))
        low, high = cells.min((1, 3)), cells.max((1, 3))
        prefix = "check." if check else "main.base."
        main = [self.conv(x, prefix + "patch.0", 6), center, high, low]
        residual = []
        for k in (3, 7, 11):
            kernel = np.ones((k, k), np.uint8)
            hi, lo = cv.dilate(center, kernel), cv.erode(center, kernel)
            span = hi - lo
            main.extend([(hi + lo - 2 * center) / (span + 0.016), span])
            if not check and k < 11:
                residual.extend([(center - lo) / (span + 0.012), span])
        logits = self.stack(np.concatenate(main, -1), prefix + "net")
        p = sigmoid(logits[:, :, 0] / 2)
        if pol < 0:
            p = 1 - p
        if check:
            return p
        # Low visibility pulls the ink probability toward the undecided value 0.5.
        features = [
            self.conv(x, "main.detail.0", 6),
            center,
            1 - (1 - cells).max((1, 3)),
            logits * 0.1,
            *residual,
        ]
        vis = sigmoid(self.stack(np.concatenate(features, -1), "main.res")[:, :, 1] + 2)
        return 0.5 + (p - 0.5) * vis


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--weights', required=True, type=Path)
    parser.add_argument('--patch', required=True, type=Path, help='Full symbol without quiet zone, rectified and grid-aligned')
    parser.add_argument('--modules', required=True, type=int)
    parser.add_argument('--polarity', choices=(-1, 1), default=1, type=int)
    parser.add_argument('--out', required=True, type=Path, help='New NPZ file')
    args = parser.parse_args()
    try:
        bgr = cv.imread(str(args.patch), cv.IMREAD_COLOR)
        if bgr is None:
            raise ValueError('could not read patch')
        if args.out.suffix != '.npz':
            raise ValueError('output must end in .npz')
        p = Scorer(args.weights).predict(cv.cvtColor(bgr, cv.COLOR_BGR2RGB), args.modules, args.polarity)
        with args.out.open('xb') as output:
            np.savez_compressed(output, p_dark=p, ambiguity=1 - np.abs(2*p - 1))
    except (OSError, KeyError, ValueError, cv.error) as exc:
        parser.exit(1, f'cell_reference: {exc}\n')


if __name__ == '__main__':
    main()
