import os

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps

def configure_boost(recipe):
    recipe.options["boost"].shared = True

class FLibrary(ConanFile):
    name = "FLibrary"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("boost/1.88.0")

    def configure(self):
        configure_boost(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()
