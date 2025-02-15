from conan import ConanFile
from conan.tools.cmake import cmake_layout

class BabelRecipe(ConanFile):
    name = "babel"
    version = "pre-alpha"
    user = "WehrWolff"
    channel = "unstable"
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
    test_requires = "gtest/1.11.0"
    generators = "CMakeDeps", "CMakeToolchain"
    build_policy = "missing"
    upload_policy = "skip"

    """
    source_folder = "src"
    build_folder = "build"
    package_folder = "artifacts"
    test_package_folder = "tests"
    """

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.compiler.libcxx

    def layout (self):
        cmake_layout(self)
