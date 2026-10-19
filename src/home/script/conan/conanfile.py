import os
import platform

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps

def configure_boost(recipe):
    recipe.options["boost"].shared = True
    recipe.options["boost"].header_only = True
    recipe.options["boost"].error_code_header_only = True

def configure_xercesc(recipe):
    recipe.options["xerces-c"].char_type = 'char16_t'
    recipe.options["xerces-c"].shared = False
    recipe.options["xerces-c"].network = False
    if recipe.settings.os == "Linux":
        recipe.options["xerces-c"].transcoder = "icu"

def configure_icu(recipe):
    recipe.options["icu"].shared = True
    recipe.options["icu"].with_extras = False
    recipe.options["icu"].with_samples = False

def configure_libjxl(recipe):
    recipe.options["libjxl"].shared = False

def configure_djvulibre(recipe):
    recipe.options["djvulibre"].shared = True

def configure_poppler(recipe):
    recipe.options["poppler"].shared = False

def configure_libxml2(recipe):
    recipe.options["libxml2"].shared = False

def configure_zlib(recipe):
    recipe.options["zlib"].shared = False

def configure_sqlite3(recipe):
    recipe.options["sqlite3"].threadsafe = 0
    recipe.options["sqlite3"].enable_fts5 = True
    
def run_local_recipe(recipe, lib, version):
    local_recipe = os.path.join(recipe.recipe_folder, "recipe", lib, version)
    if os.path.exists(local_recipe):
        recipe.run(f"conan export {local_recipe} --version={version}")
    else:
        recipe.output.warning(f"Cannot find local {lib} recipe")

class FLibrary(ConanFile):
    name = "FLibrary"
    settings = "os", "compiler", "build_type", "arch"

    icu_version = "78.2"
    if platform.system() == "Linux":
        icu_version = "74.2"
    
    def requirements(self):
        self.requires("boost/1.89.0")
        self.requires("plog/1.1.10")
        self.requires("xerces-c/3.3.0")
        self.requires("libjxl/0.12.0")
        self.requires("cimg/3.3.2")
        self.requires("sqlite3/3.53.4")
        self.requires("djvulibre/3.5.28")
        self.requires("poppler/25.11.0")
        self.requires("libxml2/2.15.3")
        self.requires("zlib/1.3.2")
        self.requires(f"icu/{self.icu_version}")

    def configure(self):
        configure_boost(self)
        configure_xercesc(self)
        configure_icu(self)
        configure_libjxl(self)
        configure_sqlite3(self)
        configure_djvulibre(self)
        configure_poppler(self)
        configure_libxml2(self)
        configure_zlib(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()

    def init(self):
        run_local_recipe(self, "sqlite3", "3.53.4")
        run_local_recipe(self, "icu", self.icu_version)
