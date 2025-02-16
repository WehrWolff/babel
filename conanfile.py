import os

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import get
from conan.tools.system.package_manager import Apt, Dnf, PacMan, Zypper
from conan.errors import ConanInvalidConfiguration

class BabelRecipe(ConanFile):
    name = "babel"
    version = "v0.0.0-pre-alpha"
    user = "WehrWolff"
    channel = "unstable" # Consider changing this to "fresh" for a release version
    description = "The Babel programming language"
    license = "BSL-1.0"
    author = "WehrWolff <135031808+WehrWolff@users.noreply.github.com>"
    topics = ("babel", "language", "programming-language", "compiler")
    homepage = "https://github.com/WehrWolff/babel"
    url = "https://github.com/WehrWolff/babel"
    
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    options = {"compiler.cppstd": [11, 14, 17, 20, 23], "compiler.libcxx": ["libstdc++", "libstdc++11", "libc++"]}
    default_options = {"compiler.cppstd": 20, "compiler.libcxx": "libstdc++"} # compiler.version
    languages = "C++"

    requires = "boost/[>=1.83.0]"
    test_requires = "gtest/[>=1.11.0]"
    generators = "CMakeDeps", "CMakeToolchain"
    build_policy = "missing"
    upload_policy = "skip"

    #source_folder = "src"
    #build_folder = "build"
    #package_folder = "artifacts"
    #test_package_folder = "tests"

    def config_options(self):
        if self.settings.get_safe("os") == "Windows":
            self.options.rm_safe("compiler.libcxx")

    def build(self):
        cmake = CMake(self)

        # Note that build_type is needed because the default Windows generator (Visual Studio) is a multi-configuration generator
        #self.run("cmake -B ./build -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=./build/Release/generators/conan_toolchain.cmake -S ./..")

        cmake.configure(
            #build_script_folder=self.source_folder,  # Equivalent to -B ${{ steps.strings.outputs.build-output-dir }}
            variables={
                "CMAKE_POLICY_DEFAULT_CMP0091": "NEW",
                "CMAKE_CXX_COMPILER": "clang++",
                "CMAKE_C_COMPILER": "clang",
                "CMAKE_BUILD_TYPE": self.settings.build_type,
                "CMAKE_TOOLCHAIN_FILE": "./build/build/Release/generators/conan_toolchain.cmake" 
            },
            cli_args=[  # Use CLI args for anything that is not a variable or needs to be passed directly
                f"-B {self.build_folder}",  # Equivalent to -B ${{ steps.strings.outputs.build-output-dir }}
                f"-S {self.source_folder}"  # Equivalent to -S ${{ github.workspace }}
            ]
        )

        #self.run(f"cmake --build {os.path.join(self.recipe_folder, self.build_folder)} --config {self.settings.build_type}")
        cmake.build(cli_args=[f"--config {self.settings.build_type}"])

    def layout (self):
        cmake_layout(self)

    def source(self):
        # Add a sha256 hash code to check integrity in the future 
        get(self, f"{self.url}/releases/archive/refs/tags/{self.version}.tar.gz", sha256=None)

    def system_requirements(self):
        # Depending on the platform or the tools.system.package_manager:tool configuration
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
    # TODO: Change this to the intended behavior and provide another method for unit tests
    def test(self):
        self.run(f"ctest --build-config {self.settings.build_type} --output-on-failure --verbose")

    def validate(self):
        if self.settings.os not in ["Linux", "Windows", "Macos"]:
            raise ConanInvalidConfiguration("Currently only Linux, Windows and MacOS are supported")
