import os

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps

class FLibrary(ConanFile):
    name = "FLibrary"
    settings = "os", "compiler", "build_type", "arch"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()
