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
    recipe.options["icu"].shared = False

def configure_libjxl(recipe):
    recipe.options["libjxl"].shared = False

def configure_qt(recipe):
    recipe.options["qt"].shared = True
    recipe.options["qt"].commercial = False
    recipe.options["qt"].openssl = False
    recipe.options["qt"].with_pcre2 = True
    recipe.options["qt"].with_glib = False
    recipe.options["qt"].with_doubleconversion = True
    recipe.options["qt"].with_harfbuzz = False
    recipe.options["qt"].with_libjpeg = 'libjpeg'
    recipe.options["qt"].with_libpng = True
    recipe.options["qt"].with_sqlite3 = False
    recipe.options["qt"].with_mysql = False
    recipe.options["qt"].with_pq = False
    recipe.options["qt"].with_odbc = False
    recipe.options["qt"].with_openal = False
    recipe.options["qt"].with_zstd = False
    recipe.options["qt"].with_gstreamer = False
    recipe.options["qt"].with_pulseaudio = False
    recipe.options["qt"].with_dbus = False
    recipe.options["qt"].with_atspi = False
    recipe.options["qt"].with_md4c = False
    recipe.options["qt"].gui = True
    recipe.options["qt"].widgets = True
    recipe.options["qt"].device = False
    recipe.options["qt"].cross_compile = False
    recipe.options["qt"].sysroot = False
    recipe.options["qt"].config = False
    recipe.options["qt"].multiconfiguration = False
    recipe.options["qt"].qtsvg = True
    recipe.options["qt"].qtdeclarative = False
    recipe.options["qt"].qtactiveqt = False
    recipe.options["qt"].qtscript = False
    recipe.options["qt"].qtmultimedia = False
    recipe.options["qt"].qttools = True
    recipe.options["qt"].qtxmlpatterns = False
    recipe.options["qt"].qttranslations = True
    recipe.options["qt"].qtdoc = False
    recipe.options["qt"].qthttpserver = True
    recipe.options["qt"].qtlocation = False
    recipe.options["qt"].qtsensors = False
    recipe.options["qt"].qtconnectivity = False
    recipe.options["qt"].qtwayland = False
    recipe.options["qt"].qt3d = False
    recipe.options["qt"].qtimageformats = True
    recipe.options["qt"].qtgraphicaleffects = False
    recipe.options["qt"].qtquickcontrols = False
    recipe.options["qt"].qtserialbus = False
    recipe.options["qt"].qtserialport = False
    recipe.options["qt"].qtwinextras = False
    recipe.options["qt"].qtandroidextras = False
    recipe.options["qt"].qtwebsockets = True
    recipe.options["qt"].qtwebchannel = False
    recipe.options["qt"].qtwebengine = False
    recipe.options["qt"].qtwebview = False
    recipe.options["qt"].qtquickcontrols2 = False
    recipe.options["qt"].qtpurchasing = False
    recipe.options["qt"].qtcharts = False
    recipe.options["qt"].qtdatavis3d = False
    recipe.options["qt"].qtvirtualkeyboard = False
    recipe.options["qt"].qtgamepad = False
    recipe.options["qt"].qtscxml = False
    recipe.options["qt"].qtspeech = False
    recipe.options["qt"].qtnetworkauth = False
    recipe.options["qt"].qtremoteobjects = False
    recipe.options["qt"].qtwebglplugin = False
    recipe.options["qt"].qtlottie = False
    recipe.options["qt"].qtquicktimeline = False
    recipe.options["qt"].qtquick3d = False
    recipe.options["qt"].qtknx = False
    recipe.options["qt"].qtmqtt = False
    recipe.options["qt"].qtcoap = False
    recipe.options["qt"].qtopcua = False
    recipe.options["qt"].essential_modules = False
    recipe.options["qt"].addon_modules = False
    recipe.options["qt"].deprecated_modules = False
    recipe.options["qt"].preview_modules = False

class FLibrary(ConanFile):
    name = "FLibrary"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("boost/1.88.0")
        self.requires("plog/1.1.10")
        self.requires("xerces-c/3.3.0")
        self.requires("icu/77.1")
        self.requires("7zip/25.00")
        self.requires("qt/6.8.3")
        self.requires("libjxl/0.11.1")

    def configure(self):
        configure_boost(self)
        configure_xercesc(self)
        configure_icu(self)
        configure_qt(self)
        configure_libjxl(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()
