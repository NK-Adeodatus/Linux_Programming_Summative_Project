# sensor_analysis — Python C Extension for High-Performance Data Processing

A native Python C extension module that performs statistical calculations
on environmental sensor data (e.g. soil moisture, temperature, humidity)
entirely in C, avoiding the overhead of pure-Python loops when processing
large volumes of real-time IoT readings.

## Files

| File | Purpose |
|---|---|
| `sensor_analysis.c` | C source: method implementations, method table, module definition, and init function. |
| `setup.py` | Build script (setuptools) that compiles `sensor_analysis.c` into an importable shared library. |
| `test_sensor_analysis.py` | Python test program exercising every function, including invalid input and boundary cases. |
| `.gitignore` | Excludes build artifacts (`build/`, `*.so`, `__pycache__/`) from version control. |

## Requirements

- Python 3.x with development headers (`python3-dev` / `python3-devel`)
- A C compiler (gcc/clang on Linux/macOS, MSVC on Windows)
- `setuptools` (usually bundled with Python)

## Build

The compiled `.so` / `.pyd` file is **not** committed to this repo — it's
platform- and Python-version-specific, so it must be built locally:

```bash
python3 setup.py build_ext --inplace
```

This compiles `sensor_analysis.c` and produces a shared library
(e.g. `sensor_analysis.cpython-312-x86_64-linux-gnu.so` on Linux) in the
project directory, ready to `import`.

## Run the tests

```bash
python3 test_sensor_analysis.py
```

Expected: every function is called and its result printed, followed by a
series of intentional error cases (wrong type, empty dataset, non-numeric
element, single-point variance) that each correctly raise a Python
exception instead of crashing.

## API

All functions accept a Python `list` or `tuple` of numeric values (`int`
or `float`); anything else raises `TypeError`, and an empty sequence
raises `ValueError`.

### `average(data) -> float`
Arithmetic mean: `sum(data) / len(data)`. O(n).

### `range_value(data) -> float`
`max(data) - min(data)`, computed in a single pass. O(n).

### `variance(data) -> float`
Sample variance (Bessel's correction, divides by `n - 1`), computed with
**Welford's online algorithm** for numerical stability — it never
subtracts two large, similar floating-point numbers, unlike the naive
"sum of squares minus n·mean²" approach. Requires at least 2 data points;
raises `ValueError` otherwise. O(n).

### `count_above(data, limit) -> int`
Number of readings strictly greater than `limit`. O(n).

### `statistics(data) -> dict`
Returns `{"samples": int, "average": float, "minimum": float, "maximum": float}`,
computed in one pass (cheaper than calling `average()` and `range_value()`
separately). O(n).

## Design notes

**Memory management.** No dynamic memory (`malloc`/`free`) is used. Every
function iterates directly over the caller's existing Python list/tuple
via `PySequence_GetItem`, converts each item to a C `double` with
`PyFloat_AsDouble`, and immediately releases the temporary reference with
`Py_DECREF`. `statistics()` additionally builds a result dict — the only
"new" allocations are the ordinary Python objects (`PyLong`, `PyFloat`,
`PyDict`) returned to the caller, which Python's own reference counting
then owns and manages.

**Numerical accuracy.** `variance()` uses Welford's algorithm specifically
to avoid catastrophic cancellation that both the two-pass and naive
one-pass formulas are prone to when values are large or clustered closely
together.

**Error handling.** All input validation is centralized in a single
helper, `validate_and_get_size()`, so every public function enforces the
same type/emptiness checks consistently and raises the appropriate
built-in Python exception (`TypeError` or `ValueError`) rather than
crashing or returning a sentinel value.