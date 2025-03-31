# Building the Project

## Prerequisites

- A modern C++17 compiler ([GCC-8](https://gcc.gnu.org/), [Clang-6.0](https://clang.llvm.org/), MSVC 2017 or above)
- [CMake](https://cmake.org/)
- [Conan](https://conan.io/) (optional, for dependency management)
- Coverage tools like lcov or llvm-cov (optional, for code coverage reports)
- External dependencies:
    - [Boost](https://www.boost.org/)
    - [Google test (gtest)](https://github.com/google/googletest) (optional, for testing)

## Build Instructions

### CMake (without Conan)

When not using Conan, dependencies have to be installed manually. Most of the time there isn't anything else to do since `find_package()` in CMakeLists.txt will find the needed packages if you have them installed.
If CMake can't find a package, using the `--debug-find-pkg=<package>` flag might help you in figuring out why.
Generally these are the steps to follow, for further information see https://github.com/WehrWolff/babel/blob/main/.github/workflows/cmake-multi-platform.yml:

1. Clone the repository: `git clone https://github.com/WehrWolff/babel.git` <br> or download the latest [release](https://github.com/WehrWolff/babel/releases) (recommended)
2. Configure CMake (optionally pass [CMakeOptions](#cmake-options)): `cmake -B <build_directory> -S <source_directory>`
3. Build project with configured options: `cmake --build <build_directory> --config <build_type>`

### Conan (with CMake)

When using Conan with CMake, dependencies are managed automatically. You can build the project with `conan build` or use `conan create` for a full build, test, and install cycle, using the same settings and options.
Generally these are the steps to follow, for further information see https://github.com/WehrWolff/babel/blob/main/.github/workflows/conan-multi-platform.yml:

1. Clone the repository: `git clone https://github.com/WehrWolff/babel.git` <br> or download the latest [release](https://github.com/WehrWolff/babel/releases) (recommended)
2. Build using Conan:
    - `conan build . --output-folder=./build --build=missing --settings=compiler=<compiler> --settings=compiler.version=<compiler_version> --settings=compiler.cppstd=20 --settings=build_type=<build_type> --options='&:c_compiler=<c_compiler>' --options='&:cxx_compiler=<cxx_compiler>' --settings=compiler.libcxx=libstdc++`
    - Or in short: `conan build . -s compiler=<compiler> -s compiler.version=<compiler_version> -s compiler.cppstd=20 -s build_type=<build_type> -o c_compiler<c_compiler> -o cxx_compiler=<cxx_compiler> -s compiler.libcxx=libstdc++`
    - Or if you have a [profile](#conan-profiles): `conan build . -pr <name>`

### Compiler Flags

Certain compiler flags are known to cause issues with the project and should not be used. For instance, aggressive optimization 
flags like `-O3` and `-Ofast` can cause the compiler to make optimizations that may not be compatible with the code, leading to 
unexpected behavior or bugs. Therefore, these flags should not be used.

For testing, certain flags should be used to make the program less error-prone. The `-Wall`, `-Werror`, and `-Wpedantic` flags are 
particularly useful. The `-Wall` flag enables all warning messages, which can help catch potential issues in the code. The `-Werror` 
flag treats warnings as errors, causing the compilation to fail if any warnings are generated. The `-Wpedantic` flag ensures that 
the code adheres to the ISO C and ISO C++ standards, which can help prevent non-standard behavior.

### CMake Options

Below are some commonly used CMake options for configuring your build:

- **`BUILD_SHARED_LIBS`:** Enables or disables the generation of shared libraries.
- **`BUILD_WITH_MT`:** Valid only for MSVC, builds libraries as MultiThreaded DLL.
- **`BUILD_TESTING`:** Controls whether generation of tests is enabled.
- **`CMAKE_BUILD_TYPE`:** Specifies the build type (e.g., `Release`, `Debug`, `RelWithDebInfo`). The default is `Release`.
- **`CMAKE_TOOLCHAIN_FILE`:** Use this to specify the Conan-generated toolchain file. Example: `-DCMAKE_TOOLCHAIN_FILE=./build/build/Release/generators/conan_toolchain.cmake`.
- **`CMAKE_CXX_COMPILER` & `CMAKE_C_COMPILER`:** Specify which compilers to use (e.g., `clang++`, `gcc`, `cl` for MSVC).
- **`CMAKE_CXX_OUTPUT_EXTENSION_REPLACE`:** Replaces the default C++ object file extension (with the one specified by `CMAKE_CXX_OUTPUT_EXTENSION`).
- **`Template_MEMCHECK`:** Enables the LLVM AddressSanitizer to detect memory errors when using the Clang compiler.
- **`ENABLE_DOXYGEN`:** Enables the generation of project documentation using Doxygen. Activate it with `-D<project_name>_ENABLE_DOXYGEN=1` during CMake configuration, then build with the target `doxygen-docs` to generate a `docs/` directory.

### Conan Profiles

- Use `conan profile detect --exist-ok` to ensure you have a default profile.
- You can create a new profile with `conan profile detect --name <name>`.
- Then edit it in you preferred text editor, for example `nvim $(conan profile path <name>)`. Below is an example conan configuration for Babel:

    ```ini
    [settings]
    arch=x86_64
    build_type=Release
    compiler=clang
    compiler.cppstd=20
    compiler.libcxx=libstdc++11
    compiler.version=19
    os=Linux

    [options]
    &:c_compiler=clang
    &:cxx_compiler=clang++

    [buildenv]
    C=clang
    CXX=clang++
    ```

## Installing the Project

When using `conan create`, there is no need to manually run the installation step, as it is automatically handled. If you're not using `conan create`, you can install an already built project by running the `install` target with CMake:

```bash
cmake --build <build_directory> --target install --config <desired_config>
```

This will install the built project to the appropriate location on your system.

## Running the Application

After building the project, you can run the application with `./project_name`

## Running the Tests

When using `conan create`, unit tests are automatically run, eliminating the need for manual testing (e.g., with `ctest`). Conan also validates the package for production use, ensuring correct installation, dependency configuration, and consistent behavior across platforms. If you're not using `conan create`, you can run the tests manually with the `ctest` command, which is part of the CMake toolset and is used to manage and run tests for CMake-based projects. It's a powerful tool that 
allows you to specify which tests to run, control the verbosity of the output, and even run tests in parallel.

- **Running All Tests:** To run all the tests in your test suite, you simply use the `ctest` command.
- **Listing Tests:** You can list the names of the tests in the test suite with the `-N` flag.
- **Running a Subset of Tests:** ctest provides several options for running a subset of tests. You can run tests by name with the `-R` flag,
  by label with the `-L` flag, or by number with the `-I` flag.
- **Running Tests in Parallel:** ctest also supports running tests in parallel using the `-j` flag.
- **Rerunning Failed Tests:** If some tests fail, you can rerun only the failed tests using the `--rerun-failed` flag.

## codecov

Code coverage allow you to see how much of your code is actually covered by your 
test suite. That is, which lines of code are actually run during the tests. 

To do this, the code must be compiled with compiler flags that allow the 
executable to collect information about which lines of code are called during 
execution. For GCC or Clang this means adding the `--coverage` flag, which is 
done in the `CMakeLists.txt` configuration file.

## .clang-format

[Clang Format](https://clang.llvm.org/docs/ClangFormat.html) is a tool to 
automatically format your code according to a set style configuration. This is 
incredibly useful because it frees you up from having to worry about things like 
indenting, spacing out your code or breaking long lines of code, just type in 
your code and let Clang Format clean it up for you.

If you're using Arch btw, here's how to install clang-format:

```bash
$ sudo pacman -S clang-format
```

Note that most IDEs will allow you to automatically run Clang Format when the 
file is saved, which even saves you from manually running the tool in the first 
place.

## .clang-tidy

[Clang Tidy](http://clang.llvm.org/extra/clang-tidy/) is a clang-based C++ 
linter tool. A *linter* will analyzing your code to check for common programming 
bugs and stylistic errors. This might seem similar to the warnings often given 
by the compiler, but a linter will have a much more comprehensive set of tests 
that examines the *structure* of your code rather than the often line-by-line 
checking done by the compiler.  

If you're using arch btw, here's how to install clang-tidy:

```bash
$ sudo pacman -S clang-tidy
```
