import os
import platform
import sys

from pathlib import Path
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps

current_file = Path(__file__).resolve()
project_root = current_file.parent.parent.parent.parent
sys.path.append(str(project_root))

import ext.scripts.conan.util as u


class FLibrary(ConanFile):
    name = "FLibrary"
    settings = "os", "compiler", "build_type", "arch"

    icu_version = "78.2"
    if platform.system() == "Linux":
        icu_version = "74.2"
    
    def requirements(self):
        self.requires("boost/1.89.0")
        self.requires("gtest/1.18.0")
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
        u.configure_boost(self)
        u.configure_xercesc(self)
        u.configure_icu(self)
        u.configure_libjxl(self)
        u.configure_sqlite3(self)
        u.configure_djvulibre(self)
        u.configure_poppler(self)
        u.configure_libxml2(self)
        u.configure_zlib(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()

    def init(self):
        local_recipe_path = os.path.join(self.recipe_folder, "..", "..", "..", "ext", "scripts", "conan", "recipe")
        u.run_local_recipe(self, local_recipe_path, "sqlite3", "3.53.4")
        u.run_local_recipe(self, local_recipe_path, "icu", self.icu_version)
