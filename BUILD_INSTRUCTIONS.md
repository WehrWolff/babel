# Building the Project

## Prerequisites

- A modern C++17 compiler ([GCC-8](https://gcc.gnu.org/), [Clang-6.0](https://clang.llvm.org/), MSVC 2017 or above)
- [CMake](https://cmake.org/)
- Conan (optional, for dependency management)
- [Other Dependencies]

Please note that certain compilers or compiler flags are not supported and should not be used. This is because they might 
not work as expected with this project. For instance, GDB is currently unsupported for debugging.

## Build Instructions

1. Clone the repository: `git clone https://github.com/WehrWolff/babel.git` <br> or download the latest [release](https://github.com/WehrWolff/babel/releases) (recommended)
2. Create a build directory: `mkdir build && cd build`
3. Run CMake: `cmake ..`
4. Compile the code: `make`

## CMake Options

- `BUILD_SHARED_LIBS`: Enables or disables the generation of shared libraries
- `BUILD_WITH_MT`: Valid only for MSVC, builds libraries as MultiThreaded DLL
- `BUILD_TESTING`: If activated, you need to perform a `conan install` step in advance to fetch the `doctest` dependency.

## Using CMake with a Compiler

CMake is a powerful tool for managing the build process of your project. It allows you to write platform-independent build scripts, 
which can then be used to generate build files for a variety of build systems.

To use CMake with Clang, you can follow these steps:

1. Create a `CMakeLists.txt` file in the root directory of your project. This file will contain the instructions for building your project.
2. Run CMake with the path to your source files and the path to the build directory as arguments. For example: `cmake -S . -B build`
3. Build your project with the `cmake --build` command. For example: `cmake --build build`

Clang is a compiler based on the LLVM compiler infrastructure. It's known for its excellent diagnostics and support for modern C++ features. 
To use Clang with CMake, you can set the `CXX` and `CC` variables in your `CMakeLists.txt` file to point to the Clang compiler. For example:

```cmake
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_C_COMPILER "/usr/bin/clang")
```

This will tell CMake to use Clang for compiling C++ and C code.

GCC is another popular C++ compiler. It's widely used and has excellent support for the C++ standard library. However, since Clang is known to 
provide better diagnostics and support for modern C++ features, we recommend using Clang for building your project.

## codecov

Code coverage allow you to see how much of your code is actually covered by your 
test suite. That is, which lines of code are actually run during the tests. 

To do this, the code must be compiled with compiler flags that allow the 
executable to collect information about which lines of code are called during 
execution. For GCC or Clang this means adding the `--coverage` flag, which is 
done in the `CMakeLists.txt` configuration file.

## AddressSanitizer

There is an optional component enabled via CMake that can use the [LLVM AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html) to detect memory errors.
This is turned on by default for the clang builds on Travis, so you will see any errors on there if it's configured.

You can also run it yourself, provided you are using the clang compiler, by using the `Template_MEMCHECK` option when running CMake.
Simply enable the option, then configure, build, and test:

```bash
cmake -DTemplate_MEMCHECK=TRUE /path/to/project
make
ctest
```
The test will fail at the first error.
It is unlikely that you will encounter a false positive when using the address sanitizer, so if you do see an error, best not to ignore it!

## .clang-format

[Clang Format](https://clang.llvm.org/docs/ClangFormat.html) is a tool to 
automatically format your code according to a set style configuration. This is 
incredibly useful because it frees you up from having to worry about things like 
indenting, spacing out your code or breaking long lines of code, just type in 
your code and let Clang Format clean it up for you.

You can install Clang Format on Ubuntu using `apt`:

```bash
$ sudo apt install clang-format
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

You can install Clang Tidy on Ubuntu using `apt`:

```bash
$ sudo apt install clang-tidy
```

## Compiling with Clang

It is advised that you use Clang to build the project with CMake but you can also use it directly, using Clang directly could look similar to this:

    /usr/bin/clang++ -fcolor-diagnostics -fansi-escape-codes -g -std=c++20 -I/<include_path> program.cpp -o program

## Compiling with GCC

Although it is recommended to compile with Clang and/or CMake you can use GCC as an alternative:

    g++ -g -std=c++20 -I/<include_path> program.cpp -o program

## Compiling with GDB

> [!NOTE]
> Currently compiling with GDB is known to cause issues and is therefor not supported. A fix to this issue will be worked on but this does not have a high priority, that
means that compiling using GDB might not be available for a long time or might even never be supported.

## Compiler Flags

Certain compiler flags are known to cause issues with the project and should not be used. For instance, aggressive optimization 
flags like `-O3` and `-Ofast` can cause the compiler to make optimizations that may not be compatible with the code, leading to 
unexpected behavior or bugs. Therefore, these flags should not be used.

For testing, certain flags should be used to make the program less error-prone. The `-Wall`, `-Werror`, and `-Wpedantic` flags are 
particularly useful. The `-Wall` flag enables all warning messages, which can help catch potential issues in the code. The `-Werror` 
flag treats warnings as errors, causing the compilation to fail if any warnings are generated. The `-Wpedantic` flag ensures that 
the code adheres to the ISO C and ISO C++ standards, which can help prevent non-standard behavior.

## Running the Application

After building the project, you can run the application with `./project_name`

## Testing

To run the tests, use the following command: `ctest`

## Installing the Project

To install an already built project, you need to run the `install` target with CMake:

    cmake --build <build_directory> --target install --config <desired_config>

## Generating the Documentation

In order to generate documentation for the project, you need to configure the build to use Doxygen. This is easily done, by modifying 
the workflow shown above as follows:

    mkdir build/
    cd build/ cmake .. -D<project_name>_ENABLE_DOXYGEN=1 -DCMAKE_INSTALL_PREFIX=/absolute/path/to/custom/install/directory cmake --build . --target doxygen-docs

This will generate a `docs/` directory in the project's root directory.

## Running the Tests

The ctest command is part of the CMake toolset and is used to manage and run tests for a CMake-based project. It's a powerful tool that 
allows you to specify which tests to run, control the verbosity of the output, and even run tests in parallel.

- **Running All Tests:** To run all the tests in your test suite, you simply use the `ctest` command.
- **Listing Tests:** You can list the names of the tests in the test suite with the `-N` flag.
- **Running a Subset of Tests:** ctest provides several options for running a subset of tests. You can run tests by name with the `-R` flag,
  by label with the `-L` flag, or by number with the `-I` flag.
- **Running Tests in Parallel:** ctest also supports running tests in parallel using the `-j` flag.
- **Rerunning Failed Tests:** If some tests fail, you can rerun only the failed tests using the `--rerun-failed` flag.
