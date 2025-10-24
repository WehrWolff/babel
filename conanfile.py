from conan import ConanFile
from conan.tools.files import get, copy
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.system.package_manager import Apt, Dnf, PacMan, Zypper
from conan.errors import ConanInvalidConfiguration

import os
from pathlib import Path
class BabelRecipe(ConanFile):
    name = "babel"
    version = "v0.0.0-pre-alpha"
    user = "wehrwolff"
    channel = "unstable" # Consider changing this to "stable" for a release version
    description = "The Babel programming language"
    license = "BSL-1.0"
    author = "WehrWolff <135031808+WehrWolff@users.noreply.github.com>"
    topics = ("babel", "language", "programming-language", "compiler")
    homepage = "https://github.com/WehrWolff/babel"
    url = "https://github.com/WehrWolff/babel"
    
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    options = {"c_compiler": ["ANY", None], "cxx_compiler": ["ANY", None]}
    # Default option to prevent errors on conan install, where these are not needed and/or used
    default_options = {"c_compiler": "N/A", "cxx_compiler": "N/A"}
    languages = "C++"

    requires = "boost/[>=1.83.0]", "llvm-core/[>=19.1.7]"
    test_requires = "gtest/[>=1.11.0]"
    generators = "CMakeDeps", "CMakeToolchain"
    build_policy = "missing"
    upload_policy = "skip"

    # Consider adding source, package and test package folders
    build_folder = os.path.dirname(__file__) + "/build"
    test_package_folder = os.path.dirname(__file__) + "/test_package"

    def configure(self):
        if self.settings.get_safe("os") == "Windows":
            self.settings.rm_safe("compiler.libcxx")

    def build(self):
        cmake = CMake(self)

        # Note that build_type is needed because the default Windows generator (Visual Studio) is a multi-configuration generator
        cmake.configure(
            variables={
                "CMAKE_POLICY_DEFAULT_CMP0091": "NEW",
                "CMAKE_CXX_COMPILER": self.options.cxx_compiler,
                "CMAKE_C_COMPILER": self.options.c_compiler,
                "CMAKE_BUILD_TYPE": self.settings.build_type,
            },
            cli_args=[
                f"-B {self.build_folder}",
                f"-S {self.source_folder}"
            ]
        )

        cmake.build(cli_args=[f"--config {self.settings.build_type}"])

        if can_run(self):
            # Note that --build-config is needed because the default Windows generator (Visual Studio) is a multi-configuration generator
            cmake.ctest(["--build-config", f"{self.settings.build_type}", "--output-on-failure", "--verbose"])

    def export_sources(self):
        copy(self, "*", src=self.recipe_folder, dst=self.export_sources_folder)

    def package(self):
        cmake = CMake(self)
        cmake.install()
        cmake.install(cli_args=["--prefix", f"{Path.home() / '.babel'}"])

    def layout (self):
        cmake_layout(self)

    def source(self):
        # Add a sha256 hash code to check integrity in the future 
        try:
            get(self, f"{self.url}/releases/archive/refs/tags/{self.version}.tar.gz", sha256=None)
        except Exception:
            self.output.warning(f"Failed to download {self.url}/releases/archive/refs/tags/{self.version}.tar.gz")
            self.output.warning("Falling back to the local source folder")

    def system_requirements(self):
        # Depends on the platform or the tools.system.package_manager:tool configuration
        # Only one of these will be executed
        if self.settings.compiler == "gcc":
            Apt(self).install(["lcov"])
            Dnf(self).install(["lcov"])
            PacMan(self).install(["lcov"])
            Zypper(self).install(["lcov"])
        elif self.settings.compiler == "clang":
            Apt(self).install(["llvm"])
            Dnf(self).install(["llvm"])
            PacMan(self).install(["llvm"])
            Zypper(self).install(["llvm"])

    # Test is intended to prove the package is correctly created
    # It is not intended to run unit, integration or functional tests
    def test(self):
        if can_run(self):
            cmd = os.path.join(self.cpp.build.bindir, "example")
            self.run(cmd, env="conanrun")

    def validate(self):
        if self.settings.os not in ["Linux", "Windows", "Macos"]:
            raise ConanInvalidConfiguration("Currently only Linux, Windows and MacOS are supported.")
        
        if self.options.c_compiler == None:
            raise ConanInvalidConfiguration("The C compiler must be set. Use -o='&:c_compiler=' to specify the compiler.")
        
        if self.options.cxx_compiler == None:
            raise ConanInvalidConfiguration("The C++ compiler must be set. Use -o='&:cxx_compiler=' to specify the compiler.")
