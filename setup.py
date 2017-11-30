from Cython.Build import cythonize
from setuptools import find_packages, setup
from setuptools.extension import Extension

setup(
    name="wac2wav",
    version="0.1",
    packages=find_packages(),
    ext_modules=cythonize(
        [Extension("wac2wav", ["wac2wav.pyx", "c/wac2wav.c"])]))
