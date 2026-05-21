# mcfx benchmark log

Tracks performance of the FFT-bound code in `mcfx_convolver` (also used by
`mcfx_mimoeq` for its long-FIR path). Numbers are produced by
[bench_convolver.py](bench_convolver.py):

```
pytest tests/bench_convolver.py --bench -s
```

The testhost reports the wall time spent inside the `processBlock` loop only
— plugin load, IR loading, `prepareToPlay`, and WAV I/O are excluded. Each
configuration processes 30 s of noise through a diagonal IR matrix.

## How to add a run

Compare apples-to-apples: same machine, same OS, no other heavy load. Take
2–3 runs and quote the median realtime factor (single-run jitter is around
±10–20 % on the larger configurations). Add a new section below in
chronological order with:

- Date (ISO)
- FFT backend (e.g. `FFTW3 3.3.11 (vcpkg)`, `chowdsp_fft @ <sha>`)
- Hardware (CPU model, RAM, OS)
- Build flags relevant to the FFT path (SIMD set enabled, static/shared)
- The raw table from the bench

---

## 2026-05-21 — FFTW3 3.3.11 via vcpkg (baseline)

- **Backend:** FFTW3 3.3.11, static, single-precision, `WITH_COMBINED_THREADS=ON`
- **SIMD enabled:** SSE2, AVX, AVX2 (vcpkg `fftw3[sse2,avx,avx2,threads]` feature set on the `x64-windows-static-md-release` overlay triplet)
- **Hardware:** Intel Core i7-6700T @ 2.80 GHz (4 cores / 8 threads, Skylake), 16 GB RAM, Windows 10 Pro
- **Toolchain:** MSVC 19.40 (Visual Studio 2022 Community), Release

| config                       | process (s) | realtime |
|------------------------------|------------:|---------:|
| 2ch x 1024-tap diagonal      |        0.02 |    1244x |
| 2ch x 8192-tap diagonal      |        0.06 |     513x |
| 8ch x 8192-tap diagonal      |        0.19 |     162x |
| 8ch x 32768-tap diagonal     |        0.22 |     135x |
| 8ch x 131072-tap diagonal    |        0.38 |      78x |
| 16ch x 8192-tap diagonal     |        0.35 |      85x |
| 32ch x 8192-tap diagonal     |        1.19 |      25x |

> Single run, not median — bring more data on the next entry.
