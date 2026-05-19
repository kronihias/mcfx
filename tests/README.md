# mcfx Plugin Test Suite

Python-based unit and regression tests for the mcfx audio plugin suite.

---

## Setup

### 1. Create and activate a virtual environment

```bash
# From the repo root
python3 -m venv tests/.venv
source tests/.venv/bin/activate
```

> To deactivate later: `deactivate`

### 2. Install Python dependencies

```bash
pip install -r tests/requirements.txt
```

---

## Running tests

### Tier-1 only (pedalboard — no extra build step needed)

Tests `mcfx_filter`, `mcfx_gain_delay`, `mcfx_delay`, and the diagonal-chain
bands of `mcfx_mimoeq` using stereo VST3 plugins.

```bash
source tests/.venv/bin/activate
pytest tests/test_filter.py tests/test_gain_delay.py tests/test_delay.py tests/test_mimoeq.py -v
```

### All tests including convolver and mimoeq MIMO (needs `mcfx_testhost`)

Build the testhost first (only needed once, or after changing plugin code):

```bash
cmake -B _build -DBUILD_VST3=ON -DBUILD_TESTHOST=ON
cmake --build _build --target mcfx_testhost mcfx_mimoeq_VST3
```

Then run everything:

```bash
source tests/.venv/bin/activate
pytest tests/ -v
```

### Update golden reference files

Run this after an **intentional** DSP change to accept the new output as correct:

```bash
pytest tests/ --update-golden
```

---

## Structure

```
tests/
├── reference/            # Python DSP reference implementations
│   ├── filter_ref.py     # IIR coefficients matching JUCE IIRCoefficients
│   ├── gain_ref.py       # Gain/delay formulas
│   ├── convolver_ref.py  # scipy.signal.fftconvolve wrapper
│   └── mimoeq_ref.py     # APVTS parameter conversions (freq/Q/gain ↔ normalized)
├── golden/               # Stored .npy reference outputs (committed to git)
├── conftest.py           # pytest fixtures, plugin paths, shared helpers
├── test_filter.py        # mcfx_filter — IIR accuracy + regression
├── test_gain_delay.py    # mcfx_gain_delay — gain level, delay, mute, phase
├── test_delay.py         # mcfx_delay — sample-accurate delay
├── test_convolver.py     # mcfx_convolver — matrix convolution (needs testhost)
├── test_mimoeq.py        # mcfx_mimoeq — diagonal IIR, MIMO routing, JSON config
├── requirements.txt
└── pytest.ini
```

---

## How it works

**Tier-1 (pedalboard, stereo):** Loads VST3 plugins directly from `_build/vst3/`.
Per-channel DSP (filter, delay, gain) is channel-independent, so stereo results
are fully representative of any channel count.

**Tier-2 (mcfx_testhost, arbitrary channels):** A small JUCE console app reads a
WAV file and JSON parameter file, processes through the plugin at any channel
count, and writes a WAV output. Python calls it via `subprocess` and compares
the output to a scipy reference. For `mcfx_mimoeq`, a JSON config file is passed
via the `"Mimoeq Config File"` key, which loads diagonal and MIMO path bands
directly (including FIR, delay, gain, and IIR band types).

**Golden files:** On the first run of each regression test, the plugin output is
saved to `tests/golden/<name>.npy`. Subsequent runs compare against it.
Commit the golden files alongside the code so CI can detect regressions.

---

## Tolerance reference

| Plugin / DSP type          | atol   | Why |
|----------------------------|--------|-----|
| IIR filter (freq resp)     | 0.5 dB | parameter smoothing ramp, float32 vs float64 |
| HP/LP filter (freq resp)   | 1.0 dB | same, slightly looser for Butterworth |
| Gain                       | 0.2 dB | ramp-up over one block |
| Delay (sample index)       | exact  | integer ring buffer, no fractional delay |
| Convolution / FIR          | 1e-3   | partitioned FFT vs single-pass scipy |
| MIMO amplitude             | 1e-4   | float32 multiply chain |
| Golden regression (IIR)    | 1e-5   | bit-exact float32 output |
| Golden regression (MIMO)   | 1e-4   | FIR partitioned convolution tolerance |
