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
 * Method table: maps Python function names to C functions.
 * Currently empty, we will add our statistical functions here later.
 */
static PyMethodDef SensorMethods[] = {
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
