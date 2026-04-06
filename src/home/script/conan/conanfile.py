import os

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps

def configure_boost(recipe):
    recipe.options["boost"].shared = True
    recipe.options["boost"].header_only = True
    recipe.options["boost"].error_code_header_only = True

def configure_xercesc(recipe):
    recipe.options["xerces-c"].char_type = 'char16_t'
    recipe.options["xerces-c"].shared = False

def configure_icu(recipe):
    recipe.options["icu"].shared = True

def configure_libjxl(recipe):
    recipe.options["libjxl"].shared = False

def configure_sqlite3(recipe):
    recipe.options["sqlite3"].threadsafe = 0
    recipe.options["sqlite3"].enable_fts5 = True

class FLibrary(ConanFile):
    name = "FLibrary"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("boost/1.90.0")
        self.requires("plog/1.1.10")
        self.requires("xerces-c/3.3.0")
        self.requires("icu/78.2")
        self.requires("libjxl/0.11.2")
        self.requires("cimg/3.3.2")
        self.requires("sqlite3/3.51.0")

    def configure(self):
        configure_boost(self)
        configure_xercesc(self)
        configure_icu(self)
        configure_libjxl(self)
        configure_sqlite3(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()
