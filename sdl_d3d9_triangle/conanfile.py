from conans import ConanFile, tools
from os import getenv
from random import getrandbits
from distutils.dir_util import copy_tree

class SdlPure(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    # dependencies used in deploy binaries
    # conan-center
    requires = ["sdl/2.0.18"]
    # optional dependencies
    def requirements(self):
        if self.settings.os == "Windows":
            # storm.jfrog.io
            self.requires("directx/9.0@storm/prebuilt")

    generators = "cmake_multi"
