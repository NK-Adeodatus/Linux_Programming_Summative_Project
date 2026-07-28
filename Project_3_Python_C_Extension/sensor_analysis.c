#define PY_SSIZE_T_CLEAN
#include <Python.h>

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
