import numpy
from distutils.core import setup, Extension

extension_mod = Extension("cwdtw", ["pymodule.cpp"], include_dirs=[numpy.get_include(), "lib/wavelet/header"], library_dirs=["build/lib"], libraries=["proc", "5mer", "m", "wavelib"]) # "wletdtw"

setup(name = "cwdtw", ext_modules=[extension_mod])
