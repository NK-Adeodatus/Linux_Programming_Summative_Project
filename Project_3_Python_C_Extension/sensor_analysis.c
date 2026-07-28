#define PY_SSIZE_T_CLEAN
#include <Python.h>

/*
 * Helper function: validate_and_get_size
 * Checks if the input is a list or tuple, ensures it is not empty,
 * and returns the size. Returns -1 if validation fails and sets a Python exception.
 */
static Py_ssize_t validate_and_get_size(PyObject *data) {
    if (!PyList_Check(data) && !PyTuple_Check(data)) {
        PyErr_SetString(PyExc_TypeError, "Input data must be a list or tuple.");
        return -1;
    }

    Py_ssize_t size = PySequence_Size(data);
    
    if (size == 0) {
        PyErr_SetString(PyExc_ValueError, "Dataset cannot be empty.");
        return -1;
    }
    
    return size;
}

/*
 * Function: sensor_average
 * Python name: average(data)
 *
 * Calculates the arithmetic mean of a list or tuple of floats.
 * Formula: mean = (sum of all values) / count
 * Time complexity: O(n) — one pass through the dataset.
 *
 * Memory management: No dynamic allocation is performed. We iterate directly
 * over the existing Python sequence objects, converting each to a C double on
 * the fly. All temporary PyObject* items retrieved via PySequence_GetItem are
 * new references and are immediately released with Py_DECREF after use.
 */
static PyObject *sensor_average(PyObject *self, PyObject *args) {
    PyObject *data;

    /* Parse the single argument from Python: 'O' means any Python object */
    if (!PyArg_ParseTuple(args, "O", &data)) {
        return NULL;
    }

    /* Validate type and get size using our helper */
    Py_ssize_t size = validate_and_get_size(data);
    if (size == -1) {
        return NULL; /* Exception already set by helper */
    }

    double sum = 0.0;

    for (Py_ssize_t i = 0; i < size; i++) {
        /* PySequence_GetItem returns a new reference; we must Py_DECREF it */
        PyObject *item = PySequence_GetItem(data, i);
        if (item == NULL) {
            return NULL; /* Index error — should not happen but defensive */
        }

        /* Convert the Python numeric object to a C double.
         * PyFloat_AsDouble also handles Python int objects gracefully. */
        double val = PyFloat_AsDouble(item);
        Py_DECREF(item); /* Release reference immediately after conversion */

        if (val == -1.0 && PyErr_Occurred()) {
            return NULL; /* Item was not a number; TypeError already set */
        }

        sum += val;
    }

    /* Return result as a Python float object */
    return PyFloat_FromDouble(sum / (double)size);
}

/*
 * Function: sensor_range_value
 * Python name: range_value(data)
 *
 * Returns the difference between the largest and smallest values.
 * Formula: range = max - min
 * Time complexity: O(n) — single pass, tracking min and max simultaneously.
 *
 * Memory management: Same pattern as average(). No dynamic allocation.
 * Each PyObject item is retrieved, converted to double, and immediately
 * released with Py_DECREF before moving to the next element.
 */
static PyObject *sensor_range_value(PyObject *self, PyObject *args) {
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O", &data)) {
        return NULL;
    }

    Py_ssize_t size = validate_and_get_size(data);
    if (size == -1) {
        return NULL;
    }

    /* Seed min and max with the first element */
    PyObject *first = PySequence_GetItem(data, 0);
    if (first == NULL) { return NULL; }
    double min_val = PyFloat_AsDouble(first);
    Py_DECREF(first);
    if (min_val == -1.0 && PyErr_Occurred()) { return NULL; }
    double max_val = min_val;

    /* Iterate from index 1, updating min and max */
    for (Py_ssize_t i = 1; i < size; i++) {
        PyObject *item = PySequence_GetItem(data, i);
        if (item == NULL) { return NULL; }
        double val = PyFloat_AsDouble(item);
        Py_DECREF(item);
        if (val == -1.0 && PyErr_Occurred()) { return NULL; }

        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    return PyFloat_FromDouble(max_val - min_val);
}

/*
 * Function: sensor_variance
 * Python name: variance(data)
 *
 * Returns the SAMPLE variance of the dataset.
 * Formula: s^2 = sum((xi - mean)^2) / (n - 1)  [Bessel's correction]
 * Time complexity: O(n) — one pass using Welford's online algorithm.
 *
 * NUMERICAL STABILITY NOTE:
 * The naive two-pass approach (compute mean, then sum squared deviations)
 * can suffer from catastrophic cancellation when values are very large or
 * very close together. The naive one-pass approach (sum of squares minus
 * n*mean^2) also suffers from precision loss for the same reason.
 * Welford's online algorithm avoids this entirely by computing variance
 * incrementally: each new value updates the mean and M2 accumulator
 * in a numerically stable way, never subtracting two large similar numbers.
 *
 * Memory management: No dynamic allocation. Items are fetched, converted,
 * and released (Py_DECREF) within each loop iteration.
 */
static PyObject *sensor_variance(PyObject *self, PyObject *args) {
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O", &data)) {
        return NULL;
    }

    Py_ssize_t size = validate_and_get_size(data);
    if (size == -1) {
        return NULL;
    }

    /* Sample variance requires at least 2 data points (denominator = n-1) */
    if (size < 2) {
        PyErr_SetString(PyExc_ValueError,
            "Sample variance requires at least 2 data points.");
        return NULL;
    }

    /* Welford's online algorithm variables */
    double mean = 0.0;  /* Running mean */
    double M2   = 0.0;  /* Running sum of squared deviations from the mean */

    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject *item = PySequence_GetItem(data, i);
        if (item == NULL) { return NULL; }
        double val = PyFloat_AsDouble(item);
        Py_DECREF(item);
        if (val == -1.0 && PyErr_Occurred()) { return NULL; }

        /* Welford's update step:
         * delta  = how far is this value from the current mean?
         * mean   = shift the mean toward this value
         * delta2 = how far is this value from the NEW updated mean?
         * M2     = accumulate the product of both deltas (cross-term)
         * This avoids ever subtracting two large, similar floating-point numbers. */
        double delta  = val - mean;
        mean         += delta / (double)(i + 1);
        double delta2 = val - mean;
        M2           += delta * delta2;
    }

    /* Bessel's correction: divide by (n-1) for sample variance */
    return PyFloat_FromDouble(M2 / (double)(size - 1));
}

/*
 * Function: sensor_count_above
 * Python name: count_above(data, limit)
 *
 * Returns the number of readings strictly greater than the given limit.
 * Formula: count of all x_i where x_i > limit
 * Time complexity: O(n) — one pass through the dataset.
 *
 * This function parses TWO arguments: a sequence (O) and a double (d).
 * The format string "Od" tells PyArg_ParseTuple to expect an object
 * followed by a C double directly converted from a Python float/int.
 *
 * Memory management: No dynamic allocation. Items are fetched, converted
 * to double, compared, and immediately released with Py_DECREF.
 * Returns a Python integer (PyLong), not a float.
 */
static PyObject *sensor_count_above(PyObject *self, PyObject *args) {
    PyObject *data;
    double limit;

    /* Parse two arguments: one Python object ('O') and one double ('d') */
    if (!PyArg_ParseTuple(args, "Od", &data, &limit)) {
        return NULL;
    }

    Py_ssize_t size = validate_and_get_size(data);
    if (size == -1) {
        return NULL;
    }

    long count = 0; /* Use a C long to hold the count before returning */

    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject *item = PySequence_GetItem(data, i);
        if (item == NULL) { return NULL; }
        double val = PyFloat_AsDouble(item);
        Py_DECREF(item);
        if (val == -1.0 && PyErr_Occurred()) { return NULL; }

        if (val > limit) {
            count++;
        }
    }

    /* Return an integer count as a Python long object */
    return PyLong_FromLong(count);
}

/* 
 * Method table: maps Python function names to C functions.
 */
static PyMethodDef SensorMethods[] = {
    {"average",     sensor_average,     METH_VARARGS, "Compute the arithmetic mean of a dataset."},
    {"range_value", sensor_range_value, METH_VARARGS, "Compute the range (max - min) of a dataset."},
    {"variance",    sensor_variance,    METH_VARARGS, "Compute the sample variance of a dataset."},
    {"count_above", sensor_count_above, METH_VARARGS, "Count readings strictly greater than a limit."},
    {NULL, NULL, 0, NULL}        /* Sentinel indicating the end of the array */
};

/* 
 * Module definition structure
 */
static struct PyModuleDef sensormodule = {
    PyModuleDef_HEAD_INIT,
    "sensor_analysis",           /* Name of the module */
    "C extension for fast sensor data processing.", /* Module documentation */
    -1,                          /* Size of per-interpreter state (-1 means global state) */
    SensorMethods                /* Pointer to the method table */
};

/* 
 * Module initialization function.
 * This is the ONLY non-static function exported.
 * Python looks for a function named PyInit_<module_name>.
 */
PyMODINIT_FUNC PyInit_sensor_analysis(void) {
    return PyModule_Create(&sensormodule);
}
