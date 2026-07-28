from setuptools import setup, Extension

# Define the C extension module
module = Extension('sensor_analysis', sources=['sensor_analysis.c'])

setup(
    name='sensor_analysis',
    version='1.0',
    description='Python C Extension for High-Performance Data Processing',
    ext_modules=[module]
)
