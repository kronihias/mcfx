"""
Reference DSP for mcfx_convolver — multichannel matrix convolution.

The plugin uses partitioned FFT convolution (Gardner scheme).
For numerical reference we use scipy.signal.fftconvolve which produces
the same result modulo float32 rounding (atol ~1e-3 for long IRs).
"""

import numpy as np
from scipy.signal import fftconvolve


def convolve_matrix(input_audio: np.ndarray, ir_matrix: np.ndarray) -> np.ndarray:
    """
    Matrix convolution: output[out_ch] = sum_over_in_ch(input[in_ch] * ir[in_ch, out_ch])

    Args:
        input_audio: float32 array, shape (n_in_ch, n_samples)
        ir_matrix:   float32 array, shape (n_in_ch, n_out_ch, ir_length)

    Returns:
        float32 array, shape (n_out_ch, n_samples)
    """
    n_in, n_samples = input_audio.shape
    n_in_ir, n_out, ir_len = ir_matrix.shape
    assert n_in == n_in_ir, "Input channels must match IR matrix rows"

    out = np.zeros((n_out, n_samples), dtype=np.float64)
    for i in range(n_in):
        for o in range(n_out):
            conv = fftconvolve(input_audio[i].astype(np.float64),
                               ir_matrix[i, o].astype(np.float64))
            out[o] += conv[:n_samples]

    return out.astype(np.float32)


def make_identity_ir(n_ch: int, length: int = 1) -> np.ndarray:
    """
    Create an identity IR matrix (each input routes directly to the same output).
    Shape: (n_ch, n_ch, length)
    """
    ir = np.zeros((n_ch, n_ch, length), dtype=np.float32)
    for i in range(n_ch):
        ir[i, i, 0] = 1.0
    return ir


def make_dirac(n_samples: int, delay: int = 0, amplitude: float = 1.0) -> np.ndarray:
    """Single-channel impulse signal."""
    sig = np.zeros(n_samples, dtype=np.float32)
    if delay < n_samples:
        sig[delay] = amplitude
    return sig
