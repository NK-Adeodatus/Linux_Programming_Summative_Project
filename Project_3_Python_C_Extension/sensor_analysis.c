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
 * Method table: maps Python function names to C functions.
 */
static PyMethodDef SensorMethods[] = {
    {"average",     sensor_average,     METH_VARARGS, "Compute the arithmetic mean of a dataset."},
    {"range_value", sensor_range_value, METH_VARARGS, "Compute the range (max - min) of a dataset."},
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
